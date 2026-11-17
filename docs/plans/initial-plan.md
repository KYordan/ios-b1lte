# iOS 26 port to Galaxy S10 (SM-G973F, beyond1lte, Exynos 9820)

State of the sources as of 2026-11-01. Nothing has been built or tested yet.
`[VERIFY]` = to be proven on your phone or in the DTB. `[HYPOTHESIS]` = my deduction, not a documented fact.

## 1. Strategy

**We do not start from zero: we adapt `beyond2lte-ios` from the S10+ to the S10.** It is the only repository that combines the same SoC and iOS 26.

| Repository | Role | What we take from it |
| --- | --- | --- |
| `orhunnx/beyond2lte-ios` | **Base** (XNU 12377, iOS 26, Exynos 9820) | Everything: patches, EFI loader, packaging, arm64e emulation, CPU, UFS, HFS, touch |
| `orhunnx/beyond2lte-uefi` (branch `master`) | Firmware reference | UEFI overlay of the S10+ |
| `Project-Silicium/Mu-Silicium` | S10 firmware | `Platforms/Samsung/beyond1ltePkg` already exists |
| `wardevsa/ios-a5xelte` (`herolte`) | **Mali GPU reference** | Kernel job service, GPU scaling, optimization method. iOS 16 and XNU 8796: to adapt, not to copy |
| `superboyyy/hakios-arm64e-dada` | **Metal and audio reference** | Chain `metal2vulkan` → Vulkan → kernel, Metal plugin |
| `oosexgang/hakios-xnu-kernel` | Wi-Fi reference | Integration of Wi-Fi into iOS. XNU 8796, Qualcomm only |
| `UniXNU/UniXNU` | Architecture reference | Split of drivers. No Exynos 9820 in the catalog |

**Real starting point (S10+, build 0100):** boot, 8 cores, ~7 GB of RAM, UFS, HFS root, SpringBoard, touch and Settings work. Lock screen at ~90 s. Icons drawn incorrectly, wrong alert background, SpringBoard assertion, no app installed. No GPU, Wi-Fi, Bluetooth, audio, USB under iOS, or sleep.

## 2. What changes the picture

1. **All of iOS 26 is arm64e, and the Exynos 9820 has no hardware PAC.** The missing instructions go through kernel traps and a native code cache (~5.5 GiB). The first boot will be very slow, and this cost will remain the limit on smoothness, even with the GPU.
2. **iOS 26 is not iOS 16 on the GPU side.** The OpenGL ES/EAGL bridge of the S7 works on iOS 16. On iOS 26, dada goes through Metal → Vulkan. For Mali, the probable route is `metal2vulkan` → PanVK (Mesa) → Mali kernel service `[HYPOTHESIS]`. Nobody has published this on Mali-G76: it is research, not a port. The Metal part is not guaranteed.
3. **None of the repositories has usable Wi-Fi on this chip.** The S10 chip is a Broadcom PCIe device, probably BCM4375 `[VERIFY]`, whereas the existing drivers target Qualcomm. We will need an Exynos PCIe controller, a chip driver and a connection to the iOS network stack. Since Samsung's `bcmdhd` code is under GPL, we use it to understand the protocol, as dada did with Linux, without copying it.
4. **App installations are blocked on HFS** (error 166, iOS requires APFS). We will need an HFS-compatible staging path to populate the home screen, or copy the apps directly into the image as on the S7.
5. **BOOT is 55 MiB on your S10** (`sda14`), against a default packaging limit of 64 MiB. We must check that firmware + kernel fit there.
6. **`ro.product.device` is `r13s` under ArtisanROM.** A flash guard based on this property would be wrong. It must be replaced: `ro.boot.em.model` under Android, the TWRP identity in recovery.

## 3. Phases

Rule for the whole project: **one single change per test boot, each build has its log `docs/journal/build-XXXX.md`.** "Verified" means observed on your S10.

