/* SPDX-License-Identifier: BSD-2-Clause
 * Opt-in, stripped-pointer ARM64e development ABI. This does not authenticate
 * pointers, claim hardware PAC, or change kernel pointer/security contracts.
 */
#ifndef B2L_COMPAT_HOST_TEST
#include <arm64/b2l_arm64e_compat.h>
#include <arm64/b2l_early.h>
#include <arm64/proc_reg.h>
#include <kern/locks.h>
#include <kern/startup.h>
#include <kern/misc_protos.h>
#include <kern/clock.h>
#include <kern/thread.h>
#include <mach/mach_time.h>
#include <string.h>
#include <mach/machine.h>
#include <mach/arm/thread_status.h>
#include <mach/arm/vm_param.h>
#include <mach/vm_region.h>
#include <pexpert/pexpert.h>
#include <vm/vm_map_internal.h>
#include <vm/vm_map_xnu.h>
#include <vm/vm_map_lock_internal.h>
#include <vm/vm_object_internal.h>
#include <vm/memory_object_internal.h>
#include <vm/vm_protos_internal.h>
#include <sys/errno.h>
extern void b2l_native_cache_observe(uint64_t,uint32_t);
#endif
#include "b2l_compat.h"

static bool b2l_compat_requested;
static bool b2l_compat_pair;   /* boot-arg b2l_pacga_pair=1: two adjacent PACGA in one trap */
/*
 * B2L_ARM64E_DIAG (build option, default 0): the measurements of B1L-0050..0110 -- trap counts by kind, a watched
 * thread, the PACPROF hot-spot table (boot-arg b2l_pacprof=1), sampled timings, and the full execute-map check of the
 * trap PC (boot-arg b2l_pac_fastcheck=0). Off, the trap path does none of it and the readers below return zeros.
 */
#ifndef B2L_ARM64E_DIAG
#define B2L_ARM64E_DIAG 0
#endif
#if B2L_ARM64E_DIAG
static bool b2l_compat_fastcheck = true;   /* B1L-0100: boot-arg b2l_pac_fastcheck (b2l_compat_validate_pc_fast) */
static bool b2l_prof_on;   /* B1L-0110: the PAC trap profile (B1L-0101), boot-arg b2l_pacprof=1 */
/* Samples cover successful wrapper work only, not exception entry/return. */
static uint64_t b2l_compat_perf[6];
struct b2l_compat_measurement { bool sample; uint64_t vm_ticks; };
static uint64_t b2l_compat_telemetry[4];
#ifndef B2L_COMPAT_HOST_TEST
extern int proc_selfpid(void);
#endif
/* B1L-0050: traps by kind -- 0 PAC/AUT register forms, 1 hint forms, 2 RETAA/RETAB, 3 BRAA/BLRAA(Z), 4 LDRAA/LDRAB,
 * 5 LDAPR, 6 PACGA, 7 other -- and 8: of all, those whose PC is in the shared cache (0x180000000-0x280000000 + slide
 * range, taken as 0x180000000-0x300000000) */
static uint64_t b2l_compat_class[9];
/* B1L-0077: one watched thread (sysctl kern.hakios.pac_watch): its traps by kind and the 64 first distinct PCs that
 * trapped, with a count each -- what one Metal draw costs, read by the test that asked */
