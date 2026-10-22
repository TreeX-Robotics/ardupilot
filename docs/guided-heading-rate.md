# Guided heading-rate control without heading stabilization

Base: Copter-4.6.2; worktree feat-guided-z-position; board CubeOrange-bdshot.
This extends the existing Z option in the same authorized local build/test task.

## Contract and design

GUID_OPTIONS bit 9 (512), DoNotStabilizeHeading, removes heading-angle feedback
only for an explicit Guided heading-rate command (AutoYaw RATE). Preserve yaw
rate shaping, limits, rate PID and integral, thrust-vector stabilization, and
large thrust-error protection. Absolute angle, angle+rate, pilot yaw, takeoff,
pause and other flight modes retain their existing behavior. Full body-rate
SET_ATTITUDE_TARGET commands are a separate interface and remain unchanged.

Select a distinct HeadingMode for this command. Scope the low-level heading
hold selector to one heading-command dispatch, including virtual overrides.
Before rate-command shaping and before attitude-error/feedforward calculation,
reanchor the target quaternion to actual attitude plus only its thrust-vector
correction. This removes stored heading rotation without replacing the desired
thrust direction or introducing Euler-angle singularities. With ATC_RATE_FF_ENAB
zero, continue using shaped yaw-rate demand; roll/pitch retain existing non-FF
behavior. Restore the selector after dispatch; no option leaks to later commands.
Do not reset yaw-rate PID integral when suppressing angle feedback.

SIM_BLOCK_MASK is a runtime, multicopter-only ideal constraint: bits 0/1/2 block
NED north/east/down translation, bits 3/4/5 block body X/Y/Z rotation. Combine
bits to lock multiple axes. Zero selected physical acceleration and velocity
before integration, and report constraint-consistent specific force to IMU.
Locks engage at the current pose and release without teleporting. Rotational
bits constrain body angular velocity, not independent Euler angles during tilt.
Default zero leaves physics unchanged. This is not an elastic contact model.

## Verification plan

Use native Copter SITL autotests. For yaw, compare bit clear/set under physical
body-Z blockage, stream nonzero then zero rate while still blocked, release,
and measure target/actual heading and post-release rotation. Test both FF
settings, free yaw-rate tracking, absolute heading, timeout and mode/option
transitions, and thrust-vector stabilization while tilted. For X/Y/Z, compare
velocity commands with position stabilization enabled/disabled, validate the
physical locks, inspect logged position targets, and measure release behavior.
Build SITL and CubeOrange-bdshot with native waf; retain logs, numerical tables,
and firmware outside the checkout. Native suite is the reproducible CI path;
Robopilot Jenkins/wiki do not own this standalone ArduPilot change.

## Audit

Implementation and results will be appended. No flashing or robot deployment.

## Parameter usage

| Parameter | Mask | Meaning |
| --- | ---: | --- |
| GUID_OPTIONS | 512 | Disable heading hold for explicit heading-rate commands |
| GUID_OPTIONS | 784 | Bits 4+8+9: disable XY/Z position and rate-command heading hold |
| SIM_BLOCK_MASK | 1 | Block NED X (north) translation |
| SIM_BLOCK_MASK | 2 | Block NED Y (east) translation |
| SIM_BLOCK_MASK | 4 | Block NED Z (down) translation |
| SIM_BLOCK_MASK | 8 | Block body X rotation |
| SIM_BLOCK_MASK | 16 | Block body Y rotation |
| SIM_BLOCK_MASK | 32 | Block body Z rotation (yaw when level) |
| SIM_BLOCK_MASK | 63 | Block all six axes |
| SIM_BLOCK_MASK | 0 | Release all constraints |

Add the desired bits to existing GUID_OPTIONS rather than replacing other
options. The physical fixture exists only in the multicopter SITL model.

## Session log — 2026-10-08

One agent extended the same linked checkout; no subagents, robot operations,
commits or PRs. The owner approved implementation/build/test of the reviewed
heading design and explicitly requested blocked X/Y/Z tests. The existing
local ArduPilot build authorization covers this continuation. Native waf
output and autotest runtime remain outside the checkout. This standalone
ArduPilot feature does not change Robopilot, so no Robopilot wiki page applies.

SITL compiled successfully in 6m34s. The first native test run found two fixture
issues: zero yaw-rate I prevents arming (move the isolated-I setting after
takeoff), and the Y-distance calculation used a changing latitude to scale
longitude (use a fixed reference latitude). The physical rotational/all-axis
fixture test and the existing Z-option regression passed in that run. Initial
X-axis measurements confirmed that suppressing position error does not reset
velocity PID integral: target error was zero but release motion persisted.
Both initial failure logs are retained; the controller was not changed to
accommodate those fixture errors.

