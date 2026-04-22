#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include "AFormParser/AFormParser.hpp"

bool writeFile(const QString &filePath, const QString &content, QString *error)
{
	QFileInfo fi(filePath);
	QDir dir = fi.absoluteDir();
	if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
		if (error) {
			*error = QStringLiteral("无法创建目录: ") + dir.absolutePath();
		}
		return false;
	}

	QFile outFile(filePath);
	if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
		if (error) {
			*error = QStringLiteral("无法打开文件写入: ") + outFile.errorString();
		}
		return false;
	}
	outFile.write(content.toUtf8());
	outFile.close();
	return true;
}

int main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);
	QTextStream qout(stdout);
	QTextStream qerr(stderr);

	const QStringList args = app.arguments();
	if (args.size() < 2) {
		qerr << "Usage: form_to_cfg <input_form.asul> [output_dir] [--source=<asul_path>]\n";
		qerr << "  output_dir: Optional output directory for .cfg files\n";
		qerr << "  --source=<path>: Set source file path for relative path resolution\n";
		return 1;
	}

	const QString inputPath = args.at(1);
	QFile inFile(inputPath);
	if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
		qerr << "Failed to open input: " << inputPath << "\n";
		return 2;
	}

	const QString formText = QString::fromUtf8(inFile.readAll());
	inFile.close();

	AFormParser::ParseError err;
	auto doc = AFormParser::Document::from(formText, &err);
	if (!doc) {
		qerr << "Parse error at line " << err.line
			 << ", column " << err.column
			 << ": " << err.message << "\n";
		return 3;
	}

	QString sourcePath = inputPath;
	QString outputDir;

	for (int i = 2; i < args.size(); ++i) {
		const QString &arg = args.at(i);
		if (arg.startsWith(QLatin1String("--source="))) {
			sourcePath = arg.mid(9);
		} else if (!arg.startsWith(QLatin1Char('-'))) {
			outputDir = arg;
		}
	}

	doc->setSourceFilePath(sourcePath);

	auto cfgItems = doc->toCFGs();

	if (cfgItems.isEmpty()) {
		qerr << "No CFG items generated\n";
		return 4;
	}

	int successCount = 0;
	for (const auto &item : cfgItems) {
		QString outputPath;
		if (!outputDir.isEmpty()) {
			QFileInfo fi(item.absolutePath);
			outputPath = outputDir + QLatin1String("/") + fi.fileName();
		} else {
			outputPath = item.absolutePath;
		}

		if (!item.content.isEmpty()) {
			QString writeError;
			if (writeFile(outputPath, item.content, &writeError)) {
				qout << "Written: " << outputPath << "\n";
				++successCount;
			} else {
				qerr << "Error writing " << outputPath << ": " << writeError << "\n";
			}
		} else {
			qout << "Skipped (empty content): " << outputPath << "\n";
		}
	}

	qout << "\nGenerated " << successCount << " of " << cfgItems.size() << " CFG files\n";
	return 0;
}