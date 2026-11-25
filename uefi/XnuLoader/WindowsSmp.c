/* SPDX-License-Identifier: BSD-2-Clause
 * Windows SMP on the S10e through the ACPI parking protocol (only on the "Boot Windows" path; XNU is untouched).
 *
 * Why: with CPU1..7 enabled in the MADT, Windows resets the SoC the moment it starts a secondary (numproc 1 boots,
 * numproc 4 resets at the logo, also with truncatememory 4 GiB). Windows starts a secondary with
 * PSCI CPU_ON(mpidr, entry, context_id = its mailbox) and its entry code takes the mailbox from x0; Samsung's EL3
 * does not deliver context_id (exynos9810-woa boot-v20: x0 = the same constant on every core), so the core runs on
 * with a bogus pointer. The S9 port fixed it with the parking protocol (boot-v22, four A55s up under Windows 10);
 * this is the same scheme for the 9820:
 *
 *   1. one EfiReservedMemoryType page below 4 GiB per enabled secondary: mailbox (ProcessorId +0, JumpAddress +8,
 *      OS area to 0x7FF), ParkStub.S at +0x800, the core's own MPIDR_EL1 / liveness / CurrentEL at +0xF00..0xF10;
 *   2. firmware PSCI CPU_ON (SMC64 0xC4000003, Samsung's DT conduit) with the firmware CPU IDs the iOS port starts
 *      (A55 1-3, A75 4-5; low entry, as its build 0042 needed), ONE AT A TIME (build 44: back-to-back CPU_ONs all
 *      returned 0 but every late core ran the last core's stub - Samsung's EL3 keeps one entry address);
 *   3. MADT GICC: ParkingProtocolVersion 1 + ParkedAddress for every core that checked in, and MPIDR set to what the
 *      core itself reads (this boot chain's EL1 view puts the core index in Aff1: CPU5 reads 0x500, build 44),
 *      CPU0's too; a core that did not check in is disabled so Windows never tries its own CPU_ON on it.
 *
 * Build 45 parked all five, yet Windows 11 still reset the SoC at the logo with numproc 2. To tell "Windows never
 * used the mailbox (still PSCI CPU_ON)" from "the core died entering Windows", a released core paints its colour
 * band on the GOP framebuffer before jumping (CPU1 red, 2 green, 3 blue, 4 yellow, 5 magenta, rows 400 + 40*n),
 * and \b2l-smp.txt on the Windows EFI partition selects experiments without a rebuild (words, any order):
 *   off      park nothing and disable every secondary in the MADT: one-CPU Windows whatever the BCD says;
 *   hold     a released core paints its band and stays parked instead of entering Windows;
 *   nopsci   clear the FADT's PSCI_COMPLIANT/PSCI_USE_HVC bits, so Windows has only the parking protocol;
 *   fwmpidr  keep the table's firmware MPIDRs (0..5) instead of the observed EL1 values.
 */
#include <Uefi.h>
#include <Guid/Acpi.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleFileSystem.h>
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
#define B2L_PARK_SEEN_OFF   0xF18u
#define B2L_PARK_FB_OFF     0xF20u
#define B2L_PARK_WORDS_OFF  0xF28u
#define B2L_PARK_COLOR_OFF  0xF30u
#define B2L_PARK_HOLD_OFF   0xF38u
#define B2L_PARK_ALIVE      0x5A5ABEEFull
#define B2L_MAX_CPUS        8
#define B2L_MAX_TABLES      4
#define B2L_MPIDR_AFF_MASK  0xFF00FFFFFFull
#define B2L_GICC_TYPE       0x0B
#define B2L_GICC_MIN_LEN    80
#define B2L_BAND_ROW0       400
#define B2L_BAND_STEP       40
#define B2L_BAND_ROWS       30

/* PSCI (firmware) CPU IDs by ACPI processor UID: the overlay MADT's MPIDRs = Samsung's DT cpu@ reg values. */
STATIC CONST UINT64 mPsciTarget[B2L_MAX_CPUS] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x100, 0x101 };
/* Band colours (32-bit BGRX / RGBX alike for these): red, green, blue, yellow, magenta, cyan, white. */
STATIC CONST UINT32 mBandColor[B2L_MAX_CPUS] = { 0, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FFFF00, 0x00FF00FF,
                                                 0x0000FFFF, 0x00FFFFFF };

