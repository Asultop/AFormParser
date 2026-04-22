#include "AFormParser/AFormParser.hpp"
#include "AFormParser/LuaRuntime.hpp"

#include <QFileInfo>
#include <QRegularExpression>

#include <functional>

namespace AFormParser {

namespace {

struct ExportContext {
	QMap<QString, QMap<QString, QString>> propertiesById;
	QMap<QString, QString> argValues;
	QMap<QString, QString> globalVars;
};

QString indentString(int indent)
{
	if (indent <= 0) {
		return QString();
	}
	return QString(indent * 4, QLatin1Char(' '));
}

QString escapeQuoted(const QString &value)
{
	QString out = value;
	out.replace(QStringLiteral("\\"), QStringLiteral("\\\\"));
	out.replace(QStringLiteral("\""), QStringLiteral("\\\""));
	out.replace(QStringLiteral("\n"), QStringLiteral("\\n"));
	out.replace(QStringLiteral("\t"), QStringLiteral("\\t"));
	return out;
}

QString quote(const QString &value)
{
	return QStringLiteral("\"") + escapeQuoted(value) + QStringLiteral("\"");
}

QString unescapeQuoted(const QString &value)
{
	QString out;
	out.reserve(value.size());

	bool escape = false;
	for (int i = 0; i < value.size(); ++i) {
		const QChar ch = value.at(i);
		if (!escape) {
			if (ch == QLatin1Char('\\')) {
				escape = true;
			} else {
				out.append(ch);
			}
			continue;
		}

		switch (ch.unicode()) {
		case 'n':
			out.append(QLatin1Char('\n'));
			break;
		case 't':
			out.append(QLatin1Char('\t'));
			break;
		case 'r':
			out.append(QLatin1Char('\r'));
			break;
		case '"':
			out.append(QLatin1Char('"'));
			break;
		case '\\':
			out.append(QLatin1Char('\\'));
			break;
		default:
			out.append(ch);
			break;
		}
		escape = false;
	}

	if (escape) {
		out.append(QLatin1Char('\\'));
	}
	return out;
}

bool isQuoted(const QString &value)
{
	const QString trimmed = value.trimmed();
	return trimmed.size() >= 2 && trimmed.startsWith(QLatin1Char('"')) && trimmed.endsWith(QLatin1Char('"'));
}

QString unquoteMaybe(const QString &value)
{
	const QString trimmed = value.trimmed();
	if (!isQuoted(trimmed)) {
		return trimmed;
	}
	return unescapeQuoted(trimmed.mid(1, trimmed.size() - 2));
}

QString stripInlineComment(const QString &line)
{
	bool inSingle = false;
	bool inDouble = false;
	bool escape = false;

	for (int i = 0; i < line.size(); ++i) {
		const QChar ch = line.at(i);
		const QChar next = (i + 1 < line.size()) ? line.at(i + 1) : QChar();

		if (!inSingle && !inDouble && ch == QLatin1Char('/') && next == QLatin1Char('/')) {
			return line.left(i);
		}

		if (escape) {
			escape = false;
			continue;
		}

		if ((inSingle || inDouble) && ch == QLatin1Char('\\')) {
			escape = true;
			continue;
		}

		if (!inSingle && ch == QLatin1Char('"')) {
			inDouble = !inDouble;
			continue;
		}

		if (!inDouble && ch == QLatin1Char('\'')) {
			inSingle = !inSingle;
			continue;
		}
	}

	return line;
}

struct ParsedLine {
	QString raw;
	QString noComment;
	QString text;
	int contentOffset = 0; // 0-based index in raw/noComment where text starts

	int toColumn(int textIndex) const
	{
		if (textIndex < 0) {
			textIndex = 0;
		}
		return contentOffset + textIndex + 1;
	}

	int tokenColumn(const QString &token) const
	{
		const int idx = text.indexOf(token);
		if (idx >= 0) {
			return toColumn(idx);
		}
		return toColumn(0);
	}
};

ParsedLine makeParsedLine(const QString &raw)
{
	ParsedLine out;
	out.raw = raw;
	out.noComment = stripInlineComment(raw);

	int begin = 0;
	while (begin < out.noComment.size() && out.noComment.at(begin).isSpace()) {
		++begin;
	}

	int end = out.noComment.size() - 1;
	while (end >= begin && out.noComment.at(end).isSpace()) {
		--end;
	}

	out.contentOffset = begin;
	if (end >= begin) {
		out.text = out.noComment.mid(begin, end - begin + 1);
	}

	return out;
}

int findUnterminatedQuoteColumnInText(const QString &text)
{
	bool inSingle = false;
	bool inDouble = false;
	bool escape = false;
	int singleStart = -1;
	int doubleStart = -1;

	for (int i = 0; i < text.size(); ++i) {
		const QChar ch = text.at(i);

		if (escape) {
			escape = false;
			continue;
		}

		if ((inSingle || inDouble) && ch == QLatin1Char('\\')) {
			escape = true;
			continue;
		}

		if (!inSingle && ch == QLatin1Char('"')) {
			if (!inDouble) {
				inDouble = true;
				doubleStart = i;
			} else {
				inDouble = false;
				doubleStart = -1;
			}
			continue;
		}

		if (!inDouble && ch == QLatin1Char('\'')) {
			if (!inSingle) {
				inSingle = true;
				singleStart = i;
			} else {
				inSingle = false;
				singleStart = -1;
			}
		}
	}

	if (inDouble && doubleStart >= 0) {
		return doubleStart + 1;
	}
	if (inSingle && singleStart >= 0) {
		return singleStart + 1;
	}
	return -1;
}

int findCharOutsideQuotes(const QString &text, QChar target, int from = 0)
{
	bool inSingle = false;
	bool inDouble = false;
	bool escape = false;

	for (int i = qMax(0, from); i < text.size(); ++i) {
		const QChar ch = text.at(i);

		if (escape) {
			escape = false;
			continue;
		}

		if ((inSingle || inDouble) && ch == QLatin1Char('\\')) {
			escape = true;
			continue;
		}

		if (!inSingle && ch == QLatin1Char('"')) {
			inDouble = !inDouble;
			continue;
		}

		if (!inDouble && ch == QLatin1Char('\'')) {
			inSingle = !inSingle;
			continue;
		}

		if (!inSingle && !inDouble && ch == target) {
			return i;
		}
	}

	return -1;
}

int findMatchingRightParenOutsideQuotes(const QString &text, int leftParenPos)
{
	if (leftParenPos < 0 || leftParenPos >= text.size() || text.at(leftParenPos) != QLatin1Char('(')) {
		return -1;
	}

	bool inSingle = false;
	bool inDouble = false;
	bool escape = false;
	int depth = 0;

	for (int i = leftParenPos; i < text.size(); ++i) {
		const QChar ch = text.at(i);

		if (escape) {
			escape = false;
			continue;
		}

		if ((inSingle || inDouble) && ch == QLatin1Char('\\')) {
			escape = true;
			continue;
		}

		if (!inSingle && ch == QLatin1Char('"')) {
			inDouble = !inDouble;
			continue;
		}

		if (!inDouble && ch == QLatin1Char('\'')) {
			inSingle = !inSingle;
			continue;
		}

		if (inSingle || inDouble) {
			continue;
		}

		if (ch == QLatin1Char('(')) {
			++depth;
		} else if (ch == QLatin1Char(')')) {
			--depth;
			if (depth == 0) {
				return i;
			}
		}
	}

	return -1;
}

bool equalsIgnoreCase(const QString &lhs, const QString &rhs)
{
	return lhs.compare(rhs, Qt::CaseInsensitive) == 0;
}

QString normalizeDollarFunctionCall(const QString &input)
{
	static const QRegularExpression re(QStringLiteral("\\$([A-Za-z_][A-Za-z0-9_]*)\\s*\\("));

	QString result;
	result.reserve(input.size());
	int cursor = 0;

	QRegularExpressionMatchIterator it = re.globalMatch(input);
	while (it.hasNext()) {
		const QRegularExpressionMatch match = it.next();
		result.append(input.mid(cursor, match.capturedStart() - cursor));
		result.append(match.captured(1));
		result.append(QLatin1Char('('));
		cursor = match.capturedEnd();
	}
	result.append(input.mid(cursor));
	return result;
}

QString trimLuaLine(const QString &line)
{
	QString out;
	out.reserve(line.size());

	bool inSingle = false;
	bool inDouble = false;
	bool escape = false;

	for (int i = 0; i < line.size(); ++i) {
		const QChar ch = line.at(i);
		const QChar next = (i + 1 < line.size()) ? line.at(i + 1) : QChar();

		if (!inSingle && !inDouble && ch == QLatin1Char('-') && next == QLatin1Char('-')) {
			break;
		}

		if (escape) {
			escape = false;
			out.append(ch);
			continue;
		}

		if ((inSingle || inDouble) && ch == QLatin1Char('\\')) {
			escape = true;
			out.append(ch);
			continue;
		}

		if (!inSingle && ch == QLatin1Char('"')) {
			inDouble = !inDouble;
			out.append(ch);
			continue;
		}

		if (!inDouble && ch == QLatin1Char('\'')) {
			inSingle = !inSingle;
			out.append(ch);
			continue;
		}

		out.append(ch);
	}

	out = out.trimmed();
	while (out.endsWith(QLatin1Char(';'))) {
		out.chop(1);
		out = out.trimmed();
	}
	return out;
}

QString normalizeLuaExpression(QString expr)
{
	expr.replace(QStringLiteral("!="), QStringLiteral("~="));
	expr.replace(QStringLiteral("&&"), QStringLiteral(" and "));
	expr.replace(QStringLiteral("||"), QStringLiteral(" or "));

	return expr;
}

QString normalizeScriptCodeForLua(const QString &rawCode)
{
	if (rawCode.trimmed().isEmpty()) {
		return rawCode;
	}

	static const QRegularExpression legacyFnRe(
		QStringLiteral("^(\\s*)([A-Za-z_][A-Za-z0-9_]*)\\s*\\(([^)]*)\\)\\s*\\{\\s*$"));
	static const QRegularExpression jsFnRe(
		QStringLiteral("^(\\s*)function\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*\\(([^)]*)\\)\\s*\\{\\s*$"));
	static const QRegularExpression jsIfRe(
		QStringLiteral("^(\\s*)if\\s*\\((.*)\\)\\s*\\{\\s*$"));
	static const QRegularExpression varAssignRe(
		QStringLiteral("^(\\s*)var\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*(.+)$"));
	static const QRegularExpression plusAssignRe(
		QStringLiteral("^(\\s*)([A-Za-z_][A-Za-z0-9_]*)\\s*\\+=\\s*(.+)$"));

	QStringList outLines;
	const QStringList lines = rawCode.split(QLatin1Char('\n'));
	outLines.reserve(lines.size());

	for (const QString &line : lines) {
		const QString statement = trimLuaLine(line);
		if (statement.isEmpty()) {
			outLines.append(QString());
			continue;
		}

		QRegularExpressionMatch match = legacyFnRe.match(statement);
		if (match.hasMatch()) {
			outLines.append(QStringLiteral("%1function %2(%3)")
							 .arg(match.captured(1), match.captured(2), match.captured(3).trimmed()));
			continue;
		}

		match = jsFnRe.match(statement);
		if (match.hasMatch()) {
			outLines.append(QStringLiteral("%1function %2(%3)")
							 .arg(match.captured(1), match.captured(2), match.captured(3).trimmed()));
			continue;
		}

		match = jsIfRe.match(statement);
		if (match.hasMatch()) {
			outLines.append(QStringLiteral("%1if %2 then")
							 .arg(match.captured(1), normalizeLuaExpression(match.captured(2).trimmed())));
			continue;
		}

		if (statement == QStringLiteral("}")) {
			outLines.append(QStringLiteral("end"));
			continue;
		}

		match = varAssignRe.match(statement);
		if (match.hasMatch()) {
			outLines.append(QStringLiteral("%1local %2 = %3")
							 .arg(match.captured(1), match.captured(2), normalizeLuaExpression(match.captured(3).trimmed())));
			continue;
		}

		match = plusAssignRe.match(statement);
		if (match.hasMatch()) {
			const QString varName = match.captured(2);
			outLines.append(QStringLiteral("%1%2 = %2 .. %3")
							 .arg(match.captured(1), varName, normalizeLuaExpression(match.captured(3).trimmed())));
			continue;
		}

		if (statement.startsWith(QStringLiteral("return "))) {
			outLines.append(QStringLiteral("return %1").arg(normalizeLuaExpression(statement.mid(7).trimmed())));
			continue;
		}

		if (statement.startsWith(QStringLiteral("if ")) && statement.endsWith(QLatin1Char('{'))) {
			QString cond = statement.mid(3, statement.size() - 4).trimmed();
			if (cond.startsWith(QLatin1Char('(')) && cond.endsWith(QLatin1Char(')')) && cond.size() >= 2) {
				cond = cond.mid(1, cond.size() - 2).trimmed();
			}
			outLines.append(QStringLiteral("if %1 then").arg(normalizeLuaExpression(cond)));
			continue;
		}

		outLines.append(normalizeLuaExpression(statement));
	}

	return outLines.join(QLatin1Char('\n'));
}

struct LuaFunctionDef {
	QString name;
	QStringList params;
	QStringList body;
};

QStringList splitTopLevelComma(const QString &text)
{
	QStringList out;
	QString current;
	int parenDepth = 0;
	bool inSingle = false;
	bool inDouble = false;
	bool escape = false;

	for (int i = 0; i < text.size(); ++i) {
		const QChar ch = text.at(i);

		if (escape) {
			current.append(ch);
			escape = false;
			continue;
		}

		if ((inSingle || inDouble) && ch == QLatin1Char('\\')) {
			current.append(ch);
			escape = true;
			continue;
		}

		if (!inSingle && ch == QLatin1Char('"')) {
			inDouble = !inDouble;
			current.append(ch);
			continue;
		}

		if (!inDouble && ch == QLatin1Char('\'')) {
			inSingle = !inSingle;
			current.append(ch);
			continue;
		}

		if (!inSingle && !inDouble) {
			if (ch == QLatin1Char('(')) {
				++parenDepth;
				current.append(ch);
				continue;
			}
			if (ch == QLatin1Char(')') && parenDepth > 0) {
				--parenDepth;
				current.append(ch);
				continue;
			}
			if (ch == QLatin1Char(',') && parenDepth == 0) {
				out.append(current.trimmed());
				current.clear();
				continue;
			}
		}

		current.append(ch);
	}

	if (!current.trimmed().isEmpty() || text.endsWith(QLatin1Char(','))) {
		out.append(current.trimmed());
	}

	return out;
}

bool parseLuaFunctions(const QString &rawCode, QMap<QString, LuaFunctionDef> *functions, ParseError *error)
{
	if (!functions) {
		return false;
	}
	functions->clear();

	const QString normalized = normalizeScriptCodeForLua(rawCode);
	const QStringList lines = normalized.split(QLatin1Char('\n'));
	static const QRegularExpression fnRe(
		QStringLiteral("^function\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*\\(([^)]*)\\)\\s*$"));
	static const QRegularExpression idRe(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));

