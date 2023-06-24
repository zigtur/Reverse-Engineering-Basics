# Practical Reverse Engineering
Authors: Bruce Dang, Alexandre Gazet and Elias Bachaalany
Contributions: Sébastien Josse

This book is divided in 5 chapters:
- [x86 and x64](#x86-and-x64)
- ARM
- Windows Kernel
- Debugging and Automation
- Obfuscation


## x86 and x64

x86 is a little-endian architecture based on the Intel 8086 processor. It is the 32-bit implementation of the Intel architecture (IA-32). x64 or x86-64 is the 64-bit extension of the architecture.

It can operate in two modes:
- **real**: 
    - It is the processor state when powered on.
    - It only supports 16-bit instructions.
- **protected**: 
    - It is the processor state that supports virtual memory, paging and more features.
    - Modern operating systems execute in this mode.

The book focuses on the **protected** mode of the x86 architecture.

x86 is known for its **ring levels**, a sort of abstraction of the privilege separation concept. It supports 4 ring levels:
- **0**: 
    - The highest privilege level
    - Kernels are executed at this level
- **1**: 
    - Not commonly used
- **2**: 
    - Not commonly used
- **3**: 
    - The lowest privilege level
    - "User-mode" terms are usually used
    - Apps are executed in this state
    - Apps have access to a limited subset of settings

The ring level is encoded in the `CS` register, and can be referred to as the **CPL** (Current Privilege Level).

### Register Set and Data Types
#### General Purpose Registers (GPRs)
In protected mode, there are 8 general purpose registers of 32 bits. Some of them can be divided into 8-bit and 16-bit register:
- `EAX`, the same pattern apply to `EBX`, `ECX` and `EDX`
    - `AX` corresponds to the 16 less significant bits of the register (bit 0 to 15)
        - `AH` corresponds to the bit 8 to 15. (The most significant bits of `AX`)
        - `AL` corresponds to the bit 0 to 7. (The less significant bits of `AX`)
- `EBP`
    - `BP` corresponds to the 16 less significant bits of the register
- `ESP`
    - `SP` corresponds to the 16 less significant bits of the register
- `ESI`
    - `SI` corresponds to the 16 less significant bits of the register
- `EDI`
    - `DI` corresponds to the 16 less significant bits of the register

Here are some usage of those General Purpose Registers (GPRs):

| Register   |  Usage                                   |
|------------|------------------------------------------|
|   ECX      |  Counter in loops                        |
|   ESI      |  Source in string/memory operations      |
|   EDI      |  Destination in string/memory operations |
|   EBP      |  Base frame pointer                      |
|   ESP      |  Stack pointer                           |

The common data types used are:
- Bytes (8 bits): `AL`, `BL`, `CL` or `DL` can be used for example
- Word (16 bits): `AX`, `BX`, `CX` or `DX` can be used for example
- Double Word (32 bits): `EAX`, `EBX`, `ECX` or `EDX` can be used for example
- Quad Word (64 bits): With x86, 64-bit values can be treated by combining to registers, usually `EDX:EAX`. `RDTSC` is an instruction that writes 64-bit value to `EDX:EAX`.

#### EIP and EFLAGS

The `EFLAGS` 32-bit register is a special one. It stores the status of arithmetic operations and execution states. For example, `ZF` ("Zero Flag") can be used to verify if an operation resulted in a zero. It is mainly used for conditional branching.


The `EIP` 32-bit register is the instruction pointer. It points to the next instruction to execute.

#### Other registers

There exist others registers than GPRs, which control some important low-level system mechanisms (virtual memory, interrupts, debugging). Here is a non-exhaustive list of registers and their usage:
- `CR0`: controls wether paging is on or off
- `CR2`: contains the linear address that caused a page fault
- `CR3`: the base address of a paging data structure
- `CR4`: controls the hardware virtualization settings
- `DR0-DR7`: used to set memory breakpoints. In fact, only `DR0-DR3` are used for breakpoints, remaining ones are used for status.

#### Model-Specific Registers
MSRs are registers that may vary between different processors. It can be used through the `RDMSR` and `WRMSR` instructions. Only ring 0 code can access it, it is mainly used to store special counters and for low-level functionalities.

For example, `SYSENTER` instruction transfers execution to the address stored in the `IA32_SYSENTER_EIP` register (0x176). It is usually the system call handler.

### Instruction Set
x86 instructions is flexible for data movement. Movement can be caracterized into 5 methods:
- Immediate to Register
- Register to Register
- Immediate to Memory
- Register to Memory and Memory to Register
- Memory to Memory

All modern architectures support the first 4 methods. But "Memory to Memory" is x86 specific.
In a ARM architecture (RISC), manipulating memory will be done by load/store instructions. Let's compare x86 and ARM on a simple example which is incrementing a value in memory:
- ARM:
    - `LDR`: read data from memory to a register
    - `ADD`: add one to the register
    - `STR`: write the register to memory
- x86:
    - `INC` or `ADD`: directly access memory and increment it (or add one)

x86 uses variable-length instruction size, the instruction length can range from 1 to 15 bytes. ARM instructions are 2 or 4 bytes long.

There are two main syntax for assembly code (Intel and AT&T). Nowadays, Intel is most widely used.

#### Data Movement
For moving data, the most common instruction is `MOV`. x86 uses square brackets [] to indicate memory access. (Exception: `LEA` instruction uses it, but does not reference memory).

```assembly
; set the memory at address EAX to 1
C7 00 01 00 00+ mov dword ptr [eax], 1
; set ECX to the value at address EAX
8B 08 mov ecx, [eax]
; set the memory at address EAX to EBX
89 18 mov [eax], ebx
; set the memory address at (ESI+34) to EAX
89 46 34 mov [esi+34h], eax
; set EAX to the value at address (EAX+34)
8B 46 34 mov eax, [esi+34h]
; set EDX to the value at address (ECX+EAX)
8B 14 01 mov edx, [ecx+eax]
```
In pseudo C, it corresponds to:
```c
*eax = 1;
ecx = *eax;
*eax = ebx;
*(esi+34) = eax;
eax = *(esi+34);
edx = *(ecx+eax);
```

A `MOV` instruction in assembly has multiple "machine code" corresponding values. `MOV` can work with byte, word and double-word. A prefix can be set to identify which length should `MOV` use.

Operations can be used inside brackets, to loop over an array for example. Here is an example code:
```assembly
01: loop_start:
02: 8B 47 04 mov eax, [edi+4]           // reads dword from memory [edi+4] = base address
03: 8B 04 98 mov eax, [eax+ebx*4]       // get value from base address + ebx*4 (ebx is an index)
04: 85 C0 test eax, eax
...
05: 74 14 jz short loc_7F627F
06: loc_7F627F:
07: 43 inc ebx                          // incremente the index
08: 3B 1F cmp ebx, [edi]                // compare ebx to the value stored in memory [edi] (should be the size)
09: 7C DD jl short loop_start
```

##### MOVSB, MOVSW, MOVSD 
`MOVSB`, `MOVSW`, `MOVSD` instructions move data with 1-, 2-, or 4-byte granularity between two memory addresses. They implicitly use `EDI` as destination and `ESI` as source. They automatically update the addresses in `EDI` and `ESI`, depending on `DF` ("Direction Flag"). If it is 0, addresses are decremented, incremented if 1. These instructions are typically used to implement memory copy functions, when length is known at compile time. They can be prefixed by `REP`, which will repeat an instruction `ECX` times.

Example:
*Note: `LEA` instruction does not read from a memory address, even if the syntax can let us think it does (misleading syntax).*
```assembly
; ESI = pointer to RamdiskBootDiskGuid
01: BE 28 B5 41 00 mov esi, offset _RamdiskBootDiskGuid
; EDI is an address somewhere on the stack
02: 8D BD 40 FF FF+ lea edi, [ebp-0C0h]
; copies 4 bytes from EDI to ESI; increment each by 4
03: A5 movsd
; same as above
04: A5 movsd
; save as above
05: A5 movsd
; same as above
06: A5 movsd
```
It corresponds to the pseudo-C:
```c
/* a GUID is 16-byte structure */
GUID RamDiskBootDiskGuid = ...; // global
...
GUID foo;
memcpy(&foo, &RamdiskBootDiskGuid, sizeof(GUID));
```

Here is an example with `REP` instruction:
```assembly
01: 6A 08 push 8 ; push 8 on the stack (will explain stacks
 ; later)
02: ...
03: 59 pop ecx ; pop the stack. Basically sets ECX to 8.
04: ...
05: BE 00 44 61 00 mov esi, offset _KeServiceDescriptorTable
06: BF C0 43 61 00 mov edi, offset _KeServiceDescriptorTableShadow
07: F3 A5 rep movsd ; copy 32 bytes (movsd repeated 8 times)
; from this we can deduce that whatever these two objects are, they are
; likely to be 32 bytes in size.
```

`MOVSD` is mainly used when copying a lot of bytes. If the copy size is not a multiple of 4, then `MOVSW` and `MOVSB` will be used to fit the copy size.

##### SCAS, STOS and LODS
`SCAS` implicitly compares `AL`, `AX` or `EAX` with data starting at the memory address in `EDI`, which is automatically incremented or decremented (based on `DF`). This can be used with `strlen()` function, for example:
```assembly
; set AL to 0 (NUL byte). You will frequently observe the XOR reg, reg
; pattern in code.
01: 30 C0 xor al, al
; save the original pointer to the string
02: 89 FB mov ebx, edi
; repeatedly scan forward one byte at a time as long as AL does not match the
; byte at EDI when this instruction ends, it means we reached the NUL byte in
; the string buffer
03: F2 AE repne scasb
; edi is now the NUL byte location. Subtract that from the original pointer
; to the length.
04: 29 DF sub edi, ebx
```

`STOS` is the same as `SCAS`, except it writes the value `AL`, `AX` or `EAX` to the address in `EDI`. It is commonly used to initialize a buffer to a constant value, like memset does. Example:
```assembly
; set EAX to 0
01: 33 C0 xor eax, eax
; push 9 on the stack
02: 6A 09 push 9
; pop it back in ECX. Now ECX = 9.
03: 59 pop ecx
; set the destination address
04: 8B FE mov edi, esi
; write 36 bytes of zero to the destination buffer (STOSD repeated 9 times)
; this is equivalent lent to memset(edi, 0, 36)
05: F3 AB rep stosd
```

`LODS` is an instruction that reads a 1-, 2-, or 4-byte value from `ESI` and stores it in `AL`, `AX` or `EAX`.

##### Exercise

See [Data Movement - Exercise](Exercises/README.md#exercise)

### Arithmetic Operations

## ARM


## Windows Kernel



## Debugging and Automation


## Obfuscation



