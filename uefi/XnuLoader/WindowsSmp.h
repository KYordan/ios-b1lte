/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef B2L_WINDOWS_SMP_H
#define B2L_WINDOWS_SMP_H

#include <Uefi.h>

/* Before Windows Boot Manager: start every secondary the MADT enables into a parking-protocol mailbox below 4 GiB
 * (firmware PSCI CPU_ON, ParkStub.S) and publish the mailboxes, plus each core's real MPIDR_EL1, in the MADT.
 * A core that does not check in is disabled in the MADT, so Windows never issues its own CPU_ON for it. Runs once;
 * prints one line per CPU. Experiments come from \b2l-smp.txt on Volume (the Windows EFI partition; see
 * WindowsSmp.c). Returns EFI_NOT_FOUND without an MADT (single-CPU Windows, as before). */
EFI_STATUS B2lWindowsParkSecondaries(EFI_HANDLE Volume);

#endif
