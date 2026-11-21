# SPDX-License-Identifier: BSD-2-Clause
# Sourced by tools/b1l and tools/flash-kit: the target phone's identity and partitions.
# B2L_BOARD selects it (default beyond1lte, the S10 these tools were written for).
# Values mirror scripts/b2l_board.py; tests/test_board_profiles.py keeps them equal.
#   beyond1lte  Galaxy S10   SM-G973F  8 GiB
#   beyond0lte  Galaxy S10e  SM-G970F  6 GiB (docs/device-facts-beyond0lte.md)
B2L_BOARD=${B2L_BOARD:-beyond1lte}
case "$B2L_BOARD" in
	beyond1lte)
		B2L_MODEL=SM-G973F
		B2L_BOOT_DEV=/dev/block/sda14; B2L_BOOT_BYTES=57671680
		B2L_SYSTEM_DEV=/dev/block/sda25; B2L_SYSTEM_BYTES=7340032000
		B2L_USERDATA_DEV=/dev/block/sda34; B2L_USERDATA_BYTES=499641200640
		B2L_UEFI_PKG=beyond1ltePkg
		;;
	beyond0lte)
		B2L_MODEL=SM-G970F
		B2L_BOOT_DEV=/dev/block/sda14; B2L_BOOT_BYTES=57671680
		B2L_SYSTEM_DEV=/dev/block/sda25; B2L_SYSTEM_BYTES=5976883200
		B2L_USERDATA_DEV=/dev/block/sda31; B2L_USERDATA_BYTES=119126622208
		B2L_UEFI_PKG=beyond0ltePkg
		;;
	*)
		echo "board.sh: unknown B2L_BOARD '$B2L_BOARD' (beyond1lte or beyond0lte)" >&2
		return 1 2>/dev/null || exit 1
		;;
esac
export B2L_BOARD B2L_MODEL B2L_BOOT_DEV B2L_BOOT_BYTES B2L_SYSTEM_DEV B2L_SYSTEM_BYTES \
	B2L_USERDATA_DEV B2L_USERDATA_BYTES B2L_UEFI_PKG
# File size on macOS (BSD stat) and Linux/WSL (GNU stat).
b2l_size() { stat -c %s "$1" 2>/dev/null || stat -f %z "$1"; }
