/* SPDX-License-Identifier: BSD-2-Clause
 * Boot menu, UEFI side: keys from the firmware keypad (Silicium KeypadDxe on beyond1lte: volume up = SCAN_UP,
 * volume down = SCAN_DOWN, power = Enter on release, Bixby = Esc), the screen drawn by BootMenuDraw.c and blitted
 * to the GOP (text console as a fallback), actions through the runtime services. The decisions themselves are in BootMenu.c (host tested).
 *
 * Nothing here writes a partition. The remembered choice is a UEFI variable: on this firmware the variable store
 * is emulated in RAM (SiliciumPkg PcdEmuVariableNvModeEnable=TRUE), so it is kept across a return to the menu, not
 * across a reboot.
 */
#include <PiDxe.h>   /* Uefi.h plus EFI_DXE_SERVICES, which DxeServicesTableLib.h needs */
#include <Library/BaseLib.h>
#include <Library/ArmLib.h>
#include <Library/IoLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/PrintLib.h>
#include <Protocol/GraphicsOutput.h>
#include "BootMenu.h"
#include "BootMenuEfi.h"
#include "BootMenuDraw.h"

#ifndef B2L_MENU_TIMEOUT_S
#define B2L_MENU_TIMEOUT_S 10       /* build option: -D B2L_MENU_TIMEOUT_S=n, 0 = no automatic boot */
#endif
#define B2L_MENU_PROBE_MS 700u      /* a volume key held at start repeats after 500 ms (KeypadDeviceLib) */

/* Exynos 9820 PMU (firmware MemoryMapLib "PMU" 0x15860000, mapped as device memory). Samsung's kernel asks the
 * bootloader for recovery by writing INFORM3 = 0x12345674 (drivers/samsung/sec_reboot.c, SEC_RESET_REASON_RECOVERY)
 * and SYSIP_DAT0 = 0xFF (drivers/soc/samsung/exynos-reboot.c, REBOOT_MODE_RECOVERY) before the software reset.
 * Confirmed on the S10e (beyond0lte): "Reboot to recovery" lands in TWRP. */
#define B2L_PMU_BASE          0x15860000ULL
#define B2L_PMU_INFORM3       0x080C
#define B2L_PMU_SYSIP_DAT0    0x0810
/* exynos-reboot.c (non-9810 path): clear RST_STAT, then SWRESET = 1 << 1. Silicium's S5E9820 ResetSystemLib writes
 * 1 (the 9810 trigger bit), which does nothing on the S10: both reboot entries said "Reboot failed" in B1L-0216. */
#define B2L_PMU_RST_STAT      0x0404
#define B2L_PMU_SWRESET       0x3A00
#define B2L_SWRESET_TRIGGER   (1u << 1)
#define B2L_RESET_RECOVERY    0x12345674u
#define B2L_REBOOT_RECOVERY   0xFFu
/* exynos-reboot.c exynos_power_off(): PS_HOLD_CONTROL bit 8 (PS_HOLD data) low, the PMIC then cuts the supply.
 * With a charger connected Samsung's PMIC may start the phone again (charging mode). */
#define B2L_PMU_PS_HOLD       0x030C
#define B2L_PS_HOLD_DATA      (1u << 8)

STATIC EFI_GUID mB2lMenuGuid = { 0x78da37fb, 0x7929, 0x467d, { 0xbc, 0xd8, 0xd3, 0x44, 0x2c, 0x84, 0x3d, 0x39 } };
STATIC CONST CHAR16 *mItems[B2L_MENU_COUNT] = {
    L"Boot iOS", L"Stay here (don't boot)", L"Reboot to recovery", L"Reboot", L"Power off",
};
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL *mGop;
STATIC UINT32 *mPixels;     /* back buffer, one GOP mode in size, kept for the loader's lifetime */

STATIC UINT64
NowMs(VOID)
{
    UINT64 Count, Frequency;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(Count));
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(Frequency));
    return Frequency ? DivU64x64Remainder(Count, Frequency / 1000 ? Frequency / 1000 : 1, NULL) : 0;
}

STATIC enum b2l_menu_key
ReadKey(VOID)
{
    EFI_INPUT_KEY Key;
    if (gST->ConIn == NULL || EFI_ERROR(gST->ConIn->ReadKeyStroke(gST->ConIn, &Key))) return B2L_KEY_NONE;
    if (Key.ScanCode == SCAN_UP) return B2L_KEY_UP;
    if (Key.ScanCode == SCAN_DOWN) return B2L_KEY_DOWN;
    if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) return B2L_KEY_SELECT;
    if (Key.ScanCode == SCAN_ESC) return B2L_KEY_BACK;
    return B2L_KEY_NONE;    /* SCAN_LEFT/RIGHT: the keypad's power+volume long-press combos, unused here */
}

