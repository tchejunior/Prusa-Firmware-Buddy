# Personal CORE One firmware

This branch starts at upstream `v6.10.1` (`1ce23f33e`). It builds the COREONE
target from that source tree; it is not an official Prusa CORE One release.
Changes are maintained at <https://github.com/tchejunior/Prusa-Firmware-Buddy>,
on `codex/personal-coreone`. No upstream pull request is required.

## MMU runout

With an enabled MMU and ADC extruder filament sensor:

1. FINDA becoming empty during printing emits one beep. Printing continues with
   the filament remaining in the tube. Sound follows the printer sound settings.
2. The pending slot stays latched even if FINDA flickers or new filament covers it.
3. When the extruder sensor becomes empty, the printer interrupts the print,
   beeps, parks, and opens replacement loading for the same slot. It skips the
   normal unload/eject workflow. Invalid sensors cause a pause rather than
   assuming the filament path is clear.
4. The printer checks the MMU selector slot and resets the MMU's remembered
   filament position through existing registers. It requires an empty extruder
   sensor before attempting this. A tool change arriving before the tail has
   cleared instead asks the user to clear the tube/extruder gears first.
5. The replacement uses a temporary 3 mm/s slow feed profile through the extruder
   and final positive loading moves, retaining loading obstruction detection.
   Normal MMU loading rates are restored afterward. Successful loading and ADC
   filament presence are required before clearing the pending runout.

Custom natural runout uses manual refill of the exhausted slot; it does not
automatically select another slot through Spool Join. Ordinary manual M600,
non-ADC printer runout, and normal printing loadcell stall detection retain their
existing behavior. A pending runout is completed before ordinary T/tool-change
commands can switch slots.

The pending slot is stored in the configuration journal. Power-panic recovery
requires confirmation and waits for print restoration before requesting M600.
If the MMU selector no longer matches after restarting, recovery refuses to move
it automatically. Use **Abort** on the recovery error screen, clear the filament
path and reload before starting another print. This conservative case cannot
always resume the interrupted print.

## Internal filtration with separate rear cooling

Select **Custom filtration** in the existing **Chamber Filtration** hardware
configuration selector. It is available with the xBuddy extension enabled.
Keep the chamber cooling and filtration fan controls on **Auto** for normal use.

- The rear cooling fan pair remains on the cooling outputs. Its automatic demand
  follows chamber temperature, without the exhaust-filter speed multiplier.
- The internal recirculating fan remains on the filtration output. Its automatic
  demand follows the existing filtration on/off, speed and post-print settings.
  Filtration alone does not require exhausting warm chamber air.
- Manual fan controls, fan startup assistance and emergency overheating behavior
  remain available. Thermal protection may still force rear exhaust cooling.
- Fan selftest and fan checking include both rear fans and the internal fan.
- The filter reminder retains the DIY 600-hour estimate; it is not a measured
  service life for a custom filter.

Before reverting to stock firmware, select **None** or a supported stock
filtration backend. The new backend has persistent ID 4, which older firmware
does not understand. Do this while the custom firmware is still installed.

## Repository workflow

Local remotes are configured as follows:

| Remote | Purpose | Push |
| --- | --- | --- |
| `origin` | Personal GitHub fork | Allowed; default destination |
| `upstream` | `prusa3d/Prusa-Firmware-Buddy` | Disabled locally |

`push.default=current` and `remote.pushDefault=origin` keep ordinary pushes on
the personal branch. `rerere.enabled=true` remembers conflict resolutions.
These settings belong to the local clone; configure them again for a new clone.

For a future upstream update, preserve this working release branch and port the
small personal commit series onto a new branch. Start with a clean worktree:

```powershell
git fetch upstream --tags
git log --oneline v6.10.1..codex/personal-coreone
# Replace NEW_TAG with the chosen compatible upstream release tag.
git switch -c codex/coreone-NEW_TAG NEW_TAG
# Apply the personal commits shown above, oldest first:
git cherry-pick COMMIT_1 COMMIT_2 COMMIT_3
# Resolve conflicts, rebuild, review and run the acceptance checks below.
git push -u origin HEAD
```

