#!/bin/sh
# Copies app/dist/ onto a connected PocketBook (USB, "PC link" mode) and checks the binary arrived.
set -e
here=$(cd "$(dirname "$0")/.." && pwd)
dist="$here/dist"
[ -f "$dist/system/inkrss/bin/inkrss" ] || { echo "Нет сборки: сначала app/package/build-device.sh"; exit 1; }
root=""
for d in /run/media/"$USER"/* /media/"$USER"/*; do
    [ -d "$d/system/inkrss" ] || [ -d "$d/system/config" ] || continue
    root="$d"
    break
done
[ -n "$root" ] || { echo "Читалка не подключена (выберите на ней «Связь с ПК»)"; exit 1; }
cp -r "$dist/." "$root/"
sync
want=$(stat -c %s "$dist/system/inkrss/bin/inkrss")
got=$(stat -c %s "$root/system/inkrss/bin/inkrss")
[ "$want" = "$got" ] || { echo "Ошибка: на читалке $got байт вместо $want"; exit 1; }
build=$(strings -e l "$dist/system/inkrss/bin/inkrss" | grep -m1 -E '^20[0-9]{2}-[0-9]{2}-[0-9]{2}_[0-9]{2}:[0-9]{2}' || true)
echo "Установлено на $root: build $build"
