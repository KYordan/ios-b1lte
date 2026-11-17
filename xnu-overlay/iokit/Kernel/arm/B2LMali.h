/* SPDX-License-Identifier: BSD-2-Clause
 * Mali-G76 (Bifrost, Job Manager) bring-up, stage G2a: reset and power only.
 * Register offsets follow the Arm kbase/Panfrost register map (GPU_CONTROL
 * 0x0000, JOB_CONTROL 0x1000, MMU 0x2000, AS n at 0x2400 + 0x40*n).
 * No job is submitted, no MMU page table is programmed, IRQs stay masked
 * (the GIC does not route the Mali SPIs yet; everything is polled).
 * Base 0x18500000/0x5000 and the power-domain status come from the S10 DTB
 * (mali@18500000, pd-embedded_g3d@15862100). Kill switch: boot-arg b2l_gpu=0.
 */
#ifndef B2L_MALI_H
#define B2L_MALI_H
#include <stdint.h>

#define B2L_MALI_PA          0x18500000ULL
#define B2L_MALI_SIZE        0x5000u
#define MALI_GPU_ID          0x000
#define MALI_L2_FEATURES     0x004
#define MALI_CORE_FEATURES   0x008
#define MALI_TILER_FEATURES  0x00c
#define MALI_MEM_FEATURES    0x010
#define MALI_MMU_FEATURES    0x014
#define MALI_AS_PRESENT      0x018
#define MALI_JS_PRESENT      0x01c
#define MALI_GPU_IRQ_RAWSTAT 0x020
#define MALI_GPU_IRQ_CLEAR   0x024
#define MALI_GPU_IRQ_MASK    0x028
#define MALI_GPU_COMMAND     0x030
#define MALI_GPU_STATUS      0x034
#define MALI_GPU_FAULTSTATUS 0x03c
#define MALI_THREAD_MAX_THREADS 0x0a0
#define MALI_SHADER_PRESENT  0x100
#define MALI_TILER_PRESENT   0x110
#define MALI_L2_PRESENT      0x120
#define MALI_SHADER_READY    0x140
#define MALI_TILER_READY     0x150
#define MALI_L2_READY        0x160
#define MALI_SHADER_PWRON    0x180
#define MALI_TILER_PWRON     0x190
#define MALI_L2_PWRON        0x1a0
#define MALI_SHADER_PWROFF   0x1c0
#define MALI_TILER_PWROFF    0x1d0
#define MALI_L2_PWROFF       0x1e0
#define MALI_SHADER_PWRTRANS 0x200
#define MALI_L2_PWRTRANS     0x220
#define MALI_JOB_IRQ_RAWSTAT 0x1000
#define MALI_JOB_IRQ_CLEAR   0x1004
#define MALI_JOB_IRQ_MASK    0x1008
#define MALI_MMU_IRQ_RAWSTAT 0x2000
#define MALI_MMU_IRQ_CLEAR   0x2004
#define MALI_MMU_IRQ_MASK    0x2008
#define MALI_AS(n)           (0x2400u + 0x40u * (n))
#define MALI_AS_TRANSTAB_LO  0x00
#define MALI_AS_TRANSTAB_HI  0x04
#define MALI_AS_MEMATTR_LO   0x08
#define MALI_AS_MEMATTR_HI   0x0c
#define MALI_AS_COMMAND      0x18
#define MALI_AS_FAULTSTATUS  0x1c
#define MALI_AS_FAULTADDR_LO 0x20
#define MALI_AS_FAULTADDR_HI 0x24
#define MALI_AS_STATUS       0x28
#define MALI_AS_TRANSCFG_LO  0x30
#define MALI_AS_TRANSCFG_HI  0x34
#define MALI_AS_CMD_UPDATE   0x01u
#define MALI_AS_CMD_FLUSH_MEM 0x05u
/* Job slot n: JOB_CONTROL + 0x800 + 0x80*n (kbase JOB_SLOT_REG). */
#define MALI_JS(n)           (0x1800u + 0x80u * (n))
#define MALI_JS_HEAD_LO      0x00
#define MALI_JS_HEAD_HI      0x04
#define MALI_JS_STATUS       0x24
#define MALI_JS_HEAD_NEXT_LO 0x40
#define MALI_JS_HEAD_NEXT_HI 0x44
#define MALI_JS_AFFINITY_NEXT_LO 0x50
#define MALI_JS_AFFINITY_NEXT_HI 0x54
#define MALI_JS_CONFIG_NEXT  0x58
#define MALI_JS_COMMAND_NEXT 0x60
#define MALI_JS_CMD_START    0x01u
/* JS_CONFIG: AS index | start/end clean+invalidate | thread priority 8 (Panfrost). */
#define MALI_JS_CONFIG(as)   ((as) | (3u << 8) | (3u << 12) | (8u << 16))
/* AArch64 4K translation (Bifrost; Panfrost mmu_cfg_init_aarch64_4k). */
#define MALI_TRANSCFG_AARCH64_4K   6u
#define MALI_TRANSCFG_INA_BITS(x)  ((uint32_t)(x) << 6)
#define MALI_TRANSCFG_PTW_NC       (1u << 24)
#define MALI_TRANSCFG_PTW_RA       (1u << 30)
/* MEMATTR byte: inner/outer non-cacheable, implementation-defined allocation. */
#define MALI_MEMATTR_NC_ALL        0x4848484848484848ULL
/* Stage-1 style descriptors: table = PA|3; page = PA|3|AP_UNPRIV|SH_INNER|AF. */
#define MALI_PTE_TABLE             0x3ULL
#define MALI_PTE_PAGE              (0x3ULL | (1ULL << 6) | (3ULL << 8) | (1ULL << 10))
/* Job header (Panfrost v7 genxml): type at word4 bits[7:1], 64-bit descriptor bit0,
 * index word4[31:16]; WRITE_VALUE = 2, payload {address u64, type u32, _, value u64}. */
#define MALI_JOB_TYPE_WRITE_VALUE  2u
#define MALI_WRITE_VALUE_IMM32     6u
#define MALI_JOB_IRQ_DONE(n)       (1u << (n))
#define MALI_JOB_IRQ_FAIL(n)       (1u << ((n) + 16))

#define MALI_CMD_SOFT_RESET  0x01u
#define MALI_IRQ_RESET_DONE  (1u << 8)
#define MALI_IRQ_POWER_ALL   (1u << 10)

/* Product id: GPU_ID[31:16]. 0x7211 = Mali-G76 (Bifrost v7, Job Manager). */
static inline bool b2l_mali_is_g76(uint32_t gpu_id) { return (gpu_id >> 16) == 0x7211u; }
static inline unsigned b2l_mali_popcount64(uint64_t v) { unsigned n = 0; while (v) { v &= v - 1; n++; } return n; }
#endif
