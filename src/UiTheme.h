/**
 * @file UiTheme.h
 * @brief Persistent UI theme (palette + chrome stylesheet) and application font preferences.
 *
 * Theme modes map to QPalette overrides; optional custom UI font is stored in QSettings
 * and applied via applyUiTheme().
 */

#pragma once

#include <QString>

class QApplication;

namespace skrat {

/** @brief High-level appearance mode for the Qt application palette. */
enum class ThemeMode {
    System = 0,
    Light,
    Dark,
    WarmSepia,
};

/** @brief User-editable theme and font settings persisted under QSettings. */
struct UiThemePrefs {
    ThemeMode mode = ThemeMode::System; ///< Palette mode (system, fixed light/dark, or sepia).
    QString fontFamily;                 ///< Empty means “use system default family”.
    int fontPointSize = 0;              ///< 0 or negative means “use system default size”.
};

/**
 * @brief Reads theme and UI font preferences from application QSettings.
 *
 * @return Populated UiThemePrefs; missing keys yield defaults (system theme, system font).
 */
UiThemePrefs loadUiThemePrefs();

/**
 * @brief Writes theme and UI font preferences to application QSettings.
 *
 * @param[in] prefs Values to persist; font point size is clamped to non-negative on save.
 */
void saveUiThemePrefs(const UiThemePrefs &prefs);

/**
 * @brief Applies palette, widget chrome stylesheet, and application font to a QApplication.
 *
 * Chooses the palette from @a prefs.mode, rebuilds the tab/toolbar stylesheet from the
 * active palette, then sets QApplication::font() from optional family/size in @a prefs.
 *
 * @param[in,out] app Application instance whose palette, stylesheet, and font are updated.
 * @param[in] prefs Theme mode and optional custom UI font settings.
 */
void applyUiTheme(QApplication &app, const UiThemePrefs &prefs);

} // namespace skrat
