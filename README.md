# MiniOS - a tiny simulated operating system in C++

MiniOS is a small operating system **simulation** written in one C++ file. It runs as a normal
program on Linux/WSL, but inside it you get a shell, a process table with a live-switchable CPU
scheduler, a memory manager, a file system that keeps every version of every file, and a
`doctor` command that diagnoses deadlocks, starvation and memory fragmentation.

> It is a simulation, not a real kernel: it does not boot on hardware, and "processes" are
> entries in a table that consume simulated CPU time. This is deliberate - it lets every OS
> concept (scheduling, contiguous memory allocation, deadlock, file versioning) be
> implemented, observed and measured in a few hundred lines.

## What makes it different

| Feature | What it does |
|---|---|
| **Live scheduler switching** | `sched rr 3` / `sched srtf` / ... can be changed *while processes are half done*. Gantt chart and stats let you compare algorithms on the same workload. |
| **`doctor`** | Scans the OS and explains problems in plain English: real deadlock cycles (wait-for graph), starvation, blocked processes, external fragmentation. `doctor fix` breaks a deadlock. |
| **Time-travel file system** | Every `write`/`append` saves a *new version*. `versions`, `diff`, `restore` let you go back. `rm` only moves a file to a trash bin; `recover` brings it back. **Memory-efficient:** versions are stored as small deltas (only the changed part), identical saves are skipped, and each file keeps at most 20 versions. `rmver`, `prune`, `purge`, `rmdir -r` delete versions, files or folders for good; `fsstat` shows the memory saved. |
| **Aging** | `aging on` stops starvation under priority scheduling: waiting processes get a priority boost every 5 ticks. |
| **Memory map** | `memmap` draws RAM, shows fragmentation, supports first/best/worst fit and `compact`. A process is refused if no single hole is big enough, even when total free memory is enough. |
| **Crash simulation** | `crash` kills a process "with a segfault"; the OS frees its memory and locks and keeps running. |
| **Kernel log** | Every event is written to `kernel.log`; view it with `dmesg`. |

## Build and run (WSL / Linux)

```bash
make              # compile everything into ./minios
./minios          # run interactively  (or: make run)
./minios --fast   # skip the boot-animation pauses
./minios < demos/demo_scheduler.txt   # run a script of commands
make demos        # re-run all demo scripts and regenerate results/
make test         # regression tests: compare fresh output with the saved expected output
make asan         # rebuild with AddressSanitizer/UBSan and run every script
make clean
```
Needs `g++` and `make` (`sudo apt install build-essential` in WSL). It uses `unistd.h`, so build inside WSL, not native Windows. Type `help` inside MiniOS.

## Quick tour

```
toshit@minios:/ [t=0]$ run web 8 2 64        # name, CPU burst, priority (0=best), memory KB
toshit@minios:/ [t=0]$ run db 5 1 32
toshit@minios:/ [t=0]$ sched srtf
toshit@minios:/ [t=0]$ runall                # runs everything, draws the Gantt chart
toshit@minios:/ [t=13]$ stats                # waiting / turnaround time, CPU use, ...
toshit@minios:/ [t=13]$ write notes.txt hello
toshit@minios:/ [t=13]$ append notes.txt \nworld
toshit@minios:/ [t=13]$ versions notes.txt   # time travel
```

## Command reference

| Area | Commands |
|---|---|
| Processes | `run <name> <burst> [pri 0-9] [memKB]`, `ps`, `step [n]`, `runall`, `kill <pid>`, `crash [pid]` |
| Scheduler | `sched fcfs\|sjf\|srtf\|priority\|rr [quantum]`, `aging on\|off`, `gantt`, `stats` |
| Memory | `memmap`, `fit first\|best\|worst`, `compact` |
| Locks / health | `lock <pid> <res>`, `unlock <pid> <res>`, `res`, `doctor [fix]` |
| Files | `ls`, `pwd`, `cd`, `mkdir`, `rmdir`, `touch`, `write`, `append`, `cat`, `versions`, `restore`, `diff`, `rm`, `trash`, `recover`, `emptytrash`, `rmver`, `prune`, `purge`, `fsstat`, `rmdir -r` |
| System | `dmesg [n]`, `history`, `!n`, `!!`, `uptime`, `whoami`, `echo`, `clear`, `reboot`, `exit` |

Time is measured in **ticks** (one tick = one unit of CPU time). `step` advances the clock by hand,
`runall` runs until nothing is ready. Processes are created at the *current* tick, so you can
create late arrivals by using `step` first.

## Design (how each part works)

The code is split by OS subsystem; see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the module diagram.

