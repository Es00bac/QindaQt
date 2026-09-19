# QindaQt System Monitor

System Monitor is a native QindaQt application for watching a machine work
and for managing the processes doing the work. It belongs alongside the
bundled editor, terminal, and file manager.

## Everything at once

The window is a dashboard, not a set of tabs. Processor, memory, storage,
network and processes answer each other — a disk that is saturated and a
process that is writing are one observation, not two — so all of them are on
screen together, and the reading you need is never one click away behind
another view.

- **Processor:** the whole-machine trace, then every logical CPU with its own
  load bar and, where the hardware reports one, its temperature. Underneath:
  load average, current clock, uptime, and how many processes exist.
- **Memory:** used, cached and available as separate bars, plus swap. They
  are separate on purpose — on Linux, cache counts as used but is available
  on demand, and reading one bar as the other is the most common way people
  misdiagnose a machine.
- **Storage:** how full each filesystem is, and how hard each block device is
  being worked. Space and traffic are two questions about two different
  things — a mount point and a device — so they are two lists rather than
  rows implying a mapping that is not there.
- **Network:** received above the line, sent below it, on one shared scale so
  the two directions are directly comparable.
- **Processes:** filter, sort, fold into a tree, and act. Each row carries a
  small trace of its own recent CPU use.
- **Hardware:** GPU utilization, video memory, temperatures, fan speeds and
  power, from whatever the installed driver exposes.

Every reading is coloured by its magnitude, so a core at 95% and a filesystem
at 98% look urgent without your having to read the number first.

## Rearranging it

Panels are dock panels. Drag a panel's header to move it to another edge, to
stack it with another panel, or to tear it off as a floating panel; drag the
seam between panels to resize them. **Layout → Reset arrangement** puts
everything back, and **Layout → Show every panel** restores anything closed.
Your arrangement is remembered between sessions.

The process table is the centre and cannot be closed — it is the panel this
application would be pointless without.

**Open this panel in its own window** on any panel starts a second instance
showing only that panel, so you can keep the disk graph beside a build
terminal. The windows are independent: each samples on its own, and closing
one leaves the others running. This is also what `--panel` does from a
terminal:

```sh
qindaqt-system-monitor --panel network
```

Panel names are `dashboard`, `cpu`, `memory`, `disks`, `network`,
`processes` and `hardware`.

## Managing processes

Select a process to see its command line, resource use, thread count, nice
value and state, and to act on it. **Terminate** asks it to exit; **Kill**
stops it immediately and loses unsaved work; **Pause** and **Continue**
suspend and resume it. The **Nice** field changes scheduling priority —
lower runs sooner, and going below zero needs privileges. The same actions
are on the right-click menu of any row.

Actions are bound to process identity, not to a PID alone: a PID is reused,
and a signal sent to a recycled number would hit whatever inherited it.

Use the filter to match on name, command line, user or PID. The tree button
folds processes under their parents; the user button narrows the table to
your own processes.

## Controlling the sampling

**View → Pause updates** (`Ctrl+Alt+P`) freezes sampling while keeping what
is already on screen. **View → Refresh now** (`F5`) takes one sample. The
interval — Fast (500 ms), Normal (1 s) or Relaxed (3 s) — trades freshness
for sampling work. Hardware readings are collected on their own, slower
cadence: GPU and sensor files are slower to read than procfs, and some of
them wake a device.

## Reading the numbers

CPU percentages use the whole machine's capacity as 100%, in the process
table too. Memory in the process table is resident memory (RSS); shared
pages appear in more than one process, so adding those rows does not give
the machine's used-memory total.

Disk and network figures are rates in bytes per second, and use binary units
because every counter behind them is binary. The first reading establishes a
baseline, so a rate is unavailable until a second sample exists.

**An unavailable reading shows a dash, never a zero.** "This process did no
I/O" and "procfs would not tell us" are different statements about your
machine, and so are "this GPU drew no power" and "this driver does not
report power". Missing process I/O permissions, unsupported driver counters
and absent sensors all stay blank.

The filesystem list hides mounts with no meaningful capacity — the
pseudo-filesystems and the one-megabyte credential mounts — because twenty
rows of "0 B free" bury the four mounts worth watching. The eye button in the
Storage header puts them back.

## Implementation boundary

The [detachable-view decision](../adr/0108-compose-system-monitor-from-detachable-views.md)
defines ownership, and
[ADR-0220](../adr/0220-rebuild-the-system-monitor-on-qindatk.md) records why
the interface is QindaTK. Linux sampling and process actions belong to the
app's core; GPU and sensor adapters to its hardware module. The interface
consumes published snapshots and process models and never reads procfs
itself. Sampling runs away from the GUI thread and retains bounded history.

`--grab <file.png>` renders one frame and exits, which is how the interface
is reviewed and tested without a display.

## Installing and opening System Monitor

The Gentoo package is `gui-apps/qindaqt-system-monitor` in the QindaQt
overlay. It depends on the desktop's shared runtime and on
`dev-libs/qindatk`, and installs its own executable, launcher and icon. It
does not replace the compositor.

```sh
emerge --ask gui-apps/qindaqt-system-monitor
qindaqt-system-monitor
```

Open **QindaQt System Monitor** from the application launcher, or run the
command above. A desktop restart is not needed after installation.

## Design and data references

Counter semantics follow the kernel's
[procfs documentation](https://www.kernel.org/doc/html/latest/filesystems/proc.html)
and [I/O statistics reference](https://www.kernel.org/doc/html/latest/admin-guide/iostats.html).
AMD readings follow the driver's
[monitoring interface](https://www.kernel.org/doc/html/latest/gpu/amdgpu/thermal.html).
NVIDIA readings use NVML when it is already installed with the driver.
Installing System Monitor does not install or change a graphics driver.
Intel engine utilization is not currently collected.
