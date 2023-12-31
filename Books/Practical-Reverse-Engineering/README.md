# Practical Reverse Engineering
Authors: Bruce Dang, Alexandre Gazet and Elias Bachaalany
Contributions: Sébastien Josse

This book is divided in 5 chapters:
- [x86 and x64](#x86-and-x64)
    - [Register set and Data types](#register-set-and-data-types)
    - [Instruction set](#instruction-set)
    - [System Mechanism](#system-mechanism)


- [ARM](#arm)
- [Windows Kernel](#windows-kernel)
- [Debugging and Automation](#debugging-and-automation)
- [Obfuscation](#obfuscation)


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

#### Arithmetic Operations
Most of the supported arithmetic operations are straight forward. Here is a non-exhaustive list of instructions for arithmetic operations:
- `add`: addition
- `sub`: substraction
- `inc`: increment (add one)
- `dec`: decrement (sub one)
- `or`: logic or, bit-level
- `xor`: logic xor, bit-level
- `and`: logic and, bit-level
- `not`: logic not, bit-level
- `shl`: shift left, bit-level
- `shr`: shift right, bit-level
- `rol`: rotate left
- `ror`: rotate right

`shr` and `shl` are frequently used to optimize multiplication and division, when it is possible (power of two).

Multiplication is done with `mul` (unsigned) and `imul` (signed) instructions. The form of `mul` is `mul reg` or `mul mem`. Only one argument is used, because the other one is `EAX`, `AX` or `AL`. The result is respectively stored in `EAX:EDX`, `DX:AX`, or `AX`. For example:
```assembly
01: B8 03 00 00 00 mov eax,3                ; set EAX=3

02: B9 22 22 22 22 mov ecx,22222222h        ; set ECX=0x22222222

03: F7 E1 mul ecx                           ; EDX:EAX = 3 * 0x22222222 =
                                            ; 0x66666666
                                            ; hence, EDX=0, EAX=0x66666666

04: B8 03 00 00 00 mov eax,3                ; set EAX=3

05: B9 00 00 00 80 mov ecx,80000000h        ; set ECX=0x80000000

06: F7 E1 mul ecx                           ; EDX:EAX = 3 * 0x80000000 =
                                            ; 0x180000000
                                            ; hence, EDX=1, EAX=0x80000000
```
The result can't always fit in one register, so it is stored in `EDX:EAX` for 32-bit multiplication.

`imul` has three forms:
- `imul reg/mem`: Same as `mul`
- `imul reg1, reg2/mem`: reg1 = reg1 * reg2/mem
- `imul reg1, reg2/mem, imm`: reg1 = reg2/mem * imm

The same pattern does apply to `div` and `idiv`. Here is an example:
```assembly
01: F7 F1 div ecx                       ; EDX:EAX / ECX, quotient in EAX,

02: F6 F1 div cl                        ; AX / CL, quotient in AL, remainder in AH

03: F7 76 24 div dword ptr [esi+24h]    ; see line 1

04: B1 02 mov cl,2                      ; set CL = 2

05: B8 0A 00 00 00 mov eax,0Ah          ; set EAX = 0xA

06: F6 F1 div cl                        ; AX/CL = A/2 = 5 in AL (quotient),
                                        ; AH = 0 (remainder)

07: B1 02 mov cl,2                      ; set CL = 2

08: B8 09 00 00 00 mov eax,09h          ; set EAX = 0x9

09: F6 F1 div cl                        ; AX/CL = 9/2 = 4 in AL (quotient),
                                        ; AH = 1 (remainder)
```

#### Stack Operations and Function Invocation
##### PUSH and POP
A stack is a contguous memory region pointed to by `ESP` and it grows downwards. `PUSH` and `POP` instructions implicitly modifies `ESP`. `PUSH` decrements `ESP` to allocate space, and then writes data to the allocated space. `POP` reads the data and then increments `ESP` to delete the allocated space. OS require the stack to be double-word aligned, so the increment/decrement value is 4 most of the time. Moreover, the `ESP` register can be manipulated with `ADD` and `SUB`.

```assembly
; initial ESP = 0xb20000
01: B8 AA AA AA AA mov eax,0AAAAAAAAh
02: BB BB BB BB BB mov ebx,0BBBBBBBBh
03: B9 CC CC CC CC mov ecx,0CCCCCCCCh
04: BA DD DD DD DD mov edx,0DDDDDDDDh
05: 50 push eax
; address 0xb1fffc will contain the value 0xAAAAAAAA and ESP
; will be 0xb1fffc (=0xb20000-4)
06: 53 push ebx
; address 0xb1fff8 will contain the value 0xBBBBBBBB and ESP
; will be 0xb1fff8 (=0xb1fffc-4)
07: 5E pop esi
; ESI will contain the value 0xBBBBBBBB and ESP will be 0xb1fffc
; (=0xb1fff8+4)
08: 5F pop edi
; EDI will contain the value 0xAAAAAAAA and ESP will be 0xb20000
; (=0xb1fffc+4)
```

##### CALL and RET
The processor operates on concrete objects, such as registers and data from memory. A processor doesn't know the concept of functions, like they exist in the high-level programming languages. Functions are implemented through the stack. Each function has its own stack, with a `EBP` value and a `ESP` value.

A function `int __cdecl addme(short a, short b)` can be shown like this:
```assembly
01: 004113A0 55 push ebp
02: 004113A1 8B EC mov ebp, esp
03: ...
04: 004113BE 0F BF 45 08 movsx eax, word ptr [ebp+8]            ; first argument
05: 004113C2 0F BF 4D 0C movsx ecx, word ptr [ebp+0Ch]          ; second argument
06: 004113C6 03 C1 add eax, ecx
07: ...
08: 004113CB 8B E5 mov esp, ebp
09: 004113CD 5D pop ebp
10: 004113CE C3 retn
```

And it will be called this way:
```assembly
01: 004129F3 50 push eax
02: ...
03: 004129F8 51 push ecx
04: 004129F9 E8 F1 E7 FF FF call addme
05: 004129FE 83 C4 08 add esp, 8
```

In those two examples, there two interesting instructions:
- `CALL` which does two operations
    - It pushes the return address on the stack (address after the `CALL` instruction)
    - It changes `EIP` (instruction pointer) to point to the beginning of the function code
- `RET` pops the address stored on the top of the stack into `EIP`. It is equivalent to `POP EIP` (which does not exist on x86).

```assembly
; This code would set 0x12345678 into EIP
01: 68 78 56 34 12 push 0x12345678
02: C3 ret
```

To call a function, there exist multiple convention. It is a set of rules that define how the functions should be called. It can define how the parameters should be passed (on stack? in registers? both?), how return value should be passed, first parameter first or last, ... There exist some popular *calling conventions*, 3 of them are presented below.

|  Conventions      |   Parameters        |    Return value         |  Non-volatile registers      |
|-------------------|---------------------|-------------------------|------------------------------|
| `CDECL`             | Pushed on stack, from right-to-left. Caller must clean up the stack after the call | Stored in EAX | EBP, ESP, EBX, ESI, EDI |
| `STDCALL`           | Same as `CDECL`, except callee must clean the stack | Stored in EAX | EBP, ESP, EBX, ESI, EDI |
| `FASTCALL`          | First two parameters in `ECX` and `EDX`. The rest on the stack. | Stored in EAX | EBP, ESP, EBX, ESI, EDI |

The *function prologue* is a sequence of two instructions at the beginning of a function, and it establishes a new function frame. Here are the two instructions:
```assembly
01: 004113A0 55 push ebp            ; save old base stack pointer
02: 004113A1 8B EC mov ebp, esp     ; set current stack pointer as the base stack pointer
```

The *function epilogue* is a sequence of two instructions at the end of a function, and it restores the previous function frame.

##### Exercises
1. Given what you learned about CALL and RET, explain how you would read
the value of EIP? Why can’t you just do MOV EAX, EIP?
> The solution would be to push it on stack, and then pop it.\
> *Note: with x86-64, `lea rax, [rip]` works*
```assembly
push eip
pop eax
```
2. Come up with at least two code sequences to set EIP to 0xAABBCCDD.
> First solution
```assembly
push 0xAABBCCDD
pop eip
```
> Second solution
```
call 0xAABBCCDD
```
3. In the example function, addme, what would happen if the stack pointer were not properly restored before executing RET?
> If `EBP` was not removed and always on stack, then `RET` would set `EIP` value to `EBP`, and there will be a stack execution (if not protected).
4. In all of the calling conventions explained, the return value is stored in a
32-bit register (EAX). What happens when the return value does not fit in a
32-bit register? Write a program to experiment and evaluate your answer.
Does the mechanism change from compiler to compiler?
> A pointer can be used for this. The pointer will then be used to access data.
> Let's test it in x86-64, under Ubuntu.

```c
#include <stdio.h>

const char* amazingString() {
    return "An Amazing String";
}

int main()
{
    printf("%s has been returned\n", amazingString());
}
```

The `amazingString()` function can be read as assembly with gdb. The C code has been compiled with **GCC**.
```assembly
; Dump of assembler code for function amazingString:
=> 0x0000555555555149 <+0>:     endbr64 
   0x000055555555514d <+4>:     push   rbp
   0x000055555555514e <+5>:     mov    rbp,rsp
   0x0000555555555151 <+8>:     lea    rax,[rip+0xeac]        # 0x555555556004
   0x0000555555555158 <+15>:    pop    rbp
   0x0000555555555159 <+16>:    ret
```

By using gdb, we can read the value pointed by the pointer:
```
(gdb) x /s 0x555555556004
0x555555556004: "An Amazing String"
```

By using **Clang** compiler, the assembly code of amazingString is:
```assembly
Dump of assembler code for function amazingString:
   0x0000000000401130 <+0>:     push   rbp
   0x0000000000401131 <+1>:     mov    rbp,rsp
=> 0x0000000000401134 <+4>:     movabs rax,0x402004
   0x000000000040113e <+14>:    pop    rbp
   0x000000000040113f <+15>:    ret  
```
We can see that the code is almost the same. The returned pointer points to the string:
```
(gdb) x /s 0x402004
0x402004:       "An Amazing String"
```


#### Control flow
Conditional execution is widely used with constructs like if/else, switch/case, and while/for loops. All those are implemented at low level through the `CMP`, `TEST`, `JMP` and `Jcc` instructions combined with the `EFLAGS` register. Note that `cc` is a Conditional Code, depending on flags from `EFLAGS`.

Here are some common flags from `EFLAGS` register:
- `ZF` - Zero Flag : Set if the result of previous arithmetic operation is zero
- `SF` - Sign Flag : Most significant bit of the result
- `CF` - Carry Flag : Set when result requires a carry (applies to unsigned numbers)
- `OF` - Overflow Flag : Set if result overflow the max size (signed numbers)

Here are some common Conditional Codes:
|CONDITIONAL CODE   | ENGLISH DESCRIPTION                                               | MACHINE DESCRIPTION  |
|-------------------|-------------------------------------------------------------------|----------------------|
| B/NAE             | Below/Neither Above nor Equal. Used for unsigned operations.      | CF=1                 |
| NB/AE             | Not Below/Above or Equal. Used for unsigned operations.           | CF=0                 |
| E/Z               | Equal/Zero                                                        | ZF=1                 |
| NE/NZ             | Not Equal/Not Zero                                                | ZF=0                 |
| L                 | Less than/Neither Greater nor Equal. Used for signed operations.  | (SF ^ OF) = 1        |
| GE/NL             | Greater or Equal/Not Less than. Used for signed operations.       | (SF ^ OF) = 0        |
| G/NLE             | Greater/Not Less nor Equal. Used for signed operations.           | ((SF ^ OF) | ZF) = 0 |

The `CMP` instruction compares two operands, and set the flags in `EFLAGS`. It uses a substraction between both elements. `TEST` does the same, but uses a logical AND between the two elements.

##### If/Else
It uses a compare, with a `Jcc`.
Here is an example:
```assembly
01: mov esi, [ebp+8]
02: mov edx, [esi]
03: test edx, edx                   ; test
04: jz short loc_4E31F9             ; jump if zero
05: mov ecx, offset _FsRtlFastMutexLookasideList
06: call _ExFreeToNPagedLookasideList@8
07: and dword ptr [esi], 0
08: lea eax, [esi+4]
09: push eax
10: call _FsRtlUninitializeBaseMcb@4
11: loc_4E31F9:
12: pop esi
13: pop ebp
14: retn 4
15: _FsRtlUninitializeLargeMcb@4 endp
```

##### Switch/Case
Switch-case uses multiple If/Else statements. This can be optimized by compilers.

##### Loops
Loops are implemented with `Jcc` and `JMP` instructions, and a If/Else statement.

Here is an example:
```assembly
01: 00401002 mov edi, ds:__imp__printf
02: 00401008 xor esi, esi
03: 0040100A lea ebx, [ebx+0]
04: 00401010 loc_401010:
05: 00401010 push esi
06: 00401011 push offset Format ; "%d\n"
07: 00401016 call edi ; __imp__printf
08: 00401018 inc esi
09: 00401019 add esp, 8
10: 0040101C cmp esi, 0Ah
11: 0040101F jl short loc_401010
12: 00401021 push offset aDone ; "done!\n"
13: 00401026 call edi ; __imp__printf
14: 00401028 add esp, 4
```

It corresponds to this C code:
```c
for (int i=0; i<10; i++) {
 printf("%d\n", i);
}
printf("done!\n");
```

### System Mechanism

#### Address Translation
On a computer, the physical memory is divided into 4KB units named *pages*. There are *physical* and *virtual* addresses.

Virtual addresses are used by instructions executed in the processor when paging is enabled.

Physical addresses are the actual memory locations used by the processor when accessing memory. The **MMU** transparently translates every virtual address into a physical address before accessing it. A virtual address has a structure, which is read by the MMU. On x86 with PAE (Physical Address Extension) support, a virtual memory address can be divided into:
- A Page Directory Pointer Table (PDPT)
- A Page Directory (PD)
- A Page Table (PT)
- A Page Table Entry (PTE).

A PDPT is an array of 4 elements, each pointing to a PD. A PD is an array of 512 elements, each pointing to a PT. A PT is an array of 512 elements containing a PTE.
Each element we are talking about is 8-byte long, and it contains data about the tables, memory permission (rwx), and other characterisitics.

Let's apply this to the virtual address `0xBF80EE6B`, which is `10111111 10000000 11101110 01101011` in binary format. This can be decoded as:
- `10` --> Index into PDPT
    - PDPT contains 4 elements, it is coded on 2 bits.
- `111111 100` --> Index into PD
- `00000 1110` --> Index into PT
- `1110 01101011` --> Page offset

The address translation process is made possible thanks to those three tables (PDPT, PD, PT) and the `CR3` register. This register holds the physical base address of the PDPT.

The bottom 12 bits of a PDPT entry are flags/reserved bits. The remaining ones are used as the physical address of the PD base. Bit63 is the `NX` flag in PAE. 

Let's take an example with `0xBF80EE6B`
```
kd> r @cr3 ; CR3 is the physical address for the base of a PDPT
cr3=085c01e0
kd> !dq @cr3+2*8 L1 ; read the PDPT entry at index 2 (reading at cr3 + 2)
# 85c01f0 00000000`0d66e001 ; Returned element is 8-byte long as discussed previously
; 00000000 0d66e001 = 00001101 01100110 11100000 00000001
; cleaning the bottom 12 bits (flags/reserved bits)
; 00000000 0d66e000 = 00001101 01100110 11100000 00000000
; PD base is at physical address 0x0d66e000
kd> !dq 0d66e000+0x1fc*8 L1 ; read the PD entry at index 0x1FC
# d66efe0 00000000`0964b063
```

The documentation also says that the bottom 12 bits of a PD entry are flags/reserved.

Let's continue our example
```
kd> !dq 0d66e000+0x1fc*8 L1 ; read the PD entry at index 0x1FC
# d66efe0 00000000`0964b063
; 0x0964b063 = 00001001 01100100 10110000 01100011
; after clearing the bottom 12 bits
; 0x0964b000 = 00001001 01100100 10110000 00000000
; This tells us that the PT base is at 0x0964b000
kd> !dq 0964b000+e*8 L1 ; read the PT entry at index 0xE
# 964b070 00000000`06694021
```

The same pattern applis to Page Table (PT), the bottom 12 bits of a PT entry are flags/reserved and can be cleared to get a page entry.

Let's finish our example
```
; 0x06694021 = 00000110 01101001 01000000 00100001
; after clearing bottom 12 bits, we get
; 0x06694000 = 00000110 01101001 01000000 00000000
; This tells us that the page entry base is at 0x06694000
; 0xe6b are the last 12 bits of 0xBF80EE6B
kd> !db 06694000+e6b L8 ; read 8 bytes from the page entry at offset
0xE6B
# 6694e6b 8b ff 55 8b ec 83 ec 0c ..U.....[).t.... ; our data at that physical page
kd> db bf80ee6b L8 ; read 8 bytes from the virtual address
bf80ee6b 8b ff 55 8b ec 83 ec ..U.....[).t.... ; same data!
```

#### Interrupts and Exceptions
Processor is typically connected to peripheral devices, through a data bus (PCI Express, ...). A device can require processor's attention, by causing an interrupt that will pause the processor to let it handle device's request. When processor receive an interrupt, it executes the function that is associated with this type of interruption. It then resumes its execution, going back to what he was doing before getting interrupted. Those are called *hardware interrupts*.

For errors, there exist two types: *faults* and *traps*. A fault is correctable. A trap is caused by executing special kinds of instructions, like `SYSENTER`.

Operating systems commonly implement system calls through the interrupt and exception mechanism.

#### Walk-through

`SIDT` instruction writes the content of the IDT register to a 6-byte memory location. IDT is an array of 256 8-byte entries, each containing a pointer to an interrupt handler. When an interrupt or exception
occurs, the processor uses the interrupt number as an index into the IDT and
calls the entry’s specifi ed handler. IDT register is 6-byte, the first 4 bytes are the base of the IDT table and the other 2 bytes store the table limit.

This assembly code:
```assembly
01: ; BOOL __stdcall DllMain(HINSTANCE hinstDLL, DWORD fdwReason,
 ; LPVOID lpvReserved)
02: _DllMain@12 proc near
03: 55 push ebp
04: 8B EC mov ebp, esp
05: 81 EC 30 01 00+ sub esp, 130h
06: 57 push edi
07: 0F 01 4D F8 sidt fword ptr [ebp-8]
08: 8B 45 FA mov eax, [ebp-6]
09: 3D 00 F4 03 80 cmp eax, 8003F400h
10: 76 10 jbe short loc_10001C88 (line 18)
11: 3D 00 74 04 80 cmp eax, 80047400h
12: 73 09 jnb short loc_10001C88 (line 18)
13: 33 C0 xor eax, eax
14: 5F pop edi
15: 8B E5 mov esp, ebp
16: 5D pop ebp
17: C2 0C 00 retn 0Ch
18: loc_10001C88:
```
can be decoded as this C code:
```c
typedef struct _IDTR {
    DWORD base;
    SHORT limit;
} IDTR, *PIDTR;

BOOL __stdcall DllMain (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    IDTR idtr;
    __sidt(&idtr);
    if (idtr.base > 0x8003F400 && idtr.base < 0x80047400h) { return FALSE; }
    //line 18
    ...
}
```

#### Exercises
An awesome resource: [bin.re](https://bin.re/projects/solutions-to-practical-reverse-engineering/)

## ARM


## Windows Kernel



## Debugging and Automation


## Obfuscation