	for (int i = 0; i < lines.size(); ++i) {
		const QString stmt = trimLuaLine(lines.at(i));
		if (stmt.isEmpty()) {
			continue;
		}

		const QRegularExpressionMatch fnMatch = fnRe.match(stmt);
		if (!fnMatch.hasMatch()) {
			if (error) {
				error->line = i + 1;
				error->column = 1;
				error->message = QStringLiteral("Scripts 语法错误: 期望 function 定义");
			}
			return false;
		}

		LuaFunctionDef def;
		def.name = fnMatch.captured(1);

		const QString paramsText = fnMatch.captured(2).trimmed();
		if (!paramsText.isEmpty()) {
			const QStringList params = paramsText.split(QLatin1Char(','));
			for (const QString &paramRaw : params) {
				const QString param = paramRaw.trimmed();
				if (!idRe.match(param).hasMatch()) {
					if (error) {
						error->line = i + 1;
						error->column = 1;
						error->message = QStringLiteral("Scripts 语法错误: 参数名非法");
					}
					return false;
				}
				def.params.append(param);
			}
		}

		int ifDepth = 0;
		bool closed = false;
		for (++i; i < lines.size(); ++i) {
			const QString bodyStmt = trimLuaLine(lines.at(i));
			if (bodyStmt.isEmpty()) {
				continue;
			}

			if (bodyStmt.startsWith(QStringLiteral("if ")) && bodyStmt.endsWith(QStringLiteral(" then"))) {
				const QString condExpr = bodyStmt.mid(3, bodyStmt.size() - 8).trimmed();
				if (condExpr.isEmpty()) {
					if (error) {
						error->line = i + 1;
						error->column = 1;
						error->message = QStringLiteral("Scripts 语法错误: if 条件不能为空");
					}
					return false;
				}
			}

			if (bodyStmt.startsWith(QStringLiteral("return "))) {
				const QString returnExpr = bodyStmt.mid(7).trimmed();
				if (returnExpr.isEmpty() || returnExpr.endsWith(QStringLiteral(".."))) {
					if (error) {
						error->line = i + 1;
						error->column = 1;
						error->message = QStringLiteral("Scripts 语法错误: return 表达式不完整");
					}
					return false;
				}
			}

			QRegularExpressionMatch assignMatch =
				QRegularExpression(QStringLiteral("^(?:local\\s+)?([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*(.+)$")).match(bodyStmt);
			if (assignMatch.hasMatch()) {
				const QString rhs = assignMatch.captured(2).trimmed();
				if (rhs.isEmpty() || rhs.endsWith(QStringLiteral(".."))) {
					if (error) {
						error->line = i + 1;
						error->column = 1;
						error->message = QStringLiteral("Scripts 语法错误: 赋值表达式不完整");
					}
					return false;
				}
			}

			if (bodyStmt.startsWith(QStringLiteral("if ")) && bodyStmt.endsWith(QStringLiteral(" then"))) {
				++ifDepth;
				def.body.append(bodyStmt);
				continue;
			}

			if (bodyStmt == QStringLiteral("end")) {
				if (ifDepth > 0) {
					--ifDepth;
					def.body.append(bodyStmt);
					continue;
				}
				closed = true;
				break;
			}

			def.body.append(bodyStmt);
		}

		if (!closed) {
			if (error) {
				error->line = lines.size();
				error->column = 1;
				error->message = QStringLiteral("Scripts 语法错误: function 缺少 end");
			}
			return false;
		}

		functions->insert(def.name, def);
	}

	return true;
}

QString unquoteLuaLiteral(const QString &token)
{
	const QString trimmed = token.trimmed();
	if (trimmed.size() >= 2 && trimmed.startsWith(QLatin1Char('\'')) && trimmed.endsWith(QLatin1Char('\''))) {
		return trimmed.mid(1, trimmed.size() - 2);
	}
	return unquoteMaybe(trimmed);
}

QString evalLuaConcatExpr(const QString &expr, const QMap<QString, QString> &vars)
{
	QStringList parts;
	QString current;
	int parenDepth = 0;
	bool inSingle = false;
	bool inDouble = false;
	bool escape = false;

	for (int i = 0; i < expr.size(); ++i) {
		const QChar ch = expr.at(i);

		if (escape) {
			current.append(ch);
			escape = false;
			continue;
		}

		if ((inSingle || inDouble) && ch == QLatin1Char('\\')) {
			current.append(ch);
			escape = true;
			continue;
		}

		if (!inSingle && ch == QLatin1Char('"')) {
			inDouble = !inDouble;
			current.append(ch);
			continue;
		}

		if (!inDouble && ch == QLatin1Char('\'')) {
			inSingle = !inSingle;
			current.append(ch);
			continue;
		}

		if (!inSingle && !inDouble) {
			if (ch == QLatin1Char('(')) {
				++parenDepth;
				current.append(ch);
				continue;
			}
			if (ch == QLatin1Char(')') && parenDepth > 0) {
				--parenDepth;
				current.append(ch);
				continue;
			}
			if (parenDepth == 0 && ch == QLatin1Char('.') && i + 1 < expr.size() && expr.at(i + 1) == QLatin1Char('.')) {
				parts.append(current.trimmed());
				current.clear();
				++i;
				continue;
			}
		}

		current.append(ch);
	}

	if (!current.trimmed().isEmpty() || expr.endsWith(QStringLiteral(".."))) {
		parts.append(current.trimmed());
	}

	if (parts.isEmpty()) {
		parts.append(expr.trimmed());
	}

	QString out;
	for (const QString &part : parts) {
		const QString token = part.trimmed();
		if (token.isEmpty()) {
			continue;
		}
		if ((token.size() >= 2 && token.startsWith(QLatin1Char('"')) && token.endsWith(QLatin1Char('"')))
			|| (token.size() >= 2 && token.startsWith(QLatin1Char('\'')) && token.endsWith(QLatin1Char('\'')))) {
			out.append(unquoteLuaLiteral(token));
			continue;
		}

		auto it = vars.constFind(token);
		if (it != vars.constEnd()) {
			out.append(it.value());
			continue;
		}

		out.append(token);
	}

	return out;
}

bool evalLuaConditionExpr(const QString &expr, const QMap<QString, QString> &vars)
{
	QString cond = expr.trimmed();
	if (cond.isEmpty()) {
		return false;
	}

	int opPos = cond.indexOf(QStringLiteral("~="));
	if (opPos < 0) {
		opPos = cond.indexOf(QStringLiteral("!="));
	}
	if (opPos >= 0) {
		const QString left = evalLuaConcatExpr(cond.left(opPos), vars);
		const QString right = evalLuaConcatExpr(cond.mid(opPos + 2), vars);
		return left != right;
	}

	opPos = cond.indexOf(QStringLiteral("=="));
	if (opPos >= 0) {
		const QString left = evalLuaConcatExpr(cond.left(opPos), vars);
		const QString right = evalLuaConcatExpr(cond.mid(opPos + 2), vars);
		return left == right;
	}

	const QString value = evalLuaConcatExpr(cond, vars).trimmed();
	if (value.isEmpty()) {
		return false;
	}
	if (equalsIgnoreCase(value, QStringLiteral("false")) || value == QStringLiteral("0")) {
		return false;
	}
	return true;
}

QString executeLuaBody(const QStringList &lines, int begin, int end, QMap<QString, QString> &vars, bool *didReturn)
{
	for (int i = begin; i < end; ++i) {
		const QString stmt = trimLuaLine(lines.at(i));
		if (stmt.isEmpty()) {
			continue;
		}

		if (stmt.startsWith(QStringLiteral("if ")) && stmt.endsWith(QStringLiteral(" then"))) {
			const QString condExpr = stmt.mid(3, stmt.size() - 8).trimmed();
			int nested = 0;
			int closeIndex = -1;
			for (int j = i + 1; j < end; ++j) {
				const QString nestedStmt = trimLuaLine(lines.at(j));
				if (nestedStmt.startsWith(QStringLiteral("if ")) && nestedStmt.endsWith(QStringLiteral(" then"))) {
					++nested;
					continue;
				}
				if (nestedStmt == QStringLiteral("end")) {
					if (nested == 0) {
						closeIndex = j;
						break;
					}
					--nested;
				}
			}

			if (closeIndex < 0) {
				if (didReturn) {
					*didReturn = true;
				}
				return QString();
			}

			if (evalLuaConditionExpr(condExpr, vars)) {
				const QString returned = executeLuaBody(lines, i + 1, closeIndex, vars, didReturn);
				if (didReturn && *didReturn) {
					return returned;
				}
			}

			i = closeIndex;
			continue;
		}

		if (stmt == QStringLiteral("end")) {
			continue;
		}

		if (stmt.startsWith(QStringLiteral("return "))) {
			if (didReturn) {
				*didReturn = true;
			}
			return evalLuaConcatExpr(stmt.mid(7), vars);
		}

		QRegularExpressionMatch match =
			QRegularExpression(QStringLiteral("^local\\s+([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*(.+)$")).match(stmt);
		if (match.hasMatch()) {
			vars.insert(match.captured(1), evalLuaConcatExpr(match.captured(2), vars));
			continue;
		}

		match = QRegularExpression(QStringLiteral("^([A-Za-z_][A-Za-z0-9_]*)\\s*=\\s*(.+)$")).match(stmt);
		if (match.hasMatch()) {
			vars.insert(match.captured(1), evalLuaConcatExpr(match.captured(2), vars));
			continue;
		}
	}

	return QString();
}

QString evalLuaFunctionCall(const QString &name,
							 const QStringList &argTokens,
							 const QMap<QString, LuaFunctionDef> &functions,
							 const QMap<QString, QString> &ctxArgs)
{
	auto fnIt = functions.constFind(name);
	if (fnIt == functions.constEnd()) {
		return QString();
	}

	QMap<QString, QString> vars = ctxArgs;
	const LuaFunctionDef &fn = fnIt.value();
	for (int i = 0; i < fn.params.size(); ++i) {
		QString value;
		if (i < argTokens.size()) {
			const QString token = argTokens.at(i).trimmed();
			if ((token.size() >= 2 && token.startsWith(QLatin1Char('"')) && token.endsWith(QLatin1Char('"')))
				|| (token.size() >= 2 && token.startsWith(QLatin1Char('\'')) && token.endsWith(QLatin1Char('\'')))) {
				value = unquoteLuaLiteral(token);
			} else {
				auto argIt = vars.constFind(token);
				value = (argIt != vars.constEnd()) ? argIt.value() : token;
			}
		}
		vars.insert(fn.params.at(i), value);
	}

	bool didReturn = false;
	return executeLuaBody(fn.body, 0, fn.body.size(), vars, &didReturn);
}

void setContextProperty(ExportContext &ctx, const QString &id, const QString &key, const QString &value)
{
	const QString trimmedId = id.trimmed();
	const QString trimmedKey = key.trimmed();
	if (trimmedId.isEmpty() || trimmedKey.isEmpty()) {
		return;
	}
	ctx.propertiesById[trimmedId][trimmedKey] = value;
}

QString getContextProperty(const ExportContext &ctx, const QString &id, const QString &key)
{
	if (id.trimmed().isEmpty() || key.trimmed().isEmpty()) {
		return QString();
	}

	auto idIt = ctx.propertiesById.constFind(id);
	if (idIt == ctx.propertiesById.constEnd()) {
		for (auto it = ctx.propertiesById.constBegin(); it != ctx.propertiesById.constEnd(); ++it) {
			if (equalsIgnoreCase(it.key(), id)) {
				idIt = it;
				break;
			}
		}
	}

	if (idIt == ctx.propertiesById.constEnd()) {
		return QString();
	}

	const auto &props = idIt.value();
	auto propIt = props.constFind(key);
	if (propIt != props.constEnd()) {
		return propIt.value();
	}

	for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
		if (equalsIgnoreCase(it.key(), key)) {
			return it.value();
		}
	}