#define B2L_WATCH_PCS 64
static void *volatile b2l_watch_thread;
static uint64_t b2l_watch_class[9], b2l_watch_pc[B2L_WATCH_PCS], b2l_watch_pc_n[B2L_WATCH_PCS], b2l_watch_lost;
void
b2l_arm64e_watch(void *thread)
{
    b2l_watch_thread = NULL;
    memset(b2l_watch_class, 0, sizeof(b2l_watch_class));
    memset(b2l_watch_pc, 0, sizeof(b2l_watch_pc));
    memset(b2l_watch_pc_n, 0, sizeof(b2l_watch_pc_n));
    b2l_watch_lost = 0;
    b2l_watch_thread = thread;
}
/* 9 kinds, then 64 PCs and 64 counts, then the traps whose PC found no slot: 138 words */
void
b2l_arm64e_watch_read(uint64_t *out)
{
    memcpy(out, b2l_watch_class, sizeof(b2l_watch_class));
    memcpy(out + 9, b2l_watch_pc, sizeof(b2l_watch_pc));
    memcpy(out + 9 + B2L_WATCH_PCS, b2l_watch_pc_n, sizeof(b2l_watch_pc_n));
    out[9 + 2 * B2L_WATCH_PCS] = b2l_watch_lost;
}
#ifndef B2L_COMPAT_HOST_TEST
/*
 * B1L-0101: WHERE THE TRAPS COME FROM. backboardd's render thread spends ~88 s of 90 in the kernel, and the traps are
 * 20 000-80 000 a second -- yet the cache translated on the Mac has almost no PAC left but corecrypto's measured text
 * (5 187 words kept original). Every trap is counted by (PC, LR, process) in a lossy table; every 30 s the 24 most
 * frequent are said (b2l_persistent_log.c) and the table starts again:
 *   B2L PACPROF: <count> <process>[pid] pc=0x.. lr=0x.. insn=0x..
 * The PC and LR are runtime (slid) addresses: the slide is found on the Mac by matching the instructions.
 */
#define B2L_PROF_SLOTS 1024u
struct b2l_prof_entry { uint64_t pc, lr, n; uint32_t insn, pid; };
static struct b2l_prof_entry b2l_prof_tab[B2L_PROF_SLOTS];
static uint64_t b2l_prof_lost;
static void
b2l_prof_note(uint64_t pc, uint64_t lr, uint32_t insn, uint32_t pid)
{
    unsigned h = (unsigned)(((pc >> 2) * 0x9e3779b1u) ^ ((lr >> 2) * 0x85ebca6bu) ^ pid) & (B2L_PROF_SLOTS - 1);
    for (unsigned k = 0; k < 16; k++) {
        struct b2l_prof_entry *e = &b2l_prof_tab[(h + k) & (B2L_PROF_SLOTS - 1)];
        uint64_t cur = __atomic_load_n(&e->pc, __ATOMIC_ACQUIRE);
        if (cur == pc && e->lr == lr && e->pid == pid) {
            __atomic_fetch_add(&e->n, 1, __ATOMIC_RELAXED);
            return;
        }
        if (cur == 0) {
            uint64_t zero = 0;
            if (__atomic_compare_exchange_n(&e->pc, &zero, UINT64_MAX, false, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED)) {
                e->lr = lr; e->insn = insn; e->pid = pid; e->n = 1;
                __atomic_store_n(&e->pc, pc, __ATOMIC_RELEASE);
                return;
            }
        }
    }
    __atomic_fetch_add(&b2l_prof_lost, 1, __ATOMIC_RELAXED);
}
extern void proc_name(int pid, char *buf, int size);
void
b2l_arm64e_prof_report(void)
{
    if (!b2l_prof_on) return;
    struct b2l_prof_entry top[24];
    unsigned used = 0;
    memset(top, 0, sizeof(top));
    for (unsigned i = 0; i < B2L_PROF_SLOTS; i++) {
        struct b2l_prof_entry e = b2l_prof_tab[i];
        if (e.pc == 0 || e.pc == UINT64_MAX) continue;
        used++;
        unsigned at = 24;
        while (at > 0 && top[at - 1].n < e.n) at--;
        if (at >= 24) continue;
        memmove(&top[at + 1], &top[at], (23 - at) * sizeof(top[0]));
        top[at] = e;
    }
    uint64_t lost = __atomic_exchange_n(&b2l_prof_lost, 0, __ATOMIC_RELAXED);
    memset(b2l_prof_tab, 0, sizeof(b2l_prof_tab));
    b2l_early_trace("B2L PACPROF: %u places, %llu not counted (table full)\n", used, lost);
    for (unsigned i = 0; i < 24 && top[i].n; i++) {
        char name[32] = "?";
        proc_name((int)top[i].pid, name, sizeof(name));
        b2l_early_trace("B2L PACPROF: %llu %s[%u] pc=0x%llx lr=0x%llx insn=0x%08x\n", top[i].n, name, top[i].pid,
            top[i].pc, top[i].lr, top[i].insn);
    }
}
#endif

