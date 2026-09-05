# ZMK Holyiot Boards

ZMK board support for the **HolyIOT YJ-17120 USB Dongle** (nRF52840).

The YJ-17120 is a **keyless BLE split-central dongle**: it has no keys of its own. It pairs with one or more BLE split-keyboard peripherals and forwards their keystrokes to the host over **USB HID**. It runs ZMK in `ZMK_SPLIT_ROLE_CENTRAL`.

## Quick start

1. Set up the [zmk-workspace](https://github.com/Thunderbird2086/zmk-workspace) repository with Docker, the `devcontainer` CLI, a ZMK checkout in `zmk/`, and a ZMK config repository in `zmk-modules/`.
2. Place this module at `zmk-modules/zmk-holyiot-board` and build from the `zmk-workspace` root:

  ```bash
  ./build-docker.sh -b yj17120 -d build/holyiot_yj17120 -c non-nemo-zmk-config -e zmk-holyiot-board
  ```

3. Flash `zmk/build/holyiot_yj17120/zephyr/zmk.hex` with the Programmer **Write** action. Do not use **Erase & Write** for a routine application update.
4. Reset the dongle and verify that it enumerates as a USB HID keyboard.

The existing SoftDevice and OpenDFU bootloader are reused by this build. See [Full provisioning](#full-provisioning) only if the chip has been erased or the SoftDevice is missing.

## Prerequisites

- Docker and the [`devcontainer` CLI](https://github.com/devcontainers/cli).
- The [zmk-workspace](https://github.com/Thunderbird2086/zmk-workspace) directory layout, including `zmk/` and `zmk-modules/`.
- A ZMK config repository such as `non-nemo-zmk-config` under `zmk-modules/`.
- This module under `zmk-modules/zmk-holyiot-board`.

The `-e zmk-holyiot-board` argument tells `build-docker.sh` to load this module's board and `led.c` implementation.

## Layout

```
boards/holyiot/yj17120/
  board.yml           HWM v2 board descriptor (name yj17120, vendor holyiot, nrf52840)
  Kconfig.yj17120     selects SOC_NRF52840_QIAA + ZMK_BOARD_COMPAT
  board.cmake         nrfjprog flash runner args
  CMakeLists.txt      compiles led.c into the app
  led.c               drives LED1 (P1.01) high at boot
  yj17120.dts         devicetree (flash partitions, mock kscan, USB, LED)
  yj17120.keymap      single no-op binding (keyless central)
  yj17120_defconfig   ZMK_USB / ZMK_BLE / ZMK_SPLIT_ROLE_CENTRAL / code-partition / NVS
  holyiot_yj17120.yaml HWM v1 fallback descriptor
zephyr/module.yml     build.settings.board_root = .
```

## Flash layout

The device ships with a **SoftDevice (S140)** and an **OpenDFU** USB bootloader. ZMK is linked into the application partition only; the MBR, SoftDevice and OpenDFU regions are left untouched.

```
0x00000000 - 0x00000FFF   MBR
0x00001000 - 0x00026FFF   SoftDevice S140
0x00027000 - 0x000D2FFF   ZMK application      (app ORIGIN = 0x27000)
0x000D3000 - 0x000DFFFF   ZMK settings (NVS / FCB)
0x000E0000 - 0x000FDFFF   OpenDFU USB bootloader (protected)
0x000FE000 - 0x000FFFFF   UICR
```

The app start address (`0x27000`) is driven by `CONFIG_USE_DT_CODE_PARTITION=y` plus the `zephyr,code-partition` chosen in `yj17120.dts`. If you change the SoftDevice and it moves the app start, update the `code_partition` `reg` offset/size in `yj17120.dts` and the `sd_partition` size, then rebuild.

> This layout assumes the **0x27000** app start. When the dongle was first mapped the app started at `0x23000`; after a newer SoftDevice it moved to `0x27000`.

## SoftDevice

ZMK BLE on nRF52840 runs on top of the **Nordic S140 SoftDevice**, which lives in the `0x00001000` partition (the `sd_partition`). It is **already present** on the dongle. Routine ZMK application builds reuse the existing SoftDevice and do not include it:

```
CONFIG_ZMK_BLE=y
CONFIG_BT_CTLR default (nRF52840 SoftDevice)
```

This layout targets **S140 v7.3.0**, whose **application start address is `0x00027000`**. The installed SoftDevice must match the partition layout below.

Key points:

- **Do not erase the `0x00001000–0x00026FFF` region** on every flash. Only the application (`0x27000…`) is safe to rewrite via normal flashing. If you do a *full* chip erase (e.g. `nrfjprog --eraseall` over SWD), see [Full provisioning](#full-provisioning) before flashing ZMK or BLE will stop working.
- The ZMK `.hex` only contains the application; it is linked to start at `0x27000` and is written over the existing application slot. The SoftDevice and OpenDFU boot regions are left intact.
- If you upgrade the SoftDevice version, its size/end address may change. Confirm the new app start and adjust `yj17120.dts`:

  ```dts
  &flash0 {
      partitions {
          compatible = "fixed-partitions";
          sd_partition: partition@0 {
              reg = <0x00000000 0xNEW_APP_START>;   /* end of MBR+SoftDevice */
          };
          code_partition: partition@NEW_APP_START {
              reg = <0xNEW_APP_START 0xSIZE>;
          };
          ...
      };
  };
  ```

  then rebuild and re-verify the `FLASH (rx)` ORIGIN in `linker.cmd` matches the new start.

## Build

From the `zmk-workspace` root (where `build-docker.sh` lives):

```bash
./build-docker.sh -b yj17120 -d build/holyiot_yj17120 -c non-nemo-zmk-config -e zmk-holyiot-board
```

Note: `-b yj17120` — the board's HWM v2 name has no vendor prefix.
`-e zmk-holyiot-board` is required so the module (and its board + `led.c`) are loaded into the build.

Artifact:

```bash
zmk/build/holyiot_yj17120/zephyr/zmk.hex
```

Sanity-check the link address after a build:

```bash
grep "FLASH (rx)" zmk/build/holyiot_yj17120/zephyr/linker.cmd
# expect: FLASH (rx) : ORIGIN = (0x0 + 0x27000), LENGTH = (0xac000 - 0x0)
```

## Flash with nRF Connect for Desktop (Programmer)

The YJ-17120 presents a standard **nRF52840 USB** device, so it flashes over USB with the **Programmer** app from *nRF Connect for Desktop* — no J-Link / external debugger required.

### One-time setup

1. Install **nRF Connect for Desktop** (ncs) from https://www.nordicsemi.com/SoftwareTools/nRF-Connect-for-desktop and add the **Programmer** module.
2. Open Programmer and, on first run, add the board if it isn't offered:
   - Device to flash: choose **nRF52840** (or the USB / `nrf52840` target).

### Flash the firmware (application / SoftDevice-safe)

The dongle's **OpenDFU bootloader** and **SoftDevice** are reserved/protected regions and must **not** be erased during a routine application flash. `zmk.hex` already targets only the application slot, so use **Write** without a full-chip erase.

1. Connect the dongle to the computer over **USB**.
2. In Programmer:
   - Select the connected **nRF52840** device.
   - **Click** `Add Files` and **Browse** to `zmk/build/holyiot_yj17120/zephyr/zmk.hex`.
   - Press **Write**.
3. After flashing, **reset** the dongle to run the new app (Programmer may offer "Reset", or simply unplug/replug the USB cable).

### Full provisioning

Use this procedure only for a new or fully erased chip. A full erase removes the existing MBR, SoftDevice, and OpenDFU bootloader. Obtain the matching S140 and OpenDFU images from the device vendor or [Nordic Semiconductor](https://www.nordicsemi.com/Products/Development-software/S140/Download), restore them in the required order, and then flash `zmk.hex` with the application-only procedure above. Do not use this procedure for normal firmware updates.

If you change the SoftDevice version, its size/end address may change. Update the `sd_partition` and `code_partition` values in `yj17120.dts`, rebuild, and confirm the `FLASH (rx)` ORIGIN in `linker.cmd` matches the new application start.

### SoftDevice / layout caveat

- Normal app flashing over USB **preserves the SoftDevice** (`0x00001000`) and OpenDFU (`0x000E0000`); only the `0x27000` application region is written. This is the important part of the memory layout — the app start is **not** address `0`.
- If you **do** change the SoftDevice, or must use an SWD programmer (J-Link / ST-Link) and run a **full erase**, follow [Full provisioning](#full-provisioning) before the ZMK application. Over USB/DFU this is unnecessary.
- Verify the link address so the app really lands at `0x27000`:

  ```bash
  grep "FLASH (rx)" zmk/build/holyiot_yj17120/zephyr/linker.cmd
  # expect: FLASH (rx) : ORIGIN = (0x0 + 0x27000), LENGTH = (0xac000 - 0x0)
  ```

### If flashing fails

- Make sure nothing else (ZMK Studio, udev rules, `nrfutil`, another Programmer instance) is holding the USB device.
- Prefer the `.hex` file; it already carries the correct absolute base address (`0x00027000`), so no offset needs to be specified.
- If the **Erase & Write** button is greyed out, you are in **DFU mode** — use the **Write** button.
- To enter/exit OpenDFU, **double-tap reset** on the dongle.

## Verify on macOS

The dongle should enumerate as a USB HID **keyboard**:

- VID `0x1D50` (7504), PID `0x615E` (24926), Vendor `ZMK Project`.
- Product string: **`YJ17120`** (set by `CONFIG_ZMK_KEYBOARD_NAME`).

Quick check:

```bash
ioreg -l -w0 -r -c IOHIDInterface | grep -A1 -i "serial\|product"
```

Look for an interface with `PrimaryUsagePage = 1` (Generic Desktop) and `PrimaryUsage = 6` (Keyboard) bound to your dongle's serial.

### Notes on behavior

- **Keyless central**: it only emits keystrokes after a BLE peripheral is paired and connected. With no peripheral attached it still enumerates as a USB keyboard (that's expected) but sends nothing.
- **LED1** (P1.01) is driven high at boot as a "powered / running" indicator.
- **Console/UART** is intentionally disabled to save flash; enable `zephyr,console` in `yj17120.dts` + `CONFIG_UART_CONSOLE=y` if you want debug output.

## Pairing a peripheral

The dongle is the split **central** and has no keys of its own. It can enumerate as a USB keyboard before pairing, but it cannot send keystrokes until at least one split peripheral is connected.

Build the paired keyboard half with `ZMK_SPLIT_ROLE_CENTRAL=n`. For the `non-nemo-zmk-config` repository, the available peripheral targets are `non_nemo_left` and `non_nemo_right` on `xiao_ble`; both are listed in its `build.yaml`.

Flash the peripheral, reset both devices, and use the keyboard's normal ZMK split pairing/reset procedure if it was previously paired to another central. Once connected, key presses from the peripheral should appear through the YJ-17120 USB HID interface.

## ZMK split config (central)

From `yj17120_defconfig`:

```
CONFIG_ZMK_BLE=y
CONFIG_ZMK_SPLIT=y
CONFIG_ZMK_SPLIT_ROLE_CENTRAL=y
CONFIG_ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS=3
CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y
```

The paired peripheral half must be built with `ZMK_SPLIT_ROLE_CENTRAL=n` (peripheral) to pair to this dongle.
