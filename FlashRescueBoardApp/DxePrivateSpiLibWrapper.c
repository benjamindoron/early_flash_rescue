/** @file
  PCH SPI DXE Library implements the SPI Host Controller Interface.

Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <IndustryStandard/Pci30.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PciSegmentLib.h>
#include <Library/SpiLib.h>

#include <Protocol/Spi2.h>
#include <PchReservedResources.h>
#include <Register/PchRegsSpi.h>

#include <Library/PchSpiCommonLib.h>

SPI_INSTANCE  *mSpiInstance;


/**
  Initializes SPI for access from future services.

  @retval EFI_SUCCESS         The SPI service was initialized successfully.
  @retval EFI_OUT_OF_RESOUCES Insufficient memory available to allocate structures required for initialization.
  @retval Others              An error occurred initializing SPI services.

**/
EFI_STATUS
EFIAPI
SpiServiceInit (
  VOID
  )
{
  EFI_STATUS  Status;

  ///
  /// Allocate pool for SPI protocol instance
  ///
  Status = gBS->AllocatePool (
                 EfiBootServicesData, /// Maybe care
                 sizeof (SPI_INSTANCE),
                 (VOID **) &mSpiInstance
                 );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (mSpiInstance == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  ZeroMem ((VOID *) mSpiInstance, sizeof (SPI_INSTANCE));
  ///
  /// Initialize the SPI protocol instance
  /// - NOTE: Initialise instance. Functions are called manually
  ///
  Status = SpiProtocolConstructor (mSpiInstance);
  if (EFI_ERROR (Status)) {
    gBS->FreePool (mSpiInstance);
    return Status;
  }

  return EFI_SUCCESS;
}

VOID
EFIAPI
SpiServiceDeInit (
  VOID
  )
{
  gBS->FreePool (mSpiInstance);
}

/**
  Acquires the PCH SPI BAR0 MMIO address.

  @param[in] SpiInstance          Pointer to SpiInstance to initialize

  @retval    UINTN                The SPIO BAR0 MMIO address

**/
UINTN
AcquireSpiBar0 (
  IN  SPI_INSTANCE                *SpiInstance
  )
{
  return MmioRead32 (SpiInstance->PchSpiBase + R_PCH_SPI_BAR0) & ~(B_PCH_SPI_BAR0_MASK);
}

/**
  Release the PCH SPI BAR0 MMIO address.

  @param[in] SpiInstance          Pointer to SpiInstance to initialize

  @retval None
**/
VOID
ReleaseSpiBar0 (
  IN  SPI_INSTANCE                *SpiInstance
  )
{
  return;
}

/**
  Disables BIOS Write Protect

  @retval EFI_SUCCESS             BIOS Write Protect was disabled successfully

**/
EFI_STATUS
EFIAPI
DisableBiosWriteProtect (
  VOID
  )
{
  UINT64  SpiBaseAddress;

  SpiBaseAddress =  PCI_SEGMENT_LIB_ADDRESS (
                      0,
                      0,
                      PCI_DEVICE_NUMBER_PCH_SPI,
                      PCI_FUNCTION_NUMBER_PCH_SPI,
                      0
                      );

  //
  // Set BIOSWE bit (SPI PCI Offset DCh [0]) = 1b
  // Enable the access to the BIOS space for both read and write cycles
  //
  PciSegmentOr8 (
    SpiBaseAddress + R_PCH_SPI_BC,
    B_PCH_SPI_BC_WPD
    );

  return EFI_SUCCESS;
}

/**
  Enables BIOS Write Protect

**/
VOID
EFIAPI
EnableBiosWriteProtect (
  VOID
  )
{
  UINT64  SpiBaseAddress;

  SpiBaseAddress =  PCI_SEGMENT_LIB_ADDRESS (
                      0,
                      0,
                      PCI_DEVICE_NUMBER_PCH_SPI,
                      PCI_FUNCTION_NUMBER_PCH_SPI,
                      0
                      );

  //
  // Disable the access to the BIOS space for write cycles
  //
  PciSegmentAnd8 (
    SpiBaseAddress + R_PCH_SPI_BC,
    (UINT8) (~B_PCH_SPI_BC_WPD)
    );
}
