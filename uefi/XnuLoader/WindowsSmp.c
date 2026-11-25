/* SPDX-License-Identifier: BSD-2-Clause
 * Windows SMP on the S10e through the ACPI parking protocol (only on the "Boot Windows" path; XNU is untouched).
 *
 * Why: with CPU1..7 enabled in the MADT, Windows resets the SoC the moment it starts a secondary (numproc 1 boots,
 * numproc 4 resets at the logo, also with truncatememory 4 GiB). Windows starts a secondary with
 * PSCI CPU_ON(mpidr, entry, context_id = its mailbox) and its entry code takes the mailbox from x0; Samsung's EL3
 * does not deliver context_id (exynos9810-woa boot-v20: x0 = the same constant on every core), so the core runs on
 * with a bogus pointer. The S9 port fixed it with the parking protocol (boot-v22, four A55s up under Windows); this
 * is the same scheme for the 9820:
 *
 *   1. one EfiReservedMemoryType page below 4 GiB per enabled secondary: mailbox (ProcessorId +0, JumpAddress +8,
 *      OS area to 0x7FF), ParkStub.S at +0x800, the core's own MPIDR_EL1 / liveness / CurrentEL at +0xF00..0xF10;
 *   2. firmware PSCI CPU_ON (SMC64 0xC4000003, Samsung's DT conduit) with the firmware CPU IDs the iOS port starts
 *      (A55 1-3, A75 4-5; low entry, as its build 0042 needed);
 *   3. MADT GICC: ParkingProtocolVersion 1 + ParkedAddress for every core that checked in, and MPIDR set to what the
 *      core itself reads (this boot chain's EL1 view puts the core index in Aff1: CPU1 reads 0x100, iOS build 0045),
 *      CPU0's too; a core that did not check in is disabled so Windows never tries its own CPU_ON on it.
 * Every MADT copy the XSDT/RSDT point at is patched and re-checksummed. Windows then writes JumpAddress and the
 * parked core enters it with x0 = mailbox.
 */
#include <Uefi.h>
#include <Guid/Acpi.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include "WindowsSmp.h"

extern UINT8 B2LParkStub[], B2LParkStubMailbox[], B2LParkStubEnd[];

#define B2L_PSCI_CPU_ON64   0xC4000003u
#define B2L_PARK_STUB_OFF   0x800u
#define B2L_PARK_MPIDR_OFF  0xF00u
#define B2L_PARK_ALIVE_OFF  0xF08u
#define B2L_PARK_EL_OFF     0xF10u
#define B2L_PARK_ALIVE      0x5A5ABEEFull
#define B2L_MAX_CPUS        8
#define B2L_MAX_MADTS       4
#define B2L_MPIDR_AFF_MASK  0xFF00FFFFFFull
#define B2L_GICC_TYPE       0x0B
#define B2L_GICC_MIN_LEN    80

/* PSCI (firmware) CPU IDs by ACPI processor UID: the overlay MADT's MPIDRs = Samsung's DT cpu@ reg values. */
STATIC CONST UINT64 mPsciTarget[B2L_MAX_CPUS] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x100, 0x101 };

STATIC BOOLEAN mParked;

STATIC UINT64
B2lSmc4(UINT64 Fn, UINT64 A1, UINT64 A2, UINT64 A3)
{
    register UINT64 X0 __asm__("x0") = Fn;
    register UINT64 X1 __asm__("x1") = A1;
    register UINT64 X2 __asm__("x2") = A2;
    register UINT64 X3 __asm__("x3") = A3;
    __asm__ volatile("smc #0"
                     : "+r"(X0), "+r"(X1), "+r"(X2), "+r"(X3)
                     :
                     : "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17",
                       "memory");
    return X0;
}

