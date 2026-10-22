# Guided Z position stabilization option

Base: upstream `Copter-4.6.2` (`1ebd4d996e8b6bbeb1462a9655c00aa861d5beef`).
Hardware build target: `CubeOrange-bdshot`.

## Contract

Add `GUID_OPTIONS` bit 8 (256), `DoNotStabilizePositionZ`, analogous to
bit 4, `DoNotStabilizePositionXY`. With this bit enabled, Guided velocity,
acceleration, and combined position/velocity/acceleration control set the
internal altitude target to the current estimated altitude on every update.
Vertical velocity and acceleration feedback, their integrators, input shaping,
and throttle limits remain active. A zero acceleration command still retains
the integrated desired velocity; this option is not a stop command.

In combined position/velocity/acceleration control, ignore the supplied Z
position before input shaping, just as bit 4 ignores the supplied XY position.
Subtract vertical offsets and terrain from the desired altitude when matching
the final altitude target to the estimate.

With the bit clear, existing behavior is unchanged. Position-only control,
waypoint navigation, takeoff, attitude control, pause, RTL, and other modes
retain altitude stabilization. In velocity control, active avoidance limits
temporarily retain position stabilization, matching the XY option's policy.
Acceleration-command timeout retains the existing zero-velocity braking and
altitude stabilization path; velocity and combined-command timeout continue
using the selected option, matching bit 4.

For external position-to-velocity control on all axes, set bits 4 and 8
(`GUID_OPTIONS = 272` if no other options are required); leave bit 5 clear.

## Verification plan

Extend ArduPilot's native Copter SITL autotest suite to compare bit clear/set
under a vertical shove in velocity and acceleration control, inspect PSCD
altitude targets, and test combined commands with a deliberately displaced
altitude. Verify vertical velocity tracking, pure-position control, and timeout
behavior. Run existing Guided regressions, native unit tests, and compile the
CubeOrange-bdshot target. Keep build outputs outside the checkout.

## Audit

Validation results and residual limitations are appended after execution.
No robot deployment or flight-controller flashing is part of this task.
This is a standalone ArduPilot change; the Robopilot wiki is not changed.

## SITL results — 2026-10-08

The new `GuidedNoPositionZ` test and five existing native Copter tests passed:
`GuidedSubModeChange`, `PAUSE_CONTINUE_GUIDED`, `SetpointBadVel`,
`SetpointGlobalVel`, and `GuidedYawRate`.

| Measurement | Bit 8 clear | Bit 8 set |
| --- | ---: | ---: |
| Peak PSCD altitude error after a shove, velocity control | 0.249264 m | 0 m |
| Peak PSCD altitude error after a shove, acceleration control | 0.227671 m | 0 m |
| Displacement toward a supplied 3 m altitude change, combined control | 3.000306 m | 0.113307 m |
| Peak PSCD altitude error, combined control | 0.060591 m | 0 m |

The enabled tests also verified climb/descent velocity tracking with
`GUID_OPTIONS=272`, acceleration integration and velocity retention, braking
from motion after `GUID_TIMEOUT`, position-only altitude commands, takeoff,
and RTL landing. These are simulator measurements, not a physical collision
or a flight test.

Builds and logs live at `/home/nir/dev/build/ardupilot-guided-z-position`;
the checkout's ignored `build` symlink points there so the native autotest CLI
can find its expected `build/sitl/bin/arducopter` path. Runtime files also live
outside the checkout. The final feature test log is `autotest-z-final.log`;
the five existing regressions passed in `autotest.log`, which also preserves
an earlier failed version of the new test fixture. Machine-readable passing
results are in `validation-summary.json`.

## Session log

One agent used the linked ArduPilot worktree
`/home/nir/dev/worktrees/ardupilot-guided-z-position`, branch
`feat-guided-z-position`, based on upstream `Copter-4.6.2`. The existing
`/home/nir/dev/ardupilot` checkout and its local changes were preserved.
The owner clarified that bdshot means the Cube Orange board target and
explicitly permitted local ArduPilot builds/tests for this task.

The behavior contract was written before implementation. Native SITL logs
were used to check internal controller targets, rather than echoed setpoints.
Initial test fixtures exposed two mistakes: a weak shove did not produce the
assumed altitude error, and successful combined-position tracking produces
small error even while following a displaced target. The final fixture uses
a stronger shove and checks actual displacement for combined commands.

The release SITL binary compiled with warnings treated as errors. Native unit
tests were initially compiled in release mode, but their internal-error death
assertions require `HAL_DEBUG_BUILD`; debug tests are being built separately.
`waf check --targets=...` unexpectedly compiled unrelated vehicle libraries,
so that redundant build was interrupted and the native test executables are
run directly instead. Five notch-filter response-plot cases are excluded
from the scoped run after their release execution exceeded 120 seconds; its
two short correctness cases are included. Board dependencies `lwip` and
`CrashDebug/CrashCatcher` had to be initialized for the hardware build.

