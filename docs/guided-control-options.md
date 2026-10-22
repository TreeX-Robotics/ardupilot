# Guided position and heading stabilization options

This TreeX change targets **Copter-4.6.2**, commit
`1ebd4d996e8b6bbeb1462a9655c00aa861d5beef`. It does not target 4.8-dev master.

## Configuration

| GUID_OPTIONS bit | Value | Effect |
| --- | ---: | --- |
| 4 (existing) | 16 | Suppress XY position stabilization |
| 8 (new) | 256 | Suppress Z position stabilization |
| 9 (new) | 512 | Suppress heading stabilization for explicit heading-rate commands |

For an external position/heading controller sending velocity and heading-rate
commands, bits 4+8+9 combine to **784**. Preserve any other required option bits.
Defaults are unchanged. The options retain rate/velocity/acceleration feedback,
input shaping, limits and PID integrators. Zero outer-loop error does not mean
zero motion after a blockage is released: the remaining integrals can still
produce a transient. Bit 9 applies to rate-only heading commands through Guided
position-target interfaces; full body-rate SET_ATTITUDE_TARGET is unchanged.

The low-level Z controller reanchors its altitude target to the current estimate,
accounting for terrain and position offsets. The attitude controller removes
residual heading rotation using quaternion thrust-vector decomposition, retaining
roll/pitch thrust control. Heading suppression is scoped to one command dispatch.
Yaw-rate demand remains available when attitude feedforward is disabled.

At boot, INFO STATUSTEXT reports the loaded mask and one line per set bit:

```text
GUID_OPTIONS loaded=784
GUID bit4: DoNotStabilizePositionXY
GUID bit8: DoNotStabilizePositionZ
GUID bit9: DoNotStabilizeHeading
```

Keep the ground station connected across reboot. This reports loaded
configuration, not execution of a controller branch or parameter-write acceptance.

## Multicopter SITL constraints

SIM_BLOCK_MASK is a runtime ideal physical constraint. Add bits to combine axes:

| Bit | Value | Axis |
| --- | ---: | --- |
| 0 | 1 | North translation |
| 1 | 2 | East translation |
| 2 | 4 | Down translation |
| 3 | 8 | Body X rotation |
| 4 | 16 | Body Y rotation |
| 5 | 32 | Body Z rotation |

Zero releases all axes; 63 blocks all six. Reaction forces are reflected in
simulated accelerometer data. Rotational constraints block body rates, not
independent Euler angles during tilt. This is not an elastic contact model.

## Build and native validation

Initialize the needed submodules and keep output outside the checkout:

```sh
git submodule update --init --recursive modules/waf modules/mavlink modules/gtest modules/ChibiOS modules/DroneCAN modules/CrashDebug modules/CrashCatcher modules/lwip
./waf configure --board CubeOrangePlus-bdshot --out /absolute/external/build --no-submodule-update --extra-hwdef=libraries/AP_HAL_ChibiOS/hwdef/CubeOrangePlus-bdshot/size_optimized.dat
./waf copter -j6
```

The connected hardware was CubeOrange+ (board ID 1063), not CubeOrange (140).
Use the matching board target. The committed extra hwdef retains all features
and selects -Os; the full-feature Plus build exceeded flash under -O2.

For SITL, configure --board sitl with an external output directory, then build
copter. Native autotest expects build/sitl/bin/arducopter; an ignored build
symlink to the external output directory provides this path. Run autotest from
an external runtime directory, with BUILDLOGS set to an external log directory:

```sh
python3 /absolute/checkout/Tools/autotest/autotest.py --speedup=10 test.Copter.GuidedNoPositionZ test.Copter.GuidedNoHeadingHold test.Copter.GuidedBlockedVelocity test.Copter.SITLBlockedAxes test.Copter.GuidedOptionsStartupReport
```

Earlier directed runs of those tests and five Guided regressions passed in the
original checkout. The startup report was also captured on a disarmed CubeOrange+
after a CRC-verified upload. No armed motor or flight validation was performed.
Fresh migration validation is recorded in the PR. Historical design, numerical
measurements, deployment/rollback records and limitations are retained in
[the Z note](guided-z-position.md) and [the heading note](guided-heading-rate.md).
