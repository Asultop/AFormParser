#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

#include "AFormParser.hpp"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream qout(stdout);
    QTextStream qerr(stderr);

    const QStringList args = app.arguments();
    if (args.size() < 2) {
        qerr << "Usage: form_to_cfg <input_form.asul> [output_cfg.txt]\n";
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

    const QString cfg = doc->toCFG();

    if (args.size() >= 3) {
        const QString outputPath = args.at(2);
        QFile outFile(outputPath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            qerr << "Failed to open output: " << outputPath << "\n";
            return 4;
        }
        outFile.write(cfg.toUtf8());
        outFile.close();
    }

    qout << cfg << "\n";
    return 0;
}
