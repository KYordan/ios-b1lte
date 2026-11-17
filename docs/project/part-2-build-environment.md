# Project documentation — iOS 26 port to the Galaxy S10 (beyond1lte)

## Part 2 — Build environment, build process and security

---

### 7. Hardware and build environment

All the work is done on a **ThinkPad T470s (Intel i7-7500U, hackintosh, macOS 26 Tahoe, 12 GB RAM)**.

| Component | Detail |
|-----------|--------|
| Computer | ThinkPad T470s, hackintosh macOS 26 |
| RAM | 12 GB (16 GB recommended) |
| CPU | 2 cores / 4 threads |
| Disk space | ~150 GB free for the iOS image, external USB-C SSD possible |
| Kernel / iOS | Xcode 26.5 + KDK 26.5 (`25F71`), SDK iPhoneOS 26.5 |
| UEFI / tools | micromamba env `~/Developer/tools/mamba/envs/s10`: clang 23, lld, llvm-tools, cmake, ninja, meson, dtc, bash 5, Python 3.12, cryptography |
| adb | `~/Developer/android/platform-tools/adb` |
| Phone | Galaxy S10 SM-G973F, TWRP 3.7.1, root ADB, serial `<SERIAL>` |

**Constraints:**
- Homebrew is **not** available on Intel macOS 26.
- The agent has **no sudo**. Steps requiring root (edits to the iOS root that preserve owners) are run by the owner with `sudo`.
- UEFI and Mesa are built in a **Linux container** (OrbStack, Colima or Docker) on the hackintosh.
- Packaging `boot.img` and the Python scripts are native on macOS.
- Flashing and logs use `adb` + TWRP from macOS.
- Windows is only a fallback (Odin to restore the stock firmware or reinstall TWRP).

---

### 8. Build process — step by step

The official workflow is described in `engineer.md` section 5 and `beyond2lte-ios/README.md`.

#### Step 0 — Prerequisites (one time only)

1. Fetch the pinned XNU sources:
   ```sh
   scripts/fetch-xnu.sh
   ```
2. Apply the overlay and the B2L patches:
   ```sh
   scripts/prepare-xnu.sh
   ```
3. Prepare the external dependencies (KDK, SDK, libraries in `deps/` and `deps-src/`).

#### Step 1 — Build the XNU kernel

```sh
KDKROOT=/Library/Developer/KDKs/KDK_26.5_25F71.kdk \
B2L_DEPSROOT=~/Developer/s10-port/deps \
BUILD_JOBS=4 \
BUILD_MEMORY_BYTES=12884901888 \
BUILD_DIR=$PWD/out/xnu-b2l-ref \
bash scripts/build-kernel.sh CWARNFLAGS=-w CXXWARNFLAGS=-w
```

This script:
- Checks that the XNU repository is at revision `ac9718fb1af618d5ce8678d0dc6e8a58f252216f`.
- Checks that the B2L overlay is applied (`pexpert/pexpert/arm64/B2L.h` present).
- Integrates libpthread, BearSSL and the HFS driver into XNU.
- Produces `out/xnu-b2l-ref/obj/DEVELOPMENT_ARM64_B2L/mach.development.b2l`.

#### Step 2 — Build the UEFI firmware

In a disposable Mu working tree prepared by `scripts/make-board-platform.py`:

```sh
B2L_BOARD=beyond1lte B2L_ENABLE_TOUCH=1 \
[B2L_FB_LOGICAL=WxH B2L_DISPLAY_SCALE=N] \
scripts/build-boot-firmware.sh
```

Produces:
- `Build/beyond1ltePkg/RELEASE_CLANGPDB/FV/SILICIUM_UEFI.fd` (UEFI firmware, 4 MiB)
- `Build/B2LXnu/RELEASE_CLANGPDB/AARCH64/XnuLoader.efi` (EFI loader)

For the S10+: `bash yap.sh --clean` / `--tar` / `--acpi` from `beyond2lte-uefi/`.

#### Step 3 — Package the boot.img

```sh
python3 scripts/package-boot.py \
  --mu $W \
  --firmware $W/Build/beyond1ltePkg/RELEASE_CLANGPDB/FV/SILICIUM_UEFI.fd \
  --kernel $K \
  --loader $W/Build/B2LXnu/RELEASE_CLANGPDB/AARCH64/XnuLoader.efi \
  --output-dir out/b1l-metal-NNNN \
  --max-image-bytes 57671680 \
  --boot-partition-bytes 57671680 \
  --clang $E/clang --objcopy $E/llvm-objcopy
```

Verification: `python3 scripts/package-boot.py --verify /path/to/boot.img`

The `boot.img` contains: Android v1 header, UEFI firmware (4 MiB), compressed XNU kernel, embedded EFI loader, `B2LXNU02` trailer with 256-byte seed, DTB at the end of the kernel field.

#### Step 4 — Build the iOS root (first time only)

```sh
sudo bash b1l-tools/build-ios-root.sh
```