| Part | Data structure | Idea |
|---|---|---|
| Process manager | array of `Process` structs (a Process Control Block) | states READY / RUNNING / BLOCKED / DONE / KILLED; pid = array index |
| Scheduler | `run_one_tick()` | each tick the algorithm picks one process, it runs 1 tick, then finished processes are cleaned up and waiting processes age. FCFS/SJF are non-preemptive, SRTF/Priority/RR preemptive. Round Robin uses a `ready_seq` number as a queue position. |
| Gantt chart | `timeline[t]` = pid that ran at tick t | printed per process; `#` running, `.` waiting |
| Memory | array of `Block {start, size, pid}` | contiguous variable partitions; allocation splits a hole, freeing merges neighbours (coalescing); `compact` slides blocks together |
| Locks and deadlock | `res_holder[]` and each process's `waiting_for` | every blocked process waits for one resource held by one process, so the wait-for graph has out-degree 1; `find_deadlock` follows the chain and reports a cycle if it returns to the start |
| File system | array of `FileNode`; each file has a `vector<Version>` (delta-compressed, keyframe every 8, max 20) | `write` never overwrites; `rm` sets a `deleted` flag (trash) |
| Doctor | combines all of the above | deadlock, starvation (ready > 12 ticks without CPU), blocked, fragmentation >= 30%, memory >= 90% full |

## Results

All results in `results/` are produced by `make demos` from the scripts in `demos/`
(each `.out` is the full transcript, each `.kernel.log` the kernel log of that run).
Workload for the first table: A(8) B(4) C(9) D(5), all arriving at t=0, priorities A=2 B=4 C=1 D=3.

| Scheduler | Avg waiting | Avg turnaround | Avg response | Context switches |
|---|---|---|---|---|
| FCFS | 10.25 | 16.75 | 10.25 | 3 |
| SJF | 7.50 | 14.00 | 7.50 | 3 |
| SRTF | 7.50 | 14.00 | 7.50 | 3 |
| Priority | 12.00 | 18.50 | 12.00 | 3 |
| Round Robin (q=3) | 15.00 | 21.50 | 4.50 | 9 |

Late arrival (L needs 10 ticks, S needs 2 and arrives at t=3): SJF cannot preempt, so S waits 7 ticks
(average waiting 3.50); SRTF preempts L (average waiting 1.00, 2 context switches).
Starvation: with priority scheduling `Small` waits 30 ticks behind `Big` (average waiting 15.00); with `aging on` it gets the CPU at tick 15 (average waiting 10.00).

Also tested (`tests/cases/`): bad input, killing a running process, blocking a running process, a 3-process
deadlock cycle, a lock handed over when its holder is killed, and idle gaps in the Gantt chart.
`make test` re-runs all of it, and `make asan` runs everything under AddressSanitizer and UBSan (no findings).
Limits were also stress-tested: full process table, full file system, full resource table and the 5000-tick cap.

## Limitations (honest list)

- It is a simulation; there is no real context switch, MMU or disk.
- Memory is contiguous allocation; there is no paging or virtual memory.
- Processes do not execute code. Their CPU need is just a number (`burst`); locks are requested by hand with `lock`.
- File names are looked up in the current folder only (no paths like `a/b/c.txt`); `diff` compares line N with line N, it is not a full LCS diff.
- 100 process slots (finished processes keep their slot for statistics); use `reboot` to reset.

## Possible extensions

Paging with page-replacement (FIFO/LRU), a Banker's-algorithm *avoidance* mode for `lock`,
multi-level feedback queue scheduler, saving/loading the whole OS state to disk.

## Project structure

```
include/           headers: what each module offers
  types.h            constants and structs (process, memory block, file node)
  globals.h          the OS tables (declared here, defined in src/globals.cpp)
  memory.h  process.h  scheduler.h  resources.h  doctor.h
  filesystem.h  reports.h  shell.h  kernel.h  kernel_log.h  utils.h
src/               implementation, one file per subsystem
  main.cpp           boot screen, login, command loop
  shell.cpp          command interpreter, help, dmesg
  kernel.cpp         boot / reboot (reset all tables)
  kernel_log.cpp     kernel.log
  process.cpp        run, ps, kill, crash
  scheduler.cpp      clock tick, FCFS/SJF/SRTF/Priority/RR, aging
  reports.cpp        Gantt chart, stats
  memory.cpp         allocation, coalescing, compaction, memmap
  resources.cpp      lock / unlock
  doctor.cpp         deadlock, starvation, memory checks
  filesystem.cpp     versioned file system, trash
  utils.cpp  globals.cpp
demos/             input scripts: scheduler, memory, filesystem, doctor
results/           transcripts + kernel logs produced by the demos
tests/             cases/ (edge-case scripts), expected/ (expected output), run_tests.sh
docs/              ARCHITECTURE.md, REPORT_OUTLINE.md, VIVA_NOTES.md
Makefile           make / run / demos / test / asan / clean
```
