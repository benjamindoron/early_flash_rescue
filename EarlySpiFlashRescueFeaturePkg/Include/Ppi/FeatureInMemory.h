/** @file
  This file declares that this feature is relocated into memory from XIP.

  This PPI is published by the feature PEIM after relocation. There is no
  interface, it is used as a global variable. This is necessary to mitigate
  possible bugs from flashing over XIP code.

  Copyright (c) 2022, Baruch Binyamin Doron.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FEATURE_IN_MEMORY_H__
#define __FEATURE_IN_MEMORY_H__

#define PEI_FLASH_RESCUE_READY_IN_MEMORY_PPI_GUID \
  { \
    0xe5147285, 0x4d34, 0x415e, {0x8e, 0xa8, 0x85, 0xbd, 0xd8, 0xc6, 0x5b, 0xde } \
  }

extern EFI_GUID  gPeiFlashRescueReadyInMemoryPpiGuid;

#endif
