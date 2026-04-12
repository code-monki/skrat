#!/usr/bin/env bash
# Rasterize packaging/icons/skrat.svg into freedesktop hicolor PNGs (requires ImageMagick or rsvg-convert).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SVG="${ROOT}/skrat.svg"
if [[ ! -f "${SVG}" ]]; then
  echo "Missing ${SVG}" >&2
  exit 1
fi
export_sizes=(16 22 24 32 48 64 128 256 512)
if command -v rsvg-convert >/dev/null 2>&1; then
  for s in "${export_sizes[@]}"; do
    out="${ROOT}/hicolor/${s}x${s}/apps/skrat.png"
    mkdir -p "$(dirname "${out}")"
    rsvg-convert --width="${s}" --height="${s}" -o "${out}" "${SVG}"
  done
elif command -v magick >/dev/null 2>&1; then
  for s in "${export_sizes[@]}"; do
    out="${ROOT}/hicolor/${s}x${s}/apps/skrat.png"
    mkdir -p "$(dirname "${out}")"
    magick -background none "${SVG}" -resize "${s}x${s}" "${out}"
  done
elif command -v convert >/dev/null 2>&1; then
  for s in "${export_sizes[@]}"; do
    out="${ROOT}/hicolor/${s}x${s}/apps/skrat.png"
    mkdir -p "$(dirname "${out}")"
    convert -background none "${SVG}" -resize "${s}x${s}" "${out}"
  done
else
  echo "Install librsvg (rsvg-convert) or ImageMagick to export PNGs." >&2
  exit 1
fi
echo "Wrote skrat.png under ${ROOT}/hicolor/*/apps/"
