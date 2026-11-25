/* SPDX-License-Identifier: BSD-2-Clause
 * ACPI Multiprocessor Startup (parking protocol v1) stub for Windows secondaries. WindowsSmp.c copies it to
 * mailbox + 0x800 (the firmware half of a 4 KiB mailbox page below 4 GiB) and fills B2LParkStubMailbox with that
 * page's address; Samsung's EL3 enters it after the firmware's own PSCI CPU_ON, at EL1 with the MMU and caches off.
 *
 * Windows' PSCI CPU_ON path needs the context_id back in x0 and Samsung's EL3 does not deliver it (the S9 port,
 * exynos9810-woa boot-v20/v22); on the S10e a Windows CPU_ON reset the SoC at once. Parked here, the core records its
 * own MPIDR_EL1/CurrentEL and a liveness word (firmware area, read back by WindowsSmp.c for the MADT), then polls
 * JumpAddress (mailbox + 8) - see .Lpainted for what happens on release. Each poll cleans and invalidates the mailbox line first: with caches off this core's
 * loads do not snoop, so a cacheable store by the OS could otherwise stay in another core's cache. When the OS writes
 * a jump address, the core enters it with x0 = its mailbox, which is what the OS expects.
 */
.text
.p2align 3
.globl B2LParkStub
.globl B2LParkStubMailbox
.globl B2LParkStubEnd
B2LParkStub:
    msr daifset, #0xf
    ldr x1, .Lmailbox
    mrs x2, MPIDR_EL1
    str x2, [x1, #0xF00]
    ldr x2, .Lalive
    str x2, [x1, #0xF08]
    mrs x2, CurrentEL
    str x2, [x1, #0xF10]
    add x3, x1, #0xF00
    dc civac, x3
    dsb sy
.Lpoll:
    mov w3, #0x2000
.Ldelay:
    subs w3, w3, #1
    b.ne .Ldelay
    dc civac, x1
    dsb sy
    ldr x2, [x1, #8]
    cbz x2, .Lpoll
    /* Released. Diagnostics set up by WindowsSmp.c in the firmware area: record the jump address (+0xF18), paint
     * this CPU's colour band into the GOP framebuffer (+0xF20 address, +0xF28 32-bit words, +0xF30 colour; 0 = no
     * band) so a release is visible on screen, and with +0xF38 set (b2l-smp.txt "hold") stay here for good. */
    str x2, [x1, #0xF18]
    ldr x4, [x1, #0xF20]
    ldr x5, [x1, #0xF28]
    ldr w6, [x1, #0xF30]
    cbz x4, .Lpainted
    cbz x5, .Lpainted
.Lpaint:
    str w6, [x4], #4
    subs x5, x5, #1
    b.ne .Lpaint
.Lpainted:
    add x3, x1, #0xF00
    dc civac, x3
    dsb sy
    ldr x7, [x1, #0xF38]
    cbz x7, .Lenter
.Lhold:
    wfe
    b .Lhold
.Lenter:
    mov x0, x1
    br x2
.balign 8
.Lalive:
    .quad 0x5A5ABEEF
B2LParkStubMailbox:
.Lmailbox:
    .quad 0
B2LParkStubEnd:
