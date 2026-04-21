#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTextStream>

#include "AFormParser.hpp"

namespace {

QString unescapeLegacy(const QString &value)
{
    QString out;
    out.reserve(value.size());

    bool esc = false;
    for (int i = 0; i < value.size(); ++i) {
        const QChar ch = value.at(i);
        if (!esc) {
            if (ch == QLatin1Char('\\')) {
                esc = true;
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
        esc = false;
    }

    if (esc) {
        out.append(QLatin1Char('\\'));
    }
    return out;
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

QStringList extractQuotedStrings(const QString &line)
{
    static const QRegularExpression re(QStringLiteral("\"((?:[^\"\\\\]|\\\\.)*)\""));
    QStringList values;

    auto it = re.globalMatch(line);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        values.append(unescapeLegacy(match.captured(1)));
    }
    return values;
}

QString makeId(const QString &prefix, int index)
{
    return QStringLiteral("%1_%2").arg(prefix).arg(index, 3, 10, QLatin1Char('0'));
}

void appendTextField(QStringList &out,
                     const QString &indent,
                     const QString &id,
                     const QString &desc,
                     const QString &subDesc,
                     const QString &text,
                     const QString &command)
{
    out << (indent + QStringLiteral("TextField{"));
    out << (indent + QStringLiteral("    .Id = ") + quote(id));
    out << (indent + QStringLiteral("    .Enabled = true"));
    out << (indent + QStringLiteral("    .Description = ") + quote(desc));
    out << (indent + QStringLiteral("    .SubDescription = ") + quote(subDesc));
    if (!text.isNull()) {
        out << (indent + QStringLiteral("    .Text = ") + quote(text));
    }
    out << (indent + QStringLiteral("    .Command = ") + quote(command));
    out << (indent + QStringLiteral("}"));
}

QString convertLegacyFunctionPreferenceToForm(const QString &legacyText, const QString &formId)
{
    QString normalized = legacyText;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList lines = normalized.split(QLatin1Char('\n'));

    QStringList out;
    out << QStringLiteral("@Version: 1.0");
    out << QStringLiteral("@Author: LegacyConverter");
    out << QStringLiteral("@Description: Converted from legacy asul by Qt C++ minimal converter.");
    out << QString();
    out << QStringLiteral("Form{");
    out << (QStringLiteral("    .Id = ") + quote(formId));

    bool groupOpened = false;
    QString pendingGroupTitle = QStringLiteral("默认分组");

    auto closeGroup = [&]() {
        if (groupOpened) {
            out << QStringLiteral("    }");
            groupOpened = false;
        }
    };

    auto openGroup = [&](const QString &title) {
        closeGroup();
        out << QStringLiteral("    Group{");
        out << (QStringLiteral("        .Title = ") + quote(title));
        groupOpened = true;
    };

    auto ensureGroup = [&]() {
        if (!groupOpened) {
            openGroup(pendingGroupTitle);
        }
    };

    int lineCounter = 0;
    int textCounter = 0;
    int funcCounter = 0;

    static const QRegularExpression funcHeaderRe(
        QStringLiteral("^\\s*func\\s+\"((?:[^\\\"\\\\]|\\\\.)*)\"\\s+\"((?:[^\\\"\\\\]|\\\\.)*)\"\\s+\"((?:[^\\\"\\\\]|\\\\.)*)\"\\s+(\\d+)\\s*$"));

    for (int i = 0; i < lines.size(); ++i) {
        const QString raw = lines.at(i).trimmed();
        if (raw.isEmpty()) {
            continue;
        }

        if (raw.startsWith(QStringLiteral("title"), Qt::CaseInsensitive)) {
            const QStringList vals = extractQuotedStrings(raw);
            const QString title = vals.isEmpty() ? QStringLiteral("未命名分组") : vals.first();
            pendingGroupTitle = title;
            openGroup(title);
            continue;
        }

        if (raw.startsWith(QStringLiteral("line "), Qt::CaseInsensitive)) {
            ensureGroup();
            const QStringList vals = extractQuotedStrings(raw);
            if (vals.size() < 4) {
                continue;
            }

            const QString id = makeId(QStringLiteral("Line"), ++lineCounter);
            QString command = vals.at(2);
            command.replace(QStringLiteral("%1"), QStringLiteral("${$(") + id + QStringLiteral(").Text}"));
            appendTextField(out,
                            QStringLiteral("        "),
                            id,
                            vals.at(0),
                            vals.at(1),
                            vals.at(3),
                            command);
            continue;
        }

        if (raw.startsWith(QStringLiteral("text "), Qt::CaseInsensitive)) {
            ensureGroup();
            const QStringList vals = extractQuotedStrings(raw);
            if (vals.isEmpty()) {
                continue;
            }

            const QString id = makeId(QStringLiteral("Text"), ++textCounter);
            appendTextField(out,
                            QStringLiteral("        "),
                            id,
                            QStringLiteral("静态命令") + QStringLiteral(" ") + QString::number(textCounter),
                            QStringLiteral("由 legacy text 指令转换"),
                            QString(),
                            vals.first());
            continue;
        }

        const QRegularExpressionMatch funcMatch = funcHeaderRe.match(raw);
        if (funcMatch.hasMatch()) {
            ensureGroup();

            const QString desc = unescapeLegacy(funcMatch.captured(1));
            const QString subDesc = unescapeLegacy(funcMatch.captured(2));
            const QString defaultCmd = unescapeLegacy(funcMatch.captured(3));
            const int optionCount = funcMatch.captured(4).toInt();

            const QString fieldId = makeId(QStringLiteral("Func"), ++funcCounter);

            struct Opt {
                QString id;
                QString command;
                QString detail;
                bool matched = false;
            };

            QVector<Opt> opts;
            opts.reserve(optionCount);

            for (int j = 0; j < optionCount && i + 1 < lines.size(); ++j) {
                ++i;
                const QString optRaw = lines.at(i).trimmed();
                const QStringList ov = extractQuotedStrings(optRaw);
                if (ov.size() < 2) {
                    continue;
                }
                Opt opt;
                opt.id = makeId(QStringLiteral("Option"), j + 1);
                opt.command = ov.at(0);
                opt.detail = ov.at(1);
                opt.matched = (opt.command == defaultCmd);
                opts.push_back(opt);
            }

            QString selected = opts.isEmpty() ? QString() : opts.first().id;
            for (const auto &opt : opts) {
                if (opt.matched) {
                    selected = opt.id;
                    break;
                }
            }

            out << QStringLiteral("        OptionField{");
            out << (QStringLiteral("            .Id = ") + quote(fieldId));
            out << QStringLiteral("            .Enabled = true");
            out << (QStringLiteral("            .Description = ") + quote(desc));
            out << (QStringLiteral("            .SubDescription = ") + quote(subDesc));
            out << QStringLiteral("            Options{");
            for (const auto &opt : opts) {
                out << QStringLiteral("                Option{");
                out << (QStringLiteral("                    .Id = ") + quote(opt.id));
                out << (QStringLiteral("                    .Description = ") + quote(opt.detail));
                out << (QStringLiteral("                    .Command = ") + quote(opt.command));
                out << QStringLiteral("                }");
            }
            out << QStringLiteral("            }");
            if (!selected.isEmpty()) {
                out << (QStringLiteral("            .Selected = ") + quote(selected));
            }
            out << QStringLiteral("        }");
            continue;
        }
    }

    closeGroup();
    out << QStringLiteral("}");

    return out.join(QLatin1Char('\n'));
}

bool writeTextFile(const QString &path, const QString &content, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    QTextStream stream(&file);
    stream << content;
    file.close();
    return true;
}

QString defaultOutputPath(const QString &inputPath)
{
    QFileInfo fi(inputPath);
    const QString base = fi.completeBaseName();
    return fi.dir().filePath(base + QStringLiteral("_Form.asul"));
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTextStream qout(stdout);
    QTextStream qerr(stderr);

    const QStringList args = app.arguments();
    const QString inputPath = (args.size() >= 2) ? args.at(1) : QStringLiteral("Function_Preference.asul");
    const QString outputPath = (args.size() >= 3) ? args.at(2) : defaultOutputPath(inputPath);

    QFile inFile(inputPath);
    if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qerr << "[error] failed to read input: " << inputPath << " (" << inFile.errorString() << ")\n";
        return 1;
    }

    QTextStream inStream(&inFile);
    const QString legacyText = inStream.readAll();
    inFile.close();

    const QString formId = QFileInfo(outputPath).completeBaseName();
    const QString formText = convertLegacyFunctionPreferenceToForm(legacyText, formId);

    QString writeError;
    if (!writeTextFile(outputPath, formText, &writeError)) {
        qerr << "[error] failed to write output: " << outputPath << " (" << writeError << ")\n";
        return 2;
    }

    AFormParser::ParseError parseError;
    auto doc = AFormParser::Document::from(formText, &parseError);
    if (!doc) {
        qerr << "[warn] converted Form parse failed: line=" << parseError.line
             << " col=" << parseError.column
             << " msg=" << parseError.message << "\n";
    } else {
        const QString cfg = doc->toCFG();
        qout << "[ok] converted: " << inputPath << " -> " << outputPath << "\n";
        qout << "[ok] cfg preview lines: " << cfg.split('\n').size() << "\n";
    }

    return 0;
}
