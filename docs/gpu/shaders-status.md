# Shader status (2026-11-12)

State at build **B1L-0191** (back to the state of 0187, requested by the owner). Chain: Apple's AIR → translator
`src/compiler/air` (patches in `userspace/mesa/air-b2l/apply.py`) → NIR → Bifrost compiler (panfrost) → Mali-G76.

## In short

| Library | Who uses it | Translated | Refused |
|---|---|---|---|
| QuartzCore (`default.metallib`, 189 functions) | backboardd: the whole interface, Liquid Glass | **173** | **16** (imageblocks) |
| RenderBox (`default.metallib`, 82 functions) | SwiftUI in SpringBoard, iconservicesagent, chronod: Clock/Calendar icons, Control Center glyphs, widgets | 17 on the Mac (see below) | on the phone: 2 kinds of refusal |
| RenderBox `archive.metallib` | same | — | not LLVM bitcode: Apple's archive of precompiled pipelines, not readable |

## 1. Shaders refused on the phone (log 0187)

This is the list that matters: these are the pipelines the apps really request.

| Refusal (driver message) | Process | Times | Affected shaders |
|---|---|---|---|
| `a phi of pointers whose operand comes in over a loop edge` | SpringBoard 5, iconservicesagent 2 | 7 | `uber_vertex`, `path_*_vertex`, gradients: binary search in a path segment buffer |
| `a phi of a struct, which NIR has no register for` | SpringBoard 4, iconservicesagent 3 | 7 | `filter_blur_fragment`, `resolve_sample_fragment`, `uber_fragment` (`phi { <4 x half>, i8 }`) |
| `a return in the middle` (seen in 0189 only, once the two refusals above were lifted) | SpringBoard 5, iconservicesagent 5 | 10 | `fa16dhmha4` (RenderBox specialization) |
| `a texture over a buffer was asked for` (not a shader: a texture set on a buffer) | SpringBoard 1, iconservicesagent 1 | 2 | `newTextureWithDescriptor:offset:bytesPerRow:` |

backboardd (QuartzCore): **no refusal** on the phone. The 16 kernels with imageblocks are never requested.

## 2. What was already tried (0188 to 0190, removed in 0191)

- **0188**: phi of structs (one register per field) and phi of pointers within one buffer (the offset in a
  register). `uber_vertex` translates. Result: SpringBoard crashes in Bifrost (`bi_emit_jump`, null jump target).
- **0189**: guard against jumps that Bifrost cannot emit. Result: starts, only `fa16dhmha4` stays
  refused, but **the interface flickers while moving**.
- **0190**: `nir_lower_returns` (the middle `return` passes); the pointer `select` of QuartzCore put back as before.
  Result: **no improvement, apps slow**.
- **Learned:** with these fixes, **all** RenderBox shaders compile and draw, but nothing more appears
  on screen. The blocking of icons, glyphs and widgets is therefore **after drawing**: the path of the surface drawn by
  SpringBoard/iconservicesagent to backboardd. The fixes are kept in
  `userspace/mesa/air-b2l/renderbox-0188-0190.py.txt` (to be pasted back at the end of `apply.py` to resume).

## 3. Shaders that do not translate off the phone (`air2nir` report)

Command (on the Mac, a few seconds, without the phone):

```bash
~/Developer/s10-port/build-mesa/build-host/src/compiler/air/air2nir --report ~/Developer/s10-port/beyond2lte-ios/out/rb-check/default.metallib
```

Warning: the Mac tool is dated 12/11 at 01:32, so it may not be exactly at the 0191 state (it translates
`uber_vertex` with a note about the pointer `select`). Rebuild it before relying on it.

**QuartzCore (`out/qc-check/QuartzCore-air64.metallib`): 173 translated, 16 refused**, all for `input air.imageblock`
(tile memory of Apple GPUs, with no direct equivalent on Mali). Never requested by backboardd so far.

**RenderBox (`out/rb-check/default.metallib`): 17 translated, 65 refused.** The refusals off the phone mostly come from the tool, which does not have the specialization values that the app supplies when it creates the pipeline:

| Cause | Functions | Real problem on the phone? |
|---|---|---|
| `a function constant of 4 components of 32 bits` | 23 (`stroke_*`, `filter_blur_fragment`, `spill_fragment`, `mask_*`, `uber_resolve_fragment`…) | Not seen on the phone: offline tool limit |
| `stage: air.visible, a library function (not yet)` | 22 (`distanceGradient_v1`, `ovalizeGradient_v1`, `displacementMap_v1`…) | **Yes, to do**: "visible" functions called by `uber_fragment` through a function table (`air.get_function_pointer_visible_function_table`), not supported |
| `an air.render_target input without a target` | 19 (`filter_custom_fragment`, `resolve_*_fragment`, `mask_color/gradient/image_fragment`, `alpha_effect_fragment`, `custom_effect_fragment`…) | Not seen on the phone: the target comes from the pipeline |
| `input air.function_constant` | 1 (`uber_fragment`) | Offline tool limit |

## 4. What is still missing, by order of usefulness

1. **The path of the surface to backboardd** (not a shader): this is what stops icons, glyphs and widgets
   from appearing even when everything compiles. Prove where the image gets lost before touching the shaders.
2. Resume the 0188–0190 fixes **one by one**, looking for the cause of the flickering (0189) and the slowness (0190).
3. Visible functions (`air.visible`) and function tables, for `uber_fragment`.
4. Texture set on a buffer (`newTextureWithDescriptor:offset:bytesPerRow:`).
5. chronod gets a GPU but draws nothing (widgets).
6. QuartzCore imageblocks (16 kernels): only if a log shows they are requested.
7. Precompiled pipeline archive (`archive.metallib`): refuse it cleanly, without a crash.

Sources: `docs/journal/build-b1l-0185.md` to `0191.md`, `docs/metal-progress.md`, logs `out/logs/0186-boot.txt` to `0189-boot.txt`.