The corrected X/Y/Z test passed in `autotest-heading-final.log`. The heading
matrix passed in `autotest-yaw-final.log`, including preserved PIDY integral.
Its free-rate check was subsequently strengthened from 12 s / 4 deg/s tolerance
to 60 s / 1 deg/s tolerance: with the outer angle loop removed, the unchanged
rate PI loop needs longer to converge. The final run is
`autotest-yaw-settled.log`. The normal-I blocked case produced 16.55 degrees of
release motion despite approximately 0.02 degrees of heading target error;
this demonstrates why removal of angle stabilization is not removal of all
controller integration. I=0 comparisons isolate the angle-memory mechanism.

## Final verification and measurements

Nine directed native Copter tests passed: GuidedNoHeadingHold,
GuidedBlockedVelocity, SITLBlockedAxes, GuidedNoPositionZ,
GuidedSubModeChange, PAUSE_CONTINUE_GUIDED, SetpointBadVel,
SetpointGlobalVel, and GuidedYawRate. The final heading run is
`autotest-yaw-settled.log`; the passing linear run is
`autotest-heading-final.log`; other passing tests are in
`autotest-heading.log`. Earlier fixture failures remain in those logs.
No controller source changes were needed after the initial SITL build.

| Blocked yaw case | Peak heading target error | Physical blocked motion | Release motion | Free rate after settling (20 deg/s command) |
| --- | ---: | ---: | ---: | ---: |
| FF=1, options=0, rate I=0.00 | 45.049377 deg | 0.000000 deg | 46.14 deg | 20.037 deg/s |
| FF=1, options=512, rate I=0.00 | 0.019791 deg | 0.000000 deg | 0.33 deg | 19.617 deg/s |
| FF=0, options=0, rate I=0.00 | 45.000120 deg | 0.000000 deg | 46.07 deg | 19.979 deg/s |
| FF=0, options=512, rate I=0.00 | 0.000061 deg | 0.000000 deg | 0.30 deg | 19.577 deg/s |
| FF=1, options=512, rate I=0.02 | 0.019791 deg | 0.000000 deg | 17.61 deg | 19.701 deg/s |

Yaw I=0 is set only after takeoff and retained through blockage and release;
normal I is restored for the 60-second free-rate tracking check. The final
normal-I run retained PIDY integral (peak 0.023784), had nearly zero heading error,
and still rotated 17.61 degrees after release. The earlier 16.55-degree result
above belongs to the shorter settling run, not the final artifact.

| Blocked linear axis | GUID_OPTIONS | Peak position target error | Physical blocked motion | Release motion |
| --- | ---: | ---: | ---: | ---: |
| X | 0 | 1.395539 m | 0.000000 m | 2.471304 m |
| X | 16 | 0.000000 m | 0.000000 m | 2.159608 m |
| Y | 0 | 1.384281 m | 0.000000 m | 2.432950 m |
| Y | 16 | 0.000000 m | 0.000000 m | 1.797478 m |
| Z | 0 | 0.549949 m | 0.000000 m | 0.929993 m |
| Z | 256 | 0.000000 m | 0.000000 m | 1.010010 m |

All six linear comparisons retained normal controller tuning. Zero position
error does not mean zero motion after release: velocity/acceleration integrals
remain active. The fixture was independently checked with body-axis torques
and all six axes locked simultaneously. Tilted translation, absolute heading,
option transitions, takeoff, RTL and heading-command timeout passed too.

## Firmware and reproducibility

SITL build passed in 6m34s; CubeOrange-bdshot build passed in 2m11s with GCC ARM
10.2.1 and warnings treated as errors. The APJ board ID is 140. Its decompressed
image exactly matches the raw binary; image size is 1878256 bytes and free flash
is 87824 bytes. Firmware containing both Z and heading options:
`/home/nir/dev/build/ardupilot-guided-z-position/guided-heading-and-z.apj`

APJ SHA-256: `e02f549e7253c01a64fc924c6ad994a1623245323c0382dad17a772efe311c01`.

