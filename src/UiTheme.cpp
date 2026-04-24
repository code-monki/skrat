#include "UiTheme.h"

#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QSettings>
#include <QStyle>

namespace skrat {
namespace {

constexpr const char *kThemeModeKey = "ui/themeMode";
constexpr const char *kFontFamilyKey = "ui/fontFamily";
constexpr const char *kFontPointSizeKey = "ui/fontPointSize";

QPalette lightPalette()
{
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(245, 245, 245));
    pal.setColor(QPalette::WindowText, QColor(32, 32, 32));
    pal.setColor(QPalette::Base, QColor(255, 255, 255));
    pal.setColor(QPalette::AlternateBase, QColor(238, 238, 238));
    pal.setColor(QPalette::Text, QColor(24, 24, 24));
    pal.setColor(QPalette::Button, QColor(238, 238, 238));
    pal.setColor(QPalette::ButtonText, QColor(24, 24, 24));
    pal.setColor(QPalette::Highlight, QColor(42, 130, 218));
    pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    pal.setColor(QPalette::ToolTipBase, QColor(255, 255, 220));
    pal.setColor(QPalette::ToolTipText, QColor(24, 24, 24));
    pal.setColor(QPalette::Link, QColor(30, 103, 176));
    return pal;
}

QPalette darkPalette()
{
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(40, 40, 40));
    pal.setColor(QPalette::WindowText, QColor(230, 230, 230));
    pal.setColor(QPalette::Base, QColor(28, 28, 28));
    pal.setColor(QPalette::AlternateBase, QColor(48, 48, 48));
    pal.setColor(QPalette::Text, QColor(232, 232, 232));
    pal.setColor(QPalette::Button, QColor(56, 56, 56));
    pal.setColor(QPalette::ButtonText, QColor(232, 232, 232));
    pal.setColor(QPalette::Highlight, QColor(59, 130, 246));
    pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    pal.setColor(QPalette::ToolTipBase, QColor(64, 64, 64));
    pal.setColor(QPalette::ToolTipText, QColor(236, 236, 236));
    pal.setColor(QPalette::Link, QColor(114, 179, 255));
    return pal;
}

QPalette warmSepiaPalette()
{
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(244, 240, 232));
    pal.setColor(QPalette::WindowText, QColor(44, 32, 16));
    pal.setColor(QPalette::Base, QColor(250, 246, 238));
    pal.setColor(QPalette::AlternateBase, QColor(235, 228, 214));
    pal.setColor(QPalette::Text, QColor(44, 32, 16));
    pal.setColor(QPalette::Button, QColor(237, 231, 217));
    pal.setColor(QPalette::ButtonText, QColor(44, 32, 16));
    pal.setColor(QPalette::Highlight, QColor(193, 127, 36));
    pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    pal.setColor(QPalette::ToolTipBase, QColor(251, 243, 223));
    pal.setColor(QPalette::ToolTipText, QColor(44, 32, 16));
    pal.setColor(QPalette::Link, QColor(139, 84, 19));
    return pal;
}

QString buildChromeStylesheet(const QPalette &pal)
{
    const QString window = pal.color(QPalette::Window).name();
    const QString base = pal.color(QPalette::Base).name();
    const QString button = pal.color(QPalette::Button).name();
    const QString text = pal.color(QPalette::Text).name();
    const QString buttonText = pal.color(QPalette::ButtonText).name();
    const QString border = pal.color(QPalette::Mid).name();
    const QString altBase = pal.color(QPalette::AlternateBase).name();
    const QString selectedBorder = pal.color(QPalette::Highlight).name();
    const QString selectedText = pal.color(QPalette::HighlightedText).name();

    return QStringLiteral(
               "QTabWidget::pane {"
               "  border: 1px solid %1;"
               "  top: -1px;"
               "  background: %2;"
               "}"
               "QTabWidget::left-corner, QTabWidget::right-corner {"
               "  background: %3;"
               "  border-bottom: 1px solid %1;"
               "}"
               "QTabBar::tab {"
               "  background: %3;"
               "  color: %4;"
               "  border: 1px solid %1;"
               "  border-bottom: none;"
               "  border-top-left-radius: 4px;"
               "  border-top-right-radius: 4px;"
               "  padding: 4px 10px;"
               "  margin-right: 2px;"
               "}"
               "QTabBar::tab:selected {"
               "  background: %2;"
               "  border-color: %5;"
               "}"
               "QTabBar::tab:!selected {"
               "  margin-top: 2px;"
               "}"
               "QToolBar {"
               "  background: %1;"
               "  border: 1px solid %6;"
               "}"
               "QToolButton {"
               "  background: %3;"
               "  color: %7;"
               "  border: 1px solid %6;"
               "  border-radius: 3px;"
               "  padding: 2px 6px;"
               "}"
               "QToolButton:hover {"
               "  background: %8;"
               "}"
               "QToolButton:pressed {"
               "  background: %2;"
               "}"
               "QToolButton:disabled {"
               "  color: %6;"
               "}"
               "QLineEdit {"
               "  background: %2;"
               "  color: %4;"
               "  border: 1px solid %6;"
               "  border-radius: 3px;"
               "  padding: 1px 6px;"
               "  selection-background-color: %5;"
               "  selection-color: %9;"
               "}"
               "QLineEdit:disabled {"
               "  background: %8;"
               "  color: %6;"
               "}")
        .arg(window, base, button, text, selectedBorder, border, buttonText, altBase, selectedText);
}

} // namespace

UiThemePrefs loadUiThemePrefs()
{
    QSettings s;
    UiThemePrefs prefs;
    prefs.mode = static_cast<ThemeMode>(s.value(QString::fromLatin1(kThemeModeKey), 0).toInt());
    prefs.fontFamily = s.value(QString::fromLatin1(kFontFamilyKey)).toString().trimmed();
    prefs.fontPointSize = s.value(QString::fromLatin1(kFontPointSizeKey), 0).toInt();
    return prefs;
}

void saveUiThemePrefs(const UiThemePrefs &prefs)
{
    QSettings s;
    s.setValue(QString::fromLatin1(kThemeModeKey), static_cast<int>(prefs.mode));
    s.setValue(QString::fromLatin1(kFontFamilyKey), prefs.fontFamily.trimmed());
    s.setValue(QString::fromLatin1(kFontPointSizeKey), qMax(0, prefs.fontPointSize));
}

void applyUiTheme(QApplication &app, const UiThemePrefs &prefs)
{
    const QPalette systemPalette = app.style() ? app.style()->standardPalette() : QPalette();
    const QFont systemFont = QApplication::font();

    switch (prefs.mode) {
    case ThemeMode::System:
        app.setPalette(systemPalette);
        break;
    case ThemeMode::Light:
        app.setPalette(lightPalette());
        break;
    case ThemeMode::Dark:
        app.setPalette(darkPalette());
        break;
    case ThemeMode::WarmSepia:
        app.setPalette(warmSepiaPalette());
        break;
    }

    app.setStyleSheet(buildChromeStylesheet(app.palette()));

    QFont uiFont = systemFont;
    if (!prefs.fontFamily.isEmpty()) {
        uiFont.setFamily(prefs.fontFamily);
    }
    if (prefs.fontPointSize > 0) {
        uiFont.setPointSize(prefs.fontPointSize);
    }
    app.setFont(uiFont);
}

} // namespace skrat