	return QString();
}

QString toJsLiteral(const QString &value)
{
	const QString trimmed = value.trimmed();
	if (trimmed.isEmpty()) {
		return QStringLiteral("\"\"");
	}

	if (equalsIgnoreCase(trimmed, QStringLiteral("true")) || equalsIgnoreCase(trimmed, QStringLiteral("false"))) {
		return trimmed.toLower();
	}

	bool numberOk = false;
	trimmed.toDouble(&numberOk);
	if (numberOk) {
		return trimmed;
	}

	return quote(trimmed);
}

QString replaceReferencePattern(const QString &input,
							   const QRegularExpression &pattern,
							   const ExportContext &ctx,
							   bool asJsLiteral)
{
	QString result;
	result.reserve(input.size());

	int cursor = 0;
	QRegularExpressionMatchIterator it = pattern.globalMatch(input);
	while (it.hasNext()) {
		const QRegularExpressionMatch match = it.next();
		result.append(input.mid(cursor, match.capturedStart() - cursor));

		QString value = getContextProperty(ctx, match.captured(1).trimmed(), match.captured(2).trimmed());
		if (asJsLiteral) {
			value = toJsLiteral(value);
		}
		result.append(value);
		cursor = match.capturedEnd();
	}

	result.append(input.mid(cursor));
	return result;
}

QString replaceAllReferences(const QString &input, const ExportContext &ctx, bool asJsLiteral)
{
	static const QRegularExpression refDollar(QStringLiteral(R"(\$\(([^)]+)\)\.([A-Za-z_][A-Za-z0-9_]*))"));
	static const QRegularExpression refStar(QStringLiteral(R"(\*\(([^)]+)\)\.([A-Za-z_][A-Za-z0-9_]*))"));

	QString out = replaceReferencePattern(input, refDollar, ctx, asJsLiteral);
	out = replaceReferencePattern(out, refStar, ctx, asJsLiteral);

	static const QRegularExpression dollarBrace(QStringLiteral(R"(\$\{([^}]+)\})"));
	struct Replacement { int pos; int len; QString value; };
	QVector<Replacement> toReplace;

	QRegularExpressionMatchIterator it = dollarBrace.globalMatch(out);
	while (it.hasNext()) {
		const QRegularExpressionMatch match = it.next();
		QString varName = match.captured(1).trimmed();
		if (varName.startsWith("__")) {
			auto vit = ctx.globalVars.find(varName);
			if (vit != ctx.globalVars.end()) {
				toReplace.append({static_cast<int>(match.capturedStart()), static_cast<int>(match.capturedLength()), vit.value()});
			}
		}
	}

	for (int i = toReplace.size() - 1; i >= 0; --i) {
		out.replace(toReplace[i].pos, toReplace[i].len, toReplace[i].value);
	}

	return out;
}

QStringList splitTopLevelPlus(const QString &expr)
{
	QStringList parts;
	QString current;
	int parenDepth = 0;
	bool inSingle = false;
	bool inDouble = false;
	bool escape = false;

	for (int i = 0; i < expr.size(); ++i) {
		const QChar ch = expr.at(i);

		if (escape) {
			current.append(ch);
			escape = false;
			continue;
		}

		if ((inSingle || inDouble) && ch == QLatin1Char('\\')) {
			current.append(ch);
			escape = true;
			continue;
		}

		if (!inSingle && ch == QLatin1Char('"')) {
			inDouble = !inDouble;
			current.append(ch);
			continue;
		}

		if (!inDouble && ch == QLatin1Char('\'')) {
			inSingle = !inSingle;
			current.append(ch);
			continue;
		}

		if (!inSingle && !inDouble) {
			if (ch == QLatin1Char('(')) {
				++parenDepth;
				current.append(ch);
				continue;
			}

			if (ch == QLatin1Char(')') && parenDepth > 0) {
				--parenDepth;
				current.append(ch);
				continue;
			}

			if (ch == QLatin1Char('+') && parenDepth == 0) {
				parts.append(current.trimmed());
				current.clear();
				continue;
			}
		}

		current.append(ch);
	}

	if (!current.isEmpty() || expr.endsWith(QLatin1Char('+'))) {
		parts.append(current.trimmed());
	}

	return parts;
}

bool splitTernaryExpr(const QString &expr, QString *condition, QString *whenTrue, QString *whenFalse)
{
	bool inSingle = false;
	bool inDouble = false;
	bool escape = false;
	int parenDepth = 0;
	int questionPos = -1;
	int colonPos = -1;
	int nestedQuestion = 0;

	for (int i = 0; i < expr.size(); ++i) {
		const QChar ch = expr.at(i);

		if (escape) {
			escape = false;
			continue;
		}
		if ((inSingle || inDouble) && ch == QLatin1Char('\\')) {
			escape = true;
			continue;
		}
		if (!inSingle && ch == QLatin1Char('"')) {
			inDouble = !inDouble;
			continue;
		}
		if (!inDouble && ch == QLatin1Char('\'')) {
			inSingle = !inSingle;
			continue;
		}
		if (inSingle || inDouble) {
			continue;
		}
		if (ch == QLatin1Char('(')) {
			++parenDepth;
			continue;
		}
		if (ch == QLatin1Char(')') && parenDepth > 0) {
			--parenDepth;
			continue;
		}
		if (parenDepth > 0) {
			continue;
		}

		if (ch == QLatin1Char('?')) {
			if (questionPos < 0) {
				questionPos = i;
			} else {
				++nestedQuestion;
			}
			continue;
		}

		if (ch == QLatin1Char(':') && questionPos >= 0) {
			if (nestedQuestion == 0) {
				colonPos = i;
				break;
			}
			--nestedQuestion;
		}
	}

	if (questionPos < 0 || colonPos < 0) {
		return false;
	}

	if (condition) {
		*condition = expr.left(questionPos).trimmed();
	}
	if (whenTrue) {
		*whenTrue = expr.mid(questionPos + 1, colonPos - questionPos - 1).trimmed();
	}
	if (whenFalse) {
		*whenFalse = expr.mid(colonPos + 1).trimmed();
	}
	return true;
}

