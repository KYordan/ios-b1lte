# Architecture

How iOS 26 reaches the screen of a Galaxy S10, from power-on to SpringBoard. Detailed design notes are linked from each step; the build-by-build history is in [journal/](journal).

## Boot chain

```
Samsung S-Boot ──> beyond2lte UEFI (Project Mu, external repo)
                      └─ XnuLoader.efi (uefi/XnuLoader)
                           ├─ boot menu on the GOP framebuffer      docs/boot-menu.md
                           ├─ Apple device tree built from the DTB  uefi/XnuLoader/DeviceTree.c
                           └─ exits boot services, jumps into XNU   docs/efi-xnu-loader.md
XNU (Apple source + patches/ + xnu-overlay/)
   ├─ board layer: B2LPlatformExpert, RAM banks, GIC, PSCI SMP      docs/smp-status.md
   ├─ arm64e emulation of post-ARMv8.2 instructions                 docs/arm64e-runtime.md
   ├─ drivers: UFS, framebuffer, touch, Wi-Fi, Mali, thermal, RTC
   └─ mounts the HFS+ root on UFS userdata                          docs/userdata-root.md
launchd ──> iOS 26 daemons ──> backboardd ──> SpringBoard
```

The S10 (`beyond1lte`) port reuses the Galaxy S10+ (`beyond2lte`) work, hence the `B2L`/`b2l_` prefix in file and symbol names. `B1L` numbers the S10 builds.

## Why the pieces are where they are

- **UEFI in front of XNU.** S-Boot only loads Android boot images. The external beyond2lte UEFI firmware already brings up UFS, the display and the memory map, so `XnuLoader` only has to translate that state into what XNU expects.
- **An overlay instead of a fork of XNU.** `scripts/prepare-xnu.sh` applies `patches/` and copies `xnu-overlay/` into a pinned Apple checkout. Apple's tree stays a clean, replaceable input; every change of ours is visible in this repository.
- **arm64e emulation in the kernel.** The iOS 26 dyld cache is built for Apple cores (ARMv8.3+). The Exynos cores trap on those instructions; the kernel decodes and emulates them so Apple's binaries run unmodified.
- **A native cache on SYSTEM.** USERDATA holds the HFS+ root volume; the translated dyld cache and the GPU libraries go to the SYSTEM partition so they can be updated block by block (`tools/b1l/system-add-fast.sh`) without rewriting the root volume.
- **Metal on Mesa.** Apple's frameworks expect Metal. `userspace/metal` implements the Metal objects on top of Mesa's panfrost/Bifrost compiler and the `HakiosMali` kernel driver; OpenGL ES remains as a fallback. Overview: [docs/gpu/overview.md](gpu/overview.md).

## Data that never enters the repository

Apple binaries (IPSW, dyld cache, KDK), Samsung/Broadcom firmware, NVRAM, calibration files, Wi-Fi settings and built images. Scripts read them from `$S10_PORT` (see [.env.example](../.env.example)).
