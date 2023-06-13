#!/usr/bin/env -S guix shell --pure coreutils bash unzip util-linux wget -- bash --
set -xeou pipefail

ZIP="$1" && [[ -n "$ZIP" ]]
BOOTLOADER_DEVICE="/dev/disk/by-id/usb-Adafruit_nRF_UF2_2CF56449277B2ACF-0:0"
export ZIP

cleanup () {
  mountpoint --quiet /tmp/zmk/boot && umount /tmp/zmk/boot
  for file in /tmp/zmk{/*,}; do
    if [[ -e "$file" ]]; then
      rm -r "$file";
    fi;
  done
}
trap cleanup EXIT
cleanup

if [[ ! -b "$BOOTLOADER_DEVICE" ]]; then
  echo  "Waiting for \"$BOOTLOADER_DEVICE\"..."
  while [[ ! -b "$BOOTLOADER_DEVICE" ]]; do
    sleep 5s
  done
fi

mkdir -p /tmp/zmk/boot
cd /tmp/zmk

mount "$BOOTLOADER_DEVICE" boot

unzip "$ZIP"
cp ./*.uf2 boot
umount boot

cleanup
