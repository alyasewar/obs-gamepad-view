#!/bin/sh
# Assembles the OBS .plugin bundle macOS expects. OBS 32 does not scan for bare
# .so files under plugins/<name>/bin, so the loose layout silently never loads.
#
#   ./cmake/package-macos.sh build/link/input_visualizer.so build/data [dest]
set -e

BINARY="$1"
DATA_DIR="$2"
DEST="${3:-$HOME/Library/Application Support/obs-studio/plugins}"
NAME="input-visualizer"

if [ -z "$BINARY" ] || [ -z "$DATA_DIR" ]; then
  echo "usage: package-macos.sh <binary> <data-dir> [plugins-dir]" >&2
  exit 1
fi

BUNDLE="$DEST/$NAME.plugin"
rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/Contents/MacOS" "$BUNDLE/Contents/Resources"
cp "$BINARY" "$BUNDLE/Contents/MacOS/$NAME"
cp -R "$DATA_DIR"/* "$BUNDLE/Contents/Resources/"

cat > "$BUNDLE/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleDevelopmentRegion</key><string>en</string>
	<key>CFBundleExecutable</key><string>$NAME</string>
	<key>CFBundleIdentifier</key><string>com.obsproject.$NAME</string>
	<key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
	<key>CFBundleName</key><string>$NAME</string>
	<key>CFBundlePackageType</key><string>BNDL</string>
	<key>CFBundleShortVersionString</key><string>0.2.0</string>
	<key>CFBundleVersion</key><string>0.2.0</string>
	<key>LSMinimumSystemVersion</key><string>11.0</string>
</dict>
</plist>
PLIST

echo "installed $BUNDLE"
