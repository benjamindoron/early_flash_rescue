# Design Document - Early SPI flash rescue
### (Bus Pirate implementation; fairly portable)
## Protocol
Command description:
```c
typedef struct {
  UINT8   Command;
  UINT16  BlockNumber;  // This 4K block in BIOS region
} EARLY_FLASH_RESCUE_COMMAND;
```

Response description (potentially optional for some commands, presumed necessary between packets of a 4K block):
- Consider error-handling with an appended response transfer.
```c
typedef struct {
  UINT8   Acknowledge;  // Usually, ACK == 0x01
  UINT16  Size;         // OPTIONAL?
} EARLY_FLASH_RESCUE_RESPONSE;
```

Commands:
1. **0x10 - HELLO**: Board indicates presence for user-space acknowledgement
    - This is one command that board initiates (though flow can be reversed)
2. **0x11 - CHECKSUM**: Userspace requests the CRC32 of a 4K `BlockNumber`
2. **0x12 - READ**: Reserved
3. **0x13 - WRITE**: Userspace instructs to write a 4K `BlockNumber`
    - NOTE: Potential implementation-layer buffers might be limited. Therefore, this protocol might transfer blocks in permissibly-sized packets
4. **0x14 - RESET**: Userspace verification determines that blocks have changed and the board requires a (cold) reset
4. **0x15 - EXIT**: Userspace breaks the board's polling loop


## Implementation
### User-space side
1. Parse arguments (BIOS FD; serial device; implementation mode)
    - Open the BIOS file and serial device OR exit
2. Enter the debug port (TODO: Can send F12 special key?)
3. Initiate wait-for-`HELLO` loop AND acknowledge
4. Initiate flash-loop
    - Calculate number of blocks
    - Pick our *this* block and checksum
    - Request checksum of *this* block AND acknowledge and read response
    - IF mismatched, write *this* block. Await acknowledgement, then stream data
    - Next block
    - NOTE: Verification is optional
5. Close files

### Bus Pirate side
No immediately required modifications anticipated
- Consider using `HELLO` to disable escape keys

### Board side
1. Initiate `HELLO` loop UNTIL read acknowledgement OR (timeout AND exit)
2. Initiate polling loop
    - When there is data, call helpers
3. Return?

**TODO**:
- Consider error-flow: Non-interactive verify while avoiding infinite loop
- Userspace can request a cold reset or exit?


## Notes
1. PEI phase is chosen over SEC to gain access to the SPI libraries
    - Therefore, go APRIORI
    - However, as XIP code, imposed by CAR mode, writing over this PEIM's blocks might be misbehaviour. Either:
      - This is actually obviated by the backing of the SPI by cache. Or:
      - Reload this module into CAR. Remember to reinstall shared PPIs. Alternatively, SEC might be an option.
      - (Initial PEI testing can be performed in permanent memory with `RegisterForShadow()`)
    - Might want to take MinPlatform's FDs. There's a penalty of waiting until ReportFvPei
2. Full IFWI rescue could be implemented, but it's unlikely to be useful
    - There are few cases where the CSME region can be written
    - CSME must be in at least a tolerable error state to enter PEI APRIORI
3. Other checksumming algorithms can be considered, though CRC32 is still presumed sufficient for 4K blocks
    - Requirements are to be fairly simple
      - Possibility of collision should be low, it would result in corruption by skipping the block. However, developers are expected to have a flash programmer
    - Security should be implemented elsewhere with proper verification, such as Boot Guard. Also, this a debugging feature
4. Consider modularising the user-space implementation
5. Suggested optimisation: Only perform erase if a zero-to-one is required per block
