#!/bin/sh
set -e

if [ -z "$1" ]; then
  echo "Usage: convert-svgz.sh <themes-dir>"
  exit 1
fi

THEMES_DIR="$1"

find "$THEMES_DIR" -type f -name "*.svgz" | while read -r svgz; do
  svg="${svgz%.svgz}.svg"
  png="${svgz%.svgz}.png"

  if [ ! -f "$svg" ]; then
    if gzip -t "$svgz" >/dev/null 2>&1; then
      gzip -dc "$svgz" > "$svg"
    else
      cp "$svgz" "$svg"
    fi
  fi

  if [ ! -f "$png" ]; then
    if command -v rsvg-convert >/dev/null 2>&1; then
      rsvg-convert "$svg" -o "$png"
    elif command -v qlmanage >/dev/null 2>&1; then
      tmpdir=$(mktemp -d)
      qlmanage -t -o "$tmpdir" "$svg" >/dev/null 2>&1
      if [ -f "$tmpdir/$(basename "$svg").png" ]; then
        mv "$tmpdir/$(basename "$svg").png" "$png"
      fi
      rm -rf "$tmpdir"
    else
      echo "No SVG renderer found (need rsvg-convert or qlmanage)."
      exit 1
    fi
  fi
done