/* Clean and invalidate [Start, Start + Size) to the point of coherency, by the smallest data cache line. */
STATIC VOID
B2lCleanInvalidate(UINTN Start, UINTN Size)
{
    UINT64 Ctr;
    __asm__ volatile("mrs %0, ctr_el0" : "=r"(Ctr));
    UINTN Line = (UINTN)4 << ((Ctr >> 16) & 0xF);
    for (UINTN A = Start & ~(Line - 1); A < Start + Size; A += Line)
        __asm__ volatile("dc civac, %0" : : "r"(A) : "memory");
    __asm__ volatile("dsb sy" : : : "memory");
}

STATIC UINT64
B2lOwnMpidr(VOID)
{
    UINT64 V;
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(V));
    return V & B2L_MPIDR_AFF_MASK;
}

STATIC BOOLEAN
B2lTableOk(CONST UINT8 *T, UINT32 Sig, UINT32 MaxLen)
{
    if (T == NULL || ReadUnaligned32((CONST UINT32 *)T) != Sig) return FALSE;
    UINT32 Len = ReadUnaligned32((CONST UINT32 *)(T + 4));
    if (Len < 36 || Len > MaxLen) return FALSE;
    UINT8 Sum = 0;
    for (UINT32 I = 0; I < Len; I++) Sum = (UINT8)(Sum + T[I]);
    return Sum == 0;
}

/* Every distinct MADT the XSDT and the RSDT point at. */
STATIC UINTN
B2lFindMadts(UINT8 **Madts)
{
    UINT8 *Rsdp = NULL;
    UINTN N = 0;
    if (EFI_ERROR(EfiGetSystemConfigurationTable(&gEfiAcpi20TableGuid, (VOID **)&Rsdp)) &&
        EFI_ERROR(EfiGetSystemConfigurationTable(&gEfiAcpi10TableGuid, (VOID **)&Rsdp)))
        return 0;
    if (Rsdp == NULL || CompareMem(Rsdp, "RSD PTR ", 8) != 0) return 0;
    for (UINTN Pass = 0; Pass < 2; Pass++) {
        BOOLEAN X = Pass == 0;
        UINT64 RootAddr = X ? (Rsdp[15] >= 2 ? ReadUnaligned64((CONST UINT64 *)(Rsdp + 24)) : 0)
                            : ReadUnaligned32((CONST UINT32 *)(Rsdp + 16));
        CONST UINT8 *Root = (CONST UINT8 *)(UINTN)RootAddr;
        if (RootAddr == 0 || !B2lTableOk(Root, X ? SIGNATURE_32('X', 'S', 'D', 'T') : SIGNATURE_32('R', 'S', 'D', 'T'),
                                          0x1000))
            continue;
        UINT32 Len = ReadUnaligned32((CONST UINT32 *)(Root + 4));
        UINTN Step = X ? 8 : 4;
        for (UINTN Off = 36; Off + Step <= Len && N < B2L_MAX_MADTS; Off += Step) {
            UINT64 A = X ? ReadUnaligned64((CONST UINT64 *)(Root + Off)) : ReadUnaligned32((CONST UINT32 *)(Root + Off));
            UINT8 *T = (UINT8 *)(UINTN)A;
            if (A == 0 || !B2lTableOk(T, SIGNATURE_32('A', 'P', 'I', 'C'), 0x2000)) continue;
            BOOLEAN Seen = FALSE;
            for (UINTN I = 0; I < N; I++) Seen = Seen || Madts[I] == T;
            if (!Seen) Madts[N++] = T;
        }
    }
    return N;
}

/* Calls Fn(Context, Gicc) for every GICC structure of one MADT. */
typedef VOID (*B2L_GICC_FN)(VOID *Context, UINT8 *Gicc);
STATIC VOID
B2lForEachGicc(UINT8 *Madt, B2L_GICC_FN Fn, VOID *Context)
{
    UINT32 Len = ReadUnaligned32((CONST UINT32 *)(Madt + 4));
    for (UINT32 Off = 44; Off + 2 <= Len;) {
        UINT8 Type = Madt[Off], SubLen = Madt[Off + 1];
        if (SubLen < 2 || Off + SubLen > Len) break;
        if (Type == B2L_GICC_TYPE && SubLen >= B2L_GICC_MIN_LEN) Fn(Context, Madt + Off);
        Off += SubLen;
    }
}

