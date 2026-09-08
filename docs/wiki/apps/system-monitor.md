# QindaQt System Monitor

System Monitor is a native QindaQt application for understanding resource use
and managing processes. It belongs alongside the bundled editor, terminal,
and file manager. Source integration and focused verification are complete;
Portage installation is being qualified.

## A workspace for watching your system

Use Overview for a quick look at CPU, memory, disk, and network activity. The
other views let you investigate a particular resource:

- **Processes:** search for an application, sort the table, inspect its resource
  use, and terminate, pause, resume, or change its priority.
- **CPU:** watch total utilization and compare individual logical CPUs.
- **Memory:** inspect used, available, and cached memory, plus swap.
- **Disks:** select a device to watch reads and writes, and check filesystem space.
- **Network:** select an interface to watch incoming and outgoing traffic.
- **Hardware:** inspect GPU utilization and memory, temperatures, fan speeds,
  and other readings exposed by the installed driver.

Choose **View → Open view in new window** (`Ctrl+N`) to keep a resource visible
while looking at another. Put CPU and disk graphs beside a build terminal, or
combine monitoring windows into a container. QindaQt manages the tiles and tabs.
Closing one monitoring window leaves the others running.

**View → Pause updates** (`Ctrl+Alt+P`) freezes sampling across the windows of
this app instance. The View menu also controls the sampling interval and graph
duration. A shorter interval gives more frequent readings; a longer interval
reduces sampling work. History is kept in memory with a bounded retention period.

To open a particular view from a terminal, use, for example,
`qindaqt-system-monitor --view network`. View names are `overview`, `processes`,
`cpu`, `memory`, `disks`, `network`, and `hardware`.

## Implementation boundary

The [detachable-view decision](../adr/0108-compose-system-monitor-from-detachable-views.md)
defines ownership. Linux sampling and process actions belong to the app's core;
GPU and sensor adapters belong to its hardware module. The interface consumes
published snapshots and process models, without reading procfs itself. Sampling
runs away from the GUI thread and retains bounded history.

## Installing and opening System Monitor

The Gentoo package is `gui-apps/qindaqt-system-monitor` in the QindaQt overlay.
It depends on the desktop's shared runtime and installs its own executable,
launcher, and icon. It does not replace the compositor.

```sh
emerge --ask gui-apps/qindaqt-system-monitor
qindaqt-system-monitor
```

Open **QindaQt System Monitor** from the application launcher, or run the
command above. A desktop restart is not needed after installation. The package
recipe and its pinned source are kept under
`packaging/gentoo/gui-apps/qindaqt-system-monitor/` in the repository.

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

## Hardware and filesystem coverage

AMD GPU readings come from the kernel's sysfs interface. NVIDIA readings use
NVML when it is already installed with the driver. Missing GPU counters are
reported as unavailable; installing System Monitor does not install or change
a graphics driver. Sensor availability depends on the machine and kernel.
Intel engine utilization is not currently collected.

Filesystem capacity excludes remote and FUSE mounts so an unavailable server
cannot stall collection. Disk activity still describes the local block devices.
Process I/O can be unavailable when the current account cannot read another
process's counters. Process actions use your account's permissions and show the
operating system's error when an action is denied.
