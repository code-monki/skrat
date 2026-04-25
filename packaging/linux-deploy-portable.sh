#!/usr/bin/env bash
# =============================================================================
# linux-deploy-portable.sh — Linux portable tarball (linuxdeploy + Qt plugins)
# =============================================================================
# Bundle Qt + skrat into an AppDir and pack a portable tarball (no fuse / AppImage).
# Usage: linux-deploy-portable.sh <x86_64|aarch64> <path-to/skrat-binary>

set -euo pipefail

LINUXDEPLOY_ARCH="${1:?arch}"
BINARY="${2:?path to skrat binary}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DESKTOP="${SCRIPT_DIR}/skrat.desktop"
OUT_TAR="${REPO_ROOT}/skrat-linux-portable-${LINUXDEPLOY_ARCH}.tar.gz"
ICON_THEME="${SCRIPT_DIR}/icons/hicolor"

if [[ ! -f "${BINARY}" ]]; then
  echo "Binary not found: ${BINARY}" >&2
  exit 1
fi
if [[ ! -f "${DESKTOP}" ]]; then
  echo "Desktop file not found: ${DESKTOP}" >&2
  exit 1
fi

WORKDIR="$(mktemp -d)"
GEN_ICON=""
# cleanup — remove temp AppDir and any generated icon file on EXIT.
cleanup() {
  # Never fail the script from cleanup (set -e + EXIT trap would otherwise
  # turn a harmless rm edge case into CI exit 1 after a successful bundle).
  rm -rf "${WORKDIR}" || true
  if [[ -n "${GEN_ICON}" && -f "${GEN_ICON}" ]]; then
    rm -f "${GEN_ICON}" || true
  fi
}
trap cleanup EXIT

# linuxdeploy matches --icon-file basename to the desktop Icon= key (Icon=skrat -> skrat.png).
ICON_ARGS=()
for s in 16 22 24 32 48 64 128 256 512; do
  bundled="${ICON_THEME}/${s}x${s}/apps/skrat.png"
  if [[ -f "${bundled}" ]]; then
    ICON_ARGS+=(--icon-file "${bundled}")
  fi
done

if [[ ${#ICON_ARGS[@]} -eq 0 ]]; then
  command -v convert >/dev/null 2>&1 || {
    echo "ImageMagick 'convert' is required when packaging/icons/hicolor is missing." >&2
    exit 1
  }
  GEN_ICON="${WORKDIR}/skrat.png"
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
      -annotate +0+0 'S' "${GEN_ICON}"
  else
    convert -size 256x256 'xc:#3d6a9e' "${GEN_ICON}"
  fi
  ICON_ARGS=(--icon-file "${GEN_ICON}")
fi

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
  "${ICON_ARGS[@]}"
./linuxdeploy-plugin-qt.AppImage --appdir AppDir

tar -czf "${OUT_TAR}" -C "${WORKDIR}" AppDir
echo "Wrote ${OUT_TAR}"