typedef struct {
    BOOLEAN Enabled[B2L_MAX_CPUS];
    UINT64 Mailbox[B2L_MAX_CPUS];
    UINT64 OnResult[B2L_MAX_CPUS];
    UINT64 Mpidr[B2L_MAX_CPUS];
    BOOLEAN Alive[B2L_MAX_CPUS];
} B2L_PARK_STATE;

STATIC VOID
B2lCollectEnabled(VOID *Context, UINT8 *Gicc)
{
    B2L_PARK_STATE *S = Context;
    UINT32 Uid = ReadUnaligned32((CONST UINT32 *)(Gicc + 8));
    if (Uid < B2L_MAX_CPUS && (ReadUnaligned32((CONST UINT32 *)(Gicc + 12)) & 1)) S->Enabled[Uid] = TRUE;
}

STATIC VOID
B2lPatchGicc(VOID *Context, UINT8 *Gicc)
{
    B2L_PARK_STATE *S = Context;
    UINT32 Uid = ReadUnaligned32((CONST UINT32 *)(Gicc + 8));
    UINT32 Flags = ReadUnaligned32((CONST UINT32 *)(Gicc + 12));
    if (Uid >= B2L_MAX_CPUS || !(Flags & 1)) return;
    if (Uid == 0) {
        WriteUnaligned64((UINT64 *)(Gicc + 68), B2lOwnMpidr());
    } else if (S->Alive[Uid]) {
        WriteUnaligned32((UINT32 *)(Gicc + 16), 1);                  /* ParkingProtocolVersion */
        WriteUnaligned64((UINT64 *)(Gicc + 24), S->Mailbox[Uid]);    /* ParkedAddress */
        WriteUnaligned64((UINT64 *)(Gicc + 68), S->Mpidr[Uid]);      /* what the core reads itself */
    } else {
        WriteUnaligned32((UINT32 *)(Gicc + 12), Flags & ~1u);        /* not parked: never let Windows CPU_ON it */
    }
}