The APJ identity remains base commit `1ebd4d99` because changes are uncommitted.
The exact tracked source diff is `guided-heading-and-z.patch`; patch SHA-256
is `0d724fbb63175c0f4cb5178d30df603e006bc9e13221dee5fbccaaddf90daf07`. Both docs are separate untracked task notes.
`heading-validation-summary.json` contains the checksums and all measurements;
`heading-measurements.csv` / `linear-measurements.csv` and
`blocked-motion-validation.png` / `.svg` are tangible human-review artifacts.
The 2D plot was rendered and visually inspected. The ancillary ABIN generator
still reports a missing Git checkout in the external output directory; this
has no effect on the verified APJ. Deliver only the APJ.

Native invocation (from an external runtime directory):
`BUILDLOGS=<external logs> python3 <checkout>/Tools/autotest/autotest.py --speedup=10 test.Copter.GuidedNoHeadingHold test.Copter.GuidedBlockedVelocity test.Copter.SITLBlockedAxes`.
Tests are registered in Copter tests2b, so normal native CI discovers them.
Build invocations are `./waf configure --board sitl --out <external output>
--no-submodule-update` then `./waf copter -j6`; repeat configure/copter with
`--board CubeOrange-bdshot` for firmware. Each validation invocation and its
complete output are retained in the named build/test logs.

## Final audit

Extension files: ArduCopter/Parameters.cpp, mode.h, mode_guided.cpp;
libraries/AC_AttitudeControl/AC_AttitudeControl.cpp and .h;
libraries/SITL/SITL.cpp and .h, SIM_Aircraft.cpp and .h, SIM_Multicopter.cpp;
Tools/autotest/arducopter.py; docs/guided-heading-rate.md. Existing bit-8 changes
in AC_PosControl.cpp/.h and docs/guided-z-position.md remain in this worktree;
the Z note gains an append-only link to this continuation.

Python AST parsing and git diff --check passed. Current source diff was
reviewed for scope. No gains or option defaults changed. Native unit tests
were not rerun for this extension; new low-level behavior was verified through
actual Copter SITL controller and physical state logs. This is nine directed
Copter integration tests, not the complete multi-vehicle suite. Specialized
helicopter/inverted/6DoF runtime behavior was not flight-tested. No Cube Orange
was flashed or flown. Body-rate SET_ATTITUDE_TARGET remains outside this bit's
contract. Constraints are ideal, not elastic collision/contact dynamics.
Wiki justification: no Robopilot code changed; this standalone ArduPilot note
is the equivalent spec and handoff artifact. Follow-up: owner review of the
plot, logs and firmware, then physical hardware validation. Human review is
pending; no deployment, commit or PR was performed.

## USB hardware deployment — 2026-10-12

The owner connected CubeOrange+ serial 1B002E000551333230363332 (board ID
1063), explicitly requested backup/flash/check, and approved a local matching
CubeOrangePlus-bdshot build. The board originally ran 4.6.2 / 1ebd4d99 with
GUID_OPTIONS=0. Exact firmware application flash was downloaded, wrapped in
original-firmware.apj and CRC-verified against the board; all 1197 parameters
were saved. The existing board-140 CubeOrange image was never uploaded.
The Plus -O2 build overflowed by 320 bytes; native -Os optimization retained
all features and produced a fitting image. Source patch matches prior SITL.

The matching patched image was flashed and CRC-verified. Pymavlink confirmed
the same UID, version/base revision, disarmed state, GUID_OPTIONS roundtrip
784 -> 0 and restored 0 after reboot. All 1197 parameters remain present;
only startup gyro/barometer calibration and boot/runtime counters changed.
The feature options are available but remain disabled in final configuration.
Hardware deployment/rollback artifacts and final audit:
`/home/nir/dev/build/ardupilot-guided-z-position/usb-cube-2026-10-12/README.md`;
`deployment-summary.json` holds image checksums and exact parameter differences.
The earlier 2026-10-08 no-hardware-flash audit above is historical; this USB
hardware deployment now occurred. No motor test, arming or flight was performed.
No code changes, commit, PR, or Robopilot wiki edits were made in this stage.

## Startup GUID_OPTIONS debug report — 2026-10-15 contract

Owner request: print the loaded GUID_OPTIONS value and one STATUSTEXT line per
enabled flag. Emit once during Copter init, after parameters and MAVLink UARTs
are ready and immediately before ap.initialised becomes true. Existing GCS
startup handling services STATUSTEXT immediately while vehicle_initialised is
false, avoiding a burst overflowing SITL's seven-message status queue.

