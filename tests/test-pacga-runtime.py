#!/usr/bin/env python3
"""Run the real ARM64e wrapper (b2l_arm64e_compat.c, host-mocked VM/copyin) with and without B2L_ARM64E_DIAG."""
from pathlib import Path
import argparse,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('xnu',type=Path,nargs='?',help='unused; kept for the test-arm64e.sh interface')
p.add_argument('--tsan',action='store_true')
a=p.parse_args();s=r/'xnu-overlay/osfmk/arm64'
with tempfile.TemporaryDirectory(prefix='b2l-pacga-runtime-') as td:
 d=Path(td)
 cc=subprocess.check_output(['xcrun','--find','clang'],text=True).strip()
 sdk=subprocess.check_output(['xcrun','--sdk','macosx','--show-sdk-path'],text=True).strip()
 flags=['-isysroot',sdk,'-O2','-g','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-I'+str(s)]
 flags+=['-fsanitize=thread'] if a.tsan else ['-fsanitize=address,undefined','-fno-omit-frame-pointer']
 for diag in (0,1):
  subprocess.run([cc,*flags,f'-DB2L_ARM64E_DIAG={diag}',str(r/'tests/test_arm64e_runtime.c'),str(s/'b2l_compat.c'),'-o',str(d/f'test{diag}')],check=True)
  subprocess.run([str(d/f'test{diag}')],check=True)
