#include <QApplication>
#include <QFileInfo>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("skrat"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QCoreApplication::setOrganizationName(QStringLiteral("CodeMonki"));

    MainWindow window;

    const QStringList args = QCoreApplication::arguments();
    if (args.size() > 1) {
        const QFileInfo fi(args.at(1));
        if (fi.exists() && fi.isDir()) {
            window.setRootFolder(fi.absoluteFilePath());
        } else if (fi.exists() && fi.isFile()) {
            window.setRootFolder(fi.absolutePath());
            window.selectPath(fi.absoluteFilePath());
        }
    }

    window.show();
    return app.exec();
}