STATIC BOOLEAN mParked;

typedef struct {
    BOOLEAN Off, Hold, NoPsci, FwMpidr;
} B2L_SMP_OPTIONS;

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

STATIC VOID
B2lChecksum(UINT8 *T)
{
    UINT32 Len = ReadUnaligned32((CONST UINT32 *)(T + 4));
    UINT8 Sum = 0;
    T[9] = 0;
    for (UINT32 I = 0; I < Len; I++) Sum = (UINT8)(Sum + T[I]);
    T[9] = (UINT8)(0x100 - Sum);
}

/* Every distinct table with this signature the XSDT and the RSDT point at. */
STATIC UINTN
B2lFindTables(UINT32 Sig, UINT8 **Tables)
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
        for (UINTN Off = 36; Off + Step <= Len && N < B2L_MAX_TABLES; Off += Step) {
            UINT64 A = X ? ReadUnaligned64((CONST UINT64 *)(Root + Off)) : ReadUnaligned32((CONST UINT32 *)(Root + Off));
            UINT8 *T = (UINT8 *)(UINTN)A;
            if (A == 0 || !B2lTableOk(T, Sig, 0x2000)) continue;
            BOOLEAN Seen = FALSE;
            for (UINTN I = 0; I < N; I++) Seen = Seen || Tables[I] == T;
            if (!Seen) Tables[N++] = T;
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

/* \b2l-smp.txt on the Windows EFI volume: lowercase words, see the header. A missing file selects nothing. */
STATIC VOID
B2lReadOptions(EFI_HANDLE Volume, B2L_SMP_OPTIONS *O)
{
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
    EFI_FILE_PROTOCOL *Root, *File;
    CHAR8 Buf[128];
    UINTN Size = sizeof(Buf) - 1;
    ZeroMem(O, sizeof(*O));
    if (Volume == NULL || EFI_ERROR(gBS->HandleProtocol(Volume, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs)) ||
        EFI_ERROR(Fs->OpenVolume(Fs, &Root)))
        return;
    if (!EFI_ERROR(Root->Open(Root, &File, L"\\b2l-smp.txt", EFI_FILE_MODE_READ, 0))) {
        if (EFI_ERROR(File->Read(File, &Size, Buf))) Size = 0;
        File->Close(File);
        Buf[Size] = 0;
        O->Off = AsciiStrStr(Buf, "off") != NULL;
        O->Hold = AsciiStrStr(Buf, "hold") != NULL;
        O->NoPsci = AsciiStrStr(Buf, "nopsci") != NULL;
        O->FwMpidr = AsciiStrStr(Buf, "fwmpidr") != NULL;
    }
    Root->Close(Root);
}

typedef struct {
    BOOLEAN Enabled[B2L_MAX_CPUS];
    UINT64 Mailbox[B2L_MAX_CPUS];
    UINT64 OnResult[B2L_MAX_CPUS];
    UINT64 Mpidr[B2L_MAX_CPUS];
    BOOLEAN Alive[B2L_MAX_CPUS];
    BOOLEAN FwMpidr;
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
        if (!S->FwMpidr) WriteUnaligned64((UINT64 *)(Gicc + 68), B2lOwnMpidr());
    } else if (S->Alive[Uid]) {
        WriteUnaligned32((UINT32 *)(Gicc + 16), 1);                  /* ParkingProtocolVersion */
        WriteUnaligned64((UINT64 *)(Gicc + 24), S->Mailbox[Uid]);    /* ParkedAddress */
        if (!S->FwMpidr) WriteUnaligned64((UINT64 *)(Gicc + 68), S->Mpidr[Uid]);   /* what the core reads */
    } else {
        WriteUnaligned32((UINT32 *)(Gicc + 12), Flags & ~1u);        /* not parked: never let Windows CPU_ON it */
    }
}