EFI_STATUS
B2lWindowsParkSecondaries(VOID)
{
    UINT8 *Madts[B2L_MAX_MADTS];
    B2L_PARK_STATE S;
    UINTN StubSize = (UINTN)(B2LParkStubEnd - B2LParkStub);
    UINTN LiteralOff = (UINTN)(B2LParkStubMailbox - B2LParkStub);
    UINTN Count = 0, Parked = 0;

    if (mParked) return EFI_ALREADY_STARTED;
    ZeroMem(&S, sizeof(S));
    UINTN NumMadts = B2lFindMadts(Madts);
    if (NumMadts == 0) {
        Print(L"Windows SMP: no valid MADT, one CPU.\r\n");
        return EFI_NOT_FOUND;
    }
    B2lForEachGicc(Madts[0], B2lCollectEnabled, &S);
    for (UINTN U = 1; U < B2L_MAX_CPUS; U++) Count += S.Enabled[U] ? 1 : 0;
    if (Count == 0 || StubSize > B2L_PARK_MPIDR_OFF - B2L_PARK_STUB_OFF || (LiteralOff & 7) != 0) {
        Print(L"Windows SMP: %u secondaries in the MADT, stub %u bytes: nothing to park.\r\n", (UINT32)Count,
              (UINT32)StubSize);
        return EFI_NOT_FOUND;
    }

    EFI_PHYSICAL_ADDRESS Pages = 0xFFFFFFFFull;
    EFI_STATUS Status = gBS->AllocatePages(AllocateMaxAddress, EfiReservedMemoryType, Count, &Pages);
    if (EFI_ERROR(Status)) {
        Print(L"Windows SMP: no mailbox pages below 4 GiB (%r), one CPU.\r\n", Status);
        /* Fall through with nothing alive: every secondary gets disabled below. */
        Count = 0;
    } else {
        ZeroMem((VOID *)(UINTN)Pages, Count * EFI_PAGE_SIZE);
    }

    UINTN Slot = 0;
    for (UINTN U = 1; U < B2L_MAX_CPUS && Count != 0; U++) {
        if (!S.Enabled[U]) continue;
        UINT8 *Mail = (UINT8 *)(UINTN)(Pages + Slot++ * EFI_PAGE_SIZE);
        S.Mailbox[U] = (UINT64)(UINTN)Mail;
        WriteUnaligned32((UINT32 *)Mail, (UINT32)U);                 /* ProcessorId */
        CopyMem(Mail + B2L_PARK_STUB_OFF, B2LParkStub, StubSize);
        WriteUnaligned64((UINT64 *)(Mail + B2L_PARK_STUB_OFF + LiteralOff), S.Mailbox[U]);
    }
    if (Count != 0) {
        /* The parked cores fetch and read with caches off: everything goes to the point of coherency first. */
        B2lCleanInvalidate((UINTN)Pages, Count * EFI_PAGE_SIZE);
        __asm__ volatile("ic iallu\n dsb sy\n isb" : : : "memory");
        for (UINTN U = 1; U < B2L_MAX_CPUS; U++)
            if (S.Enabled[U])
                S.OnResult[U] = B2lSmc4(B2L_PSCI_CPU_ON64, mPsciTarget[U], S.Mailbox[U] + B2L_PARK_STUB_OFF, 0);
        gBS->Stall(50000);
        for (UINTN U = 1; U < B2L_MAX_CPUS; U++) {
            if (!S.Enabled[U] || S.OnResult[U] != 0) continue;
            UINT8 *Mail = (UINT8 *)(UINTN)S.Mailbox[U];
            B2lCleanInvalidate((UINTN)(Mail + B2L_PARK_MPIDR_OFF), 0x18);
            S.Alive[U] = ReadUnaligned64((CONST UINT64 *)(Mail + B2L_PARK_ALIVE_OFF)) == B2L_PARK_ALIVE;
            S.Mpidr[U] = ReadUnaligned64((CONST UINT64 *)(Mail + B2L_PARK_MPIDR_OFF)) & B2L_MPIDR_AFF_MASK;
            Parked += S.Alive[U] ? 1 : 0;
        }
    }

    for (UINTN M = 0; M < NumMadts; M++) {
        B2lForEachGicc(Madts[M], B2lPatchGicc, &S);
        UINT32 Len = ReadUnaligned32((CONST UINT32 *)(Madts[M] + 4));
        UINT8 Sum = 0;
        Madts[M][9] = 0;
        for (UINT32 I = 0; I < Len; I++) Sum = (UINT8)(Sum + Madts[M][I]);
        Madts[M][9] = (UINT8)(0x100 - Sum);
    }
    mParked = TRUE;

    Print(L"Windows SMP: CPU0 MPIDR 0x%lx; parked %u of %u secondaries (%u MADT copies)\r\n", B2lOwnMpidr(),
          (UINT32)Parked, (UINT32)(Count ? Count : 0), (UINT32)NumMadts);
    for (UINTN U = 1; U < B2L_MAX_CPUS; U++)
        if (S.Enabled[U])
            Print(L"  CPU%u psci 0x%lx: CPU_ON %ld, %s, MPIDR 0x%lx, mailbox 0x%lx\r\n", (UINT32)U, mPsciTarget[U],
                  (INT64)S.OnResult[U], S.Alive[U] ? L"parked" : L"NOT PARKED (disabled)", S.Mpidr[U], S.Mailbox[U]);
    if (Parked != Count) gBS->Stall(3000000);   /* long enough to photograph */
    return EFI_SUCCESS;
}
