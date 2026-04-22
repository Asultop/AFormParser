#include <QApplication>
#include "PreviewWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QString defaultPath = QStringLiteral("samples/Form.asul");
    const QStringList args = app.arguments();
    if (args.size() >= 2) {
        defaultPath = args.at(1);
    }

    PreviewWindow w(defaultPath);
    w.show();

    return app.exec();
}
