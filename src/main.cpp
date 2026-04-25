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

#include <QApplication>
#include <QFileInfo>
#if defined(Q_OS_MACOS)
#include <QIcon>
#endif

#include "MainWindow.h"
#include "UiTheme.h"

/**
 * @brief Runs the Qt event loop for the skrat application.
 *
 * Sets application metadata, applies saved UI theme, optionally sets the window icon from
 * the macOS bundle, constructs MainWindow, and if a path is passed as the first argument,
 * opens that folder or selects that file in the tree.
 *
 * @param[in] argc Number of elements in @a argv (including the program name).
 * @param[in] argv Argument vector; @c argv[1] may be an initial folder or file path.
 * @return Exit code from QApplication::exec() (typically 0 on normal quit).
 */
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("skrat"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SKRAT_APP_VERSION));
    QCoreApplication::setOrganizationName(QStringLiteral("CodeMonki"));

    skrat::applyUiTheme(app, skrat::loadUiThemePrefs());

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
