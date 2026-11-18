# Viva notes - questions to expect

Learn the answers in your own words. If you can explain these, you can explain the project.

**Is this a real OS?**
No. It is a simulation that runs on top of Linux. It models the OS data structures and algorithms
(process table, scheduler, memory allocator, file system) so they can be observed and measured.

**What is a PCB? Where is it in your code?**
The Process Control Block, the record the OS keeps per process. It is the `Process` struct:
pid, state, burst, remaining time, priority, memory block, and so on.

**How does your scheduler work?**
`run_one_tick()` is called once per clock tick. It asks the selected algorithm which process should
use the CPU, runs it for one tick, then handles finishing processes and waiting-time bookkeeping.

**Preemptive vs non-preemptive - show it in your project.**
SJF vs SRTF with a late arrival: SJF lets the long job finish (avg wait 3.50), SRTF preempts it
(avg wait 1.00). Run `demos/demo_scheduler.txt`, "Late arrival" section.

**Why is RR's response time good but waiting time bad?**
Every process gets the CPU within one round (avg response 4.50), but long jobs are interrupted
and wait while others run, so waiting and turnaround grow, and there are more context switches (9).

**What is starvation and how did you solve it?**
A low-priority process may never run while higher-priority ones keep coming. `aging on` lowers the
priority number of any process that waited 5 ticks, so it eventually wins.

**What are the four conditions for deadlock? Which ones does your demo show?**
Mutual exclusion, hold and wait, no preemption, circular wait. The demo has P1 holding `printer`
and wanting `scanner` while P2 holds `scanner` and wants `printer`: all four.

**How do you detect deadlock?**
Wait-for graph. Each blocked process waits for one resource held by one process, so I follow the
chain from each blocked process; if it returns to the start there is a cycle = deadlock.
(`find_deadlock` in `src/doctor.cpp`). Recovery by process termination: `doctor fix` kills the youngest in the cycle.

**How is this different from the Banker's algorithm from Lab 11?**
Banker's *avoids* deadlock by only granting a request if the state stays safe. My doctor
*detects and recovers*. (Extension idea: add a Banker's check to `lock`.)

**First, best and worst fit - what do they do?**
First: the first hole big enough. Best: the smallest hole big enough (leaves the smallest leftover).
Worst: the biggest hole (leaves a big usable leftover). See `demos/demo_memory.txt`.

**What is external fragmentation? How do you measure it?**
Free memory split into pieces too small to use. I report (free - largest hole) / free as a percentage.
`compact` slides all used blocks together to remove it.

**Why was a process refused when there was enough free memory in total?**
Allocation is contiguous. 104 KB was free, but the biggest hole was 40 KB, so a 60 KB process did not fit.

**How does the time-travel file system work?**
Each file stores a vector of versions. `write` appends a new version; nothing is overwritten.
`restore n` copies version n to a new latest version. `rm` only sets a deleted flag (trash).

**Why is the code split into many files?**
One file per OS subsystem (process, scheduler, memory, resources, file system, doctor). Headers
say what a module offers, `.cpp` files say how. All OS data lives in `globals.cpp`, like kernel
data structures. Dependencies point downward: the shell uses everything, nothing uses the shell.
(See `docs/ARCHITECTURE.md`.)

**How do you know it works?**
`make test` runs 6 scripts and compares every output line with saved expected output. I also
ran it under AddressSanitizer/UBSan (`make asan`) to catch memory bugs.

**What would you improve?**
Paging and page replacement, a Banker's-algorithm mode, MLFQ scheduling, saving the OS state to disk.
