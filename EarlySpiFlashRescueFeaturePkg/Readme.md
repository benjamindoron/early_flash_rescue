# Overview
* **Feature Name:** Early SPI Flash Rescue
* **PI Phase(s) Supported:** PEI
* **SMM Required?** No

More Information:
* [GitHub repo](https://github.com/benjamindoron/early_flash_rescue)

## Purpose
The Early SPI Flash Rescue feature enables the use of a serial port to
rescue systems from broken firmware by delivering an update image to flash.
With an appropriate serial port library, this is an important capability in
firmware development, as debugging broken images on laptops is inefficient
and wastes the developer's time on tedium. Now, simply flash the image under
test, retrieve the broken debug log, then reflash another image. Even before
permanent memory, it is assumed that the right serial port could complete
this task in a few minutes. Presently, optimisation is underway to reduce the
time for even slow serial ports down from 30 minutes and make this less a
mere proof of concept.

The feature is intended to be used with the userspace-side [here](https://github.com/benjamindoron/early_flash_rescue/tree/main/flash_rescue_userspace)
presently implemented for POSIX Linux.

# High-Level Theory of Operation
*_TODO_*
A description of how the device works at a high-level.

The description should not be constrained to implementation details but provide a simple mental model of how the
feature is supposed to work.

## Firmware Volumes
It is the user's responsibility to configure where the module is located.
PEI APRIORI (MinPlatform PreMem BootFV) is recommended, but post-mem FVs
are also expected.

## Modules
* EarlyFlashRescueBoardPei

## <Module Name>
*_TODO_*
Each module in the feature should have a section that describes the module in a level of detail that is useful
to better understand the module source code.

## <Library Name>
*_TODO_*
Each library in the feature should have a section that describes the library in a level of detail that is useful
to better understand the library source code.

## Key Functions
*_TODO_*
A bulleted list of key functions for interacting with the feature.

Not all features need to be listed. Only functions exposed through external interfaces that are important for feature
users to be aware of.

## Configuration
*_TODO_*
Information that is useful for configuring the feature.

Not all configuration options need to be listed. This section is used to provide more background on configuration
options than possible elsewhere.

## Data Flows
*_TODO_*
Architecturally defined data structures and flows for the feature.

## Control Flows
*_TODO_*
Key control flows for the feature.

## Build Flows
*_TODO_*
Any special build flows should be described in this section.

This is particularly useful for features that use custom build tools or require non-standard tool configuration. If the
standard flow in the feature package template is used, this section may be empty.

## Test Point Results
No test points implemented

## Functional Exit Criteria
*_TODO_*
The testable functionality for the feature.

This section should provide an ordered list of criteria that a board integrator can reference to ensure the feature is
functional on their board.

## TODO: Feature Enabling Checklist
* In the board DSC file, enable the feature
```
[PcdsFeatureFlag]
  gUsb3DebugFeaturePkgTokenSpaceGuid.PcdUsb3DebugFeatureEnable|TRUE
```
* In the board DSC file, select the implementation desired
```
[PcdsFixedAtBuild]
  # 0 = Non-functional instance
  # 1 = Regular instance
  # 2 = IO MMU instance
  gUsb3DebugFeaturePkgTokenSpaceGuid.PcdUsb3DebugPortLibInstance|1
```
* In the board DSC file, configure the PCI device information
```
[PcdsFixedAtBuild]
  gUsb3DebugFeaturePkgTokenSpaceGuid.PcdUsbSerialXhciBus|0x00
  gUsb3DebugFeaturePkgTokenSpaceGuid.PcdUsbSerialXhciDev|0x14
  gUsb3DebugFeaturePkgTokenSpaceGuid.PcdUsbSerialXhciFunc|0x00
  gUsb3DebugFeaturePkgTokenSpaceGuid.PcdXhciDefaultBaseAddress|0xFEA10000
```


## Performance Impact
A general expectation for the impact on overall boot performance due to using this feature.

This section is expected to provide guidance on:
* How to estimate performance impact due to the feature
* How to measure performance impact of the feature
* How to manage performance impact of the feature

## TODO: Common Optimizations
* In the board DSC file, tune the timeout value
```
[PcdsFixedAtBuild]
  gUsb3DebugFeaturePkgTokenSpaceGuid.PcdXhciHostWaitTimeout|2000000
```
