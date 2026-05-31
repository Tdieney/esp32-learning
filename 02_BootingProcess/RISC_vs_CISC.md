# RISC vs CISC

## 1. The Core Concept

To make it easier to understand, imagine we need to solve the following problem: **Multiply two numbers stored in memory and save the result back to memory.**

### CISC (Complex Instruction Set Computer) – "Do More with a Single Instruction"

The philosophy behind CISC is to **maximize support for programmers (or compilers) at the software level**. The hardware (CPU) takes responsibility for handling complex operations.

* **How CISC handles it:** It may provide a single complex instruction, such as:

```assembly
MULT M1, M2
```

* **Under the hood:** When the CPU encounters this instruction, it internally performs a sequence of micro-operations:

  * Read data from memory location M1
  * Read data from memory location M2
  * Send both values to the ALU for multiplication
  * Write the result back to memory

* **Characteristics:**

  * Smaller code size because one instruction can perform multiple tasks.
  * Typically fewer general-purpose registers since operations can directly access RAM.
  * **Classic examples:** The **x86** architecture (Intel/AMD) used in PCs and servers, and the legendary **8051** microcontroller family.

---

### RISC (Reduced Instruction Set Computer) – "Break It Down for Speed"

The RISC philosophy takes the opposite approach: **keep the hardware as simple and streamlined as possible.** Each instruction performs only one basic operation and is designed to execute very quickly (often within a single clock cycle).

* **How RISC handles it:** RISC architectures do not allow arithmetic instructions to operate directly on memory (the **Load/Store Architecture** principle). The problem must be broken into several simple instructions:

```assembly
LOAD  R1, M1     ; Load value from M1 into R1
LOAD  R2, M2     ; Load value from M2 into R2
MUL   R3, R1, R2 ; Multiply R1 and R2, store result in R3
STORE M1, R3     ; Store result back to memory
```

* **Characteristics:**

  * Longer code sequences (requiring more Flash memory for the same logic).
  * Requires a large number of high-speed registers to hold working data.
  * **Classic examples:** **ARM** (Cortex-M in STM32 MCUs, Cortex-A in powerful SoCs), **RISC-V**, as well as PIC and AVR microcontrollers.

---

## 2. Comparison

| Feature                          | CISC (e.g., x86, 8051)                                                                                        | RISC (e.g., ARM Cortex-M, RISC-V)                                                                                                     |
| -------------------------------- | ------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------- |
| **Instruction Length**           | **Variable-length:** Instructions may range from 1 byte to more than a dozen bytes depending on complexity.   | **Fixed-length:** Typically 32-bit (or 16-bit in Thumb mode), making instruction decoding much simpler.                               |
| **Memory Access**                | Arithmetic instructions can directly access RAM.                                                              | Only `LOAD` and `STORE` instructions access memory. Arithmetic operates exclusively on registers.                                     |
| **Number of Registers**          | Relatively few (early x86 processors had only a handful of general-purpose registers such as EAX, EBX, etc.). | Many registers (ARM provides R0–R15, making compiler optimization much easier).                                                       |
| **Pipeline Efficiency**          | More difficult to optimize because instruction lengths and execution times vary.                              | Highly pipeline-friendly. Uniform instruction formats and predictable execution times simplify `Fetch → Decode → Execute` pipelining. |
| **Power Consumption & Die Area** | Complex decoding logic requires more silicon area and generally consumes more power.                          | Simpler logic reduces power consumption, die size, and cost, making it ideal for Automotive and IoT applications.                     |

---

## 3. What the Compiler Actually Generates (Assembly Example)

Suppose you are using a CMake + Ninja toolchain to build bare-metal firmware for an ARM MCU (RISC). Given the following C code:

```c
uint32_t a = b + c;
```

A compiler such as `arm-none-eabi-gcc` may generate assembly similar to:

```assembly
LDR R1, [SP, #4]   ; Load variable b from the stack into R1
LDR R2, [SP, #8]   ; Load variable c from the stack into R2
ADD R0, R1, R2     ; Add R1 and R2, store result in R0
STR R0, [SP, #12]  ; Store result back to variable a on the stack
```

On an older CISC architecture, the compiler could potentially combine this into one or two instructions that operate directly on memory locations without explicitly using intermediate registers such as R1 and R2.

---

## 4. The Modern Reality: Convergence of RISC and CISC

Having witnessed the evolution of processor architectures over the past two decades, I can confidently say that **the boundary between RISC and CISC has become increasingly blurred.**

* Modern **CISC processors** (such as Intel Core i9 and AMD Ryzen) still expose a complex CISC instruction set externally for backward compatibility. However, once instructions enter the CPU core, dedicated hardware translates them into small **micro-operations (micro-ops)** that are fundamentally RISC-like, allowing them to be executed efficiently through deeply pipelined execution units.

* Conversely, modern **RISC architectures** (such as ARMv8, ARMv9, and advanced RISC-V implementations) now include increasingly sophisticated instruction extensions, including vector processing, AES cryptography instructions, and AI/matrix computation accelerators. As a result, they are no longer as "minimalistic" as the original RISC philosophy envisioned.

In practice, today's high-performance CPUs often combine ideas from both worlds: **the software compatibility and rich instruction sets of CISC with the execution efficiency and pipelining advantages of RISC.**

## My Point of View

In the context of current embedded system development, there is no "winner" between RISC and CISC. Each architecture has its own strengths and trade-offs, and the choice depends on the specific application requirements, ecosystem support, and developer preferences.
