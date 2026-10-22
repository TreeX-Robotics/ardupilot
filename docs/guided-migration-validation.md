# TreeX Guided implementation migration — 2026-10-22

## Source and ownership

Destination: `git@github.com:TreeX-Robotics/ardupilot.git`, local checkout
`/home/nir/dev/treex-ardupilot`, branch `feat-guided-z-heading-options`.
The PR base is `Copter-4.6.2`, imported at exact upstream release commit
`1ebd4d996e8b6bbeb1462a9655c00aa861d5beef`. The fork's 4.8-dev master
was left unchanged. The previous checkout and hardware rollback artifacts were
preserved at the paths recorded in the historical notes.

All 17 implementation/test/historical-document files were compared byte for
byte against the previous worktree during migration. Additional files provide
portable build instructions and the optional CubeOrangePlus-bdshot -Os profile.
The initial hardware/SITL builds used commit
`964e197e480af9b99a909a808d549d7703554966`. Subsequent changes only adjust
a Python test tolerance and documentation; the firmware source remains identical.

## Fresh builds

The owner explicitly authorized local native builds/tests for this migration.
Both builds ran serially from the new checkout, using output directory
`/home/nir/dev/build/treex-ardupilot-guided-options`. No build output was
placed in the checkout; its ignored `build` symlink points to that external path.

```sh
./waf configure --board CubeOrangePlus-bdshot --out /home/nir/dev/build/treex-ardupilot-guided-options --no-submodule-update --extra-hwdef=libraries/AP_HAL_ChibiOS/hwdef/CubeOrangePlus-bdshot/size_optimized.dat
./waf copter -j6
./waf configure --board sitl --out /home/nir/dev/build/treex-ardupilot-guided-options --no-submodule-update
./waf copter -j6
```

Hardware build: PASS (5m2s), GCC ARM 10.2.1. Flash used 1,773,832 bytes;
192,244 bytes free. Packaged image is 1,773,836 bytes including alignment.
APJ board ID **1063** and embedded git identity **964e197e** were checked;
decompressed APJ bytes equal `arducopter.bin` exactly. APJ SHA-256:
`fe24c71cc3f193cf4b33a0dde9d5ba80991da8594f0a1716b40a4f13807b912d`.
The ancillary ABIN generator prints a nonfatal git-directory diagnostic because
its working directory is external; the APJ identity and image were verified
independently. The APJ is the firmware artifact for this task.

SITL build: PASS (7m57s). Both builds include the startup-report source file.
No new hardware flash was requested or performed during migration.

## Fresh behavior measurements

Blocked yaw comparison, with yaw-rate I zeroed after arming to isolate heading
memory (free-rate tracking restores normal I):

| Feedforward | GUID_OPTIONS | Peak heading error (deg) | Release rotation (deg) | Free rate for 20 deg/s demand |
| --- | ---: | ---: | ---: | ---: |
| On | 0 | 45.049438 | 46.17 | 20.031095 |
| On | 512 | 0.019798 | 0.32 | 19.618288 |
| Off | 0 | 45.000132 | 46.16 | 19.981171 |
| Off | 512 | 0.000076 | 0.32 | 19.590266 |

With normal yaw-rate I=0.02 and option 512, peak heading error was 0.019791 deg,
release rotation 15.78 deg, free rate 19.702625 deg/s, and peak PIDY.I 0.022055.
This explicitly demonstrates that inner-loop integral storage remains active.

Blocked linear comparison, retaining normal velocity/acceleration integrals:

| Axis | GUID_OPTIONS | Peak position error (m) | Release displacement (m) |
| --- | ---: | ---: | ---: |
| X | 0 | 1.395591 | 2.471304 |
| X | 16 | 0 | 2.193004 |
| Y | 0 | 1.384051 | 2.432950 |
| Y | 16 | 0 | 1.842869 |
| Z | 0 | 0.547688 | 0.889954 |
| Z | 256 | 0 | 0.989990 |

Physical motion on each blocked axis was zero. Suppression removes outer-loop
error; it does not promise zero release displacement. The separate Z test also
measured zero altitude-target error for velocity, acceleration and combined
position/velocity commands with bit 8 set.

## Test investigation

The initial five-feature run passed four tests. `SITLBlockedAxes` failed on
SIMSTATE zgyro=1.788139e-6 rad/s against a 1e-6 assertion. In
`Aircraft::fill_fdm`, SIMSTATE takes `smoothing.gyro`, and `smooth_sensors`
adds attitude convergence correction to the physical gyro. Therefore the
assertion was changed to 1e-4 rad/s (less than 0.006 deg/s), with an explanatory
comment. The physical constraint implementation was unchanged. The corrected
test passed body X/Y/Z separately and mask 63 together. Original failing logs
were retained; only the affected test was rerun, alongside Guided regressions.

## Final validation and audit

All ten directed native autotests passed across the two runs:

- `test.Copter.GuidedBlockedVelocity` — PASS
- `test.Copter.GuidedNoHeadingHold` — PASS
- `test.Copter.GuidedNoPositionZ` — PASS
- `test.Copter.GuidedOptionsStartupReport` — PASS
- `test.Copter.GuidedSubModeChange` — PASS
- `test.Copter.GuidedYawRate` — PASS
- `test.Copter.PAUSE_CONTINUE_GUIDED` — PASS
- `test.Copter.SITLBlockedAxes` — PASS
- `test.Copter.SetpointBadVel` — PASS
- `test.Copter.SetpointGlobalVel` — PASS

Both runs used `MAVLINK20=1`, external `BUILDLOGS`, and
`python3 /home/nir/dev/treex-ardupilot/Tools/autotest/autotest.py --speedup=10`,
followed by the selected `test.Copter.*` names above. The second run used the
committed test correction in `575d630dc08c7ff96a69c5f646f894d3fa3c3af0`.

Local evidence under the external output's `validation/` directory:
`configure-hardware.log`, `build-hardware.log`, `hardware-image.json`,
`configure-sitl.log`, `build-sitl.log`, `autotest.log`,
`autotest-regressions.log`, `test-summary.json`, `measurements.txt`, and
`autotest/` telemetry/DataFlash logs. Tests ran serially; no subagents were used.
Git diff whitespace checks passed. Submodule pins were unchanged.

The migration changed controller, Guided, SITL, autotest, optional board-profile
and documentation files listed by the PR diff. It preserves the prior behavior
and evidence while making the implementation reviewable in the TreeX fork.
The only discovered issue was an overly strict smoothed-telemetry assertion;
its failure, explanation and correction are preserved above. This run did not
repeat the historical unit suite or disarmed USB deployment. Armed motor/flight
validation and specialized heli/inverted/6DoF runtime coverage remain pending.
The next step is review of the PR against Copter-4.6.2.