/* Waits at most Ms for a key; returns at once when one arrives. */
STATIC enum b2l_menu_key
WaitKey(EFI_EVENT Timer, UINTN Ms)
{
    EFI_EVENT Events[2];
    UINTN Count = 0, Index;
    enum b2l_menu_key Key = ReadKey();
    if (Key != B2L_KEY_NONE) return Key;
    if (gST->ConIn != NULL) Events[Count++] = gST->ConIn->WaitForKey;
    if (Timer == NULL || EFI_ERROR(gBS->SetTimer(Timer, TimerRelative, (UINT64)Ms * 10000))) {
        gBS->Stall(Ms * 1000);
        return ReadKey();
    }
    Events[Count++] = Timer;
    gBS->WaitForEvent(Count, Events, &Index);
    return ReadKey();
}

/* Graphics when the GOP is there and the back buffer fits in memory; otherwise the firmware text console. */
STATIC VOID
Draw(CONST struct b2l_menu *Menu, UINT64 Now, CONST CHAR8 *Note, CONST CHAR8 *Status)
{
    unsigned Left = b2l_menu_seconds_left(Menu, Now);
    if (mPixels == NULL && mGop == NULL &&
        !EFI_ERROR(gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&mGop)) &&
        mGop->Mode && mGop->Mode->Info && mGop->Mode->Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor)
        mPixels = AllocatePool((UINTN)mGop->Mode->Info->HorizontalResolution * mGop->Mode->Info->VerticalResolution * 4);
    if (mPixels) {
        UINT32 W = mGop->Mode->Info->HorizontalResolution, H = mGop->Mode->Info->VerticalResolution;
        struct b2l_menu_view View = { Menu->selected, Left, B2L_MENU_TIMEOUT_S, Status, Note };
        b2l_menu_render(mPixels, W, W, H, &View);
        if (!EFI_ERROR(mGop->Blt(mGop, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)mPixels, EfiBltBufferToVideo, 0, 0, 0, 0, W, H, 0)))
            return;
    }
    gST->ConOut->ClearScreen(gST->ConOut);
    Print(L"iOS 26 - Galaxy S10\r\n\r\n");
    for (UINTN I = 0; I < B2L_MENU_COUNT; I++)
        Print(L" %s %s\r\n", I == Menu->selected ? L">" : L" ", mItems[I]);
    Print(L"\r\nVolume +/-: move    Power: select    Bixby: stop the timer\r\n");
    if (Status) Print(L"%a\r\n", Status);
    else if (Left) Print(L"Booting iOS in %u s\r\n", Left);
    if (Note) Print(L"\r\n%a\r\n", Note);
}

STATIC VOID
Remember(UINT8 Item)
{
    /* Best effort: a failure only means the cursor starts on "Boot iOS" next time. */
    (VOID)gRT->SetVariable(L"B2LBootMenuItem", &mB2lMenuGuid,
                           EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS, sizeof(Item), &Item);
}

STATIC unsigned
Remembered(VOID)
{
    UINT8 Item = B2L_MENU_COUNT;
    UINTN Size = sizeof(Item);
    if (EFI_ERROR(gRT->GetVariable(L"B2LBootMenuItem", &mB2lMenuGuid, NULL, &Size, &Item)) || Size != sizeof(Item))
        return B2L_MENU_COUNT;
    return Item;
}

STATIC VOID
Reset(BOOLEAN Recovery)
{
    if (Recovery) {
        MmioWrite32(B2L_PMU_BASE + B2L_PMU_INFORM3, B2L_RESET_RECOVERY);
        MmioWrite32(B2L_PMU_BASE + B2L_PMU_SYSIP_DAT0, B2L_REBOOT_RECOVERY);
    }
    MmioWrite32(B2L_PMU_BASE + B2L_PMU_RST_STAT, 0);
    MmioWrite32(B2L_PMU_BASE + B2L_PMU_SWRESET, B2L_SWRESET_TRIGGER);
    gBS->Stall(500 * 1000);
    gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);   /* fallback: the firmware's own reset */
    /* Still here: neither reset took. Say so instead of hanging silently. */
    Print(L"\r\nReboot failed. Hold Power to switch the phone off.\r\n");
}

