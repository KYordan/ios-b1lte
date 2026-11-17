# Plan: translate corecrypto's PAC without breaking its self-test (for later)

Date: 2026-11-08. **Do not start now**: the owner will start it later.

## Owner's decision

- **Method 1:** translate the PAC instructions of corecrypto's measured text, then **recompute the integrity fingerprint** (HMAC) it verifies and replace it in the file. It verifies itself and sees nothing. No security is disabled.
- **First extract from the phone (USERDATA) the current shared cache**, to start from an up-to-date copy, not from `static-work/out`.

## Why

- B1L-0098: backboardd spends ~88 s out of 90 in the kernel, 20,000 to 80,000 PAC interrupts per second, ≈ 11 µs each.
- The translated cache no longer has PAC, except for the **5,187 words of corecrypto's measured text** (`__TEXT,__text` of `libcorecrypto.dylib`, kept original by `b2l_cache_exclusion.h` and `nc-static`).
- The native cache now only learns these instructions (`measured` ≈ 43,000 per boot).
- **To confirm first with the profile of B1L-0101** (`B2L PACPROF`): if a single caller hammers corecrypto in a loop, fixing it may be enough.

## Steps

### 0. Confirm the target (log of B1L-0101)

- Match the PCs of `B2L PACPROF` with the cache: the offset is found by matching the recorded instructions with those in the file.
- Check that the most frequent spots are indeed within corecrypto's measured range (`dyld_shared_cache_arm64e.31`, `0x1516c00 + 0x874d0` according to `B2L NCACHE EXAMPLE`), and note the callers (LR).

### 1. Extract the shared cache from USERDATA (read only)

- Phone in TWRP, usual guards: TWRP root, `ro.boot.em.model` = SM-G973F, by-name path, size, partition not mounted.
- Read `sda34` in chunks with hashes, with `b1l-tools/pull-chunked.sh` or equivalent, to `out/live/` on the Mac, **outside git**. USERDATA contains personal data.
- Mount the image **read-only** on the Mac (`hdiutil attach -readonly`) and copy:
  - `private/preboot/Cryptexes/OS/System/Library/Caches/com.apple.dyld/dyld_shared_cache_arm64e*`;
  - the same name under `/System/Library/Caches/com.apple.dyld/`, if it exists. The kernel's native cache reads this path; check which one dyld actually uses.
- Compare with `static-work/out` (`cmp` per file) and note the differences: translations learned by the phone, intended NOPs, etc.
- Keep this copy intact as a **rollback**.

### 2. Understand the integrity self-test

- In `libcorecrypto.dylib` (extracted cache), find the FIPS integrity function (`fipspost_post_integrity` in Apple's published corecrypto source code), then:
  - **the measured zones** (the text; perhaps also constants);
  - **the HMAC key** (constant);
  - **the location of the expected fingerprint** (a dedicated section, for example `__fips_hmacs`);
  - **what happens on failure** (abort, refusal of operations, or nothing).
- Write a verifier on the Mac that recomputes the fingerprint **on the original cache**: it must equal the stored value. This is the proof that we understood correctly before modifying anything.

### 3. Translate and recompute

- `nc-static` without the corecrypto exclusion: `RETAA`/`RETAB` → `RET`, `BRAA`/`BLRAA` → `BR`/`BLR`, `LDAPR` → `LDAR`, etc. Same rule as `b2l_nc_native`. The `LDRAA` that cannot be translated remain.
- Recompute the HMAC over the translated text and write it in place of the expected fingerprint.
- Verify with the step 2 verifier (fingerprint equal) and with `nc-static compare` (only the expected words changed).

### 4. The kernel must not undo the translation

- `b2l_native_cache.c`, when applied before userspace, **restores** corecrypto's measured text to the original (`integrity_restored`, `b2l_xfilter`). It must be disabled, with a boot-arg or code, otherwise it will cancel the translation at the next boot.
- The kernel panics if corecrypto's metadata is "dangerous" (`integrity_open`): check that the modified cache passes this check.

### 5. Install on the phone (USERDATA write, with the owner's agreement at the time)

- Write only the blocks of `sda34` that change (the `.31` file, same size, same inode), with all the guards: TWRP root, model, by-name, size, not mounted, hash, re-read.
- Rollback: rewrite the same blocks from the copy extracted at step 1.

### 6. Measure

- `B2L EMU CLASS` and `B2L PACPROF`: number of interrupts per second before and after.
- backboardd's `caught burning CPU`: the kernel share must collapse. Perceived smoothness of the interface.

## Risks

- If the self-test fails, all processes that use cryptography may refuse to start, including boot services. Hence step 2 (proof on the original) and the rollback copy.
- The cache is shared by the whole system: a badly translated instruction crashes any process. Hence `compare` (zero unexpected changes).