static unsigned
b2l_compat_class_of(uint32_t i)
{
    if ((i & UINT32_C(0xffffc000)) == UINT32_C(0xdac10000) ||
        (i & UINT32_C(0xfffffbe0)) == UINT32_C(0xdac143e0)) return 0;   /* PAC/AUT, and XPACI/XPACD */
    if ((i & UINT32_C(0xfffff01f)) == UINT32_C(0xd503201f)) return 1;
    if (i == UINT32_C(0xd65f0bff) || i == UINT32_C(0xd65f0fff)) return 2;
    /* same masks as the decoder (b2l_compat.c): BRAA/BLRAA, then the Z forms whose Rm field must be 31 */
    if ((i & UINT32_C(0xffdff800)) == UINT32_C(0xd71f0800) || (i & UINT32_C(0xffdff81f)) == UINT32_C(0xd61f081f)) return 3;
    if ((i & UINT32_C(0xff200400)) == UINT32_C(0xf8200400)) return 4;
    if ((i & UINT32_C(0x3ffffc00)) == UINT32_C(0x38bfc000)) return 5;
    if ((i & UINT32_C(0xffe0fc00)) == UINT32_C(0x9ac03000)) return 6;
    return 7;
}
void b2l_arm64e_class_snapshot(uint64_t values[9])
{
    for (unsigned i = 0; i < 9; i++) values[i] = __atomic_load_n(&b2l_compat_class[i], __ATOMIC_RELAXED);
}
void b2l_arm64e_compat_snapshot(uint64_t values[4])
{
    for (unsigned i = 0; i < 4; i++) values[i] = __atomic_load_n(&b2l_compat_telemetry[i], __ATOMIC_RELAXED);
}
void
b2l_arm64e_perf_snapshot(uint64_t values[6])
{
    for (unsigned i = 0; i < 6; i++) values[i] = __atomic_load_n(&b2l_compat_perf[i], __ATOMIC_RELAXED);
#ifndef B2L_COMPAT_HOST_TEST
    for (unsigned i = 1; i <= 2; i++) {
        uint64_t ns = 0; absolutetime_to_nanoseconds(values[i], &ns); values[i] = ns;
    }
#endif
}

/* Before each trap is emulated: counts, profile, watched thread. Atomic: every core traps. */
static void
b2l_diag_trap(arm_saved_state_t *state, uint32_t instruction)
{
    uint64_t trap_pc = get_saved_state_pc(state);
    bool in_cache = trap_pc >= UINT64_C(0x180000000) && trap_pc < UINT64_C(0x300000000);
    unsigned kind = b2l_compat_class_of(instruction);
    __atomic_fetch_add(&b2l_compat_telemetry[0], 1, __ATOMIC_RELAXED);
    __atomic_fetch_add(&b2l_compat_class[kind], 1, __ATOMIC_RELAXED);
    if (in_cache) __atomic_fetch_add(&b2l_compat_class[8], 1, __ATOMIC_RELAXED);
#ifndef B2L_COMPAT_HOST_TEST
    __atomic_store_n(&b2l_compat_telemetry[2], (uint64_t)proc_selfpid(), __ATOMIC_RELAXED);
    if (b2l_prof_on)   /* B1L-0110: boot-arg b2l_pacprof=1 */
        b2l_prof_note(trap_pc, get_saved_state_lr(state), instruction, (uint32_t)proc_selfpid());   /* B1L-0101 */
    if (b2l_watch_thread != NULL && (void *)current_thread() == b2l_watch_thread) {   /* B1L-0077: only that thread writes */
        b2l_watch_class[kind]++;
        if (in_cache) b2l_watch_class[8]++;
        unsigned k;
        for (k = 0; k < B2L_WATCH_PCS && b2l_watch_pc[k] != 0 && b2l_watch_pc[k] != trap_pc; k++) {}
        if (k < B2L_WATCH_PCS) { b2l_watch_pc[k] = trap_pc; b2l_watch_pc_n[k]++; } else b2l_watch_lost++;
    }
#endif
    __atomic_store_n(&b2l_compat_telemetry[3], trap_pc, __ATOMIC_RELAXED);
}
#else   /* !B2L_ARM64E_DIAG: same entry points, nothing measured */
void b2l_arm64e_watch(void *thread) { (void)thread; }
void b2l_arm64e_watch_read(uint64_t *out) { memset(out, 0, (9 + 2 * 64 + 1) * sizeof(uint64_t)); }
void b2l_arm64e_prof_report(void) {}
void b2l_arm64e_class_snapshot(uint64_t values[9]) { memset(values, 0, 9 * sizeof(uint64_t)); }
void b2l_arm64e_compat_snapshot(uint64_t values[4]) { memset(values, 0, 4 * sizeof(uint64_t)); }
void b2l_arm64e_perf_snapshot(uint64_t values[6]) { memset(values, 0, 6 * sizeof(uint64_t)); }
#endif  /* B2L_ARM64E_DIAG */