Report MAV_SEVERITY_INFO, with header `GUID_OPTIONS loaded=<unsigned mask>`.
Each set bit gets `GUID_OPTIONS bit<N>: <Option enum name>`; unused/unknown
bits are printed as `unknown`. Zero prints `GUID_OPTIONS: no flags enabled`.
Use the same Option enum masks as Guided control, including new bits 8 and 9.
Every line fits a single MAVLink STATUSTEXT payload. The report describes the
loaded configuration, not parameter-write acknowledgement or controller
execution. No periodic report, parameter callback, control or default changes.
Keep the GCS connected across a reboot to observe startup messages; no replay
is added for connections established after boot.

The per-flag prefix is shortened to `GUID bit<N>: <Option enum name>` so even
the longest existing enum name stays below the 50-byte STATUSTEXT boundary
and is not split into MAVLink message chunks.

### Startup report implementation audit — 2026-10-15

Implemented report_loaded_options() in ArduCopter/mode_guided_options.cpp,
declared in mode.h and called once from system.cpp immediately before
ap.initialised=true. The method uses the private Guided Option enum for all
known masks, reports unknown set bits explicitly, and compiles to a no-op
when HAL_GCS_ENABLED is false. No generic parameter setter changes were made.

Added native GuidedOptionsStartupReport to Tools/autotest/arducopter.py and
registered in tests2b. It collects real startup STATUSTEXT across disarmed
reboots for masks 0, 256, 512, 784, 1021 (all supported flags), and 514 (unknown
bit 1 plus heading). It asserts exact message order/contents, INFO severity,
one chunk per line, no disabled flags, and disarmed state.

Fast validation passed: Python AST parse, report table covers all Guided Option
enum members with matching names, all report lines fit strictly below the
50-byte MAVLink payload boundary, startup call occurs after GCS UART setup and
before the initialized gate, and git diff --check. The message-length check
caught an initial 51-byte label; shortening the per-flag prefix to GUID fixed
it. Source changes remain local and uncommitted. No compilation or native SITL
execution was performed for this logging increment; the previous local-build
exception belonged to the board build/flash task. No hardware was accessed or
reflashed in this increment. The Cube still has the October 12 binary, without
this new report. Prior compiled binaries/patch manifests are historical and
do not contain the newly added file. Wiki justification remains standalone
ArduPilot; no Robopilot code or wiki is affected. Owner review artifact:

    GUID_OPTIONS loaded=784
    GUID bit4: DoNotStabilizePositionXY
    GUID bit8: DoNotStabilizePositionZ
    GUID bit9: DoNotStabilizeHeading

For GUID_OPTIONS=0 the two messages are GUID_OPTIONS loaded=0 and
GUID_OPTIONS: no flags enabled. This proves configuration loading when built
and deployed, not execution of the low-level controller branch. Follow-up is
native build/SITL validation and deployment if requested; the startup observer
must be connected across reboot to receive the report.

### Startup-report build, SITL and Cube verification — 2026-10-15

Owner subsequently requested build and flash to the connected Orange. Its
identity is the same CubeOrange+ (1063). Before this update it had GUID_OPTIONS
512; all 1197 parameters and the exact prior application flash were backed up,
with firmware CRC checked against the board. Corrected the report method's
const qualifier after the compiler found Mode::gcs() is non-const. The matching
-Os CubeOrangePlus-bdshot build passed and was uploaded/CRC-verified.

Hardware MAVLink2 captured GUID_OPTIONS loaded=512 and GUID bit9:
DoNotStabilizeHeading on a real reboot. MAVLink1 initially lacked uid2; using
MAVLINK20=1 enabled device identity verification. A final snapshot confirms 512
preserved and all 1197 names present. Only automatic startup calibration and
statistics changed. The board remained disarmed. SITL built and native
GuidedOptionsStartupReport passed all six cases, including all nine supported
flags together without truncation. Source remains local and uncommitted.
The preceding source-only audit is now historical; the report is now installed.

Exact build, rollback, source snapshots, captured boot messages and final audit:
/home/nir/dev/build/ardupilot-guided-z-position/usb-cube-2026-10-15-startup-report/README.md
Machine-readable record: deployment-summary.json in that directory. No motor
or flight test, no commit/push, and no Robopilot wiki/code changes.

## TreeX repository migration — 2026-10-22

The implementation is now maintained in /home/nir/dev/treex-ardupilot, on
feat-guided-z-heading-options based on Copter-4.6.2 (1ebd4d99), cloned from
TreeX-Robotics/ardupilot. Earlier paths and source-only/uncommitted statements
above describe historical steps. The old worktree remains preserved.
See [the portable configuration and build guide](guided-control-options.md).
The previously external -Os extra hwdef is now versioned for repeatable
CubeOrangePlus-bdshot builds. This migration does not flash hardware.