bool parseFunctionCallExpr(const QString &expr, QString *name, QString *argsText)
{
	const QString trimmed = expr.trimmed();
	const int leftParen = findCharOutsideQuotes(trimmed, QLatin1Char('('));
	if (leftParen <= 0) {
		return false;
	}
	const int rightParen = findMatchingRightParenOutsideQuotes(trimmed, leftParen);
	if (rightParen < 0 || rightParen != trimmed.size() - 1) {
		return false;
	}

	const QString fnName = trimmed.left(leftParen).trimmed();
	static const QRegularExpression fnNameRe(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
	if (!fnNameRe.match(fnName).hasMatch()) {
		return false;
	}

	if (name) {
		*name = fnName;
	}
	if (argsText) {
		*argsText = trimmed.mid(leftParen + 1, rightParen - leftParen - 1);
	}
	return true;
}

QString evalLuaCallFromDoc(const QString &part, const Document *doc, const ExportContext &ctx, bool *handled)
{
	if (handled) {
		*handled = false;
	}
	if (!doc || !doc->scripts || doc->scripts->rawCode.trimmed().isEmpty()) {
		return QString();
	}

	QString fnName;
	QString argsText;
	if (!parseFunctionCallExpr(part, &fnName, &argsText)) {
		return QString();
	}

	if (fnName == QLatin1String("lua_call")) {
		QStringList callArgs = splitTopLevelComma(argsText.trimmed());
		if (callArgs.isEmpty()) {
			return QString();
		}
		fnName = callArgs.first();
		if (fnName.startsWith(QLatin1Char('"')) && fnName.endsWith(QLatin1Char('"'))) {
			fnName = fnName.mid(1, fnName.length() - 2);
		} else if (fnName.startsWith(QLatin1Char('\'')) && fnName.endsWith(QLatin1Char('\''))) {
			fnName = fnName.mid(1, fnName.length() - 2);
		}
		argsText = callArgs.mid(1).join(QLatin1String(", "));
	}

	const QStringList argTokens = splitTopLevelComma(argsText.trimmed());
	QStringList evaluatedArgs;
	for (const QString &token : argTokens) {
		const QString trimmedToken = token.trimmed();
		if ((trimmedToken.size() >= 2 && trimmedToken.startsWith(QLatin1Char('"'))) && trimmedToken.endsWith(QLatin1Char('"'))) {
			evaluatedArgs.append(unquoteMaybe(trimmedToken));
		} else if ((trimmedToken.size() >= 2 && trimmedToken.startsWith(QLatin1Char('\''))) && trimmedToken.endsWith(QLatin1Char('\''))) {
			evaluatedArgs.append(unquoteMaybe(trimmedToken));
		} else {
			auto argIt = ctx.argValues.constFind(trimmedToken);
			if (argIt != ctx.argValues.constEnd()) {
				evaluatedArgs.append(argIt.value());
			} else {
				evaluatedArgs.append(trimmedToken);
			}
		}
	}

	QString result;
	QString execError;
	result = doc->executeScriptFunction(fnName, evaluatedArgs, &execError);

	if (handled) {
		*handled = !result.isEmpty() || !execError.isEmpty();
	}
	return result;
}

QString evaluateExpression(const QString &expression, const Document *doc, const ExportContext &ctx)
{
	const QString trimmedExpr = expression.trimmed();
	if (trimmedExpr.isEmpty()) {
		return QString();
	}

	if (isQuoted(trimmedExpr)) {
		return unquoteMaybe(trimmedExpr);
	}

	if (trimmedExpr.startsWith("__")) {
		auto vit = ctx.globalVars.find(trimmedExpr);
		if (vit != ctx.globalVars.end()) {
			return vit.value();
		}
	}

	QString fallbackExpr = replaceAllReferences(trimmedExpr, ctx, false);
	fallbackExpr = normalizeDollarFunctionCall(fallbackExpr);

	QString condExpr;
	QString trueExpr;
	QString falseExpr;
	if (splitTernaryExpr(fallbackExpr, &condExpr, &trueExpr, &falseExpr)) {
		const bool cond = evalLuaConditionExpr(condExpr, ctx.argValues);
		return evaluateExpression(cond ? trueExpr : falseExpr, doc, ctx);
	}

	const QStringList plusParts = splitTopLevelPlus(fallbackExpr);
	if (plusParts.size() > 1) {
		QString joined;
		for (const QString &partRaw : plusParts) {
			const QString part = partRaw.trimmed();
			if (part.isEmpty()) {
				continue;
			}

			if (isQuoted(part)) {
				joined.append(unquoteMaybe(part));
				continue;
			}

			auto argIt = ctx.argValues.constFind(part);
			if (argIt != ctx.argValues.constEnd()) {
				joined.append(argIt.value());
				continue;
			}

			bool handledCall = false;
			const QString luaCallValue = evalLuaCallFromDoc(part, doc, ctx, &handledCall);
			if (handledCall) {
				joined.append(luaCallValue);
				continue;
			}

			joined.append(part);
		}
		return joined;
	}

	bool handledCall = false;
	const QString luaCallValue = evalLuaCallFromDoc(fallbackExpr, doc, ctx, &handledCall);
	if (handledCall) {
		return luaCallValue;
	}

	auto argIt = ctx.argValues.constFind(fallbackExpr);
	if (argIt != ctx.argValues.constEnd()) {
		return argIt.value();
	}

	return fallbackExpr;
}

QString applyTemplate(const QString &text, const Document *doc, const ExportContext &ctx)
{
	static const QRegularExpression templateRe(QStringLiteral(R"(\$\{([^{}]+)\})"));

	QString result;
	result.reserve(text.size());

	int cursor = 0;
	QRegularExpressionMatchIterator it = templateRe.globalMatch(text);
	while (it.hasNext()) {
		const QRegularExpressionMatch match = it.next();
		result.append(text.mid(cursor, match.capturedStart() - cursor));
		result.append(evaluateExpression(match.captured(1), doc, ctx));
		cursor = match.capturedEnd();
	}

	result.append(text.mid(cursor));
	return result;
}

bool evaluateEnabled(const QString &enabledExpr, const Document *doc, const ExportContext &ctx)
{
	const QString trimmed = enabledExpr.trimmed();
	if (trimmed.isEmpty()) {
		return true;
	}
	if (equalsIgnoreCase(trimmed, QStringLiteral("true"))) {
		return true;
	}
	if (equalsIgnoreCase(trimmed, QStringLiteral("false"))) {
		return false;
	}

	const QString value = evaluateExpression(trimmed, doc, ctx).trimmed();
	if (value.isEmpty()) {
		return true;
	}
	if (equalsIgnoreCase(value, QStringLiteral("true")) || value == QStringLiteral("1")) {
		return true;
	}
	if (equalsIgnoreCase(value, QStringLiteral("false")) || value == QStringLiteral("0")) {
		return false;
	}
	return true;
}

ExportContext buildExportContext(const Document &doc)
{
	ExportContext ctx;
	ctx.globalVars = doc.globalVariables();
	if (doc.forms.isEmpty()) {
		return ctx;
	}

	for (const auto &group : doc.forms.first()->groups) {
		if (!group) {
			continue;
		}

		for (const auto &field : group->fields) {
			if (!field) {
				continue;
			}

			if (!field->id.trimmed().isEmpty()) {
				setContextProperty(ctx, field->id, QStringLiteral("Id"), field->id);
				setContextProperty(ctx, field->id, QStringLiteral("Enabled"), field->enabledExpression);
				setContextProperty(ctx, field->id, QStringLiteral("Description"), field->description);
				setContextProperty(ctx, field->id, QStringLiteral("SubDescription"), field->subDescription);
			}

			if (auto keyNode = field->toKeyBind()) {
				setContextProperty(ctx, keyNode->id, QStringLiteral("Bind"), keyNode->bind);
				setContextProperty(ctx, keyNode->id, QStringLiteral("Command"), keyNode->command);
				continue;
			}

			if (auto mustNode = field->toMustField()) {
				setContextProperty(ctx, mustNode->id, QStringLiteral("Bind"), mustNode->bind);
				setContextProperty(ctx, mustNode->id, QStringLiteral("Command"), mustNode->command);
				continue;
			}

			if (auto textNode = field->toTextField()) {
				setContextProperty(ctx, textNode->id, QStringLiteral("Text"), textNode->text);
				setContextProperty(ctx, textNode->id, QStringLiteral("Command"), textNode->command);
				continue;
			}

			if (auto lineNode = field->toLineField()) {
				setContextProperty(ctx, lineNode->id, QStringLiteral("Expression"), lineNode->expression);
				for (const auto &arg : lineNode->args) {
					if (!arg || arg->id.trimmed().isEmpty()) {
						continue;
					}
					ctx.argValues[arg->id] = arg->value;
					setContextProperty(ctx, arg->id, QStringLiteral("Value"), arg->value);
				}
				continue;
			}

			if (auto optionField = field->toOptionField()) {
				setContextProperty(ctx, optionField->id, QStringLiteral("Selected"), optionField->selected);
			}
		}
	}

	return ctx;
}

enum class FrameType {
	Root,
	Form,
	Group,
	KeyBind,
	MustField,
	TextField,
	LineField,
	Args,
	Arg,
	OptionField,
	Options,
	Option,
	Scripts
};

struct Frame {
	FrameType type = FrameType::Root;
	NodePtr node;
	int openLine = -1;
	int openColumn = -1;
	QString openToken;
};

} // namespace

QString nodeKindToString(NodeKind kind)
{
	switch (kind) {
	case NodeKind::Form:
		return QStringLiteral("Form");
	case NodeKind::Group:
		return QStringLiteral("Group");
	case NodeKind::Field:
		return QStringLiteral("Field");
	case NodeKind::KeyBind:
		return QStringLiteral("KeyBind");
	case NodeKind::MustField:
		return QStringLiteral("MustField");
	case NodeKind::TextField:
		return QStringLiteral("TextField");
	case NodeKind::LineField:
		return QStringLiteral("LineField");
	case NodeKind::Arg:
		return QStringLiteral("Arg");
	case NodeKind::OptionField:
		return QStringLiteral("OptionField");
	case NodeKind::Option:
		return QStringLiteral("Option");
	case NodeKind::Scripts:
		return QStringLiteral("Scripts");
	case NodeKind::Unknown:
	default:
		return QStringLiteral("Unknown");
	}
}

Node::Node(NodeKind nodeKind)
	: kind_(nodeKind)
{
}

NodeKind Node::kind() const
{
	return kind_;
}

bool Node::is(NodeKind target) const
{
	return kind_ == target;
}

QString Node::toCFG() const
{
	return QString();
}

std::shared_ptr<FormNode> Node::toForm()
{
	return nullptr;
}

std::shared_ptr<GroupNode> Node::toGroup()
{
	return nullptr;
}

std::shared_ptr<FieldNode> Node::toField()
{
	return nullptr;
}

std::shared_ptr<KeyBindNode> Node::toKeyBind()
{
	return nullptr;
}

std::shared_ptr<MustFieldNode> Node::toMustField()
{
	return nullptr;
}

std::shared_ptr<TextFieldNode> Node::toTextField()
{
	return nullptr;
}

std::shared_ptr<LineFieldNode> Node::toLineField()
{
	return nullptr;
}

std::shared_ptr<ArgNode> Node::toArg()
{
	return nullptr;
}

std::shared_ptr<OptionFieldNode> Node::toOptionField()
{
	return nullptr;
}

std::shared_ptr<OptionNode> Node::toOption()
{
	return nullptr;
}

std::shared_ptr<ScriptsNode> Node::toScripts()
{
	return nullptr;
}

FieldNode::FieldNode(NodeKind kind)
	: Node(kind)
{
}

FieldNode::Ptr FieldNode::from(const NodePtr &node)
{
	return std::dynamic_pointer_cast<FieldNode>(node);
}

std::shared_ptr<FieldNode> FieldNode::toField()
{
	return std::dynamic_pointer_cast<FieldNode>(shared_from_this());
}

QString FieldNode::dumpCommonProperties(int indent) const
{
	QStringList lines;
	const QString pad = indentString(indent);

	if (!id.isEmpty()) {
		lines << (pad + QStringLiteral(".Id = ") + quote(id));
	}
	if (!enabledExpression.isEmpty()) {
		lines << (pad + QStringLiteral(".Enabled = ") + enabledExpression);
	}
	if (!description.isEmpty()) {
		lines << (pad + QStringLiteral(".Description = ") + quote(description));
	}
	if (!subDescription.isEmpty()) {
		lines << (pad + QStringLiteral(".SubDescription = ") + quote(subDescription));
	}

	for (auto it = extraProperties.constBegin(); it != extraProperties.constEnd(); ++it) {
		if (it.key().endsWith(QStringLiteral("()"))) {
			lines << (pad + QLatin1Char('.') + it.key().left(it.key().size() - 2) + QLatin1Char('(') + it.value() + QLatin1Char(')'));
		} else {
			lines << (pad + QLatin1Char('.') + it.key() + QStringLiteral(" = ") + it.value());
		}
	}

	return lines.join(QLatin1Char('\n'));
}

QString FieldNode::dump(int indent) const
{
	Q_UNUSED(indent)
	return QString();
}

QString FieldNode::toCFG() const
{
	return QString();
}

KeyBindNode::KeyBindNode()
	: FieldNode(NodeKind::KeyBind)
{
}

KeyBindNode::Ptr KeyBindNode::create()
{
	return std::shared_ptr<KeyBindNode>(new KeyBindNode());
}

KeyBindNode::Ptr KeyBindNode::from(const NodePtr &node)
{
	return std::dynamic_pointer_cast<KeyBindNode>(node);
}

std::shared_ptr<KeyBindNode> KeyBindNode::toKeyBind()
{
	return std::dynamic_pointer_cast<KeyBindNode>(shared_from_this());
}

