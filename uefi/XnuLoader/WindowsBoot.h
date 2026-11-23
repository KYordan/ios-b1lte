/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef B2L_WINDOWS_BOOT_H
#define B2L_WINDOWS_BOOT_H

#include <Uefi.h>

/* Starts Windows Boot Manager (\EFI\Microsoft\Boot\bootmgfw.efi, else install media's \EFI\Boot\bootaa64.efi) from
 * the first FAT volume that has it. Returns
 * only when it is missing, refused by LoadImage, or came back from StartImage; the message is on screen. */
EFI_STATUS B2lBootWindows(EFI_HANDLE ImageHandle);

#endif