static void
b2l_compat_init(void)
{
    int requested = 0;
    b2l_compat_requested =
        PE_parse_boot_argn("b2l_arm64e_compat", &requested, sizeof(requested)) && requested == 1;
#if B2L_ARM64E_DIAG
    {   /* B1L-0110 */
        int prof = 0;
        (void)PE_parse_boot_argn("b2l_pacprof", &prof, sizeof(prof));
        b2l_prof_on = prof == 1;
    }
    {   /* B1L-0100 */
        int fast = 1;
        (void)PE_parse_boot_argn("b2l_pac_fastcheck", &fast, sizeof(fast));
        b2l_compat_fastcheck = fast != 0;
    }
#endif
    int pair = 0;
    b2l_compat_pair = b2l_compat_requested &&
        PE_parse_boot_argn("b2l_pacga_pair", &pair, sizeof(pair)) && pair == 1;
    if (b2l_compat_requested) {
        /* TUNABLES runs before log_lock initialization. Use only the
         * already mapped, lock-free early framebuffer sink here. */
        const char *message = "B2L ARM64e: development stripped-pointer runtime enabled; hardware PAC unchanged\n";
        while (*message) b2l_early_putc(*message++);
        message = "B2L PACGA: ABI v2 (EOR Xn, Xm ROR #32), unkeyed\n";
        while (*message) b2l_early_putc(*message++);
        if (b2l_compat_pair) {
            message="B2L PACGA: bounded adjacent pair enabled; execute checks retained\n";
            while (*message) b2l_early_putc(*message++);
        }
    }
}
STARTUP(TUNABLES, STARTUP_RANK_MIDDLE, b2l_compat_init);

bool
b2l_arm64e_compat_enabled(void)
{
    return b2l_compat_requested;
}

const char *
b2l_arm64e_pacga_name(void)
{
    return b2l_compat_requested ? "abi-v2" : "disabled";
}

/* No top-byte/PAC guessing. Every access stays in the current EL0 address
 * space and remains faultable through copyin. End is exclusive. */
static bool
b2l_user_range(uint64_t address, unsigned width)
{
    return width && address && address < MACH_VM_MAX_ADDRESS &&
           width <= MACH_VM_MAX_ADDRESS - address;
}

static int
b2l_compat_read(uint64_t address, unsigned width, uint64_t *out, void *opaque __unused)
{
    uint64_t value = 0;
    if (!out || (width != 1 && width != 2 && width != 4 && width != 8) ||
        !b2l_user_range(address, width) || copyin(address, &value, width)) return -1;
    *out = value; /* Both kernel and target are little endian. */
    return 0;
}

static int
b2l_compat_read_acquire(uint64_t address, unsigned width, uint64_t *out, void *opaque __unused)
{
    /* Fault-recoverable scalar atomics, followed by a stronger acquire
     * barrier. Byte/halfword use their containing aligned user word; it cannot
     * cross a page and remains inside the same process mapping. */
    if (!out || (width != 1 && width != 2 && width != 4 && width != 8) ||
        !b2l_user_range(address, width) || (address & (width - 1u))) return -1;
    int error;
    if (width == 8) {
        error = copyin_atomic64(address, out);
    } else {
        uint32_t word = 0;
        error = copyin_atomic32(address & ~UINT64_C(3), &word);
        if (!error) *out = (word >> ((address & 3u) * 8u)) &
                          (width == 4 ? UINT32_MAX : ((1u << (width * 8u)) - 1u));
    }
    if (!error) {
#ifndef B2L_COMPAT_HOST_TEST
        __asm__ volatile("dmb ish" ::: "memory");
#else
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
#endif
    }
    return error;
}

