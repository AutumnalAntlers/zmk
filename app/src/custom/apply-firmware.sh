#!/usr/bin/env -S guix shell --pure coreutils bash unzip util-linux wget -- bash --
set -xeou pipefail

ZIP="$1" && [[ -n "$ZIP" ]]
BOOTLOADER_DEVICE_LEFT="/dev/disk/by-id/usb-Adafruit_nRF_UF2_2CF56449277B2ACF-0:0"
BOOTLOADER_DEVICE_RIGHT="/dev/disk/by-id/usb-Adafruit_nRF_UF2_946A216013472AAB-0:0"
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

while true; do
  if [[ -b "$BOOTLOADER_DEVICE_LEFT" ]]; then
    DEV="$BOOTLOADER_DEVICE_LEFT"
    SIDE="left"
    break
  elif [[ -b "$BOOTLOADER_DEVICE_RIGHT" ]]; then
    DEV="$BOOTLOADER_DEVICE_RIGHT"
    SIDE="right"
    break
  fi
  echo  "Waiting for keyboard..."
  sleep 5s
done

mkdir -p /tmp/zmk/boot
cd /tmp/zmk

mount "$DEV" boot

unzip "$ZIP"
cp ./*"$SIDE"*.uf2 boot
umount boot

cleanup