/* FADT ARM boot architecture flags (offset 129): clear PSCI_COMPLIANT and PSCI_USE_HVC. Returns the old flags. */
STATIC UINT16
B2lClearPsci(VOID)
{
    UINT8 *Fadts[B2L_MAX_TABLES];
    UINT16 Old = 0xFFFF;
    UINTN N = B2lFindTables(SIGNATURE_32('F', 'A', 'C', 'P'), Fadts);
    for (UINTN I = 0; I < N; I++) {
        if (ReadUnaligned32((CONST UINT32 *)(Fadts[I] + 4)) < 131) continue;
        Old = ReadUnaligned16((CONST UINT16 *)(Fadts[I] + 129));
        WriteUnaligned16((UINT16 *)(Fadts[I] + 129), (UINT16)(Old & ~3u));
        B2lChecksum(Fadts[I]);
    }
    return Old;
}

EFI_STATUS
B2lWindowsParkSecondaries(EFI_HANDLE Volume)
{
    UINT8 *Madts[B2L_MAX_TABLES];
    B2L_PARK_STATE S;
    B2L_SMP_OPTIONS O;
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = NULL;
    UINTN StubSize = (UINTN)(B2LParkStubEnd - B2LParkStub);
    UINTN LiteralOff = (UINTN)(B2LParkStubMailbox - B2LParkStub);
    UINTN Count = 0, Parked = 0;

    if (mParked) return EFI_ALREADY_STARTED;
    ZeroMem(&S, sizeof(S));
    B2lReadOptions(Volume, &O);
    S.FwMpidr = O.FwMpidr;
    Print(L"Windows SMP options (\\b2l-smp.txt):%s%s%s%s%s\r\n", O.Off ? L" off" : L"", O.Hold ? L" hold" : L"",
          O.NoPsci ? L" nopsci" : L"", O.FwMpidr ? L" fwmpidr" : L"",
          (O.Off || O.Hold || O.NoPsci || O.FwMpidr) ? L"" : L" none");
    UINTN NumMadts = B2lFindTables(SIGNATURE_32('A', 'P', 'I', 'C'), Madts);
    if (NumMadts == 0) {
        Print(L"Windows SMP: no valid MADT, one CPU.\r\n");
        return EFI_NOT_FOUND;
    }
    B2lForEachGicc(Madts[0], B2lCollectEnabled, &S);
    for (UINTN U = 1; U < B2L_MAX_CPUS; U++) Count += S.Enabled[U] ? 1 : 0;
    if (StubSize > B2L_PARK_MPIDR_OFF - B2L_PARK_STUB_OFF || (LiteralOff & 7) != 0) Count = 0;
    if (O.Off) Count = 0;

    EFI_PHYSICAL_ADDRESS Pages = 0xFFFFFFFFull;
    if (Count != 0) {
        EFI_STATUS Status = gBS->AllocatePages(AllocateMaxAddress, EfiReservedMemoryType, Count, &Pages);
        if (EFI_ERROR(Status)) {
            Print(L"Windows SMP: no mailbox pages below 4 GiB (%r), one CPU.\r\n", Status);
            Count = 0;   /* nothing alive: every secondary gets disabled below */
        } else {
            ZeroMem((VOID *)(UINTN)Pages, Count * EFI_PAGE_SIZE);
        }
    }
    if (Count != 0 && EFI_ERROR(gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&Gop))) Gop = NULL;

    UINTN Slot = 0;
    for (UINTN U = 1; U < B2L_MAX_CPUS && Count != 0; U++) {
        if (!S.Enabled[U]) continue;
        UINT8 *Mail = (UINT8 *)(UINTN)(Pages + Slot++ * EFI_PAGE_SIZE);
        S.Mailbox[U] = (UINT64)(UINTN)Mail;
        WriteUnaligned32((UINT32 *)Mail, (UINT32)U);                 /* ProcessorId */
        CopyMem(Mail + B2L_PARK_STUB_OFF, B2LParkStub, StubSize);
        WriteUnaligned64((UINT64 *)(Mail + B2L_PARK_STUB_OFF + LiteralOff), S.Mailbox[U]);
        UINTN Row = B2L_BAND_ROW0 + B2L_BAND_STEP * U;
        if (Gop != NULL && Gop->Mode != NULL && Gop->Mode->Info != NULL &&
            Gop->Mode->Info->PixelFormat != PixelBltOnly && Row + B2L_BAND_ROWS <= Gop->Mode->Info->VerticalResolution) {
            UINTN Stride = Gop->Mode->Info->PixelsPerScanLine;
            WriteUnaligned64((UINT64 *)(Mail + B2L_PARK_FB_OFF), Gop->Mode->FrameBufferBase + Row * Stride * 4);
            WriteUnaligned64((UINT64 *)(Mail + B2L_PARK_WORDS_OFF), Stride * B2L_BAND_ROWS);
            WriteUnaligned32((UINT32 *)(Mail + B2L_PARK_COLOR_OFF), mBandColor[U]);
        }
        WriteUnaligned64((UINT64 *)(Mail + B2L_PARK_HOLD_OFF), O.Hold ? 1 : 0);
    }
    if (Count != 0) {
        /* The parked cores fetch and read with caches off: everything goes to the point of coherency first. */
        B2lCleanInvalidate((UINTN)Pages, Count * EFI_PAGE_SIZE);
        __asm__ volatile("ic iallu\n dsb sy\n isb" : : : "memory");
        /* One core at a time, each waiting for its own check-in; a core reporting an MPIDR already seen (CPU0's
         * included) is not trusted and stays disabled. */
        for (UINTN U = 1; U < B2L_MAX_CPUS; U++) {
            if (!S.Enabled[U]) continue;
            UINT8 *Mail = (UINT8 *)(UINTN)S.Mailbox[U];
            S.OnResult[U] = B2lSmc4(B2L_PSCI_CPU_ON64, mPsciTarget[U], S.Mailbox[U] + B2L_PARK_STUB_OFF, 0);
            if (S.OnResult[U] != 0) continue;
            for (UINTN Ms = 0; Ms < 500 && !S.Alive[U]; Ms++) {
                gBS->Stall(1000);
                B2lCleanInvalidate((UINTN)(Mail + B2L_PARK_MPIDR_OFF), 0x18);
                S.Alive[U] = ReadUnaligned64((CONST UINT64 *)(Mail + B2L_PARK_ALIVE_OFF)) == B2L_PARK_ALIVE;
            }
            S.Mpidr[U] = ReadUnaligned64((CONST UINT64 *)(Mail + B2L_PARK_MPIDR_OFF)) & B2L_MPIDR_AFF_MASK;
            for (UINTN V = 0; V < U && S.Alive[U]; V++)
                if ((V == 0 && S.Mpidr[U] == B2lOwnMpidr()) || (V != 0 && S.Alive[V] && S.Mpidr[V] == S.Mpidr[U]))
                    S.Alive[U] = FALSE;
            Parked += S.Alive[U] ? 1 : 0;
        }
    }

    for (UINTN M = 0; M < NumMadts; M++) {
        B2lForEachGicc(Madts[M], B2lPatchGicc, &S);
        B2lChecksum(Madts[M]);
    }
    UINT16 OldPsci = O.NoPsci ? B2lClearPsci() : 0;
    mParked = TRUE;

    Print(L"Windows SMP: CPU0 MPIDR 0x%lx; parked %u of %u secondaries (%u MADT copies)%s\r\n", B2lOwnMpidr(),
          (UINT32)Parked, (UINT32)Count, (UINT32)NumMadts, O.Off ? L", all disabled (off)" : L"");
    if (O.NoPsci) Print(L"  FADT ARM boot flags 0x%x -> PSCI bits cleared\r\n", (UINT32)OldPsci);
    for (UINTN U = 1; U < B2L_MAX_CPUS; U++)
        if (S.Enabled[U] && Count != 0)
            Print(L"  CPU%u psci 0x%lx: CPU_ON %ld, %s, MPIDR 0x%lx, mailbox 0x%lx\r\n", (UINT32)U, mPsciTarget[U],
                  (INT64)S.OnResult[U], S.Alive[U] ? L"parked" : L"NOT PARKED (disabled)", S.Mpidr[U], S.Mailbox[U]);
    /* Long enough to photograph whenever something is off or an experiment is selected. */
    if (Parked != Count || O.Off || O.Hold || O.NoPsci || O.FwMpidr) gBS->Stall(3000000);
    return EFI_SUCCESS;
}