Takes the iOS 26 IPSW (`23G90`, iPhone 16 Pro Max), extracts `OS`, `SystemOS` and `AppOS` according to `BuildManifest.plist`, and copies them into a **case-sensitive Journaled HFS+ image** (32 GiB), preserving owners, links and hard links.

Then: `b1l-tools/build-system-image.sh` (builds the SYSTEM image containing the native cache `b2l-cache.bin` of about 5.52 GiB and a 16 MiB journal queue).

#### Step 5 — Edit the root (on a copy)

1. Pull the live root:
   ```sh
   b1l-tools/pull-chunked.sh ios26-userdata.hfs.img userdata /dev/block/sda34 499641200640
   ```
2. Clone it as a rollback point: `cp -c`
3. Edit the clone with `sudo`:
   ```sh
   sudo bash b1l-tools/set-root-prefs.sh <profile>
   sudo bash b1l-tools/install-translated-cache.sh
   ```
4. Rewrite only the modified chunks:
   ```sh
   b1l-tools/system-add-fast.sh <set>
   ```

---

### 9. Automated full chain: `b1l-build.sh`

```sh
bash b1l-build.sh 0139 b2l-metal-v88 --mesa
```

Executed stages (each one writes to `progress/NNNN.status`):
1. **mesa** — `scripts/build-mesa.sh` (only with `--mesa`)
2. **libs** — `scripts/build-metal-scion.sh` in `build-metal/SET`
3. **kernel** — `scripts/build-kernel.sh` (graphics headers copied into XNU beforehand)
4. **package** — `scripts/package-boot.py` → `out/b1l-metal-NNNN/boot.img`
5. **system** — `system-add-fast.sh SET` (SYSTEM: only the modified blocks, re-read on every run)
6. **boot** — `flash-boot.sh` (BOOT, with guards and read-back)

The file `progress/$N.status` contains one line per stage: `name|state|start|end|detail` (state: pending, running, done, failed). `progress/current` points to the most recent status.

---

### 10. Storage policy and security

There is **no RAM boot** on this phone. S-Boot loads the kernel field from the **BOOT** partition, and iOS needs its root in **USERDATA**.

#### Partitions that may be written (explicit owner approval for each installation)

| Partition | Block (S10) | Size | Content | Written by |
|-----------|------------|--------|---------|-----------|
| **BOOT** | `sda14` | 57,671,680 bytes (55 MiB) | `boot.img` (BootShim + UEFI FD + DTB + XNU trailer) | host via TWRP `dd`, then readback |
| **USERDATA** | `sda34` | ~465 GiB | iOS HFS+ root (32 GiB image) | host via `flash-chunked.sh` |
| **SYSTEM** | `sda25` | 7,340,032,000 bytes | Journaled HFS+ containing `b2l-cache.bin` + 16 MiB journal queue | host via `flash-chunked.sh` |

#### Partitions to NEVER write

`bota*`, `efs`, `sec_efs`, `cpefs`, `radio`, `keystorage`, `keydata`, `keyrefuge`, `param`, `up_param`, `uh`, `uhcfg`, `steady`, `persistent`, `misc`, `recovery`, `dtb`, `dtbo`, `vbmeta`, the GPT, or anything not in the table above.

#### Mandatory guards before each write to the device

1. The device is in TWRP (`adb get-state` = `recovery`) with root (`id -u` = 0).
2. `ro.boot.em.model` = `SM-G973F`. `ro.product.device` is **not** reliable (custom ROMs rewrite it).
3. `/dev/block/by-name/<name>` resolves to the expected block, and `blockdev --getsize64` matches exactly.
4. The target is not mounted. TWRP mounts USERDATA at `/data` and `/sdcard`: unmount first.
5. The image hash is verified on the host and again after transfer.
6. After writing: flush caches, read back, compare SHA-256. Record everything in the build log.
7. Large images use `flash-chunked.sh` (verified 512 MiB chunks, resumable). Never stream a large image via `adb exec-in` (it stalls after ~5 GB).

#### The live root is the source of truth

The native cache rewrites translated pages **inside the iOS files on the phone**. Never rewrite the original image over a root in use. Procedure:
1. Read the root back with `pull-chunked.sh` (pass the original image as the base to skip unchanged chunks).
2. Clone (`cp -c`) as a rollback point.
3. Edit the clone. Use the `sudo` scripts for edits that preserve owners.
4. Rewrite only the modified chunks.

#### Existing backups

`~/Developer/s10-port/device-s10/backup-20261103/`: efs, sec_efs, cpefs, boot, recovery, dtb, dtbo, vbmeta, uh, uhcfg, param, up_param, keystorage, steady, persistent, misc — all SHA-256 verified. The Android SYSTEM was **not** backed up (owner's choice), and Android userdata is gone. Returning to Android requires Odin with the stock `G973F` firmware.

---

Part 3 covers the detailed build logs (current status, GPU, S7 reference, S10 facts).