/* STLUR* (LRCPC2): release barrier, then the store. 4 and 8 bytes are single-copy atomic (copyout_atomic*).
 * shortcut: 1- and 2-byte stores go through copyout, which XNU does not promise to make one access; the 23G90 cache
 * has 3 STLURH and no STLURB in code (tools/scan-isa.py). Add an atomic halfword copyout if one ever matters. */
static int
b2l_compat_write_release(uint64_t address, unsigned width, uint64_t value, void *opaque __unused)
{
    if ((width != 1 && width != 2 && width != 4 && width != 8) ||
        !b2l_user_range(address, width) || (address & (width - 1u))) return -1;
#ifndef B2L_COMPAT_HOST_TEST
    __asm__ volatile("dmb ish" ::: "memory");
#else
    __atomic_thread_fence(__ATOMIC_RELEASE);
#endif
    if (width == 8) return copyout_atomic64(value, address) ? -1 : 0;
    if (width == 4) return copyout_atomic32((uint32_t)value, address) ? -1 : 0;
    return copyout(&value, address, width) ? -1 : 0;   /* little endian: the low bytes */
}

#if B2L_ARM64E_DIAG
/* The full check of B1L-0099 and before (boot-arg b2l_pac_fastcheck=0), kept for measurements. */
static int
b2l_compat_validate_pc(uint64_t address, void *opaque)
{
    uint32_t instruction;
    if ((address & 3u) || !b2l_user_range(address, sizeof(instruction))) return -1;
    /* Inspect only the exact instruction range under shared entry locks.
     * Constant/transparent submaps must resolve to the executable leaf, with
     * bounds translated/clamped back into the caller's parent address space.
     * Never cache an unlocked permission. */
    struct b2l_compat_measurement *measurement=opaque;
    uint64_t began=measurement && measurement->sample ? mach_absolute_time() : 0;
    VM_MAP_LOCK_CTX_DECLARE(ctx);
    vm_map_t map=current_map();
    kern_return_t kr=vm_map_range_sh_lock(ctx,&map,address,address+sizeof(instruction),
        VMRL_SH_STREAM_NO_HOLES | VMRL_SH_DESCEND_INTO_CONSTANT);
    bool executable=false;
    if (kr==KERN_SUCCESS) {
        vm_map_entry_t entry=vm_map_range_stream_next_with_error(ctx,&kr);
        if (kr==KERN_SUCCESS && entry && !entry->is_sub_map &&
            (entry->protection & VM_PROT_EXECUTE)) {
            vm_map_address_t start=0,end=0;
            vm_map_lock_ctx_bounds_in_parent(ctx,&start,&end,NULL);
            executable=start<=address && end>=address+sizeof(instruction);
        }
        vm_map_range_sh_unlock(ctx,&map);
    }
    if (measurement && measurement->sample) measurement->vm_ticks += mach_absolute_time()-began;
    if (!executable) return -1;
    /* Release all map locks BEFORE faultable instruction copyin. */
    return copyin_atomic32(address, &instruction) ? -1 : 0;
}
#endif

/*
 * B1L-0100: the cheap check. The emulator only ever sets a user PC: whatever lies there is then fetched by the
 * processor at EL0, which enforces execute permission itself (a non-executable or unmapped target is an instruction
 * abort of that program, as it would be on a phone with PAC). So alignment and the user range are what keep the kernel
 * safe; the map walk (~3.5 us of the ~11 us each trap cost at B1L-0098) only restated what the hardware enforces.
 */
static int
b2l_compat_validate_pc_fast(uint64_t address, void *opaque)
{
    (void)opaque;
    return ((address & 3u) || !b2l_user_range(address, sizeof(uint32_t))) ? -1 : 0;
}

/* FJCVTZS source: the user SIMD/FP registers, saved in the thread's PCB at exception entry from EL0 and restored on
 * return (pcb.c: machine.uNeon = &contextData->ns). Only the low 64 bits (Dn) are read. */
