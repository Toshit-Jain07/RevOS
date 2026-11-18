# Report outline (write this in your own words)

The notice asks for a *detailed report* with the GitHub link, plus a zip of source and results.
Below is a structure that covers what they will look for. Fill each part yourself - the panel
will ask follow-up questions on exactly what you write here.

**Title page** - project name (MiniOS), your name, roll number, course (CSL301 Operating Systems), GitHub link.

1. **Introduction / Motivation** (half a page)
   Why a small simulated OS? Which course topics it covers: scheduling, memory management,
   deadlock, file systems, process lifecycle.

2. **Problem statement and goals**
   One paragraph, then 4-5 bullet goals (shell, scheduler, memory manager, versioned file
   system, doctor).

3. **What is unique**
   Live scheduler switching with Gantt/stats, the doctor (deadlock cycle detection + explanation),
   time-travel file system with trash, aging demo. Say clearly what is *your own idea*.

4. **System design**
   - Architecture diagram (start from `docs/ARCHITECTURE.md` and redraw it yourself): shell -> {process manager + scheduler, memory manager, resource/lock table, file system, doctor, kernel log}
   - Data structures (copy from README "Design" table and explain each in your words)
   - Process state diagram: READY <-> RUNNING, RUNNING -> BLOCKED -> READY, RUNNING -> DONE/KILLED

5. **Implementation details** (the main part, 3-4 pages)
   - 5.1 Scheduler: how one tick works, how each algorithm picks the next process, how Round Robin keeps a queue, how aging works
   - 5.2 Memory manager: first/best/worst fit, splitting, coalescing, fragmentation formula, compaction
   - 5.3 Deadlock detection: wait-for graph, why a simple chain-follow is enough here, how `doctor fix` picks a victim
   - 5.4 File system: versions, restore (non-destructive), trash and recover
   - 5.5 Shell and kernel log

6. **Testing and results**
   - Scheduler comparison table (from `results/demo_scheduler.out`) and a short discussion of *why* the numbers differ
   - Late arrival: SJF vs SRTF
   - Starvation vs aging
   - Fragmentation: before/after `compact`; first vs best vs worst fit
   - Deadlock found and fixed (screenshot of `doctor` output)
   - Edge cases tested (full tables, tick limit, killing a running process, sanitizer run)
   - Paste screenshots of real runs on YOUR machine

7. **Limitations and future work**
   Copy the honest list from the README and add what you would do next.

8. **Conclusion** (a few lines)

9. **References**
   Course slides/textbook (Silberschatz "Operating System Concepts" chapters on scheduling,
   deadlocks, memory management, file systems), man pages used for ideas.

**Appendix** - how to build and run, command list, link to the repo.

## Repo / submission checklist
- [ ] Public (or shared) GitHub repo with the link written in the report
- [ ] Commit history with several meaningful commits (not one giant upload)
- [ ] `README.md`, `include/`, `src/`, `Makefile`, `demos/`, `results/`, `tests/`, `docs/` in the repo
- [ ] Zip = source + demos + results + report PDF
- [ ] You ran `make`, `make demos` and `make test` on your own machine and used your own screenshots
- [ ] Submitted before the deadline
