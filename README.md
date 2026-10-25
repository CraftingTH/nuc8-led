# nuc8-led

Linux kernel module for controlling the front indicator LED on Intel NUC 8th Generation Mini PCs (Bean Canyon).

## Background

Legacy tools like the 2017 `intel_nuc_led` driver depend on the ACPI WMI `SLED` method. On Intel NUC 8 boards, the BIOS hardcodes `SLED` to return `0xE1` (`NOSUPPORT`), completely disabling userspace control through standard WMI.

This module talks directly to the Embedded Controller (EC) via memory-mapped I/O (MMIO at `0xFE410500`), enabling software-controlled LED management across modern Linux kernels (5.x, 6.x, and 7.x+).

### Tested Hardware
- Intel NUC8i3BEK / NUC8i3BEH
- Intel NUC8i5BEK / NUC8i5BEH
- Intel NUC8i7BEH
- OS: Debian 11/12/13, Ubuntu, Proxmox VE, OpenMediaVault

---

## BIOS Configuration

Before using this driver, you must hand control of the LED over to software in the BIOS:

1. Power on or reboot your NUC and repeatedly press **F2** to enter BIOS setup.
2. Navigate to:
   **Advanced** -> **Power** -> **Secondary Power Settings**
3. Look for the **LED** configuration section.
4. Set **HDD LED** to **Software Control** (SW Control).
5. Press **F10** to save changes and reboot into Linux.

*(Note: If left on "HDD Activity", the Embedded Controller hardware will keep overriding the LED).*

---

## Installation

### Prerequisites (Debian / Ubuntu / Proxmox / OMV)

Install build tools, DKMS, and the kernel headers:

```bash
sudo apt update
sudo apt install -y build-essential linux-headers-amd64 linux-headers-$(uname -r) dkms git
```

*(Note: `linux-headers-amd64` ensures that future kernel upgrades automatically pull matching headers so DKMS can rebuild the driver).*

### Option 1: DKMS (Recommended)

Installs the driver and automatically rebuilds it across kernel upgrades:

```bash
git clone https://github.com/CraftingTH/nuc8-led.git
cd nuc8-led
sudo cp -r . /usr/src/nuc8-led-1.1.0
sudo dkms add -m nuc8-led -v 1.1.0
sudo dkms build -m nuc8-led -v 1.1.0
sudo dkms install -m nuc8-led -v 1.1.0
echo "nuc_led" | sudo tee /etc/modules-load.d/nuc_led.conf
sudo modprobe nuc_led
```

### Option 2: Manual Build

If you prefer to compile and test manually without DKMS:

```bash
git clone https://github.com/CraftingTH/nuc8-led.git
cd nuc8-led
make
sudo insmod nuc_led.ko
```

Verify the driver is active:

```bash
cat /proc/nuc_led
```

---

## Usage

Control the LED by writing parameters directly to `/proc/nuc_led`.

### Command Syntax

```bash
# Quick syntax (solid color)
echo '<brightness>,<color>' > /proc/nuc_led

# Full syntax
echo 'hdd,<brightness>,<mode>,<color>' > /proc/nuc_led
echo 'hdd,<brightness>,blink,<frequency>,<color>' > /proc/nuc_led
echo 'hdd,<brightness>,breathe,<frequency>,<color>' > /proc/nuc_led

# Restore hardware disk activity
echo 'hdd,auto' > /proc/nuc_led
```

### Parameters

- **Brightness:** `0` (off) to `100` (full brightness)
- **Colors:** `green`, `red`, `blue`, `yellow`, `amber`, `purple`, `white`
- **Modes:**
  - `solid` - Continuous light
  - `blink` - Symmetrical 50/50 square-wave flash (kernel timer)
  - `breathe` - Sinusoidal breathing (hardware PWM)
  - `fade` - Sawtooth ramp-down (hardware pulse)
  - `auto` - Restores hardware disk activity
- **Frequencies** (for `blink` and `breathe`):
  - `fast` / `2.0hz`
  - `normal` / `1.0hz` (default)
  - `slow` / `0.5hz`

### Examples

```bash
# Solid green at 100%
echo '100,green' > /proc/nuc_led

# Solid red dimmed to 10%
echo '10,red' > /proc/nuc_led

# Blinking green at 1 Hz
echo 'hdd,100,blink,1.0hz,green' > /proc/nuc_led

# Fast flashing red alert (2 Hz)
echo 'hdd,100,blink,fast,red' > /proc/nuc_led

# Slow breathing blue
echo 'hdd,100,breathe,slow,blue' > /proc/nuc_led

# Turn LED off
echo '0,green' > /proc/nuc_led

# Restore hardware disk activity
echo 'hdd,auto' > /proc/nuc_led
```

---

## Uninstallation

To completely remove the DKMS module from your system:

```bash
sudo dkms remove -m nuc8-led -v 1.1.0 --all
sudo rm -rf /usr/src/nuc8-led-1.1.0
sudo rm -f /etc/modules-load.d/nuc_led.conf
sudo rmmod nuc_led
```

---

## License

GPL-2.0
