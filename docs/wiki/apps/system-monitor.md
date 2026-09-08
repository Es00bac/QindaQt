# QindaQt System Monitor

System Monitor is being built as a native QindaQt application for understanding
resource use and managing processes. It belongs alongside the bundled editor,
terminal, and file manager. The implementation is not installed or qualified yet.

## A workspace for watching your system

The main window will offer Overview, Processes, CPU, Memory, Disks, Network,
and Hardware. Each view can also open in its own window. For example, put CPU
and disk graphs beside a build terminal, or keep process details on another
page of the same container. QindaQt manages the tiles and tabs.

Graphs should answer a specific question: how busy a resource is, how quickly
it is transferring data, or how its use has changed. Values include units and
explain unavailable readings. Process management includes search, sorting,
details, termination, pause/resume, and priority changes with real error feedback.

## Implementation boundary

The [detachable-view decision](../adr/0108-compose-system-monitor-from-detachable-views.md)
defines ownership. Linux sampling and process actions belong to the app's core;
GPU and sensor adapters belong to its hardware module. The interface consumes
published snapshots and process models, without reading procfs itself. Sampling
runs away from the GUI thread and retains bounded history.

Portage integration and actual installed verification are part of delivery.
This page will record supported readings, shortcuts, and verified limitations
when the implementation is integrated.

## Design and data references

The navigation follows familiar desktop patterns described in the
[KDE layout guidelines](https://develop.kde.org/hig/layout_and_nav/), while
QindaQt's own tokens and container behavior determine appearance and window
composition. Counter semantics follow the kernel's
[procfs documentation](https://www.kernel.org/doc/html/latest/filesystems/proc.html)
and [I/O statistics reference](https://www.kernel.org/doc/html/latest/admin-guide/iostats.html).
AMD readings follow the driver's
[monitoring interface](https://www.kernel.org/doc/html/latest/gpu/amdgpu/thermal.html).

## Reading the numbers

CPU percentages use the whole machine's capacity as 100%, including the
process table. Memory in the process table is resident memory (RSS); shared
pages can appear in more than one process, so adding those rows does not give
the machine's used-memory total. System memory shows available and cached
memory separately, along with swap.

Disk and network activity are rates in bytes per second. The first reading
establishes a baseline; a device that disappears or resets needs a new baseline.
Missing process I/O permissions and unsupported driver readings remain
unavailable rather than appearing as zero activity.
