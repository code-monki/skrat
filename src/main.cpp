#include <QApplication>
#include <QFileInfo>
#if defined(Q_OS_MACOS)
#include <QIcon>
#endif

#include "MainWindow.h"

/**
 * @file main.cpp
 * @brief Application bootstrap entry point for skrat.
 *
 * Responsibilities:
 * - Initialize QApplication metadata.
 * - Apply platform-specific app icon behavior (macOS bundle icon).
 * - Create/show MainWindow.
 * - Optionally open an initial directory or file from argv[1].
 */

/**
 * @brief Application entry point.
 * @param argc Command-line argument count.
 * @param argv Command-line argument vector.
 * @return QApplication event-loop exit code.
 */
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("skrat"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SKRAT_APP_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("CodeMonki"));

#if defined(Q_OS_MACOS)
    {
        const QFileInfo bundleIcns(
            QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources/skrat.icns"));
        if (bundleIcns.isFile()) {
            app.setWindowIcon(QIcon(bundleIcns.absoluteFilePath()));
        }
    }
#endif

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
