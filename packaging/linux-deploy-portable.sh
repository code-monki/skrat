#!/usr/bin/env bash
# Bundle Qt + skrat into an AppDir and pack a portable tarball (no fuse / AppImage).
# Usage: linux-deploy-portable.sh <x86_64|aarch64> <path-to/skrat-binary>

set -euo pipefail

LINUXDEPLOY_ARCH="${1:?arch}"
BINARY="${2:?path to skrat binary}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DESKTOP="${SCRIPT_DIR}/skrat.desktop"
OUT_TAR="${REPO_ROOT}/skrat-linux-portable-${LINUXDEPLOY_ARCH}.tar.gz"

if [[ ! -f "${BINARY}" ]]; then
  echo "Binary not found: ${BINARY}" >&2
  exit 1
fi
if [[ ! -f "${DESKTOP}" ]]; then
  echo "Desktop file not found: ${DESKTOP}" >&2
  exit 1
fi

command -v convert >/dev/null 2>&1 || {
  echo "ImageMagick 'convert' is required to build a desktop icon." >&2
  exit 1
}

ICON="${SCRIPT_DIR}/skrat-icon-generated.png"
# -annotate needs a real font; minimal CI images often have no ImageMagick default (font '(null)').
ICON_FONT=""
for candidate in \
  /usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf \
  /usr/share/fonts/dejavu/DejaVuSans-Bold.ttf \
  /usr/share/fonts/TTF/DejaVuSans-Bold.ttf; do
  if [[ -f "${candidate}" ]]; then
    ICON_FONT="${candidate}"
    break
  fi
done
if [[ -n "${ICON_FONT}" ]]; then
  convert -size 256x256 'xc:#3d6a9e' -font "${ICON_FONT}" -pointsize 72 -fill white -gravity center \
    -annotate +0+0 'S' "${ICON}"
else
  convert -size 256x256 'xc:#3d6a9e' "${ICON}"
fi

WORKDIR="$(mktemp -d)"
cleanup() { rm -rf "${WORKDIR}" "${ICON}"; }
trap cleanup EXIT

cd "${WORKDIR}"
export APPIMAGE_EXTRACT_AND_RUN=1
# Bundled strip in linuxdeploy AppImage is too old for RELR (.relr.dyn) in modern glibc/Qt ELFs (e.g. Fedora).
export NO_STRIP=1

LD_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${LINUXDEPLOY_ARCH}.AppImage"
LDQT_URL="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${LINUXDEPLOY_ARCH}.AppImage"

curl -fsSL -o linuxdeploy.AppImage "${LD_URL}"
curl -fsSL -o linuxdeploy-plugin-qt.AppImage "${LDQT_URL}"
chmod +x linuxdeploy.AppImage linuxdeploy-plugin-qt.AppImage

rm -rf AppDir

./linuxdeploy.AppImage --appdir AppDir --executable "${BINARY}" --desktop-file "${DESKTOP}" \
  --icon-file "${ICON}"
./linuxdeploy-plugin-qt.AppImage --appdir AppDir

tar -czf "${OUT_TAR}" -C "${WORKDIR}" AppDir
echo "Wrote ${OUT_TAR}"
