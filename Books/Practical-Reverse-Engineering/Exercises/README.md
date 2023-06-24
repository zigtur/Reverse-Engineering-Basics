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

### Instruction Set

#### Data Movement


##### Exercise
This function uses a combination `SCAS` and `STOS` to do its work. First, explain
what is the type of the [EBP+8] and [EBP+C] in line 1 and 8, respectively.
Next, explain what this snippet does.

```assembly
; ebp+8 is a memory address (a pointer)
; the value stored at the address pointed by [ebp+8] is loaded in edi
; supposition: it points to a buffer
01: 8B 7D 08 mov edi, [ebp+8]
; value is copied into edx
02: 8B D7 mov edx, edi
; clean eax, set it to zero
03: 33 C0 xor eax, eax
; set ecx to 0xFFFFFFFF
04: 83 C9 FF or ecx, 0FFFFFFFFh
; find al (value = 0) into the memory, starting at the address which was pointed by [ebp+8]
; scasb will set ZF=1 if equals
; repne continues until ecx==0 or ZF==1
05: F2 AE repne scasb
; set ecx + 2 once al has been found
06: 83 C1 02 add ecx, 2
; two's complement negation of the ecx register
; instead of modifying DF, it does flip all bits of ecx to loop until ecx==0
07: F7 D9 neg ecx
; ebp+0c is a memory address (a pointer)
; set al to the value of the address pointed by [ebp+0c]
08: 8A 45 0C mov al, [ebp+0Ch]
; set edi to the first value that was pointed by [ebp+8]
09: 8B FA mov edi, edx
; get the first buffer address to edi
; write al on buffer until ecx==0
10: F3 AA rep stosb
11: 8B C2 mov eax, edx
```

I think that this code does identify a buffer. To find its size, it does look for a byte 0x00. Once it has been found, the buffer length is known. Then, all the buffer is initialized with the value 0x00.

So it is an equivalent to `strlen()` and then `memset()`.

### Arithmetic Operations

## ARM


## Windows Kernel



## Debugging and Automation


## Obfuscation



