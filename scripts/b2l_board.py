# SPDX-License-Identifier: BSD-2-Clause
"""Board selection shared by the packaging, firmware and recovery scripts.

B2L_BOARD selects the Samsung board; the default keeps the original S10+
behaviour. The XNU kernel is identical on every board: the external UEFI
platform, the appended DTB, the loader's RAM ceiling and display profile, and
the identity/partition guards differ.

The shell tools read the same table through `b2l_board.py shell`, and
tests/test_board_profiles.py keeps tools/b1l/board.sh in step with it.
"""
import os
import sys

BOARDS = {
    # Galaxy S10+ reference board.
    'beyond2lte': {
        'model': 'SM-G975F', 'name': 'Galaxy S10+',
        'boot': ('/dev/block/sda14', 57671680),
        'system': None,
        'userdata': ('/dev/block/sda31', None),
        'dt_evidence': 'beyond2lte-8g-dt.json',
        'ram_end': 0x9FD800000,
        'absent_ddr': ((0xBAB00000, 0x5500000), (0xE0000000, 0x1900000)),
        'panel': (1440, 3040), 'fb_logical': None, 'display_scale': 3,
    },
    # Galaxy S10. Same Exynos 9820 board family, DT revision 26: reserved
    # memory, HSI2C10 touch, GPA keys and panel controller match beyond2lte.
    'beyond1lte': {
        'model': 'SM-G973F', 'name': 'Galaxy S10',
        'boot': ('/dev/block/sda14', 57671680),
        'system': ('/dev/block/sda25', 7340032000),
        'userdata': ('/dev/block/sda34', 499641200640),
        'dt_evidence': 'beyond1lte-8g-dt.json',
        'ram_end': 0x9FD800000,
        'absent_ddr': ((0xBAB00000, 0x5500000), (0xE0000000, 0x1900000)),
        'panel': (1440, 3040), 'fb_logical': (880, 1912), 'display_scale': 2,
    },
    # Galaxy S10e, 6 GiB (docs/device-facts-beyond0lte.md). DT revision 25:
    # same low banks, HSI2C10/sec_ts address (chip Y761) and reserved memory
    # as the S10 except a smaller camera carve-out; DRAM ends at 0x980000000
    # and el2_earlymem moved to 0x97E000000 (32 MiB). The upper 2 GiB that an
    # 8 GiB map would describe is reserved as absent DDR.
    'beyond0lte': {
        'model': 'SM-G970F', 'name': 'Galaxy S10e',
        'boot': ('/dev/block/sda14', 57671680),
        'system': ('/dev/block/sda25', 5976883200),
        'userdata': ('/dev/block/sda31', 119126622208),
        'dt_evidence': 'beyond0lte-6g-dt.json',
        'ram_end': 0x97E000000,
        'absent_ddr': ((0xBAB00000, 0x5500000), (0xE0000000, 0x1900000),
                       (0x980000000, 0x80000000)),
        'panel': (1080, 2280), 'fb_logical': (750, 1584), 'display_scale': 2,
    },
}


def board():
    name = os.environ.get('B2L_BOARD', 'beyond2lte')
    if name not in BOARDS:
        raise SystemExit(f'unknown B2L_BOARD {name!r}; expected one of {sorted(BOARDS)}')
    return name


def profile(name=None):
    return BOARDS[name or board()]


def model(name=None):
    return profile(name)['model']


def userdata(name=None):
    return profile(name)['userdata'][0]


def platform_package(name=None):
    return f'{name or board()}Pkg'


def identity_ok(getprop, name=None):
    """Accept the recovery only when its hardware model matches the board.

    ro.boot.em.model comes from the bootloader. ro.product.device can be
    rewritten by custom ROMs (ArtisanROM reports r13s), so it is accepted
    only as the legacy S10+ identity.
    """
    name = name or board()
    if getprop('ro.boot.em.model') == model(name):
        return True
    return name == 'beyond2lte' and getprop('ro.product.device') == 'beyond2lte'


def shell_assignments(name=None):
    """Shell variables for tools/b1l and tools/flash-kit (see board.sh)."""
    name = name or board()
    p = BOARDS[name]
    out = {'B2L_BOARD': name, 'B2L_MODEL': p['model'],
           'B2L_BOOT_DEV': p['boot'][0], 'B2L_BOOT_BYTES': p['boot'][1],
           'B2L_RAM_ALLOW_END': '0x%XULL' % p['ram_end'],
           'B2L_DISPLAY_SCALE': p['display_scale']}
    if p['system']:
        out.update(B2L_SYSTEM_DEV=p['system'][0], B2L_SYSTEM_BYTES=p['system'][1])
    if p['userdata'][1]:
        out.update(B2L_USERDATA_DEV=p['userdata'][0], B2L_USERDATA_BYTES=p['userdata'][1])
    if p['fb_logical']:
        out['B2L_FB_LOGICAL'] = '%dx%d' % p['fb_logical']
    return out


if __name__ == '__main__':
    if sys.argv[1:] != ['shell']:
        raise SystemExit('usage: B2L_BOARD=NAME b2l_board.py shell')
    for key, value in shell_assignments().items():
        print(f'{key}={value}')
