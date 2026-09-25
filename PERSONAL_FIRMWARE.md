# Personal CORE One firmware (6.5.7)

This branch starts from Prusa Firmware Buddy `v6.5.7` (`7119a302d6d0bc144c57b631d778656c8a2745f6`). It contains only the personal CORE One behavior described below. The bundled `lib/Prusa-Firmware-MMU` source is not modified; MMU firmware remains the stock version bundled by Buddy 6.5.7.

## MMU filament runout

- FINDA loss during an active MMU print beeps once and records the active slot. Printing continues while the filament tail in the Bowden tube reaches the extruder.
- Extruder ADC loss beeps, parks the nozzle, skips the already-impossible unload/eject, reconciles the MMU state, and loads the recorded slot directly.
- The recovery load keeps the normal obstruction/load-cell checks and caps forward loading motion at 3 mm/s. The temporary MMU pulley rate is restored afterward.
- A tool change or manual filament change cannot move the selector while a runout is pending. If the ADC still sees the tail, the printer parks and asks for manual removal before recovery.
- The selected slot survives power panic. Automatic power-panic continuation is disabled while this special recovery is pending so it cannot resume an empty print.
- MMU startup, setup, calibration, and non-print operations retain the stock 6.5.7 FINDA behavior.

## Custom filtration

Select **Custom filtration** as the chamber filtration backend when fan 3 is the internal recirculation/filter fan:

- rear chamber fans continue to regulate chamber temperature by exhausting air;
- fan 3 follows filtration demand independently and recirculates chamber air;
- emergency temperature overrides still apply;
- chamber fan self-test includes all three fans only in this mode.

## Build

From PowerShell in the repository root:

```powershell
.\utils\build_custom_coreone.ps1
```

The helper uses the repository's pinned toolchain and builds the CORE One release image with `--bootloader yes`. Its primary output is:

```text
build/coreone_release_boot/firmware.bbf
```

Copy the reviewed artifact from `build/personal-artifacts/coreone-6.5.7/` to a USB drive and install it through the printer's normal USB firmware-update procedure.

## Hardware acceptance checks

Before relying on the firmware for long prints:

1. Boot twice and confirm the MMU starts normally.
2. Run the printer filament-sensor calibration and MMU calibration.
3. Run the chamber fan self-test in the stock and Custom filtration modes.
4. Confirm rear fans respond to a chamber-temperature target while fan 3 follows filtration demand.
5. Simulate FINDA loss during a short MMU print; confirm one beep and continued printing.
6. Let the tail leave the extruder ADC; confirm parking and direct same-slot loading without unload/eject.
7. Repeat with a tool-change command arriving while the tail still reaches the ADC; confirm the selector does not move until the path is cleared.
8. Test Stop and one controlled power interruption during recovery before using unattended.

## Keeping the fork current

`origin` is the personal fork and `upstream` is Prusa's fetch-only repository. Keep custom release branches intact and port them to a fresh branch when adopting another upstream release:

```powershell
git fetch upstream --tags
git switch -c codex/personal-coreone-<version> <upstream-tag>
git cherry-pick <personal-commit-1> <personal-commit-2>
git push -u origin codex/personal-coreone-<version>
```

Build and repeat the hardware acceptance checks after every port. Do not merge this branch into an upstream checkout or open an upstream pull request.