### Phase 0: Security and facts (blocking)
- Read in **Download mode**: `OEM LOCK`, `KG STATE`, `RMM STATE`. The Android properties may be falsified by the ROM.
- Back up from TWRP: `efs`, `sec_efs`, `cpefs`, `boot`, `recovery`, `dtb`, `dtbo`, `vbmeta`, checking the hashes. This check has never been done (Gate 0 of the ColorOS project is still open).
- Test a real return to stock via Odin.
- Copy elsewhere what matters: **`userdata` will be erased**.

**Criterion:** restoration tested and backups kept off the working PC.

### Phase 1: Build chain
- Reproduce the S10+ **without modification**: kernel, EFI, `boot.img`.
- The kernel requires macOS, Xcode and a KDK Apple 26.5: it will be compiled on the T470s (see section 5). **First test to do, because it decides the whole chain:** compile XNU 12377 unmodified, then the S10+ target without change.
- UEFI: the previous session failed here. The `MdePkg/Library/ArmLib` link of Mu-Silicium was broken under WSL. Probable cause: incomplete clone or submodules `[HYPOTHESIS]`. Remedy: fresh recursive clone in a Linux container on the T470s, as dada does.

**Criterion:** `scripts/test-loader.sh` and `package-boot.py --verify` pass.

### Phase 2: `beyond1lte` target
- Compare the G973F and G975F DTBs, then `common/memory/*`, RAM maps, panel, touch, GPIO, GPT.
- Create the `B1L` board while keeping the S10+ intact, with a console-only kernel, **without iOS**.
- Build with the real `--boot-partition-bytes`, corrected flash guard, hash re-read after writing.

**Criterion:** XNU text on screen, native UFS read, 8 cores, log recoverable from TWRP. Android stays intact apart from BOOT.

### Phase 3: iOS 26 up to rendering
- Build the HFS root (section 4), write it to USERDATA.
- Reach the lock screen, then repeat the S10+ fixes until we reach the equivalent of 0100.
- Install fix 0101 (compiled, never tested).

**Criterion:** lock screen, touch, Settings open.

### Phase 4: Priority 1, graphics patch
- Find the origin of the incorrect icons and alerts, starting from the CoreImage and CoreGraphics logs already instrumented.
- Resolve the SpringBoard assertion.
- Populate the apps: first copy into the image, HFS staging afterwards.
- Reduce the logical resolution if measurement justifies it (72% fewer pixels than on the S7).
- Measure before and after each optimization: boot time, FPS, idle CPU. Remove daemons that loop, such as `BTAudioHALPlugin` on the S7.

**Criterion:** home screen correct, apps that open, 30 min without panic.

### Phase 5: Priority 2, Wi-Fi
- Identify the chip and the bus in the G973F DTB and in the Samsung sources.
- Extract the firmware and NVRAM from **your** phone (never in the repository).
- Start PCIe, load the firmware, make the chip respond, scan, associate, get DHCP.
- First study how HakiOS connects Wi-Fi to iOS on `redwood`.

**Criterion:** `ping`, then a Safari page. Expected: long, milestone by milestone, with no guaranteed date.

### Phase 6: Priority 3, GPU acceleration
Steps, each testable on its own:
1. Power the Mali-G76 and read its ID.
2. Run a kernel test job, like `herogpu=3` on the S7, adapted from Midgard to Bifrost.
3. Scale via the GPU to lighten the CPU (possible gain without Metal).
4. Compile Mesa for iOS.
5. `metal2vulkan` + PanVK, Metal plugin, CoreAnimation composition.

**Criterion:** steps 1 to 3 considered realistic, 4 and 5 considered open research. If step 5 blocks, step 3 remains a real improvement.

### Phase 7: Bluetooth option
Only after phases 4 to 6. Same chip family, same bus to study `[VERIFY]`.

## 4. Recovering iOS 26 (guide)

**Target image: iPhone 16 Pro Max IPSW (`iPhone17,2`), iOS 26.6.2, build `23G90`.** The S10+ repository cites this build and announces itself as `iPhone17,2`, but does not explicitly name the source IPSW. To be confirmed with the author through an issue before downloading `[VERIFY]`. **Keep exactly this version**: the patches are calibrated on it, and another version moves everything. iOS 26 does not exist as a plain arm64 build.

