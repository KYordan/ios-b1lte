/* SPDX-License-Identifier: BSD-2-Clause
 * ACPI Multiprocessor Startup (parking protocol v1) stub for Windows secondaries. WindowsSmp.c copies it to
 * mailbox + 0x800 (the firmware half of a 4 KiB mailbox page below 4 GiB) and fills B2LParkStubMailbox with that
 * page's address; Samsung's EL3 enters it after the firmware's own PSCI CPU_ON, at EL1 with the MMU and caches off.
 *
 * Windows' PSCI CPU_ON path needs the context_id back in x0 and Samsung's EL3 does not deliver it (the S9 port,
 * exynos9810-woa boot-v20/v22); on the S10e a Windows CPU_ON reset the SoC at once. Parked here, the core records its
 * own MPIDR_EL1/CurrentEL and a liveness word (firmware area, read back by WindowsSmp.c for the MADT), then polls
 * JumpAddress (mailbox + 8). Each poll cleans and invalidates the mailbox line first: with caches off this core's
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
    mov x0, x1
    br x2
.balign 8
.Lalive:
    .quad 0x5A5ABEEF
B2LParkStubMailbox:
.Lmailbox:
    .quad 0
B2LParkStubEnd:
