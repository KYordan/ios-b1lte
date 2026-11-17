#!/bin/bash
# SPDX-License-Identifier: BSD-2-Clause
# GPU G6: libb2l-gl.dylib = HakiOS's EAGL bridge (userland/src/eagl.c, gl_osmesa.c, MIT) + our window-server
# hook (userspace/b2l_accel_gles.c), linked to Mesa's libOSMesa (scripts/build-mesa.sh). Output in
# $O/$V: both dylibs with install names /usr/local/lib/$V/..., ad hoc signed, arm64e.
#   V=b2l-gl-v2 bash scripts/build-gl.sh
set -euo pipefail
V=${V:-b2l-gl-v2}
R=$(cd "$(dirname "$0")/.." && pwd)
H=${HAKIOS:-$HOME/Developer/s10-port/ref-scion/hackios-xnu-kernel}/userland
M=${MESA_OUT:-$HOME/Developer/s10-port/build-mesa/out}
O=${GL_BUILD:-$HOME/Developer/s10-port/build-gl}
SDK=$(xcrun --sdk iphoneos --show-sdk-path)
mkdir -p "$O/src" "$O/$V"
cp "$H/src/eagl.c" "$H/src/gl_osmesa.c" "$H/src/hakios.gl.h" "$H/src/eagl_slots.txt" "$O/src/"
# eagl.c's lines also go to the kernel log (kern.hakios.say, bsd/dev/b2l_mali_core.c)
python3 - "$O/src/eagl.c" <<'PY'
import sys
p=sys.argv[1]; s=open(p).read()
a='\twrite(2, line, (size_t)n);\n'
i=s.index('\nsay(const char *format, ...)\n{'); j=s.index(a,i)
s=s[:j+len(a)]+'\t(void)sysctlbyname("kern.hakios.say", NULL, NULL, line, (size_t)n);\n'+s[j+len(a):]
if '#include <sys/sysctl.h>' not in s: s='#include <sys/sysctl.h>\n'+s
open(p,'w').write(s)
PY
# B1L-0034: trace hakios_gl_create around OSMesa
python3 - "$O/src/gl_osmesa.c" <<'PY'
import sys
p=sys.argv[1]; s=open(p).read()
a='\tif (gl != NULL && (gl->context = OSMesaCreateContextAttribs(attributes, NULL)) == NULL) {'
assert s.count(a)==1
s=s.replace(a,'\tglsay("b2l-gl: OSMesaCreateContextAttribs ...");\n'+a,1)
s=s.replace('#include "hakios.gl.h"','#include "hakios.gl.h"\n#include <sys/sysctl.h>\nstatic void glsay(const char *t) { (void)sysctlbyname("kern.hakios.say", NULL, NULL, (void *)t, strlen(t)); }',1)
assert s.count('\treturn gl;\n')>=1
s=s.replace('\treturn gl;\n','\tglsay(gl && gl->context ? "b2l-gl: OSMesa context made" : "b2l-gl: OSMesa context FAILED");\n\treturn gl;\n',1)
open(p,'w').write(s)
PY
python3 "$H/mkeagl.py" "$O/src/eagl_slots.txt" "$M/gl.xml" "$M/exported-gl.txt" "$O/src/eagl.c" > "$O/src/eagl_gen.h"
cp "$M/libOSMesa.8.dylib" "$O/$V/libOSMesa.8.dylib"
/usr/bin/install_name_tool -id /usr/local/lib/$V/libOSMesa.8.dylib "$O/$V/libOSMesa.8.dylib" 2>/dev/null || true
/usr/bin/codesign --force --sign - "$O/$V/libOSMesa.8.dylib"
xcrun --sdk iphoneos clang -target arm64e-apple-ios26.0 -isysroot "$SDK" -O2 -dynamiclib \
	-install_name /usr/local/lib/$V/libb2l-gl.dylib -I"$O/src" \
	"$O/$V/libOSMesa.8.dylib" \
	-framework CoreFoundation -framework IOKit -framework IOSurface -framework OpenGLES -lobjc \
	-o "$O/$V/libb2l-gl.dylib" \
	"$O/src/eagl.c" "$O/src/gl_osmesa.c" "$R/userspace/b2l_accel_gles.c"
/usr/bin/codesign --force --sign - --identifier dev.b2l.gl "$O/$V/libb2l-gl.dylib"
for f in "$O/$V"/*.dylib; do /usr/bin/codesign -v "$f"; done
# B1L-0039: eagl.c's gl* calls must reach Mesa, never Apple's OpenGLES (whose glGetString answered NULL)
if xcrun dyld_info -imports "$O/$V/libb2l-gl.dylib" | grep -E '_gl[A-Z][A-Za-z0-9]*  \(from OpenGLES\)' >/dev/null; then
	echo "build-gl: gl* still imported from OpenGLES" >&2; exit 1
fi
shasum -a 256 "$O/$V"/*.dylib
