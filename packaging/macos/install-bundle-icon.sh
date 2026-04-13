#!/usr/bin/env bash
# Copy skrat.icns into a .app bundle and set CFBundleIconFile (Finder / Dock).
# Usage: install-bundle-icon.sh /path/to/skrat.app
set -euo pipefail
APP="${1:?path to skrat.app}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ICNS="${SCRIPT_DIR}/skrat.icns"
if [[ ! -d "${APP}/Contents" ]]; then
  echo "Not an app bundle: ${APP}" >&2
  exit 1
fi
if [[ ! -f "${ICNS}" ]]; then
  echo "Missing ${ICNS}" >&2
  exit 1
fi
mkdir -p "${APP}/Contents/Resources"
cp -f "${ICNS}" "${APP}/Contents/Resources/skrat.icns"
PLIST="${APP}/Contents/Info.plist"
if /usr/libexec/PlistBuddy -c "Print :CFBundleIconFile" "${PLIST}" >/dev/null 2>&1; then
  /usr/libexec/PlistBuddy -c "Set :CFBundleIconFile skrat" "${PLIST}"
else
  /usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string skrat" "${PLIST}"
fi
touch "${APP}"