1. Download the IPSW from the official Apple links (index: ipsw.me).
2. Extract `OS`, `SystemOS` and `AppOS` according to `BuildManifest.plist`.
3. Mount the images read-only on the T470s (macOS), owners enabled. Some of the images may be in `.aea` format, to be checked on the real file `[VERIFY]`.
4. Copy into a **case-sensitive Journaled HFS+ image** (32 GiB for the S10+, ~20 GB of data), keeping owners, links and hard links.
5. Place SystemOS under `private/preboot/Cryptexes/OS`, AppOS under `private/preboot/Cryptexes/App`.
6. Run `scripts/prepare-ios-root.py` with `--dry-run`, then for real, then `--verify`.
7. Unmount cleanly, run `fsck`, note the hash.
8. Write the complete image from TWRP, zeros included. A badly written sparse image corrupts the volume.

Details: `docs/ios-userspace-root.md` of the S10+ repository. The complete guide, command by command, will be written in `docs/INSTALL.md` based on what has actually worked.

## 5. Hardware and prerequisites

- **Build Mac: ThinkPad T470s (i7, Intel), hackintosh under macOS 26 Tahoe.** It serves for:
  - the kernel: Xcode 26, iPhoneOS SDK, KDK 26.5 installed locally;
  - the iOS image: native APFS mounting, `prepare-ios-root.py` as is;
  - later, the arm64e and Metal code on the iOS side, and the analysis of binaries with the Xcode tools.
- **Gap with the reference:** the S10+ is compiled with Xcode 27.1 and the macOS 27 SDK on Apple Silicon. Here it is Xcode 26 on Intel, so the scripts or the `[VERIFY]` warnings may need adapting. If it does not compile, GitHub Actions serves as a fallback, with a public repository and nothing Apple-related in it.
- **T470s limits:** 2 cores / 4 threads, so the first full build will be long and incremental builds will be enough afterwards. For the iOS image, ~150 GB of free space is needed, or a USB-C external SSD. RAM must be checked (16 GB or more recommended).
- **The whole project is done on the T470s, except the Odin fallback.**
  - UEFI (Mu-Silicium/EDK2) and Mesa: in a Linux container, with OrbStack, Colima or Docker. The same container avoids the broken-link problem seen under WSL. VT-x must be enabled on the hackintosh `[VERIFY]`.
  - `boot.img` packaging, Python scripts: native on macOS.
  - Flash and logs: `adb` + TWRP from macOS (`android-platform-tools`), with `dd` for BOOT and USERDATA, as on the S10+.
- **Windows is kept as a fallback only:** Odin to restore stock firmware or reinstall TWRP. Heimdall exists on macOS but is not reliable on recent Samsung devices `[VERIFY]`, so we do not rely on it.
- **Remote control:** enable "Remote Login" (SSH) on the T470s, so that Claude can launch the builds from this PC.
- TWRP, Odin, platform-tools: already present in `iOS S10\s10 rom`.
- Photographing the screen: no serial console, so the first debugging goes through photos and logs read from TWRP.

## 6. Risks

- **Conflict with ColorOS:** the same phone serves both projects and iOS erases `userdata`. We must alternate, with restoration between the two.
- **Brick:** we only write BOOT and USERDATA, and `SYSTEM` for the cache. Never `bota*`, `efs`, `sec_efs`, `radio`, `keystorage`.
- **No security:** no encryption, no SEP, no real signature. No personal data on this install.
- **Heat and battery:** no sleep, and no thermal limit validated. Do not leave the device running unattended.
- **Legal:** Apple's license ties iOS to Apple hardware, and the answer depends on the country. Nothing from Apple, no firmware, IMEI or MAC in the repository. Apple services (real activation, iCloud, App Store) are out of scope.
- **Expected result:** a research demonstration, not a daily phone. No calls, 4G, camera or audio in this plan.

## 7. Verification

| Level | Proof |
| --- | --- |
| Host | Repository tests, `package-boot.py --verify`, `fsck`, image hash |
| Flash | BOOT re-read byte by byte, G973F model confirmed |
| Phone | Photo of the screen + kernel/`tracev3` log recovered from TWRP |
| Regression | The S10+ board keeps compiling and its tests pass |
