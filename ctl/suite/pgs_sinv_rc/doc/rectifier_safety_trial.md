# Single-Phase Rectifier Safety Trial

This branch is an isolated commissioning experiment based on `pgs_sinv_rc`.
It does not change the source branch and should not be merged until simulation and low-voltage bench tests pass.

## Scope

The target is the single-phase full-bridge active front end in `BUILD_LEVEL=5`:

`AC source -> filter -> PWM full bridge -> DC link -> resistive load`

The downstream Buck stage is intentionally kept disabled during this test so that DC-link behaviour belongs only to the rectifier.

## Trial changes

1. **Buck disabled**
   - `SINV_BUCK_START_VBUS_MIN_V` is overridden to `1000 V`.
   - The Buck compare output therefore remains zero at the intended low-voltage test points.

2. **DC-link loop made conservative**
   - `SINV_DC_BUS_LOOP_KP = 0.20`
   - `SINV_DC_BUS_LOOP_KI = 2.0`
   - DC-link feedback is filtered at the 1 kHz outer-loop rate with `alpha = 0.06`, approximately a 10 Hz first-order low-pass response.
   - This reduces the tendency of the voltage loop to reproduce the inherent 100 Hz single-phase DC-link ripple in the AC current command.

3. **Hardware startup threshold corrected**
   - Hardware-only `CTRL_DCBUS_READY_MIN = 25 V`.
   - A 24 Vrms input can passively precharge the bridge to approximately 34 V peak, whereas the previous 48 V threshold could prevent the CiA402 sequence from reaching operation enabled.

4. **Hardware current range made consistent with TMCS1133B5A**
   - `CTRL_CURRENT_BASE = 10 A peak`
   - `CTRL_RATED_CURRENT_RMS = 5 A`
   - `CTRL_CURRENT_LIMIT_PU = 0.80`, giving an 8 A peak command limit
   - `CTRL_PROT_IAC_PEAK_MAX = 9 A`
   - These values keep the initial trial below the approximately +/-11 A ADC measurement span of the 150 mV/A sensor.

The simulation retains its original current base and current limit because the 80 V / 30 ohm case requires about 8.4 A peak from a 36 Vrms source.

## Files changed

- `ctl/suite/pgs_sinv_rc/src/sinv_rectifier_trial_overrides.h`
- `ctl/suite/pgs_sinv_rc/src/ctl_main.h`
- `ctl/component/digital_power/sinv/sinv_outer_loop.h`

The overrides are intentionally placed after generated SDPE settings. After values are verified, migrate them into the corresponding `sdpe_requirement.json` files and regenerate the headers.

## Simulation acceptance checks

Use `PGS_STD_SINV_MODEL_Rectifier.slx` with `BUILD_LEVEL=5`.

Expected steady state for the existing simulation parameters:

- Grid voltage: approximately 36 Vrms, 50 Hz
- DC-link voltage: approaches 80 V
- DC load power: approximately `80^2 / 30 = 213 W`
- Grid current: approximately 5.9 Arms, 8.4 A peak before losses
- Current direction: rectifier direction according to the project sign convention, so measured active power is negative when energy flows from grid to DC link
- Buck duty and Buck current: zero

Monitor at minimum:

- `monitor[0]`: grid voltage
- `monitor[1]`: controlled AC inductor current
- `monitor[2]`: DC-link voltage
- `monitor[3]`: instantaneous current reference
- `monitor[4]`: modulation command
- `monitor[6]`: active power
- `monitor[7]`: reactive power
- `monitor[10]`: Buck duty, expected zero
- `monitor[14]`: active protection errors, expected zero

For power-factor and THD acceptance, measure the source-side current `IG` directly in Simulink. The controller monitor currently exposes the inverter-side inductor current `IL`.

## Bench sequence

1. Verify ADC zero and polarity with PWM disabled.
2. Confirm the DC bus passively precharges above 25 V.
3. Run `BUILD_LEVEL=1` at reduced DC voltage and verify both bridge-leg PWM polarities.
4. Run `BUILD_LEVEL=2` with a small current reference and verify current feedback polarity.
5. Run `BUILD_LEVEL=3` with a small signed active-power reference and confirm the rectifier power direction.
6. Only then run `BUILD_LEVEL=5`, initially with a current-limited source and no Buck connection.

## Known remaining item

The original `BUILD_LEVEL=5` initialization still omits controller-divergence from the fatal protection mask during rectifier takeover. It should be re-armed after the DC link reaches the normal regulation region in a later change; this trial does not alter that state-dependent protection policy yet.