QString KeyBindNode::dump(int indent) const
{
	QStringList lines;
	const QString pad = indentString(indent);
	const QString inner = indentString(indent + 1);

	lines << (pad + QStringLiteral("KeyBind{"));
	const QString common = dumpCommonProperties(indent + 1);
	if (!common.isEmpty()) {
		lines << common;
	}
	if (!command.isEmpty()) {
		lines << (inner + QStringLiteral(".Command = ") + quote(command));
	}
	lines << (inner + QStringLiteral(".Bind(") + quote(bind.isEmpty() ? QStringLiteral("None") : bind) + QLatin1Char(')'));
	lines << (pad + QLatin1Char('}'));

	return lines.join(QLatin1Char('\n'));
}

QString KeyBindNode::toCFG() const
{
	if (command.trimmed().isEmpty()) {
		return QString();
	}
	return QStringLiteral("bind %1 %2").arg(bind.isEmpty() ? QStringLiteral("None") : bind, command);
}

MustFieldNode::MustFieldNode()
	: FieldNode(NodeKind::MustField)
{
}

MustFieldNode::Ptr MustFieldNode::create()
{
	return std::shared_ptr<MustFieldNode>(new MustFieldNode());
}

MustFieldNode::Ptr MustFieldNode::from(const NodePtr &node)
{
	return std::dynamic_pointer_cast<MustFieldNode>(node);
}

std::shared_ptr<MustFieldNode> MustFieldNode::toMustField()
{
	return std::dynamic_pointer_cast<MustFieldNode>(shared_from_this());
}

QString MustFieldNode::dump(int indent) const
{
	QStringList lines;
	const QString pad = indentString(indent);
	const QString inner = indentString(indent + 1);

	lines << (pad + QStringLiteral("MustField{"));
	const QString common = dumpCommonProperties(indent + 1);
	if (!common.isEmpty()) {
		lines << common;
	}
	if (!command.isEmpty()) {
		lines << (inner + QStringLiteral(".Command = ") + quote(command));
	}
	lines << (inner + QStringLiteral(".Bind(") + quote(bind.isEmpty() ? QStringLiteral("None") : bind) + QLatin1Char(')'));
	lines << (pad + QLatin1Char('}'));

	return lines.join(QLatin1Char('\n'));
}

QString MustFieldNode::toCFG() const
{
	if (command.trimmed().isEmpty()) {
		return QString();
	}
	return QStringLiteral("bind %1 %2").arg(bind.isEmpty() ? QStringLiteral("None") : bind, command);
}

TextFieldNode::TextFieldNode()
	: FieldNode(NodeKind::TextField)
{
}

TextFieldNode::Ptr TextFieldNode::create()
{
	return std::shared_ptr<TextFieldNode>(new TextFieldNode());
}

TextFieldNode::Ptr TextFieldNode::from(const NodePtr &node)
{
	return std::dynamic_pointer_cast<TextFieldNode>(node);
}

std::shared_ptr<TextFieldNode> TextFieldNode::toTextField()
{
	return std::dynamic_pointer_cast<TextFieldNode>(shared_from_this());
}

QString TextFieldNode::dump(int indent) const
{
	QStringList lines;
	const QString pad = indentString(indent);
	const QString inner = indentString(indent + 1);

	lines << (pad + QStringLiteral("TextField{"));
	const QString common = dumpCommonProperties(indent + 1);
	if (!common.isEmpty()) {
		lines << common;
	}
	if (!text.isEmpty()) {
		lines << (inner + QStringLiteral(".Text = ") + quote(text));
	}
	if (!command.isEmpty()) {
		lines << (inner + QStringLiteral(".Command = ") + quote(command));
	}
	lines << (pad + QLatin1Char('}'));

	return lines.join(QLatin1Char('\n'));
}

QString TextFieldNode::toCFG() const
{
	return command;
}

ArgNode::ArgNode()
	: Node(NodeKind::Arg)
{
}

ArgNode::Ptr ArgNode::create()
{
	return std::shared_ptr<ArgNode>(new ArgNode());
}

ArgNode::Ptr ArgNode::from(const NodePtr &node)
{
	return std::dynamic_pointer_cast<ArgNode>(node);
}

std::shared_ptr<ArgNode> ArgNode::toArg()
{
	return std::dynamic_pointer_cast<ArgNode>(shared_from_this());
}

QString ArgNode::dump(int indent) const
{
	QStringList lines;
	const QString pad = indentString(indent);
	const QString inner = indentString(indent + 1);

	lines << (pad + QStringLiteral(".Arg{"));
	if (!id.isEmpty()) {
		lines << (inner + QStringLiteral(".Id = ") + quote(id));
	}
	if (!description.isEmpty()) {
		lines << (inner + QStringLiteral(".Description = ") + quote(description));
	}
	if (!value.isEmpty()) {
		lines << (inner + QStringLiteral(".Value = ") + quote(value));
	}
	for (auto it = extraProperties.constBegin(); it != extraProperties.constEnd(); ++it) {
		if (it.key().endsWith(QStringLiteral("()"))) {
			lines << (inner + QLatin1Char('.') + it.key().left(it.key().size() - 2) + QLatin1Char('(') + it.value() + QLatin1Char(')'));
		} else {
			lines << (inner + QLatin1Char('.') + it.key() + QStringLiteral(" = ") + it.value());
		}
	}
	lines << (pad + QLatin1Char('}'));

	return lines.join(QLatin1Char('\n'));
}

QString ArgNode::toCFG() const
{
	return value;
}

LineFieldNode::LineFieldNode()
	: FieldNode(NodeKind::LineField)
{
}

LineFieldNode::Ptr LineFieldNode::create()
{
	return std::shared_ptr<LineFieldNode>(new LineFieldNode());
}

LineFieldNode::Ptr LineFieldNode::from(const NodePtr &node)
{
	return std::dynamic_pointer_cast<LineFieldNode>(node);
}

std::shared_ptr<LineFieldNode> LineFieldNode::toLineField()
{
	return std::dynamic_pointer_cast<LineFieldNode>(shared_from_this());
}

QString LineFieldNode::dump(int indent) const
{
	QStringList lines;
	const QString pad = indentString(indent);
	const QString inner = indentString(indent + 1);

	lines << (pad + QStringLiteral("LineField{"));
	const QString common = dumpCommonProperties(indent + 1);
	if (!common.isEmpty()) {
		lines << common;
	}

	if (!args.isEmpty()) {
		lines << (inner + QStringLiteral(".Args{"));
		for (const auto &arg : args) {
			if (arg) {
				lines << arg->dump(indent + 2);
			}
		}
		lines << (inner + QLatin1Char('}'));
	}

	if (!expression.isEmpty()) {
		lines << (inner + QStringLiteral(".Expression = ") + expression);
	}

	lines << (pad + QLatin1Char('}'));

	return lines.join(QLatin1Char('\n'));
}

QString LineFieldNode::toCFG() const
{
	return expression;
}

OptionNode::OptionNode()
	: Node(NodeKind::Option)
{
}

OptionNode::Ptr OptionNode::create()
{
	return std::shared_ptr<OptionNode>(new OptionNode());
}

OptionNode::Ptr OptionNode::from(const NodePtr &node)
{
	return std::dynamic_pointer_cast<OptionNode>(node);
}

std::shared_ptr<OptionNode> OptionNode::toOption()
{
	return std::dynamic_pointer_cast<OptionNode>(shared_from_this());
}

QString OptionNode::dump(int indent) const
{
	QStringList lines;
	const QString pad = indentString(indent);
	const QString inner = indentString(indent + 1);

	lines << (pad + QStringLiteral("Option{"));
	if (!id.isEmpty()) {
		lines << (inner + QStringLiteral(".Id = ") + quote(id));
	}
	if (!description.isEmpty()) {
		lines << (inner + QStringLiteral(".Description = ") + quote(description));
	}
	if (!command.isEmpty()) {
		lines << (inner + QStringLiteral(".Command = ") + quote(command));
	}
	for (auto it = extraProperties.constBegin(); it != extraProperties.constEnd(); ++it) {
		if (it.key().endsWith(QStringLiteral("()"))) {
			lines << (inner + QLatin1Char('.') + it.key().left(it.key().size() - 2) + QLatin1Char('(') + it.value() + QLatin1Char(')'));
		} else {
			lines << (inner + QLatin1Char('.') + it.key() + QStringLiteral(" = ") + it.value());
		}
	}
	lines << (pad + QLatin1Char('}'));

	return lines.join(QLatin1Char('\n'));
}

QString OptionNode::toCFG() const
{
	return command;
}

OptionFieldNode::OptionFieldNode()
	: FieldNode(NodeKind::OptionField)
{
}

OptionFieldNode::Ptr OptionFieldNode::create()
{
	return std::shared_ptr<OptionFieldNode>(new OptionFieldNode());
}

OptionFieldNode::Ptr OptionFieldNode::from(const NodePtr &node)
{
	return std::dynamic_pointer_cast<OptionFieldNode>(node);
}

std::shared_ptr<OptionFieldNode> OptionFieldNode::toOptionField()
{
	return std::dynamic_pointer_cast<OptionFieldNode>(shared_from_this());
}

QString OptionFieldNode::dump(int indent) const
{
	QStringList lines;
	const QString pad = indentString(indent);
	const QString inner = indentString(indent + 1);

	lines << (pad + QStringLiteral("OptionField{"));
	const QString common = dumpCommonProperties(indent + 1);
	if (!common.isEmpty()) {
		lines << common;
	}

	lines << (inner + QStringLiteral("Options{"));
	for (const auto &option : options) {
		if (option) {
			lines << option->dump(indent + 2);
		}
	}
	lines << (inner + QLatin1Char('}'));

	if (!selected.isEmpty()) {
		lines << (inner + QStringLiteral(".Selected = ") + quote(selected));
	}
	lines << (pad + QLatin1Char('}'));

	return lines.join(QLatin1Char('\n'));
}

QString OptionFieldNode::toCFG() const
{
	for (const auto &option : options) {
		if (option && option->id == selected) {
			return option->command;
		}
	}
	for (const auto &option : options) {
		if (option) {
			return option->command;
		}
	}
	return QString();
}

GroupNode::GroupNode()
	: Node(NodeKind::Group)
{
}

GroupNode::Ptr GroupNode::create()
{
	return std::shared_ptr<GroupNode>(new GroupNode());
}

GroupNode::Ptr GroupNode::from(const NodePtr &node)
{
	return std::dynamic_pointer_cast<GroupNode>(node);
}

std::shared_ptr<GroupNode> GroupNode::toGroup()
{
	return std::dynamic_pointer_cast<GroupNode>(shared_from_this());
}

QString GroupNode::dump(int indent) const
{
	QStringList lines;
	const QString pad = indentString(indent);
	const QString inner = indentString(indent + 1);

	lines << (pad + QStringLiteral("Group{"));
	if (!title.isEmpty()) {
		lines << (inner + QStringLiteral(".Title = ") + quote(title));
	}
	for (const auto &field : fields) {
		if (field) {
			lines << field->dump(indent + 1);
		}
	}
	lines << (pad + QLatin1Char('}'));

	return lines.join(QLatin1Char('\n'));
}

QString GroupNode::toCFG() const
{
	QStringList lines;
	if (!title.isEmpty()) {
		lines << QStringLiteral("//title %1").arg(quote(title));
	}
	for (const auto &field : fields) {
		if (field) {
			const QString line = field->toCFG();
			if (!line.trimmed().isEmpty()) {
				lines << line;
			}
		}
	}
	return lines.join(QLatin1Char('\n'));
}

FormNode::FormNode()
	: Node(NodeKind::Form)
{
}

FormNode::Ptr FormNode::create()
{
	return std::shared_ptr<FormNode>(new FormNode());
}

FormNode::Ptr FormNode::from(const NodePtr &node)
{
	return std::dynamic_pointer_cast<FormNode>(node);
}

std::shared_ptr<FormNode> FormNode::toForm()
{
	return std::dynamic_pointer_cast<FormNode>(shared_from_this());
}

