# Native SITL motor response for Macaw5 calibration

This is an optional plant model, not a controller tuning change. The real
Macaw5 flies the recorded roll/pitch PID values; keep those values fixed when
calibrating SITL against the DataFlash recordings.

## Contract

The native multicopter frame JSON accepts `motor_response_time_constant_s`.
It defaults to zero. Negative or nonfinite values fail frame initialization.
For positive values, a first-order response follows the existing command slew
limiter, using elapsed simulation time and the exact held-input update:

```
actual += (demand - actual) * (1 - exp(-dt / tau))
```

Slew demand and actual command have separate state. Actual command starts at
zero; the first call establishes the time sample, including timestamp zero.
Both force/current calculations and `get_command()` (used for simulated RPM)
consume actual command. Zero bypasses the response calculation and preserves
the existing command arithmetic. Battery-dead handling remains unchanged.

Example optional field in an otherwise complete native frame JSON:

```json
{"motor_response_time_constant_s": 0.06}
```

## Calibration limits

Three September 2026 flights suggest a broad effective response range of
0.04–0.10 s when matching 10 Hz logged RCOU to filtered ESC RPM. This does not
identify an exact physical motor time constant. Two other flights were held
out before decoding. Do not choose a value solely to suppress oscillation.

Use the recorded calibrated Macaw5 URDF mass, COM and inertia. Native SITL
uses diagonal inertia; retain the dropped cross terms in the evidence.
Match the recorded gyro filtering and sampling settings. Set simulated ESC
RPM from the measured command/voltage relationship: zero SIM_VIB_MOT_MAX
otherwise drives an unrealistic RPM floor and changes the ESC-based notch.
Preserve noise assumptions separately from the RPM mapping.

## Validation status

Source implementation at `34154ca2f30958b34662f4250088f86e37228870` compiled
successfully on the sanctioned server through `robowire build --build-host
"$USER@treex-dev-tlv" --component ardupilot-sitl-motor-response`.
The Docker build uses native `./waf configure --board sitl
--no-submodule-update` and `./waf copter`.

Desktop image digest:
`sha256:a6f552b5a9ac2230cb2b7072bc1c225bc69a7acef0d8511870d85f4b47ce6696`.
Raw ESC RPM motor-step validation measured tau 0.060000059 s for requested
0.06 s. The 10–90% rise measured 0.132078 s versus 0.131833 s expected.
Zero-response, stopped startup/decay and negative-value initialization
rejection checks passed. Native panic can remain alive for diagnostics;
rejection is the named panic with no initialized MAVLink listener.

Original-PID tau 0/0.04 flight cases completed their sequence but oscillated
and are rejected. Tau 0.06/0.10 were bounded with zero sampled output
limiting; tau 0.06 also passed the independent command sequence. This is
model sensitivity, not validation of real-flight dynamics. A roll-P x1.5
probe, I/D unchanged, oscillated and was rejected on both sequences.
The gentle velocity profile tracks too ideally compared with real-flight
data. A corrected 50 Hz replay of recorded body-rate demands retains an
excess 8 Hz roll component: 7–9 Hz band RMS 3.063 deg/s in SITL versus
0.116–0.274 deg/s across the three real IMUs on a common anti-aliased grid.
Public Guided input shaping and initial state/disturbances remain different.
No aircraft PID recommendation follows. Full old-binary bitwise parity,
a dedicated clock-zero runtime test, and independent motor-force step
measurement remain untested.

Remaining validation:
Check zero-delay regression, a step reaching 63.21% after one tau, finite slew
plus lag, stopped startup, matching force/RPM state, and invalid JSON values.
Then compare fixed-PID rate/attitude/velocity response and limiting across
candidate time constants and held-out input sequences. Completing a flight
sequence alone does not establish dynamic fidelity.

Build only on the sanctioned TreeX build server through Robopilot's journaled
`robowire build` road. The desktop calibration artifact must be separate from
the fleet's pinned SITL image. Never deploy this experiment to an aircraft.
