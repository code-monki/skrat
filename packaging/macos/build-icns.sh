#!/usr/bin/env bash
# Rebuild packaging/macos/skrat.icns from the 512px hicolor PNG (run on macOS).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="${ROOT}/../icons/hicolor/512x512/apps/skrat.png"
OUT="${ROOT}/skrat.icns"
ICONSET="${ROOT}/skrat.iconset"
if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "This script requires macOS (iconutil)." >&2
  exit 1
fi
if [[ ! -f "${SRC}" ]]; then
  echo "Missing ${SRC}" >&2
  exit 1
fi
rm -rf "${ICONSET}" "${OUT}"
mkdir -p "${ICONSET}"
specs=(
  "16:icon_16x16.png"
  "32:icon_16x16@2x.png"
  "32:icon_32x32.png"
  "64:icon_32x32@2x.png"
  "128:icon_128x128.png"
  "256:icon_128x128@2x.png"
  "256:icon_256x256.png"
  "512:icon_256x256@2x.png"
  "512:icon_512x512.png"
  "1024:icon_512x512@2x.png"
)
for entry in "${specs[@]}"; do
  IFS=: read -r sz name <<< "${entry}"
  sips -z "${sz}" "${sz}" "${SRC}" --out "${ICONSET}/${name}"
done
iconutil -c icns "${ICONSET}" -o "${OUT}"
rm -rf "${ICONSET}"
echo "Wrote ${OUT} — run ./packaging/macos/install-bundle-icon.sh build/skrat.app to refresh an existing bundle."