QString FormNode::dump(int indent) const
{
	QStringList lines;
	const QString pad = indentString(indent);
	const QString inner = indentString(indent + 1);

	lines << (pad + QStringLiteral("Form{"));
	if (!id.isEmpty()) {
		lines << (inner + QStringLiteral(".Id = ") + quote(id));
	}
	if (!output.isEmpty()) {
		lines << (inner + QStringLiteral(".Output = ") + quote(output));
	}
	if (!description.isEmpty()) {
		lines << (inner + QStringLiteral(".Description = ") + quote(description));
	}
	for (const auto &group : groups) {
		if (group) {
			lines << group->dump(indent + 1);
		}
	}
	lines << (pad + QLatin1Char('}'));

	return lines.join(QLatin1Char('\n'));
}

QString FormNode::toCFG() const
{
	QStringList lines;
	for (const auto &group : groups) {
		if (!group) {
			continue;
		}
		const QString groupCfg = group->toCFG();
		if (!groupCfg.trimmed().isEmpty()) {
			lines << groupCfg;
		}
	}
	return lines.join(QLatin1Char('\n'));
}

ScriptsNode::ScriptsNode()
	: Node(NodeKind::Scripts)
{
}

ScriptsNode::Ptr ScriptsNode::create()
{
	return std::shared_ptr<ScriptsNode>(new ScriptsNode());
}

ScriptsNode::Ptr ScriptsNode::from(const NodePtr &node)
{
	return std::dynamic_pointer_cast<ScriptsNode>(node);
}

std::shared_ptr<ScriptsNode> ScriptsNode::toScripts()
{
	return std::dynamic_pointer_cast<ScriptsNode>(shared_from_this());
}

QString ScriptsNode::dump(int indent) const
{
	QStringList lines;
	const QString pad = indentString(indent);
	const QString inner = indentString(indent + 1);

	lines << (pad + QStringLiteral("Scripts{"));
	if (!rawCode.isEmpty()) {
		const QStringList scriptLines = rawCode.split(QLatin1Char('\n'));
		for (const QString &scriptLine : scriptLines) {
			lines << (inner + scriptLine);
		}
	}
	lines << (pad + QLatin1Char('}'));

	return lines.join(QLatin1Char('\n'));
}

QString ScriptsNode::toCFG() const
{
	Q_UNUSED(rawCode)
	return QString();
}

Document::Ptr Document::create()
{
	return std::shared_ptr<Document>(new Document());
}

Document::Ptr Document::from(const QString &formText, ParseError *error)
{
	auto doc = create();
	if (!doc->parse(formText, error)) {
		return nullptr;
	}
	return doc;
}

void Document::clear()
{
	forms.clear();
	scripts.reset();
	meta_.clear();
}

QString Document::metaValue(const QString &key) const
{
	for (const auto &entry : meta_) {
		if (equalsIgnoreCase(entry.first, key)) {
			return entry.second;
		}
	}
	return QString();
}

void Document::setMetaValue(const QString &key, const QString &value)
{
	for (auto &entry : meta_) {
		if (equalsIgnoreCase(entry.first, key)) {
			entry.second = value;
			return;
		}
	}
	meta_.append({key, value});
}

QVector<QPair<QString, QString>> Document::metaEntries() const
{
	return meta_;
}

