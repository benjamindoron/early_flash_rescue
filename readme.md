# Design Document - Early SPI flash rescue
### (Bus Pirate implementation)
## Protocol
Command description:
```c
typedef struct {
  UINT8   Command;
  UINT16  BlockNumber;  // This 4K block in BIOS region
} EARLY_FLASH_RESCUE_COMMAND;
```

In all cases, arbitrary data will then be streaming across. To ensure more sanity, consider prepending the following transfer (NOTE: also consider races):
```c
typedef struct {
  UINT8   Acknowledge;  // Usually, ACK == 0x01
  UINT16  Size;         // OPTIONAL?
} EARLY_FLASH_RESCUE_RESPONSE;
```

Consider error-handling with an appended response transfer.

Commands:
1. **0x00 - HELLO**: Board indicates presence for user-space acknowledgement
    - This is one command that board initiates
    - Ignore `BlockNumber` and `Size`
2. **0x01 - CHECKSUM**: Request a CRC32
2. **0x02 - READ**: Reserved
3. **0x03 - WRITE**: Write a flash address
    - NOTE: Are the multiple protocol-level blocks handled at the implementation-level underneath this?
4. **0x04 - EXIT**: Reserved


## Implementation
### User-space side
1. Parse arguments (BIOS FD; Bus Pirate device)
    - Open the BIOS file and Bus Pirate device OR exit
2. Enter the debug port (TODO: Reset the Bus Pirate?)
3. Initiate wait-for-`HELLO` loop AND acknowledge
4. Initiate flash-loop
    - Calculate number of blocks
    - Pick our *this* block and checksum
    - Request checksum of *this* block AND acknowledge and read response
    - IF mismatched, write *this* block. Await acknowledgement, then stream data
    - Next block
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
1. PEI phase is chosen over SEC to gain access the SPI libraries
    - Therefore, go APRIORI
    - Might want to take MinPlatform's FDs. There's a penalty of waiting until ReportFvPei
2. Full IFWI rescue could be implemented, but it's unlikely to be useful
    - There are few cases where the CSME region can be written
    - CSME must be in at least a tolerable error state to enter PEI APRIORI
3. Other checksumming algorithms can be considered
    - Requirements are to be fairly simple
      - Possibility of collision should be low, it would result in corruption by skipping the block. However, developers are expected to have a flash programmer
    - Security should be implemented elsewhere with proper verification, such as Boot Guard. Also, this a debugging feature
4. Consider modularising the user-space implementation
5. Suggested optimisation: Only perform erase if a zero-to-one is required per block
