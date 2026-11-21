#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Board profiles agree with each other and with the measured device trees.

scripts/b2l_board.py drives packaging and firmware preparation; tools/b1l/board.sh
drives the flash tools. A wrong partition or RAM value on one side would let a
guard pass on the wrong phone or hand absent DDR to XNU.
"""
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('b2l_board', ROOT / 'scripts/b2l_board.py')
b2l_board = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b2l_board)


def reg(node):
    raw = bytes.fromhex(node['reg'])
    return int.from_bytes(raw[:8], 'big'), int.from_bytes(raw[8:], 'big')


def board_sh(board):
    """Parse the case arm of tools/b1l/board.sh for one board."""
    text = (ROOT / 'tools/b1l/board.sh').read_text()
    arm = re.search(r'^\t%s\)\n(.*?)\t\t;;' % board, text, re.M | re.S)
    assert arm, board
    return dict(re.findall(r'(B2L_\w+)=(\S+?)(?:;|$)', arm.group(1), re.M))


class BoardProfiles(unittest.TestCase):
    def test_shell_table_matches_python(self):
        for board in ('beyond1lte', 'beyond0lte'):
            sh = board_sh(board)
            py = b2l_board.shell_assignments(board)
            for key in ('B2L_MODEL', 'B2L_BOOT_DEV', 'B2L_BOOT_BYTES', 'B2L_SYSTEM_DEV',
                        'B2L_SYSTEM_BYTES', 'B2L_USERDATA_DEV', 'B2L_USERDATA_BYTES'):
                self.assertEqual(sh[key], str(py[key]), f'{board} {key}')
            self.assertEqual(sh['B2L_UEFI_PKG'], b2l_board.platform_package(board))

    def test_ram_profile_matches_device_tree(self):
        for board, profile in b2l_board.BOARDS.items():
            nodes = json.loads((ROOT / 'common/memory' / profile['dt_evidence']).read_text())
            el2_base, el2_size = reg(nodes['/reserved-memory/el2_earlymem'])
            # Usable upper DDR stops where the EL2 early memory begins ...
            self.assertEqual(profile['ram_end'], el2_base, board)
            banks = sorted(reg(n) for k, n in nodes.items() if k.startswith('/memory@'))
            dram_end = max(b + s for b, s in banks)
            # ... which sits at the very top of DRAM.
            self.assertEqual(el2_base + el2_size, dram_end, board)
            # Every address the 8 GiB external map may describe but the board
            # lacks is reserved as absent DDR.
            if dram_end < 0xA00000000:
                self.assertIn((dram_end, 0xA00000000 - dram_end), profile['absent_ddr'], board)

    def test_s10e_identity(self):
        p = b2l_board.profile('beyond0lte')
        self.assertEqual(p['model'], 'SM-G970F')
        self.assertEqual(p['ram_end'], 0x97E000000)
        self.assertEqual(p['panel'], (1080, 2280))
        w, h = p['fb_logical']
        # iOS draws for the iPhone17,2 points (440 x 956) at the board scale, as on the S10.
        self.assertEqual((w, h), (440 * p['display_scale'], 956 * p['display_scale']))
        self.assertEqual(b2l_board.profile('beyond1lte')['fb_logical'], (w, h))
        # The loader accepts at most 2x downscaling and never more than the panel.
        self.assertTrue(1080 / 2 <= w <= 1080 and 2280 / 2 <= h <= 2280)
        # Aspect error on the 1080 x 2280 panel stays small (about 3 %).
        self.assertLess(abs((w / h) / (1080 / 2280) - 1), 0.04)

    def test_board_cli(self):
        out = subprocess.run([sys.executable, str(ROOT / 'scripts/b2l_board.py'), 'shell'],
                             env={**os.environ, 'B2L_BOARD': 'beyond0lte'}, capture_output=True, text=True, check=True)
        self.assertIn('B2L_RAM_ALLOW_END=0x97E000000ULL', out.stdout)
        self.assertIn('B2L_FB_LOGICAL=880x1912', out.stdout)

    def test_ram_header_default_and_kernel_copy(self):
        common = (ROOT / 'common/memory/b2l_ram_ranges.h').read_bytes()
        self.assertEqual(common, (ROOT / 'xnu-overlay/pexpert/pexpert/arm64/b2l_ram_ranges.h').read_bytes())
        self.assertIn(b'#define B2L_RAM_ALLOW_END 0x9FD800000ULL', common)
        self.assertIn(b'{0x88ad00000ULL,B2L_RAM_ALLOW_END-0x88ad00000ULL}', common)


if __name__ == '__main__':
    unittest.main()
