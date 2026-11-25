/* SPDX-License-Identifier: BSD-2-Clause
 * "Boot Windows" in the boot menu: dual boot with Windows 11 ARM64 (exynos9810-woa, ported to the S10e).
 *
 * Windows Boot Manager is an ordinary UEFI application on the Windows EFI system partition (planned: the cache
 * partition, FAT32), as bootmgfw.efi or, on install media, bootaa64.efi. It is looked up on every FAT volume the
 * firmware mounted and started with LoadImage/StartImage,
 * as the firmware's own boot manager would. Nothing here writes a partition, and iOS is untouched: the menu's
 * countdown still boots iOS, so Windows only starts when chosen.
 *
 * No watchdog is armed for Windows (the UEFI boot manager rule is 5 minutes): this firmware's ResetSystem does not
 * reset the 9820 (docs/boot-menu.md), so a watchdog expiry would only hang, and a hang on screen is easier to read.
 */
#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include "WindowsBoot.h"
#include "WindowsSmp.h"

/* An installed Windows has the first; Windows install media (an ISO's efi folder) only the second, the same
 * Windows Boot Manager under the removable-media name. Both read \EFI\Microsoft\Boot\BCD. */
STATIC CONST CHAR16 *CONST mBootManagers[] = {
    L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi",
    L"\\EFI\\Boot\\bootaa64.efi",
};

STATIC BOOLEAN
HasFile(EFI_HANDLE Volume, CONST CHAR16 *Path)
{
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
    EFI_FILE_PROTOCOL *Root, *File;
    EFI_STATUS Status;
    if (EFI_ERROR(gBS->HandleProtocol(Volume, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs)) ||
        EFI_ERROR(Fs->OpenVolume(Fs, &Root)))
        return FALSE;
    Status = Root->Open(Root, &File, (CHAR16 *)Path, EFI_FILE_MODE_READ, 0);
    if (!EFI_ERROR(Status)) File->Close(File);
    Root->Close(Root);
    return !EFI_ERROR(Status);
}

EFI_STATUS
B2lBootWindows(EFI_HANDLE ImageHandle)
{
    EFI_HANDLE *Volumes = NULL, Volume = NULL, Image = NULL;
    EFI_DEVICE_PATH_PROTOCOL *Path;
    CONST CHAR16 *Manager = NULL;
    UINTN Count = 0;
    EFI_STATUS Status;

    gST->ConOut->ClearScreen(gST->ConOut);
    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &Count, &Volumes);
    if (EFI_ERROR(Status)) Count = 0;
    for (UINTN M = 0; M < ARRAY_SIZE(mBootManagers) && Volume == NULL; M++)
        for (UINTN I = 0; I < Count && Volume == NULL; I++)
            if (HasFile(Volumes[I], mBootManagers[M])) { Volume = Volumes[I]; Manager = mBootManagers[M]; }
    if (Volumes) FreePool(Volumes);
    if (Volume == NULL) {
        Print(L"Windows: neither %s nor %s is on the %u FAT volumes.\r\n",
              mBootManagers[0], mBootManagers[1], (UINT32)Count);
        Print(L"The Windows EFI system partition is not set up yet.\r\n");
        return EFI_NOT_FOUND;
    }

    /* Secondaries into parking-protocol mailboxes (WindowsSmp.c): Windows' own PSCI CPU_ON resets the 9820. */
    B2lWindowsParkSecondaries();

    Path = FileDevicePath(Volume, Manager);
    if (Path == NULL) return EFI_OUT_OF_RESOURCES;
    Print(L"Starting Windows Boot Manager (%s)...\r\n", Manager);
    Status = gBS->LoadImage(FALSE, ImageHandle, Path, NULL, 0, &Image);
    FreePool(Path);
    if (EFI_ERROR(Status)) {
        Print(L"Windows: LoadImage of %s failed: %r\r\n", Manager, Status);
        return Status;
    }
    /* Windows Boot Manager returns only when it gives up (its own screen said why); an application is unloaded by
     * StartImage when it returns, so there is nothing to free. */
    Status = gBS->StartImage(Image, NULL, NULL);
    Print(L"Windows Boot Manager returned: %r\r\n", Status);
    return EFI_ERROR(Status) ? Status : EFI_ABORTED;
}
