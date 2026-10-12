# nuc8-led

Linux kernel module for controlling the front LED on Intel NUC 8th Generation Mini PCs (Bean Canyon).

## Background

Older tools (such as the legacy `intel_nuc_led` driver from 2017) rely on the ACPI WMI method `SLED`. On NUC 8 models, the BIOS hardcodes `SLED` to return `0xE1` (`NOSUPPORT`), preventing userspace control through WMI.

This driver bypasses ACPI by communicating directly with the Embedded Controller via MMIO mapped at `0xFE410500`.

### Tested Hardware
- Intel NUC8i3BEK / NUC8i3BEH
- Intel NUC8i5BEK / NUC8i5BEH
- Intel NUC8i7BEH
- Tested on Debian 12 / 13 and Linux kernels 5.x, 6.x, and 7.x.

## Installation

### Prerequisites (Debian/Ubuntu)
```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r) dkms git