## Files and scope

- `ArduCopter/Parameters.cpp`: expose bit 8 in parameter metadata.
- `ArduCopter/mode.h` and `mode_guided.cpp`: declare and apply the option.
- `libraries/AC_AttitudeControl/AC_PosControl.h` and `.cpp`: add the altitude
  target reset without resetting velocity/acceleration feedback.
- `Tools/autotest/arducopter.py`: register the behavioral SITL regression.
- `docs/guided-z-position.md`: contract, evidence, session log, and audit.

The Robopilot wiki does not apply to this standalone ArduPilot checkout.
No commit, PR, deployment, flashing, or physical flight was requested.
Final unit-test and firmware-build results follow below when complete.

## Final native unit results

All 51 debug SITL unit-test executables passed, running 871 test cases with
zero failures. Only the five long `NotchFilterTest.HarmonicNotchTest*`
response-plot cases were excluded; `ResetTest` and `SineTest` were run.
The complete executable/filter/result list is `unit-debug-results.json`;
its summary is also included in `validation-summary.json`.

The release-mode death-test failures were resolved by compiling with the
upstream debug configuration, without changing their source or expectations:

```sh
./waf configure --board sitl --debug --out /home/nir/dev/build/ardupilot-guided-z-position/unit-debug --no-submodule-update
./waf tests -j6
```

Each resulting native test executable was run directly. For `test_notchfilter`
the scoped selection was
`--gtest_filter=NotchFilterTest.ResetTest:NotchFilterTest.SineTest`.

## Final firmware and audit

`CubeOrange-bdshot` compiled successfully using GCC ARM 10.2.1 and the
native `waf copter` target. The APJ is
`/home/nir/dev/build/ardupilot-guided-z-position/CubeOrange-bdshot/bin/arducopter.apj`.
Board ID is 140; the decompressed APJ image matches `arducopter.bin` exactly.
Image size is 1877544 bytes, leaving 88536
bytes of flash. APJ SHA-256:

```text
ae733b950bfb191a3ad1928a5c26aa7c2ff6d77a59f6a12a47e9c6c3f70e7548
```

The APJ git identity is the base commit `1ebd4d99`; task changes remain
uncommitted on `feat-guided-z-position`. The exact tracked-source diff is
saved as `guided-z-position.patch`, with its SHA-256 in
`validation-summary.json`. This note is an additional untracked task artifact.
The ancillary `.abin` generator reports an empty git hash when building
outside a Git checkout; the delivered APJ has its proper base git identity
and separately recorded image/source-patch checksums.

Final validation: six native Copter SITL tests passed; 51 native debug
unit-test executables passed (871 cases, excluding five lengthy filter
response-plot cases); SITL and CubeOrange-bdshot compilation passed with
warnings treated as errors; Python AST parsing and `git diff --check` passed.
The scope and files are listed above. No existing controller gains or default
options were changed. The default behavior remains unchanged when bit 8 is
clear. Wiki justification is above: this is outside Robopilot.

Residual limitations: this was not the complete multi-vehicle ArduPilot
integration suite, the five expensive notch response-plot cases were omitted,
and no physical Cube Orange was flashed or flown. The supplied APJ, native
logs, numeric comparison table, patch, and machine-readable validation summary
are the artifacts for human review. Follow-up is owner review and hardware
validation before operational use; no deployment was performed.

## Heading and blocked-axis continuation — 2026-10-08

The same checkout now also includes GUID_OPTIONS bit 9 (512) for suppressing
heading stabilization on explicit heading-rate commands, plus SIM_BLOCK_MASK
for physical per-axis multicopter constraints. See [the heading spec and final
measurements](guided-heading-rate.md). Bits 4+8+9 combine to 784.
The earlier Z-only APJ is preserved as
`/home/nir/dev/build/ardupilot-guided-z-position/guided-z-position-only.apj`
with the SHA recorded above. The board build directory's arducopter.apj now
contains both features; the named delivered copy is guided-heading-and-z.apj.
The original validation-summary.json is a historical Z-only snapshot; current
validation is in heading-validation-summary.json. The blocked linear tests
confirm zero position target error with the suppression flags but retained
motion after release from the remaining controller integrals.

## TreeX repository migration — 2026-10-22

The implementation is now maintained in /home/nir/dev/treex-ardupilot, on
feat-guided-z-heading-options based on Copter-4.6.2 (1ebd4d99), cloned from
TreeX-Robotics/ardupilot. Earlier paths and source-only/uncommitted statements
above describe historical steps. The old worktree remains preserved.
See [the portable configuration and build guide](guided-control-options.md).
The previously external -Os extra hwdef is now versioned for repeatable
CubeOrangePlus-bdshot builds. This migration does not flash hardware.
