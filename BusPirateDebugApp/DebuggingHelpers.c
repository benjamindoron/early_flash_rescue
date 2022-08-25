/** @file
  Early SPI flash rescue protocol - debugging helpers

  Copyright (c) 2022, Baruch Binyamin Doron<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Base.h>
#include <Library/UefiLib.h>

VOID
EFIAPI
InternalPrintData (
  IN UINT8   *Data8,
  IN UINTN   DataSize
  )
{
  UINTN      Index;

  for (Index = 0; Index < DataSize; Index++) {
    if (Index % 0x10 == 0) {
      Print (L"\n%08X:", Index);
    }
    Print (L" %02X", *Data8++);
  }
  Print (L"\n");
}