Choose a release deliberately; fetching does not upgrade the custom branch.
When an upstream fix replaces a personal patch, omit or adapt that patch.
Keep the old branch and matching build artifacts until the replacement passes
hardware checks. There is no need to force-push or open a PR against Prusa.

## Building and validation

### Prusa Connect compatibility investigation

Connect rejected this custom build first as not supporting binary G-code, then
as not supporting file transfer even with plain G-code. The firmware contains
both implementations. As a diagnostic compatibility change, builds whose suffix
starts with `-custom` now report the actual base version (`6.10.1`) to Connect.
The printer UI and crash identification keep `6.10.1-custom+16383`; upstream
release and prerelease builds keep their original Connect version reporting.

This targets a suspected cloud version-classification problem. It is not a
confirmed fix: after flashing, verify that Connect receives `6.10.1`, then test
transfer to the printer with both `.gcode` and `.bgcode` (upload-only success is
insufficient). If transfer is still rejected, investigate Connect's printer
model/version support rather than claiming the firmware lacks file transfer.

The diagnostic artifact is
`build/personal-artifacts/connect-version-test/coreone-custom-connect-test-with-bootloader.bbf`.
The prior BBF and matching ELF/map are preserved under
`build/personal-artifacts/before-connect-version-test/` in this checkout.

### Build commands

Use the repository's `utils/bootstrap.py` and pinned dependencies. On Windows,
the convenience wrapper temporarily materializes Git symlink stubs as junctions
or file copies and restores their original bytes in `finally`:

```powershell
.\utils\build_custom_coreone.ps1
```

GNU gettext `msgfmt` must also be on PATH. This checkout has verified
[MSYS2 clang64 gettext packages](https://packages.msys2.org/packages/mingw-w64-clang-x86_64-gettext-tools)
under `.dependencies/msys2-gettext/`; the wrapper recognizes that local location.
For another machine, install gettext with its runtime, libtextstyle and libiconv
dependencies. These downloaded binaries are not committed.

It uses the local `.venv`, Python UTF-8 mode, COREONE Release, bootloader support,
and version suffix `-custom+16383`. Build outputs are in
`build/coreone_release_boot/`. Preserve the matching ELF with the BBF for crash
analysis. The wrapper does not flash the printer or upload firmware.

The isolated host suite uses the actual runout state machine and cooling code:

```text
cmake -S tests/custom_firmware -B build/custom-tests -G Ninja
cmake --build build/custom-tests
ctest --test-dir build/custom-tests --output-on-failure
```

Use a C++23 compiler (the Windows verification used LLVM MinGW). These tests
cover pending-runout state transitions, simultaneous sensor loss, missed FINDA
edges during locks, invalid sensors, power-panic readiness/NO_TOOL state, separate
fan demands and thermal overrides. They do not simulate the complete M600,
Marlin print state machine, MMU protocol, motors, heaters or loadcell.

An independent agent reviewed recovery, protocol and filtration changes.
Findings about duplicate FINDA handling, lost events, register reply checking,
different-slot tip shaping, saved-slot recovery, power-panic timing and abort
state handling were corrected. Hardware acceptance remains outstanding:

- Warm controlled test: FINDA warning only, then ADC pause and direct same-slot
  reload; verify remnant feed, obstruction retries, purging and print resumption.
- FINDA flicker/reinsertion, failed sensor, disabled MMU, manual M600, tool change
  while a tail remains, and recovery followed by a different-slot T command.
- Abort from both natural runout and a tool-change-triggered recovery screen.
- Power loss before/after MMU state reset and during partial replacement loading;
  verify confirmation, safe parking, sensor checks and selector-mismatch refusal.
- Rear fan pair and internal fan selftest, independent manual/Auto controls,
  chamber heating with filtration, post-print filtration and thermal overrides.

The standalone sibling `Prusa-Firmware-MMU` repository is the source of truth
for MMU firmware. Do not modify/build Buddy's embedded MMU copy for that purpose.
This feature uses the existing MMU 3.0.4 protocol and needs no MMU source patch.
