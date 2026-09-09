# ZMK Holyiot Boards

ZMK board support for the **HolyIOT YJ-17120** (nRF52840), now provided as a
**ZMK MCU interconnect board** (nice!nano / nice!nano style).

The YJ-17120 board itself carries **no keymap and no key matrix**: it is a bare
nRF52840 interconnect with a USB device controller, the BLE radio, and a red
power LED. The **kscan / keymap / split role** are all defined by a **shield**
that is stacked on it. This mirrors how `nice_nano//zmk` is consumed together
with a keyboard shield (kyria, corne, m60, ...).

This module ships a **tester shield** (`yj17120_tester`) so that an
out-of-the-box build target exists that produces a USB HID keyboard with a
tiny placeholder matrix, and can be used to smoke-test that the board + USB +
kscan paths all bind and link correctly.

## Quick start

1. Set up the [zmk-workspace](https://github.com/Thunderbird2086/zmk-workspace) repository with Docker, the `devcontainer` CLI, and a ZMK checkout in `zmk/`.
2. Place this module at `zmk-modules/zmk-holyiot-board`.
3. Build from the `zmk-workspace` root using the tester-shield target:

   ```bash
   ./build-docker.sh -b yj17120//zmk -S yj17120_tester \
       -d build/holyiot_yj17120_tester \
       -c non-nemo-zmk-config \
       -e zmk-holyiot-board
   ```

   `-S yj17120_tester` passes `-DSHIELD=yj17120_tester` to `west build`.
   The `build-docker.sh` wrapper in this repo has been extended with that
   `-S` option; the older `-b yj17120` (no variant) form will no longer build
   with a valid keymap since the board no longer ships one.

4. Flash `zmk/build/holyiot_yj17120_tester/zephyr/zmk.hex` with the Programmer **Write** action. Do not use **Erase & Write** for a routine application update.
5. Reset the board and verify that it enumerates as a USB HID keyboard (`YJ17120 Tester`).

The existing SoftDevice and OpenDFU bootloader are reused by this build. See [Full provisioning](#full-provisioning) only if the chip has been erased or the SoftDevice is missing.

## Choosing a shield

The board ships in the `zmk` variant only (`yj17120//zmk`). Any ZMK shield
that can reference the `yj17120` interconnect id can be built on top of it.
The tester shield is one such example:

```
zmk-modules/zmk-holyiot-board/boards/
  holyiot/yj17120/            # HWM v2 board: name=yj17120, vendor=holyiot, nrf52840/zmk
  shields/yj17120_tester/     # tester shield (placeholder 1x1 matrix on gpio0.4/gpio0.5)
```

Other shields (or a real YJ-17120 keyboard shield) can be added under
`boards/shields/<name>/` of any ZMK extra module referenced via `-e`.
Zephyr v4's `shields.cmake` resolves shields by `boards/shields/<shield>/Kconfig.shield`
+ `<shield>.overlay` across all `BOARD_ROOT` roots.

## Layout

```
boards/holyiot/yj17120/
  board.yml                     HWM v2 board descriptor (board name yj17120, variant zmk)
  Kconfig.yj17120               selects SOC_NRF52840_QIAA + ZMK_BOARD_COMPAT
  board.cmake                   nrfjprog flash runner args
  yj17120.dts                   base (HWM v1 fallback) devicetree — no kscan/chosen-zmk
  yj17120_nrf52840_zmk.dts      ZMK variant devicetree (leds, usbd, radio, flash partitions, uart0 disabled)
  yj17120_nrf52840_zmk_defconfig  ZMK_USB / ZMK_BLE / code-partition / NVS (no split role, no keymap)
  holyiot_yj17120.yaml          HWM v1 fallback descriptor
  board.cmake                   board_runner_args(nrfjprog "--nrf-family=NRF52")
boards/shields/yj17120_tester/
  Kconfig.shield                SHIELD_YJ17120_TESTER
  Kconfig.defconfig             ZMK_KEYBOARD_NAME="YJ17120 Tester" / ZMK_KSCAN_MATRIX_POLLING
  yj17120_tester.overlay        1x1 matrix on gpio0.4 (row) / gpio0.5 (col) + transform + physical layout
  yj17120_tester.keymap         single &kp A binding
  yj17120_tester.conf           (empty; defconfig carries all options)
  yj17120_tester.zmk.yml        shield metadata (id yj17120_tester, requires yj17120)
src/
  CMakeList.txt                 target_sources(app PRIVATE power_led.c)
  power_led.c                   SYS_INIT drives DT_ALIAS(led1) high at boot
zephyr/module.yml               build.settings.board_root = .
```

## Flash layout

The device ships with a **SoftDevice (S140 v7.3.0)** and an **OpenDFU** USB
bootloader. ZMK is linked into the application partition only; the MBR,
SoftDevice and OpenDFU regions are left untouched.

```
0x00000000 - 0x00000FFF   MBR
0x00001000 - 0x00026FFF   SoftDevice S140 v7.3.0
0x00027000 - 0x000D2FFF   ZMK application      (app ORIGIN = 0x27000)
0x000D3000 - 0x000DFFFF   ZMK settings (NVS / FCB)
0x000E0000 - 0x000FDFFF   OpenDFU USB bootloader (protected)
0x000FE000 - 0x000FFFFF   UICR
```

The app start address (`0x27000`) is driven by `CONFIG_USE_DT_CODE_PARTITION=y`
plus the `zephyr,code-partition` chosen node in
`yj17120_nrf52840_zmk.dts`. After any local build, sanity-check:

```bash
grep "FLASH (rx)" zmk/build/holyiot_yj17120_tester/zephyr/linker.cmd
# expect: FLASH (rx) : ORIGIN = (0x0 + 0x27000), LENGTH = (0xac000 - 0x0)
```

## SoftDevice

ZMK BLE on nRF52840 runs on top of the **Nordic S140 SoftDevice** (v7.3.0),
which lives in the `sd_partition` (`0x00001000–0x00026FFF`). It is **already
present on the board** and is not part of the ZMK `.hex`.

- Use `nrfjprog` / nRF Connect Programmer **Write** for routine app updates.
- Do **not** use **Erase & Write** for routine application updates (the SoftDevice is in `0x00001000`–`0x00026FFF`).
- If the SoftDevice is moved or replaced, update the `sd_partition` /
  `code_partition` `reg` tuples in `yj17120_nrf52840_zmk.dts` and rebuild;
  then re-check the `FLASH (rx)` ORIGIN in `linker.cmd`.

## Build

From the `zmk-workspace` root (where `build-docker.sh` lives):

```bash
./build-docker.sh -b yj17120//zmk -S yj17120_tester \
    -d build/holyiot_yj17120_tester \
    -c non-nemo-zmk-config \
    -e zmk-holyiot-board
```

Notes:

- `-b yj17120//zmk` — HWM v2 board name with the `zmk` variant.
- `-S yj17120_tester` — pass a shield; the wrapper forwards it as `-DSHIELD=...`.
- `-e zmk-holyiot-board` is required so the module (and thus its board +
  shield) are loaded into the build as `ZMK_EXTRA_MODULES`.

Artifact:

```
zmk/build/holyiot_yj17120_tester/zephyr/zmk.hex
```

### Alternate (in-container) invocation

From inside the devcontainer:

```bash
west build -s app -d build/holyiot_yj17120_tester -b yj17120//zmk \
  -- -DSHIELD=yj17120_tester \
     -DZMK_CONFIG=/workspaces/zmk-config/non-nemo-zmk-config/config \
     -DZMK_EXTRA_MODULES=/workspaces/zmk-modules/zmk-holyiot-board
```

## Flash with nRF Connect for Desktop (Programmer)

The YJ-17120 presents a standard **nRF52840 USB** device, so it flashes over
USB with the **Programmer** app from *nRF Connect for Desktop* — no J-Link /
external debugger required.

1. Connect the board over **USB**.
2. Add `/workspaces/zmk/build/holyiot_yj17120_tester/zephyr/zmk.hex` and press **Write**.
3. **Reset** the board to run the new app (unplug/replug also works).

### Full provisioning

Use only for a new or fully erased chip. Obtain the S140 and OpenDFU images,
restore them in order, then flash `zmk.hex` with the application-only
procedure above.

## Verify on macOS

```bash
ioreg -l -w0 -r -c IOHIDInterface | grep -A1 -i "product\|serial"
```

Expect a HID interface with Vendor `ZMK Project`, product string
**`YJ17120 Tester`** (set by `CONFIG_ZMK_KEYBOARD_NAME` in the tester-shield
`Kconfig.defconfig`).

### Notes on behavior

- **Board-only build (no shield)** is not currently supported: the base
  `zmk`-variant defconfig intentionally carries no split-role flags and the
  board devicetree does not bind a `zmk,kscan` node. Use the tester shield
  (or a real keyboard shield) to produce a linkable firmware image.
- **LED1** (P1.01) is driven high at boot as a "powered / running" indicator
  by `src/power_led.c`.
- **Console/UART** is disabled in the `zmk` variant devicetree to keep flash
  usage minimal. If debug output is required from the tester shield, enable
  `zephyr,console` in the tester `.overlay` and add `CONFIG_UART_CONSOLE=y`
  to the tester `.conf`.

## Open

- Actual interconnect pinout / connector is still pending. The tester shield
  uses `gpio0.4` (row) and `gpio0.5` (col) as the placeholder; replace these
  in `boards/shields/yj17120_tester/yj17120_tester.overlay` once the real
  YJ-17120 shield connector is defined.