bool Document::parse(const QString &formText, ParseError *error)
{
	clear();
	if (error) {
		*error = ParseError{};
	}

	auto fail = [&](int line, int column, const QString &message) -> bool {
		if (error) {
			error->line = line;
			error->column = column;
			error->message = message;
		}
		return false;
	};

	QString normalized = formText;
	normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
	normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
	const QStringList lines = normalized.split(QLatin1Char('\n'));

	QVector<Frame> stack;
	stack.append({FrameType::Root, nullptr, 1, 1, QStringLiteral("<root>")});

	bool capturingScripts = false;
	int scriptsDepth = 0;
	int scriptsOpenLine = -1;
	QStringList scriptLines;

	static const QRegularExpression assignRe(QStringLiteral(R"(^\.(\w+)\s*=\s*(.+)$)"));
	static const QRegularExpression callRe(QStringLiteral(R"(^\.(\w+)\s*\((.*)\)\s*$)"));
	static const QRegularExpression propNameOnlyRe(QStringLiteral(R"(^\.[A-Za-z_][A-Za-z0-9_]*$)"));

	auto pushFrame = [&](FrameType type, const NodePtr &node, const QString &token, int line, int column) {
		Frame frame;
		frame.type = type;
		frame.node = node;
		frame.openLine = line;
		frame.openColumn = column;
		frame.openToken = token;
		stack.append(frame);
	};

	for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
		const QString rawLine = lines.at(lineIndex);

		if (capturingScripts) {
			bool inSingle = false;
			bool inDouble = false;
			bool escape = false;
			int closeIndex = -1;

			for (int i = 0; i < rawLine.size(); ++i) {
				const QChar ch = rawLine.at(i);
				const QChar next = (i + 1 < rawLine.size()) ? rawLine.at(i + 1) : QChar();

				if (!inSingle && !inDouble && ch == QLatin1Char('/') && next == QLatin1Char('/')) {
					break;
				}

				if (escape) {
					escape = false;
					continue;
				}

				if ((inSingle || inDouble) && ch == QLatin1Char('\\')) {
					escape = true;
					continue;
				}

				if (!inSingle && ch == QLatin1Char('"')) {
					inDouble = !inDouble;
					continue;
				}

				if (!inDouble && ch == QLatin1Char('\'')) {
					inSingle = !inSingle;
					continue;
				}

				if (!inSingle && !inDouble) {
					if (ch == QLatin1Char('{')) {
						++scriptsDepth;
					} else if (ch == QLatin1Char('}')) {
						--scriptsDepth;
						if (scriptsDepth == 0) {
							closeIndex = i;
							break;
						}
					}
				}
			}

			if (closeIndex >= 0) {
				const QString beforeClose = rawLine.left(closeIndex);
				if (!beforeClose.trimmed().isEmpty()) {
					scriptLines.append(beforeClose);
				}

				if (!stack.isEmpty() && stack.last().type == FrameType::Scripts) {
					const auto scriptsNode = stack.last().node ? stack.last().node->toScripts() : nullptr;
					if (scriptsNode) {
						const QString rawScript = scriptLines.join(QLatin1Char('\n'));
						const QString normalizedScript = normalizeScriptCodeForLua(rawScript).trimmed();

						if (!normalizedScript.isEmpty()) {
							QMap<QString, LuaFunctionDef> parsedFunctions;
							ParseError scriptErr;
							if (!parseLuaFunctions(normalizedScript, &parsedFunctions, &scriptErr)) {
								const int scriptLine = (scriptErr.line > 0) ? scriptErr.line : 1;
								const int scriptColumn = (scriptErr.column > 0) ? scriptErr.column : 1;
								const int docLine = (scriptsOpenLine > 0) ? (scriptsOpenLine + scriptLine) : (lineIndex + 1);
								return fail(docLine,
											scriptColumn,
											scriptErr.message.isEmpty()
												? QStringLiteral("Scripts 语法错误")
												: scriptErr.message);
							}
						}

						scriptsNode->rawCode = normalizedScript;
					}
					stack.removeLast();
				}

				capturingScripts = false;
				continue;
			}

			scriptLines.append(rawLine);
			continue;
		}

		const ParsedLine parsed = makeParsedLine(rawLine);
		const QString line = parsed.text;
		if (line.isEmpty()) {
			continue;
		}

		const int unterminatedQuoteColInText = findUnterminatedQuoteColumnInText(line);
		if (unterminatedQuoteColInText > 0) {
			return fail(lineIndex + 1,
						parsed.toColumn(unterminatedQuoteColInText - 1),
						QStringLiteral("字符串未闭合"));
		}

		const int openBracePos = findCharOutsideQuotes(line, QLatin1Char('{'));
		const int closeBracePos = findCharOutsideQuotes(line, QLatin1Char('}'));
		if (line != QStringLiteral("}")) {
			if (closeBracePos >= 0) {
				return fail(lineIndex + 1,
							parsed.toColumn(closeBracePos),
							QStringLiteral("Token 不匹配: 非独立 '}'"));
			}
			if (openBracePos >= 0 && !line.endsWith(QLatin1Char('{'))) {
				return fail(lineIndex + 1,
							parsed.toColumn(openBracePos),
							QStringLiteral("Token 不匹配: '{' 只能用于块起始并位于行尾"));
			}
		}

		if (line.startsWith(QLatin1Char('@'))) {
			const int colon = line.indexOf(QLatin1Char(':'));
			if (colon <= 1) {
				const int col = (colon < 0) ? parsed.toColumn(line.size()) : parsed.toColumn(colon);
				return fail(lineIndex + 1, col, QStringLiteral("元属性格式错误，必须为 @Key: Value"));
			}

			const QString key = line.mid(1, colon - 1).trimmed();
			const QString value = line.mid(colon + 1).trimmed();
			if (key.isEmpty()) {
				return fail(lineIndex + 1, parsed.toColumn(1), QStringLiteral("元属性 Key 不能为空"));
			}

			setMetaValue(key, unquoteMaybe(value));
			continue;
		}

		if (line == QStringLiteral("}")) {
			if (stack.size() <= 1) {
				return fail(lineIndex + 1, parsed.toColumn(0), QStringLiteral("检测到多余的右大括号"));
			}
			stack.removeLast();
			continue;
		}

		if (line.endsWith(QLatin1Char('{'))) {
			QString blockToken = line.left(line.size() - 1).trimmed();
			const int blockTokenColumn = parsed.tokenColumn(blockToken);
			QString blockName = blockToken;
			if (blockToken.startsWith(QLatin1Char('.'))) {
				blockName = blockToken.mid(1);
			}

			const FrameType parentType = stack.last().type;

			if (equalsIgnoreCase(blockName, QStringLiteral("Form"))) {
				if (parentType != FrameType::Root) {
					return fail(lineIndex + 1, blockTokenColumn, QStringLiteral("Form 只能在顶层定义"));
			}
			auto newForm = FormNode::create();
			forms.append(newForm);
			pushFrame(FrameType::Form, newForm, blockName, lineIndex + 1, blockTokenColumn);
			continue;
			}

			if (equalsIgnoreCase(blockName, QStringLiteral("Group"))) {
				if (parentType != FrameType::Form) {
					return fail(lineIndex + 1, blockTokenColumn, QStringLiteral("Group 必须在 Form 内定义"));
				}
				auto group = GroupNode::create();
				auto parentForm = stack.last().node ? stack.last().node->toForm() : nullptr;
				if (!parentForm) {
					return fail(lineIndex + 1, blockTokenColumn, QStringLiteral("Form 节点无效"));
				}
				parentForm->groups.append(group);
				pushFrame(FrameType::Group, group, blockName, lineIndex + 1, blockTokenColumn);
				continue;
			}

			if (equalsIgnoreCase(blockName, QStringLiteral("KeyBind"))) {
				if (parentType != FrameType::Group) {
					return fail(lineIndex + 1, blockTokenColumn, QStringLiteral("KeyBind 必须在 Group 内定义"));
				}
				auto node = KeyBindNode::create();
				auto parentGroup = stack.last().node ? stack.last().node->toGroup() : nullptr;
				parentGroup->fields.append(node);
				pushFrame(FrameType::KeyBind, node, blockName, lineIndex + 1, blockTokenColumn);
				continue;
			}

			if (equalsIgnoreCase(blockName, QStringLiteral("MustField"))) {
				if (parentType != FrameType::Group) {
					return fail(lineIndex + 1, blockTokenColumn, QStringLiteral("MustField 必须在 Group 内定义"));
				}
				auto node = MustFieldNode::create();
				auto parentGroup = stack.last().node ? stack.last().node->toGroup() : nullptr;
				parentGroup->fields.append(node);
				pushFrame(FrameType::MustField, node, blockName, lineIndex + 1, blockTokenColumn);
				continue;
			}

			if (equalsIgnoreCase(blockName, QStringLiteral("TextField"))) {
				if (parentType != FrameType::Group) {
					return fail(lineIndex + 1, blockTokenColumn, QStringLiteral("TextField 必须在 Group 内定义"));
				}
				auto node = TextFieldNode::create();
				auto parentGroup = stack.last().node ? stack.last().node->toGroup() : nullptr;
				parentGroup->fields.append(node);
				pushFrame(FrameType::TextField, node, blockName, lineIndex + 1, blockTokenColumn);
				continue;
			}

			if (equalsIgnoreCase(blockName, QStringLiteral("LineField"))) {
				if (parentType != FrameType::Group) {
					return fail(lineIndex + 1, blockTokenColumn, QStringLiteral("LineField 必须在 Group 内定义"));
				}
				auto node = LineFieldNode::create();
				auto parentGroup = stack.last().node ? stack.last().node->toGroup() : nullptr;
				parentGroup->fields.append(node);
				pushFrame(FrameType::LineField, node, blockName, lineIndex + 1, blockTokenColumn);
				continue;
			}

			if (equalsIgnoreCase(blockName, QStringLiteral("OptionField"))) {
				if (parentType != FrameType::Group) {
					return fail(lineIndex + 1, blockTokenColumn, QStringLiteral("OptionField 必须在 Group 内定义"));
				}
				auto node = OptionFieldNode::create();
				auto parentGroup = stack.last().node ? stack.last().node->toGroup() : nullptr;
				parentGroup->fields.append(node);
				pushFrame(FrameType::OptionField, node, blockName, lineIndex + 1, blockTokenColumn);
				continue;
			}

			if (equalsIgnoreCase(blockName, QStringLiteral("Args"))) {
				if (parentType != FrameType::LineField) {
					return fail(lineIndex + 1, blockTokenColumn, QStringLiteral("Args 必须在 LineField 内定义"));
				}
				pushFrame(FrameType::Args, nullptr, blockName, lineIndex + 1, blockTokenColumn);
				continue;
			}

			if (equalsIgnoreCase(blockName, QStringLiteral("Arg"))) {
				if (parentType != FrameType::Args || stack.size() < 2 || stack.at(stack.size() - 2).type != FrameType::LineField) {
					return fail(lineIndex + 1, blockTokenColumn, QStringLiteral("Arg 必须在 Args 内定义"));
				}

				auto lineNode = stack.at(stack.size() - 2).node ? stack.at(stack.size() - 2).node->toLineField() : nullptr;
				if (!lineNode) {
					return fail(lineIndex + 1, blockTokenColumn, QStringLiteral("LineField 节点无效"));
				}

				auto argNode = ArgNode::create();
				lineNode->args.append(argNode);
				pushFrame(FrameType::Arg, argNode, blockName, lineIndex + 1, blockTokenColumn);
				continue;
			}

			if (equalsIgnoreCase(blockName, QStringLiteral("Options"))) {
				if (parentType != FrameType::OptionField) {
					return fail(lineIndex + 1, blockTokenColumn, QStringLiteral("Options 必须在 OptionField 内定义"));
				}
				pushFrame(FrameType::Options, nullptr, blockName, lineIndex + 1, blockTokenColumn);
				continue;
			}

			if (equalsIgnoreCase(blockName, QStringLiteral("Option"))) {
				if (parentType != FrameType::Options || stack.size() < 2 || stack.at(stack.size() - 2).type != FrameType::OptionField) {
					return fail(lineIndex + 1, blockTokenColumn, QStringLiteral("Option 必须在 Options 内定义"));
				}

				auto optionField = stack.at(stack.size() - 2).node ? stack.at(stack.size() - 2).node->toOptionField() : nullptr;
				if (!optionField) {
					return fail(lineIndex + 1, blockTokenColumn, QStringLiteral("OptionField 节点无效"));
				}

				auto optionNode = OptionNode::create();
				optionField->options.append(optionNode);
				pushFrame(FrameType::Option, optionNode, blockName, lineIndex + 1, blockTokenColumn);
				continue;
			}

			if (equalsIgnoreCase(blockName, QStringLiteral("Scripts"))) {
				if (parentType != FrameType::Root) {
					return fail(lineIndex + 1, blockTokenColumn, QStringLiteral("Scripts 只能在顶层定义"));
				}

				scripts = ScriptsNode::create();
				pushFrame(FrameType::Scripts, scripts, blockName, lineIndex + 1, blockTokenColumn);
				capturingScripts = true;
				scriptsDepth = 1;
				scriptsOpenLine = lineIndex + 1;
				scriptLines.clear();
				continue;
			}

			return fail(lineIndex + 1, blockTokenColumn, QStringLiteral("未知块类型: %1").arg(blockName));
		}

		if (!line.startsWith(QLatin1Char('.'))) {
			return fail(lineIndex + 1,
						parsed.toColumn(0),
						QStringLiteral("无法识别的语句: %1").arg(line));
		}

		const QRegularExpressionMatch callMatch = callRe.match(line);
		const QRegularExpressionMatch assignMatch = assignRe.match(line);
		if (!callMatch.hasMatch() && !assignMatch.hasMatch()) {
			const int equalPos = findCharOutsideQuotes(line, QLatin1Char('='));
			const int leftParenPos = findCharOutsideQuotes(line, QLatin1Char('('));
			const int rightParenPos = findCharOutsideQuotes(line, QLatin1Char(')'));

			if (leftParenPos >= 0) {
				const int matchedRight = findMatchingRightParenOutsideQuotes(line, leftParenPos);
				if (matchedRight < 0) {
					return fail(lineIndex + 1,
								parsed.toColumn(leftParenPos),
								QStringLiteral("Token 不匹配: 函数调用缺少 ')'"));
				}

				const QString head = line.left(leftParenPos).trimmed();
				if (!propNameOnlyRe.match(head).hasMatch()) {
					return fail(lineIndex + 1,
								parsed.toColumn(0),
								QStringLiteral("属性调用格式错误，应为 .Name(...)"));
				}

				if (matchedRight != line.size() - 1) {
					return fail(lineIndex + 1,
								parsed.toColumn(matchedRight + 1),
								QStringLiteral("Token 不匹配: ')' 后存在非法内容"));
				}

				if (rightParenPos >= 0 && rightParenPos < leftParenPos) {
					return fail(lineIndex + 1,
								parsed.toColumn(rightParenPos),
								QStringLiteral("Token 不匹配: ')' 出现在 '(' 之前"));
				}
			}

			if (equalPos >= 0) {
				const QString left = line.left(equalPos).trimmed();
				const QString right = line.mid(equalPos + 1).trimmed();

				if (!propNameOnlyRe.match(left).hasMatch()) {
					return fail(lineIndex + 1,
								parsed.toColumn(0),
								QStringLiteral("键值对 Key 格式错误，应为 .Name = Value"));
				}

				if (right.isEmpty()) {
					return fail(lineIndex + 1,
								parsed.toColumn(equalPos + 1),
								QStringLiteral("键值对不匹配: '=' 右侧缺少值"));
				}
			}

			if (equalPos < 0 && leftParenPos < 0) {
				return fail(lineIndex + 1,
							parsed.toColumn(line.size()),
							QStringLiteral("属性语句不完整，缺少 '=' 或 '()'"));
			}

			return fail(lineIndex + 1,
						parsed.toColumn(0),
						QStringLiteral("属性解析失败: %1").arg(line));
		}

		const Frame &frame = stack.last();
		NodePtr node = frame.node;

		auto writeExtra = [&](const QString &key, const QString &value) {
			if (node) {
				node->extraProperties.insert(key, value);
			}
		};

		if (callMatch.hasMatch()) {
			const QString propName = callMatch.captured(1).trimmed();
			const QString callArgRaw = callMatch.captured(2).trimmed();
			const QString callArg = unquoteMaybe(callArgRaw);

			bool handled = false;

			if ((frame.type == FrameType::KeyBind || frame.type == FrameType::MustField) && equalsIgnoreCase(propName, QStringLiteral("Bind"))) {
				if (auto keyNode = node ? node->toKeyBind() : nullptr) {
					keyNode->bind = callArg;
					handled = true;
				} else if (auto mustNode = node ? node->toMustField() : nullptr) {
					mustNode->bind = callArg;
					handled = true;
				}
			}

			if (!handled) {
				writeExtra(propName + QStringLiteral("()"), callArgRaw);
			}
			continue;
		}

		if (!assignMatch.hasMatch()) {
			return fail(lineIndex + 1,
						parsed.toColumn(0),
						QStringLiteral("属性解析失败: %1").arg(line));
		}

		const QString propName = assignMatch.captured(1).trimmed();
		const QString valueRaw = assignMatch.captured(2).trimmed();
		const QString valueText = unquoteMaybe(valueRaw);

		bool handled = false;

		if (auto formNode = node ? node->toForm() : nullptr) {
			if (equalsIgnoreCase(propName, QStringLiteral("Id"))) {
				formNode->id = valueText;
				handled = true;
			} else if (equalsIgnoreCase(propName, QStringLiteral("Output"))) {
				formNode->output = valueText;
				handled = true;
			} else if (equalsIgnoreCase(propName, QStringLiteral("Description"))) {
				formNode->description = valueText;
				handled = true;
			}
		}

		if (!handled) {
			if (auto groupNode = node ? node->toGroup() : nullptr) {
				if (equalsIgnoreCase(propName, QStringLiteral("Title"))) {
					groupNode->title = valueText;
					handled = true;
				}
			}
		}

		if (!handled) {
			if (auto fieldNode = node ? node->toField() : nullptr) {
				if (equalsIgnoreCase(propName, QStringLiteral("Id"))) {
					fieldNode->id = valueText;
					handled = true;
				} else if (equalsIgnoreCase(propName, QStringLiteral("Enabled"))) {
					fieldNode->enabledExpression = isQuoted(valueRaw) ? valueText : valueRaw;
					handled = true;
				} else if (equalsIgnoreCase(propName, QStringLiteral("Description"))) {
					fieldNode->description = valueText;
					handled = true;
				} else if (equalsIgnoreCase(propName, QStringLiteral("SubDescription"))) {
					fieldNode->subDescription = valueText;
					handled = true;
				}
			}
		}

		if (!handled) {
			if (auto keyNode = node ? node->toKeyBind() : nullptr) {
				if (equalsIgnoreCase(propName, QStringLiteral("Command"))) {
					keyNode->command = valueText;
					handled = true;
				} else if (equalsIgnoreCase(propName, QStringLiteral("Bind"))) {
					keyNode->bind = valueText;
					handled = true;
				}
			}
		}

		if (!handled) {
			if (auto mustNode = node ? node->toMustField() : nullptr) {
				if (equalsIgnoreCase(propName, QStringLiteral("Command"))) {
					mustNode->command = valueText;
					handled = true;
				} else if (equalsIgnoreCase(propName, QStringLiteral("Bind"))) {
					mustNode->bind = valueText;
					handled = true;
				}
			}
		}

		if (!handled) {
			if (auto textNode = node ? node->toTextField() : nullptr) {
				if (equalsIgnoreCase(propName, QStringLiteral("Text"))) {
					textNode->text = valueText;
					handled = true;
				} else if (equalsIgnoreCase(propName, QStringLiteral("Command"))) {
					textNode->command = valueText;
					handled = true;
				}
			}
		}

		if (!handled) {
			if (auto lineNode = node ? node->toLineField() : nullptr) {
				if (equalsIgnoreCase(propName, QStringLiteral("Expression"))) {
					lineNode->expression = isQuoted(valueRaw) ? valueText : valueRaw;
					handled = true;
				}
			}
		}

		if (!handled) {
			if (auto argNode = node ? node->toArg() : nullptr) {
				if (equalsIgnoreCase(propName, QStringLiteral("Id"))) {
					argNode->id = valueText;
					handled = true;
				} else if (equalsIgnoreCase(propName, QStringLiteral("Description"))) {
					argNode->description = valueText;
					handled = true;
				} else if (equalsIgnoreCase(propName, QStringLiteral("Value")) || equalsIgnoreCase(propName, QStringLiteral("Default"))) {
					argNode->value = valueText;
					handled = true;
				}
			}
		}

		if (!handled) {
			if (auto optionField = node ? node->toOptionField() : nullptr) {
				if (equalsIgnoreCase(propName, QStringLiteral("Selected"))) {
					optionField->selected = valueText;
					handled = true;
				}
			}
		}

		if (!handled) {
			if (auto optionNode = node ? node->toOption() : nullptr) {
				if (equalsIgnoreCase(propName, QStringLiteral("Id"))) {
					optionNode->id = valueText;
					handled = true;
				} else if (equalsIgnoreCase(propName, QStringLiteral("Description"))) {
					optionNode->description = valueText;
					handled = true;
				} else if (equalsIgnoreCase(propName, QStringLiteral("Command"))) {
					optionNode->command = valueText;
					handled = true;
				}
			}
		}

		if (!handled) {
			writeExtra(propName, valueRaw);
		}
	}

	if (capturingScripts) {
		const int errLine = (scriptsOpenLine > 0) ? scriptsOpenLine : lines.size();
		return fail(errLine, 1, QStringLiteral("Scripts 块未闭合"));
	}

	if (stack.size() != 1) {
		const Frame &openFrame = stack.last();
		const QString token = openFrame.openToken.isEmpty() ? QStringLiteral("<unknown>") : openFrame.openToken;
		const int errLine = (openFrame.openLine > 0) ? openFrame.openLine : lines.size();
		const int errCol = (openFrame.openColumn > 0) ? openFrame.openColumn : 1;
		return fail(errLine,
						errCol,
						QStringLiteral("存在未闭合的块定义: %1").arg(token));
	}

	if (forms.isEmpty()) {
		return fail(1, 1, QStringLiteral("未找到 Form 根节点"));
	}

	return true;
}