/* Power off as Samsung's kernel does it, in the order it does it:
 *  1. rtc-s2mps19.c s2m_rtc_shutdown(): clear WTSR_EN (bit 6) and SMPL_EN (bit 7) of the S2MPS19 RTC register
 *     WTSR_SMPL (0x01) and SUB_SMPL_EN (bit 3) of CAPSEL (0x03). Armed, the PMIC takes PS_HOLD dropping for a fault
 *     and starts the phone again: the S10e restarted on every power off. The PMIC is reached through the APM
 *     firmware's MFD service, ACPM channel 2 (DT s2mps19mfd), as the kernel's RTC read does (B2LCPU.cpp b2l_mfd).
 *  2. sec_power_off() -> exynos_acpm_reboot() -> acpm_enter_wfi(): stop the APM, cmd[0] = 1 << 22 on the DT "acpm"
 *     node's channel (4), reply awaited.
 *  3. PS_HOLD low, retried once a second.
 * The ACPM queues have the kernel's B2LACPM.h layout (framework header at SRAM + 0x7000, 72-byte channel
 * descriptors). They are touched only if the firmware maps both regions: an unmapped access would hang the menu. */
#define B2L_ACPM_SRAM         0x02039000ULL
#define B2L_ACPM_SRAM_SIZE    0x30000u
#define B2L_ACPM_MBOX         0x15900000ULL
#define B2L_ACPM_MFD_CH       2u
#define B2L_ACPM_STOP_CH      4u
#define B2L_PMIC_RTC_TYPE     0x02u

/* 0 when [Base, Base + Size) is mapped. The firmware does not describe the APM SRAM and mailbox (build 12: the menu's
 * power off skipped both steps and the PMIC restarted the phone), so a range absent from the GCD is added as uncached
 * MMIO; setting its attributes has the CPU driver map it. Negative codes say which step failed, for the menu. */
STATIC INTN
Mapped(UINT64 Base, UINT64 Size)
{
    /* Build 13: -14 for both ranges, i.e. the GCD describes them only in part (a descriptor ends before the range
     * does). Walk the range descriptor by descriptor and add only the absent pieces. */
    UINT64 At = Base & ~0xFFFULL, End = (Base + Size + 0xFFFULL) & ~0xFFFULL;
    if (gDS == NULL) return -11;
    while (At < End) {
        EFI_GCD_MEMORY_SPACE_DESCRIPTOR D;
        if (EFI_ERROR(gDS->GetMemorySpaceDescriptor(At, &D)) || D.BaseAddress + D.Length <= At) return -11;
        UINT64 Piece = (D.BaseAddress + D.Length < End ? D.BaseAddress + D.Length : End) - At;
        if (D.GcdMemoryType == EfiGcdMemoryTypeNonExistent) {
            if (EFI_ERROR(gDS->AddMemorySpace(EfiGcdMemoryTypeMemoryMappedIo, At, Piece, EFI_MEMORY_UC))) return -12;
            if (EFI_ERROR(gDS->SetMemorySpaceAttributes(At, Piece, EFI_MEMORY_UC))) return -13;
        }
        At += Piece;
    }
    return 0;
}

STATIC UINT32 Sram(UINT32 Off) { return MmioRead32(B2L_ACPM_SRAM + Off); }
STATIC BOOLEAN InSram(UINT32 Off, UINT32 Bytes) { return Off < B2L_ACPM_SRAM_SIZE && Bytes <= B2L_ACPM_SRAM_SIZE - Off && !(Off & 3); }

/* One request on channel Ch and its reply (Samsung __acpm_ipc_send_data + check_response): Cmd[0..3] in, the reply's
 * first four words out. Only the sequence field (bits 16-21) of Cmd[0] is ours. 0 on a reply, < 0 otherwise. */
