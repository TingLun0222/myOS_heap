# myOS_heap

A bare-metal operating system for RISC-V, running on QEMU's `virt` machine.

Built on top of `07-preemptive2` (preemptive multitasking) with a custom fine-grained heap allocator (boundary tags + explicit free list) integrated from `memanagement`.

---

## Features

### Scheduler
- **Preemptive scheduling**: hardware timer (CLINT) fires interrupts that force task switches
- **Priority-based**: 256 priority levels; lower number = higher priority
- **Round-robin within same priority**: tasks at the same level take turns via `task_yield()`
- **Context switch**: saves/restores 31 GP registers + pc via two paths — `switch_to` (`ret`) for voluntary yields and `switch_from_isr` (`mret`) for interrupt-driven preemption

### Memory Management (two layers)

**Layer 1 — Page Allocator**
- PAGE_SIZE = 256 bytes
- A descriptor array at the start of the heap tracks each page with 1-byte flags
- `PAGE_TAKEN` (bit 0) and `PAGE_LAST` (bit 1)

**Layer 2 — Heap Allocator (fine-grained)**
- **Boundary tags**: every block carries a header and footer storing size + alloc flag, enabling O(1) coalescing
- **Explicit doubly-linked free list**: O(1) insert and removal of free blocks
- **Immediate coalescing**: adjacent free blocks are merged on every `free()` call
- **First-fit search**: `malloc()` scans the free list for the first block large enough
- **On-demand growth**: when the free list is exhausted, new pages are requested from the page allocator via `heap_extend()`

### Trap Handling
- Unified entry point `trap_vector` registered in `mtvec`
- Distinguishes exceptions (synchronous) from interrupts (asynchronous) via `mcause` bit 31
- Timer interrupt (cause = 7) dispatches to `timer_handler()`
- Runs entirely in Machine mode (M-mode)

### Other
- **UART**: 16550A MMIO, polling mode (`uart_putc` / `uart_getc`)
- **Spin lock**: disables/restores `mstatus.MIE` to protect critical sections
- **Task management**: `task_create`, `task_startup`, `task_suspend`, `task_resume`, `task_yield`

---

## Memory Layout

```
0x80000000  .text
            .rodata
            .data        (4 KB aligned)
            .bss
            ↓ _heap_start
            Page descriptor array  (2048 × PAGE_SIZE = 512 KB)
            _alloc_start
            Heap  (malloc / free region)
            ...
0x88000000  (end of 128 MB RAM)
```

---

## Scheduling Behavior

| Scenario | Behavior |
|----------|----------|
| Multiple tasks at same priority | Timer preempts in round-robin; `task_yield()` voluntarily gives up CPU |
| Higher-priority task present | Lower-priority tasks cannot preempt a running higher-priority task |
| Task suspended | Removed from the ready queue until `task_resume()` is called |

> **Note**: `task_yield()` only switches to a task of **equal or higher** priority. If only lower-priority tasks are waiting, `task_yield()` returns immediately and the current task continues running.

---

## Configuration

| Parameter | Value |
|-----------|-------|
| CPU | RISC-V RV32IMA (hart 0 only) |
| RAM | 128 MB starting at `0x80000000` |
| Timer rate | 100 Hz (`TICK_PER_SECOND = 100`) |
| Max tasks | 257 (256 user + 1 system) |
| Priority levels | 256 |
| Page size | 256 bytes |

---

## File Structure

```
myOS_heap/
├── startup/start.S      Boot assembly: clear BSS, set up stack, jump to start_kernel
├── asm/
│   ├── entry.S          trap_vector, switch_to, switch_from_isr
│   └── mem.S            Exposes linker-script symbols as C-readable variables
├── kernel/
│   ├── kernel.c         start_kernel(): initialization sequence
│   ├── page.c           Two-layer memory manager (page allocator + heap allocator)
│   ├── sched.c          Scheduler: do_schedule, schedule, task_yield
│   ├── task.c           TCB management: task_create/init/startup/suspend/resume
│   ├── timer.c          CLINT timer: timer_init, timer_handler, tick_dec
│   └── trap.c           Trap handling: trap_init, trap_handler
├── lib/
│   ├── uart.c           UART driver
│   ├── printf.c         kprintf, panic
│   ├── memory.c         memset, memcpy
│   └── lock.c           spin_lock, spin_unlock
├── include/             Header files
├── apps/users.c         User tasks and test suite
└── os.ld                Linker script (RAM at 0x80000000, 128 MB)
```

---

## Build and Run

```bash
# Build
make

# Run on QEMU (press Ctrl-A then X to quit)
make run

# Clean build artifacts
make clean
```

**Requirements**: `riscv64-unknown-elf-gcc`, `qemu-system-riscv32`

---

## Test Suite

`apps/users.c` contains five tests that run sequentially:

| Test | What it verifies |
|------|-----------------|
| TEST 1 | Timer preemption: two spin-loop tasks both receive CPU time |
| TEST 2 | Priority: a high-priority task monopolizes CPU; low-priority task never runs while it is active |
| TEST 3 | task_yield: same-priority tasks rotate in A→B→C order |
| TEST 4 | task_suspend / task_resume: a suspended task stops running and resumes correctly |
| TEST 5 | malloc / free: allocations return distinct addresses; freed blocks are reused |