static int
b2l_compat_read_d(unsigned n, uint64_t *out, void *opaque __unused)
{
    if (n > 31) return -1;
#ifdef B2L_COMPAT_HOST_TEST
    *out = b2l_host_fp[n];
#else
    arm_neon_saved_state_t *ns = current_thread()->machine.uNeon;
    if (ns == NULL || !is_neon_saved_state64(ns)) return -1;
    *out = (uint64_t)neon_state64(ns)->v.q[n];
#endif
    return 0;
}

static bool
b2l_compat_is_pacga(uint32_t instruction)
{
    return (instruction & UINT32_C(0xffe0fc00)) == UINT32_C(0x9ac03000);
}

bool
b2l_arm64e_compat_emulate(arm_saved_state_t *state, uint32_t instruction)
{
    if (!b2l_compat_requested || !state || !is_saved_state64(state) ||
        !PSR64_IS_USER64(get_saved_state_cpsr(state)) ||
        !b2l_arm64e_current_task()) return false;
    struct b2l_compat_ops operations = {
        .mode = B2L_COMPAT_MODE_STRIPPED,
        .read = b2l_compat_read, .read_acquire = b2l_compat_read_acquire,
        .validate_pc = b2l_compat_validate_pc_fast,
        .read_d = b2l_compat_read_d,
        .write_release = b2l_compat_write_release
    };
#if B2L_ARM64E_DIAG
    b2l_diag_trap(state, instruction);
    uint64_t ticket = __atomic_load_n(&b2l_compat_telemetry[0], __ATOMIC_RELAXED);
    struct b2l_compat_measurement measurement = { .sample = (ticket & 1023u) == 0 };
    uint64_t began = measurement.sample ? mach_absolute_time() : 0;
    operations.opaque = &measurement;
    if (!b2l_compat_fastcheck) operations.validate_pc = b2l_compat_validate_pc;
#endif
    struct b2l_compat_ctx context = {0};
    for (unsigned i = 0; i < 29; ++i) context.x[i] = get_saved_state_reg(state, i);
    context.x[29] = get_saved_state_fp(state);
    context.x[30] = get_saved_state_lr(state);
    context.sp = get_saved_state_sp(state);
    context.pc = get_saved_state_pc(state);
    context.cpsr = get_saved_state_cpsr(state);
    if (operations.validate_pc(context.pc, operations.opaque)) return false;
    enum b2l_compat_result result = b2l_compat_emulate(instruction, &context, &operations);
    if (result != B2L_COMPAT_OK) {
        static unsigned rejected;   /* the program dies of it; a relaunch loop must not flood the log */
        if (__atomic_fetch_add(&rejected, 1, __ATOMIC_RELAXED) < 64)
            b2l_early_trace("B2L COMPAT: rejected result=%u pc=0x%llx insn=0x%08x x16=0x%llx lr=0x%llx\n",
                result, context.pc, instruction, context.x[16], context.x[30]);
        return false;
    }
    if (b2l_compat_pair && !(context.cpsr & PSR64_SS) &&
        current_thread()->machine.DebugData == NULL && b2l_compat_is_pacga(instruction)) {
        /* The first decoder has already validated its next PC.
         * Fetch only one following instruction through recoverable EL0 copyin.
         * Reuse the actual decoder for all operand/ZR/SP and next-PC checks.
         * Failure to consume the second instruction commits ONLY the first;
         * the next EL0 instruction then executes/traps normally.
         * Debug contexts and software single-step retain one-instruction traps. */
        uint32_t following = 0;
        bool paired = false;
        if (!copyin_atomic32(context.pc, &following) && b2l_compat_is_pacga(following)) {
            struct b2l_compat_ctx candidate = context;
            if (b2l_compat_emulate(following, &candidate, &operations) == B2L_COMPAT_OK) {
                context = candidate;
                paired = true;
            }
        }
#if B2L_ARM64E_DIAG
        __atomic_fetch_add(&b2l_compat_perf[paired ? 4 : 5], 1, __ATOMIC_RELAXED);
#else
        (void)paired;
#endif
    }
#ifndef B2L_COMPAT_HOST_TEST
    /* Enqueue only after successful emulation, using the original PC. */
    b2l_native_cache_observe(get_saved_state_pc(state), instruction);
#endif
    for (unsigned i = 0; i < 29; ++i) set_user_saved_state_reg(state, i, context.x[i]);
    set_saved_state_fp(state, context.x[29]);
    set_user_saved_state_lr(state, context.x[30]);
    set_saved_state_sp(state, context.sp);
    set_user_saved_state_pc(state, context.pc);
    {   /* the decoder changes only NZCV of CPSR; mode, masks and SS stay as the processor left them */
        uint32_t old = get_saved_state_cpsr(state), nzcv = context.cpsr & UINT32_C(0xf0000000);
        if ((old & UINT32_C(0xf0000000)) != nzcv) mask_user_saved_state_cpsr(state, nzcv, UINT32_C(0xf0000000) & ~nzcv);
    }
#if B2L_ARM64E_DIAG
    __atomic_fetch_add(&b2l_compat_telemetry[1], 1, __ATOMIC_RELAXED);
    if (measurement.sample) {
        __atomic_fetch_add(&b2l_compat_perf[0], 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(&b2l_compat_perf[1], mach_absolute_time() - began, __ATOMIC_RELAXED);
        __atomic_fetch_add(&b2l_compat_perf[2], measurement.vm_ticks, __ATOMIC_RELAXED);
    }
#endif
    return true;
}

#ifndef B2L_COMPAT_HOST_TEST

#if defined(B2L)
/* Read-only address->file resolution for the background cache learner.
 * Parent-address translation handles transparent and constant submaps; shadow
 * and backing-pager offsets are accumulated exactly as code-signing mapping.
 * The caller owns a map reference. No VFS I/O occurs while a VM lock is held.
 */
extern int vnode_getwithvid(struct vnode *,uint32_t);
extern uint32_t vnode_vid(struct vnode *);
int
b2l_cache_resolve(vm_map_t source,uint64_t address,struct vnode **out,uint64_t *file_offset)
{
 VM_MAP_LOCK_CTX_DECLARE(ctx);
 vm_map_t map=source;vm_map_entry_t entry;vm_object_t object,next;
 uint64_t offset;struct vnode *vp=NULL;uint32_t vid=0;int error=EINVAL;
 *out=NULL;*file_offset=0;
 if((address&3)||address>UINT64_MAX-4)return EINVAL;
 if(vm_map_range_sh_lock(ctx,&map,address,address+4,
    VMRL_SH_STREAM_NO_HOLES|VMRL_SH_DESCEND_INTO_CONSTANT)!=KERN_SUCCESS)return EFAULT;
 entry=vm_map_range_stream_next(ctx);
 if(!entry||entry->is_sub_map||!(entry->protection&VM_PROT_EXECUTE)||VME_OBJECT(entry)==VM_OBJECT_NULL)goto unlock;
 offset=vm_map_lock_ctx_offset_for_parent_address(ctx,address);
 object=VME_OBJECT(entry);vm_object_lock_shared(object);
 for(unsigned depth=0;depth<128;depth++){
  if(object->shadow!=VM_OBJECT_NULL){
   if(offset>UINT64_MAX-object->vo_shadow_offset)break;
   offset+=object->vo_shadow_offset;next=object->shadow;
   vm_object_lock_shared(next);vm_object_unlock(object);object=next;continue;
  }
  if(object->internal||!object->pager_ready||object->terminating||!object->alive||object->pager==MEMORY_OBJECT_NULL||offset>UINT64_MAX-object->paging_offset)break;
  offset+=object->paging_offset;
  vm_object_offset_t backing_offset;
  if(memory_object_backing_object(object->pager,offset,&next,&backing_offset)){
   if(next==VM_OBJECT_NULL||next==object)break;
   vm_object_lock_shared(next);vm_object_unlock(object);object=next;offset=backing_offset;continue;
  }
  vp=vnode_pager_lookup_vnode(object->pager);
  if(vp){vid=vnode_vid(vp);error=0;}
  break;
 }
 vm_object_unlock(object);
 unlock:vm_map_range_sh_unlock(ctx,&map);
 if(error||!vp)return EINVAL;
 if(vnode_getwithvid(vp,vid))return EAGAIN;
 *out=vp;*file_offset=offset;return 0;
}
#endif

#endif
