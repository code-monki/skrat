#pragma once

#include <QString>

class QApplication;

namespace skrat {

enum class ThemeMode {
    System = 0,
    Light,
    Dark,
    WarmSepia,
};

struct UiThemePrefs {
    ThemeMode mode = ThemeMode::System;
    QString fontFamily;
    int fontPointSize = 0;
};

UiThemePrefs loadUiThemePrefs();
void saveUiThemePrefs(const UiThemePrefs &prefs);
void applyUiTheme(QApplication &app, const UiThemePrefs &prefs);

} // namespace skrat
