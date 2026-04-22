#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

#include "AFormParser/AFormParser.hpp"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream qout(stdout);
    QTextStream qerr(stderr);

    const QStringList args = app.arguments();
    const QString inputPath = (args.size() >= 2) ? args.at(1) : QStringLiteral("samples/Register_Function_Form.asul");

    QFile inFile(inputPath);
    if (!inFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qerr << "[error] failed to open input: " << inputPath << "\n";
        return 1;
    }

    const QString formText = QString::fromUtf8(inFile.readAll());
    inFile.close();

    AFormParser::ParseError err;
    auto doc = AFormParser::Document::from(formText, &err);
    if (!doc) {
        qerr << "[error] parse failed at line=" << err.line
             << " col=" << err.column
             << " msg=" << err.message << "\n";
        return 2;
    }

    // Register a C++ function and expose it to Lua scripts.
    doc->registerFunction(QStringLiteral("RegisteredJoin"), [](QStringList fnArgs) -> QString {
        if (fnArgs.size() < 2) {
            return QStringLiteral("invalid_args");
        }
        return fnArgs.at(0) + QStringLiteral("_") + fnArgs.at(1);
    });

    // Global value used by ${__Greeting} in the sample form.
    QMap<QString, QString> globals;
    globals.insert(QStringLiteral("__Greeting"), QStringLiteral("HelloFromRegisterFunctionDemo"));
    doc->setGlobalVariables(globals);

    const QString cfg = doc->toCFG();

    if (args.size() >= 3) {
        const QString outputPath = args.at(2);
        QFile outFile(outputPath);
        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            qerr << "[error] failed to open output: " << outputPath << "\n";
            return 3;
        }
        outFile.write(cfg.toUtf8());
        outFile.close();
    }

    qout << cfg << "\n";
    return 0;
}
