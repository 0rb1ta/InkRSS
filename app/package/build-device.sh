#!/bin/sh
# Cross-compiles InkRSS for PocketBook (SDK B288 6.8) and lays out app/dist/ to mirror
# the reader's internal storage: copy the contents of dist/ onto the reader's root.
set -e
here=$(cd "$(dirname "$0")/.." && pwd)
. "${PB_ENV:-$HOME/pocketbook-sdk/env.sh}"

mkdir -p "$here/build"
cd "$here/build"
qmake ../app.pro CONFIG+=release
# The build time is a define: objects that print it are rebuilt every time, or they keep an old one.
rm -f main.o SettingsScreen.o
make -s -j"$(nproc)"
arm-obreey-linux-gnueabi-strip inkrss

dist="$here/dist"
rm -rf "$dist"
mkdir -p "$dist/applications/icons" "$dist/system/inkrss/bin"
cp inkrss "$dist/system/inkrss/bin/inkrss"
# The firmware has no CA store Qt can use. A fixed copy of the Mozilla set (Ubuntu ca-certificates
# 20260816, taken 2026-10-29) ships with the app, so every build trusts the same roots.
cp "$here/package/cacert.pem" "$dist/system/inkrss/bin/cacert.pem"
cp "$here/package/InkRSS.app" "$dist/applications/InkRSS.app"
# Article fonts (SIL OFL, unmodified, licence texts alongside).
mkdir -p "$dist/system/inkrss/fonts"
cp "$here"/third_party/fonts/*/*.ttf "$dist/system/inkrss/fonts/"
cp "$here/third_party/fonts/ptserif/OFL.txt" "$dist/system/inkrss/fonts/OFL-PTSerif.txt"
cp "$here/third_party/fonts/literata/OFL.txt" "$dist/system/inkrss/fonts/OFL-Literata.txt"
# Licence texts travel with the app.
cp "$here/../LICENSE" "$dist/system/inkrss/LICENSE.txt"
cp "$here/third_party/LICENSES.md" "$dist/system/inkrss/LICENSES.md"
# Launcher icons for the Applications menu (used only if the user adds them to view.json).
[ -f "$here/third_party/inkrss/icon.bmp" ] && cp "$here/third_party/inkrss/icon.bmp" "$dist/applications/icons/inkrss.bmp"
[ -f "$here/third_party/inkrss/icon_f.bmp" ] && cp "$here/third_party/inkrss/icon_f.bmp" "$dist/applications/icons/inkrss_f.bmp"
echo "Ready: copy the contents of $dist onto the reader's internal storage root."
