# MiniOS architecture

## Big picture

```
                        +---------------------+
   you type a command ->|  shell.cpp          |  splits the line, calls the right function
                        |  execute_command()  |
                        +----------+----------+
                                   |
     +------------+-----------+----+------+-------------+--------------+
     |            |           |           |             |              |
+----v----+  +----v-----+ +---v------+ +--v-------+ +---v--------+ +---v--------+
|process  |  |scheduler | |memory    | |resources | |filesystem  | |doctor      |
|.cpp     |  |.cpp      | |.cpp      | |.cpp      | |.cpp        | |.cpp        |
|run ps   |  |ticks,    | |fit,free, | |lock,     | |versions,   | |deadlock,   |
|kill     |  |5 algos,  | |compact,  | |unlock    | |trash       | |starvation, |
|crash    |  |aging     | |memmap    | |          | |            | |memory      |
+----+----+  +----+-----+ +---+------+ +--+-------+ +---+--------+ +---+--------+
     |            |           |           |             |              |
     +------------+-----------+-----+-----+-------------+--------------+
                                    |
                   +----------------v-----------------+
                   | globals.cpp  (all the OS tables) |   types.h: structs + constants
                   | kernel_log.cpp -> kernel.log     |   kernel.cpp: init_kernel() / reboot
                   +----------------------------------+
```

reports.cpp (Gantt chart, stats) only *reads* the tables. utils.cpp holds small text helpers.

## Modules

| File | Responsibility | Main functions | Uses |
|---|---|---|---|
| `main.cpp` | boot screen, login, read-execute loop, `!n` history | `main`, `boot_screen` | shell, kernel |
| `shell.cpp` | command interpreter, `help`, `dmesg` | `execute_command` | every module |
| `kernel.cpp` | reset all tables (boot and `reboot`) | `init_kernel` | globals |
| `kernel_log.cpp` | write events to `kernel.log` | `log_event` | globals |
| `process.cpp` | process lifecycle | `cmd_run`, `end_process`, `cmd_kill`, `cmd_crash` | memory, resources |
| `scheduler.cpp` | the CPU clock | `run_one_tick`, `cmd_step`, `cmd_runall`, `cmd_sched`, `cmd_aging` | process |
| `reports.cpp` | Gantt chart and statistics | `cmd_gantt`, `cmd_stats` | memory |
| `memory.cpp` | contiguous memory allocation | `allocate_memory`, `free_memory`, `compact_memory`, `memory_info` | globals |
| `resources.cpp` | locks that processes hold or wait for | `find_resource`, `release_resource` | globals |
| `doctor.cpp` | diagnose deadlock, starvation, memory | `find_deadlock`, `cmd_doctor` | process, memory |
| `filesystem.cpp` | versioned in-memory file system | `create_node`, `cmd_write`, `cmd_restore`, `cmd_rm`, `cmd_recover` | utils |
| `utils.cpp` | text helper | `get_text_after` | - |
| `globals.cpp` / `types.h` | the data: PCB, memory block, file node, all tables | - | - |

Rules the structure follows:
- **Headers (`include/*.h`) say WHAT a module offers; `.cpp` files say HOW.**
- **Data lives in one place** (`globals.cpp`), like kernel data structures, so every module sees the same OS state.
- **Dependencies point downward**: the shell uses everything, the scheduler uses the process manager, the process manager uses memory and resources, nothing uses the shell.

## What happens when you type `runall`

1. `main.cpp` reads the line and passes it to `execute_command` (`shell.cpp`).
2. `execute_command` sees the word `runall` and calls `cmd_runall` (`scheduler.cpp`).
3. `cmd_runall` calls `run_one_tick()` again and again while some process is READY or RUNNING.
4. Each `run_one_tick()` (a) asks the chosen algorithm which process gets the CPU, (b) runs it for 1 tick, (c) advances `current_tick`, (d) if the process finished, `end_process` (`process.cpp`) frees its memory (`memory.cpp`) and releases its locks (`resources.cpp`), (e) ages the waiting processes.
5. The shell then calls `cmd_gantt` (`reports.cpp`) to draw what happened.

## How to add a new command (3 steps)

1. Write `void cmd_yourthing(vector<string> &args)` in the module it belongs to and declare it in that module's header.
2. Add `else if (cmd == "yourthing") { cmd_yourthing(args); }` in `execute_command` (`shell.cpp`) and a line in `cmd_help`.
3. Add a few lines using it to a script in `demos/`, run `make demos`, check the output, and `make test` will protect it from now on.

## Testing

- `make test` runs 4 demo scripts and 2 edge-case scripts and compares every output line with the saved expected output.
- `make asan` rebuilds with AddressSanitizer and UBSan and runs the same scripts to catch memory bugs.

## File versioning: memory use and deletion
- A `Version` is either a full copy (first version and every 8th) or a delta: keep `prefix` bytes and `suffix` bytes of the previous text and replace the middle with `data`. Appending a line to a large file therefore stores only that line.
- Saving identical content is a no-op. A file keeps at most `MAX_VERSIONS` (20); the oldest is dropped automatically.
- `delete_version` first converts the next delta into a full copy, so removing any version never corrupts later ones.
- Deleting: `rmver` (one version), `prune` (keep newest N), `purge` (file, skips trash), `rmdir -r` (folder tree). `fsstat` reports stored vs full-copy bytes.