STATIC INTN
AcpmXfer(UINT32 Ch, UINT32 Cmd[4])
{
    STATIC UINT32 Seq;
    INTN M = Mapped(B2L_ACPM_SRAM, B2L_ACPM_SRAM_SIZE);
    if (M) return M;                 /* -11..-14: SRAM */
    M = Mapped(B2L_ACPM_MBOX, 0x1000);
    if (M) return M - 10;            /* -21..-24: mailbox */
    UINT32 Chans = Sram(0x7000 + 8), ApMax = Sram(0x7000 + 24);
    if (ApMax <= Ch || ApMax > 32 || !InSram(Chans, 72u * ApMax)) return -2;
    UINT32 C = Chans + 72u * Ch, Ci = C + 16;
    UINT32 Id = Sram(C), TxRear = Sram(Ci), TxFront = Sram(Ci + 4), TxBase = Sram(Ci + 8);
    UINT32 RxRear = Sram(Ci + 24), RxFront = Sram(Ci + 28), RxBase = Sram(Ci + 32), Len = Sram(Ci + 36), Elem = Sram(Ci + 40);
    if (Id >= 16 || Elem < 16 || Elem > 64 || (Elem & 3) || Len < 1 || Len > 64 ||
        !InSram(TxRear, 4) || !InSram(TxFront, 4) || !InSram(TxBase, Elem * Len) ||
        !InSram(RxRear, 4) || !InSram(RxFront, 4) || !InSram(RxBase, Elem * Len))
        return -3;
    if (Sram(RxFront) < Len) MmioWrite32(B2L_ACPM_SRAM + RxRear, Sram(RxFront));   /* drop stale replies */
    UINT32 Front = Sram(TxFront), Next;
    if (Front >= Len) return -4;
    Next = Front + 1 >= Len ? 0 : Front + 1;
    for (UINTN Wait = 0; Next == Sram(TxRear); Wait++) {
        if (Wait > 1500) return -5;   /* 15 ms, as Linux */
        gBS->Stall(10);
    }
    if (++Seq >= 64) Seq = 1;
    Cmd[0] = (Cmd[0] & ~(0x3fu << 16)) | (Seq << 16);
    for (UINT32 I = 0; I < Elem / 4; I++) MmioWrite32(B2L_ACPM_SRAM + TxBase + Elem * Front + 4 * I, I < 4 ? Cmd[I] : 0);
    ArmDataSynchronizationBarrier();
    MmioWrite32(B2L_ACPM_SRAM + TxFront, Next);
    ArmDataSynchronizationBarrier();
    MmioWrite32(B2L_ACPM_MBOX + 0x08 /* INTGR0 */, (1u << Id) << 16);
    for (UINTN Wait = 0; Wait < 5000; Wait++) {   /* 50 ms */
        UINT32 F = Sram(RxFront), R = Sram(RxRear);
        if (F >= Len || R >= Len) return -6;
        for (UINT32 I = R; I != F; I = (I + 1) % Len) {
            UINT32 At = RxBase + Elem * I;
            if (((Sram(At) >> 16) & 0x3f) != Seq) continue;
            for (UINT32 K = 0; K < 4; K++) Cmd[K] = Sram(At + 4 * K);
            R = (R + 1) % Len;
            MmioWrite32(B2L_ACPM_SRAM + RxRear, R);
            if (R == Sram(RxFront)) MmioWrite32(B2L_ACPM_MBOX + 0x20 /* INTCR1 */, 1u << Id);
            return 0;
        }
        gBS->Stall(10);
    }
    return -7;
}

/* S2MPS19 RTC register through the ACPM MFD service: Func 0 read, 1 write. Reply cmd[1] bits 24-31 = APM status,
 * bits 8-15 = the byte read. */
STATIC INTN
PmicRtc(UINT32 Reg, UINT32 Func, UINT32 Arg, UINT32 *Value)
{
    UINT32 Cmd[4] = { (B2L_PMIC_RTC_TYPE << 8) | (Reg & 0xff), (Func & 0xff) | ((Arg & 0xff) << 8), 0, 0 };
    INTN E = AcpmXfer(B2L_ACPM_MFD_CH, Cmd);
    if (E) return E;
    if ((Cmd[1] >> 24) & 0xff) return -8;
    if (Value) *Value = (Cmd[1] >> 8) & 0xff;
    return 0;
}

STATIC INTN
DisarmWtsrSmpl(VOID)
{
    UINT32 V;
    INTN E;
    if ((E = PmicRtc(0x01, 0, 0, &V)) || (E = PmicRtc(0x01, 1, V & ~0xc0u, NULL))) return E;
    if ((E = PmicRtc(0x03, 0, 0, &V)) || (E = PmicRtc(0x03, 1, V & ~0x08u, NULL))) return E - 10;
    return 0;
}

/* Steps 1 and 2; Wtsr / Stop: their results (0 = done), which the menu shows before step 3. */
STATIC VOID
PowerOffPrepare(INTN *Wtsr, INTN *Stop)
{
    UINT32 Cmd[4] = { 1u << 22, 0, 0, 0 };
    *Wtsr = DisarmWtsrSmpl();
    *Stop = AcpmXfer(B2L_ACPM_STOP_CH, Cmd);
}

/* Step 3. Returns only when the phone is still on. */
STATIC VOID
PowerOffHold(VOID)
{
    for (UINTN Try = 0; Try < 3; Try++) {   /* sec_power_off() retries once a second */
        MmioAnd32(B2L_PMU_BASE + B2L_PMU_PS_HOLD, ~B2L_PS_HOLD_DATA);
        gBS->Stall(1000 * 1000);
    }
}

