# hwckr

hwckr is a Linux hardware information and monitoring tool, inspired by HWiNFO64, written in C++ with Qt6.

## Features

**Summary**
- Live cards for CPU usage, clock speed, temperature and memory
- Device tree: CPU, motherboard/BIOS, memory, graphics cards, drives, network adapters, OS
- Detail panel for each device, including the CPU's supported instruction sets

**Sensors**
- Every sensor the kernel exposes: temperatures, fans, voltages, power, clocks, usage
- Current / Minimum / Maximum / Average columns plus a live history graph
- Drive read/write rates and network download/upload rates
- NVIDIA GPU sensors via `nvidia-smi` (when the proprietary driver is installed)
- Temperatures and loads turn amber, then red, as they climb
- Filter box, pause, adjustable refresh rate, reset min/max
- Log all sensors to a CSV file

## Requirements

Ubuntu / Debian / Mint:
```bash
sudo apt install build-essential cmake qt6-base-dev
```

Fedora:
```bash
sudo dnf install gcc-c++ make cmake qt6-qtbase-devel
```

Arch:
```bash
sudo pacman -S base-devel cmake qt6-base
```

Optional, for more sensors (motherboard fans and voltages):
```bash
sudo apt install lm-sensors
sudo sensors-detect
```

Optional, for GPU names in the Summary page: `pciutils` (provides the PCI ID database).

## Build and run

```bash
./build.sh   # compile
./run.sh     # run without compiling
./dev.sh     # compile and run
```

## Project layout

| File | What it does |
|---|---|
| `src/sensorreader.*` | Reads live sensor values from `/sys` and `/proc` (no Qt) |
| `src/systeminfo.*` | Reads static hardware info for the Summary page (no Qt) |
| `src/sensorstab.*` | Sensors table, graphs and CSV logging |
| `src/summarytab.*` | Summary page: live cards, device tree, details |
| `src/mainwindow.*` | Window, header bar, status bar, refresh timer |
| `src/theme.*` | Colours, stylesheet and the drawn device icons |

## Planned
- Low-level CPU detection using Assembly (CPUID)
- Per-DIMM memory details
