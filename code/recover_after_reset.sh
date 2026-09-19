#!/bin/sh
set -eu

mkdir -p build

while :; do
    openocd -f interface/jlink.cfg -c 'transport select swd' \
        -f target/stm32f4x.cfg -c 'adapter speed 100' \
        -c 'init' -c 'reset halt' \
        -c 'program build/wm8978_usb_audio.elf verify' \
        -c 'reset run' -c 'shutdown' > build/openocd_recovery.log 2>&1 || true

    if grep -q '\*\* Verified OK \*\*' build/openocd_recovery.log; then
        cat build/openocd_recovery.log
        exit 0
    fi

    printf '.'
done