/* Volume up held at start: menu with recovery preselected. Volume down held: menu without countdown. */
STATIC enum b2l_menu_start
ProbeStart(EFI_EVENT Timer)
{
    UINT64 Until = NowMs() + B2L_MENU_PROBE_MS;
    while (NowMs() < Until) {
        enum b2l_menu_key Key = WaitKey(Timer, 50);
        if (Key == B2L_KEY_UP) return B2L_START_FORCE_RECOVERY;
        if (Key == B2L_KEY_DOWN) return B2L_START_FORCE_MENU;
    }
    return B2L_START_NORMAL;
}

B2L_BOOT_CHOICE
B2lBootMenu(CONST CHAR8 *Note, BOOLEAN NoCountdown)
{
    EFI_EVENT Timer = NULL;
    struct b2l_menu Menu;
    B2L_BOOT_CHOICE Choice = B2L_CHOICE_BOOT;

    if (gST->ConIn == NULL || gST->ConOut == NULL) return B2L_CHOICE_BOOT;   /* no keypad: old behaviour */
    /* BDS arms a 5-minute watchdog for applications; "Stay here" must not end in a firmware reset. */
    (VOID)gBS->SetWatchdogTimer(0, 0, 0, NULL);
    if (EFI_ERROR(gBS->CreateEvent(EVT_TIMER, 0, NULL, NULL, &Timer))) Timer = NULL;

    enum b2l_menu_start Start = NoCountdown ? B2L_START_FORCE_MENU : ProbeStart(Timer);
    b2l_menu_init(&Menu, Remembered(), B2L_MENU_TIMEOUT_S * 1000u, Start, NowMs());
    Draw(&Menu, NowMs(), Note, NULL);
    for (;;) {
        enum b2l_menu_key Key = WaitKey(Timer, 100);
        UINT64 Now = NowMs();
        enum b2l_menu_action Action = b2l_menu_event(&Menu, Key, Now);
        if (Action == B2L_ACT_NONE) continue;
        if (Action == B2L_ACT_REDRAW) { Draw(&Menu, Now, Note, NULL); continue; }
        Remember((UINT8)Menu.selected);
        if (Action == B2L_ACT_BOOT) { Choice = B2L_CHOICE_BOOT; break; }
        if (Action == B2L_ACT_HOLD) {
            Draw(&Menu, Now, Note, "Not booting. Press any key for the menu.");
            while (WaitKey(Timer, 1000) == B2L_KEY_NONE) {}
            b2l_menu_init(&Menu, Menu.selected, 0, B2L_START_FORCE_MENU, NowMs());
            Draw(&Menu, NowMs(), Note, NULL);
            continue;
        }
        if (Action == B2L_ACT_POWEROFF) {
            STATIC CHAR8 Still[96];
            STATIC CHAR8 Steps[64];
            INTN Wtsr, Stop;
            Draw(&Menu, Now, Note, "Switching off...");
            PowerOffPrepare(&Wtsr, &Stop);
            /* On screen for 3 s before PS_HOLD, so a photo tells which step failed if the phone restarts. */
            AsciiSPrint(Steps, sizeof(Steps), "Power off: wtsr %d, acpm %d", (INT32)Wtsr, (INT32)Stop);
            Draw(&Menu, NowMs(), Steps, "Switching off...");
            gBS->Stall(3 * 1000 * 1000);
            PowerOffHold();
            /* Still here (on a charger the PMIC keeps the supply): back to the menu, no countdown, with the
             * ACPM results so a photo says which step did not take. */
            AsciiSPrint(Still, sizeof(Still), "Still on (charger connected?). Unplug it and try again. [wtsr %d acpm %d]",
                        (INT32)Wtsr, (INT32)Stop);
            b2l_menu_init(&Menu, Menu.selected, 0, B2L_START_FORCE_MENU, NowMs());
            Draw(&Menu, NowMs(), Still, NULL);
            continue;
        }
        Draw(&Menu, Now, Note, Action == B2L_ACT_RECOVERY ? "Rebooting to recovery..." : "Rebooting...");
        Reset(Action == B2L_ACT_RECOVERY);
        b2l_menu_init(&Menu, Menu.selected, 0, B2L_START_FORCE_MENU, NowMs());
    }
    if (Timer) gBS->CloseEvent(Timer);
    gST->ConOut->ClearScreen(gST->ConOut);
    return Choice;
}

VOID
B2lBootFailed(EFI_STATUS Status)
{
    Print(L"\r\nBoot failed: %r. The messages above say where.\r\n", Status);
    if (gST->ConIn == NULL) return;
    Print(L"Press any key to return to the menu.\r\n");
    while (WaitKey(NULL, 1000) == B2L_KEY_NONE) {}
}