QString Document::dump() const
{
	QStringList lines;

	for (const auto &metaPair : meta_) {
		lines << (QStringLiteral("@") + metaPair.first + QStringLiteral(": ") + metaPair.second);
	}

	if (!meta_.isEmpty()) {
		lines << QString();
	}

	for (const auto &formNode : forms) {
		if (formNode) {
			lines << formNode->dump(0);
			lines << QString();
		}
	}

	if (scripts) {
		if (lines.last().trimmed().isEmpty()) {
			lines.removeLast();
		}
		lines << scripts->dump(0);
	}

	return lines.join(QLatin1Char('\n'));
}

QString Document::toCFG() const
{
	if (forms.isEmpty()) {
		return QString();
	}
	return toCFGs().first().content;
}

QVector<CFGExportItem> Document::toCFGs() const
{
	QVector<CFGExportItem> results;

	for (const auto &formNode : forms) {
		if (!formNode) {
			continue;
		}

		CFGExportItem item;
		item.output = formNode->output;
		item.description = formNode->description;
		item.sourceFormId = formNode->id;

		ExportContext ctx = buildExportContext(*this);
		ctx.globalVars = globalVars_;

		for (const auto &group : formNode->groups) {
			if (!group) {
				continue;
			}

			if (!group->title.trimmed().isEmpty()) {
				item.content += QStringLiteral("//title %1\n").arg(quote(group->title));
			}

			for (const auto &field : group->fields) {
				if (!field) {
					continue;
				}

				if (!evaluateEnabled(field->enabledExpression, this, ctx)) {
					continue;
				}

				if (auto keyNode = field->toKeyBind()) {
					QString cmd = applyTemplate(keyNode->command, this, ctx).trimmed();
					if (cmd.isEmpty()) {
						cmd = keyNode->command.trimmed();
					}
					if (!cmd.isEmpty()) {
						item.content += QStringLiteral("bind %1 %2\n").arg(
							keyNode->bind.isEmpty() ? QStringLiteral("None") : keyNode->bind, cmd);
					}
					continue;
				}

				if (auto mustNode = field->toMustField()) {
					QString cmd = applyTemplate(mustNode->command, this, ctx).trimmed();
					if (cmd.isEmpty()) {
						cmd = mustNode->command.trimmed();
					}
					if (!cmd.isEmpty()) {
						item.content += QStringLiteral("bind %1 %2\n").arg(
							mustNode->bind.isEmpty() ? QStringLiteral("None") : mustNode->bind, cmd);
					}
					continue;
				}

				if (auto textNode = field->toTextField()) {
					QString cmd = applyTemplate(textNode->command, this, ctx).trimmed();
					if (cmd.isEmpty()) {
						cmd = textNode->command.trimmed();
					}
					if (cmd.isEmpty() && !textNode->text.trimmed().isEmpty()) {
						cmd = textNode->text.trimmed();
					}
					if (!cmd.isEmpty()) {
						item.content += cmd + QStringLiteral("\n");
					}
					continue;
				}

				if (auto lineNode = field->toLineField()) {
					for (const auto &arg : lineNode->args) {
						if (!arg || arg->id.trimmed().isEmpty()) {
							continue;
						}
						ctx.argValues[arg->id] = arg->value;
					}

					QString exprResult = evaluateExpression(lineNode->expression, this, ctx).trimmed();
					if (exprResult.isEmpty()) {
						exprResult = lineNode->expression.trimmed();
					}
					exprResult = applyTemplate(exprResult, this, ctx).trimmed();
					if (!exprResult.isEmpty()) {
						item.content += exprResult + QStringLiteral("\n");
					}
					continue;
				}

				if (auto optionField = field->toOptionField()) {
					OptionNode::Ptr selectedNode;
					for (const auto &opt : optionField->options) {
						if (opt && opt->id == optionField->selected) {
							selectedNode = opt;
							break;
						}
					}

					if (!selectedNode) {
						for (const auto &opt : optionField->options) {
							if (opt) {
								selectedNode = opt;
								break;
							}
						}
					}

					if (selectedNode) {
						QString cmd = applyTemplate(selectedNode->command, this, ctx).trimmed();
						if (cmd.isEmpty()) {
							cmd = selectedNode->command.trimmed();
						}
						if (!cmd.isEmpty()) {
							item.content += cmd + QStringLiteral("\n");
						}
					}
					continue;
				}
			}
		}

		item.content = item.content.trimmed();

		if (!formNode->output.isEmpty()) {
			item.relativePath = formNode->output;
			item.fileName = formNode->output + QStringLiteral(".cfg");
			item.absolutePath = resolvePath(sourceFilePath_, formNode->output);
		}

		results.append(item);
	}

	return results;
}

QVector<NodePtr> Document::allNodes() const
{
	QVector<NodePtr> nodes;

	for (const auto &formNode : forms) {
		if (!formNode) {
			continue;
		}
		nodes.append(formNode);
		for (const auto &group : formNode->groups) {
			if (!group) {
				continue;
			}
			nodes.append(group);

			for (const auto &field : group->fields) {
				if (!field) {
					continue;
				}
				nodes.append(field);

				if (auto line = field->toLineField()) {
					for (const auto &arg : line->args) {
						if (arg) {
							nodes.append(arg);
						}
					}
				}

				if (auto optionField = field->toOptionField()) {
					for (const auto &option : optionField->options) {
						if (option) {
							nodes.append(option);
						}
					}
				}
			}
		}
	}

	if (scripts) {
		nodes.append(scripts);
	}

	return nodes;
}

NodePtr Document::findById(const QString &id) const
{
	if (id.trimmed().isEmpty()) {
		return nullptr;
	}

	const auto nodes = allNodes();
	for (const auto &node : nodes) {
		if (!node) {
			continue;
		}

		if (auto field = node->toField()) {
			if (field->id == id) {
				return node;
			}
		}

		if (auto arg = node->toArg()) {
			if (arg->id == id) {
				return node;
			}
		}

		if (auto option = node->toOption()) {
			if (option->id == id) {
				return node;
			}
		}
	}

	return nullptr;
}

void Document::registerFunction(const QString &name, LuaFunction func)
{
	if (!luaRuntime_) {
		luaRuntime_ = std::make_shared<LuaRuntime>();
	}
	luaRuntime_->registerFunction(name, func);
}

void Document::registerGlobalVariable(const QString &name, const QString &value)
{
	if (!luaRuntime_) {
		luaRuntime_ = std::make_shared<LuaRuntime>();
	}
	luaRuntime_->registerGlobalVariable(name, value);
}

void Document::setGlobalVariables(const QMap<QString, QString> &vars)
{
	if (!luaRuntime_) {
		luaRuntime_ = std::make_shared<LuaRuntime>();
	}
	globalVars_ = vars;
	luaRuntime_->setGlobalVariables(vars);
}

QString Document::executeFunction(const QString &fnName, const QStringList &args, QString *error) const
{
	if (!luaRuntime_) {
		luaRuntime_ = std::make_shared<LuaRuntime>();
	}

	if (scripts && !scripts->rawCode.isEmpty() && !luaScriptLoaded_) {
		QString loadError;
		if (!luaRuntime_->loadScript(scripts->rawCode, &loadError)) {
			if (error) {
				*error = loadError;
			}
			return QString();
		}
		luaScriptLoaded_ = true;
	}

	QString result;
	QString execError;
	if (!luaRuntime_->executeFunction(fnName, args, &result, &execError)) {
		if (error) {
			*error = execError;
		}
		return QString();
	}

	return result;
}

QString Document::executeScriptFunction(const QString &fnName, const QStringList &args, QString *error) const
{
	return executeFunction(fnName, args, error);
}

QStringList Document::importPaths() const
{
	QStringList paths;
	const QString importValue = metaValue(QStringLiteral("Import"));
	if (importValue.isEmpty()) {
		return paths;
	}

	QString trimmed = importValue.trimmed();
	if (trimmed.startsWith(QLatin1Char('['))) {
		int endBracket = trimmed.lastIndexOf(QLatin1Char(']'));
		if (endBracket > 0) {
			QString arrayContent = trimmed.mid(1, endBracket - 1).trimmed();
			if (arrayContent.startsWith(QLatin1Char('{'))) {
				arrayContent = arrayContent.mid(1, arrayContent.length() - 2).trimmed();
			}

			QRegularExpression pathRe(QStringLiteral("\"([^\"]+)\""));
			QRegularExpressionMatchIterator it = pathRe.globalMatch(arrayContent);
			while (it.hasNext()) {
				QRegularExpressionMatch match = it.next();
				paths.append(match.captured(1));
			}
		}
	} else {
		paths.append(importValue);
	}

	return paths;
}

QString Document::resolvePath(const QString &basePath, const QString &inputPath)
{
	if (inputPath.isEmpty()) {
		return QString();
	}

	QString normalizedInput = inputPath;
	normalizedInput.replace(QLatin1Char('\\'), QLatin1Char('/'));
	while (normalizedInput.endsWith(QLatin1Char('/'))) {
		normalizedInput.chop(1);
	}

	if (QFileInfo(normalizedInput).isAbsolute()) {
		return normalizedInput;
	}

	QString baseDir;
	if (!basePath.isEmpty()) {
		QFileInfo fi(basePath);
		baseDir = fi.absolutePath();
	}

	QString result;
	if (!baseDir.isEmpty()) {
		result = baseDir + QLatin1Char('/') + normalizedInput;
	} else {
		result = normalizedInput;
	}

	result.replace(QLatin1Char('\\'), QLatin1Char('/'));
	while (result.contains(QLatin1String("//"))) {
		result.replace(QLatin1String("//"), QLatin1String("/"));
	}

	return result;
}

} // namespace AFormParser

