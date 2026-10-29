#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Builds tools/animation/host/vuurwerk-host: the firmware's own UI and app
# code running on the PC, for exact screen captures. Defines mirror the
# release build in the Makefile.
set -e
cd "$(dirname "$0")/../../.."
OUT=tools/animation/host/vuurwerk-host
DEFS='-DPRINTF_INCLUDE_CONFIG_H -DAUTHOR_STRING="VUURWERK" -DVERSION_STRING="v1.5.1"
 -DENABLE_SPECTRUM -DENABLE_UART -DENABLE_KEEP_MEM_NAME -DENABLE_WIDE_RX -DENABLE_TX_WHEN_AM
 -DENABLE_NO_CODE_SCAN_TIMEOUT -DENABLE_AM_FIX -DENABLE_SQUELCH_MORE_SENSITIVE
 -DENABLE_FASTER_CHANNEL_SCAN -DENABLE_ADAPTIVE_SCAN -DENABLE_RSSI_BAR -DENABLE_COPY_CHAN_TO_VFO
 -DENABLE_SCAN_RANGES -DENABLE_FLASHLIGHT -DENABLE_CUSTOM_MENU_LAYOUT'
INC='-I. -Itools/animation/host/cmsis -Itools/animation/host'
SRC="$(ls ui/*.c app/*.c helper/*.c external/printf/printf.c *.c | grep -v -e '^main.c$' -e '^syscalls.c$' -e '^sram-overlay.c$' -e '^init.c$' -e '^board.c$' -e '^scheduler.c$')"
# scheduler.c and board.c: scheduler's SystickHandler is kept, its SYSTICK init is not;
# board.c's port setup touches only the mapped register RAM, so it is kept too.
CC="gcc -std=gnu11 -O1 -g -w -fno-strict-aliasing -fcommon $DEFS $INC"
B=tools/animation/host/obj; mkdir -p $B
$CC -c -DAPP_Update=host_loop main.c -o $B/main.o
# bk4819.c with only the two bus-level functions renamed, so every helper in
# the driver (GetRSSI, SetFrequency, ...) calls the host register model
sed -e 's/^uint16_t BK4819_ReadRegister(/uint16_t real_BK4819_ReadRegister(/' \
    -e 's/^void *BK4819_WriteRegister(/void real_BK4819_WriteRegister(/' driver/bk4819.c > $B/bk4819_host.c
$CC -c -Idriver $B/bk4819_host.c -o $B/bk4819.o
$CC $SRC board.c scheduler.c driver/gpio.c driver/backlight.c \
  tools/animation/host/stubs.c tools/animation/host/harness.c $B/main.o $B/bk4819.o -lm -o "$OUT"
echo "built $OUT"
