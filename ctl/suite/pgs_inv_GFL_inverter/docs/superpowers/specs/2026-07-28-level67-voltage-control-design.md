# Level 6/7 Voltage Control Design

## Goal

Add an islanded voltage outer loop without changing BUILD_LEVEL 1-5 behavior. BUILD_LEVEL 6 reuses the existing notch-based negative-sequence controller with Level 6 tuning. BUILD_LEVEL 7 keeps the same positive-sequence voltage loop and replaces sequence extraction with DDSRF decoupling.

## Build-Level Behavior

- BUILD_LEVEL 1-5 retain their current initialization, dispatch order, references, PLL selection, and modulation behavior.
- BUILD_LEVEL 6 uses the ramp-generator angle, the new positive-sequence voltage outer loop, the existing positive current loop, and `inv_neg_ctrl_t` with its voltage and current loops enabled.
- BUILD_LEVEL 7 uses the same ramp-generator angle, voltage references, positive voltage PI, current-reference limiter, positive current loop, and modulation path as Level 6. It adds angle-driven DDSRF extraction for voltage and current before positive/negative sequence control.
- Neither Level 6 nor Level 7 controls zero sequence. The existing `vab0_out[phase_0]` behavior is retained.

## Level 6 Data Flow

1. The existing GFL core samples `vabc/iabc`, performs Clarke/Park transforms, and runs its positive current controller.
2. A 100 Hz notch is applied only to the positive voltage-loop `vdq` feedback. The positive current loop continues using the original current feedback.
3. The voltage PI calculates `idref/iqref` from `vdref-vd` and `vqref-vq`.
4. A circular current-reference limiter applies one vector magnitude limit and back-calculates both voltage-PI integrators.
5. The limited current reference is written to the existing GFL current controller for the next ISR.
6. The existing negative-sequence controller runs its voltage outer loop and current inner loop with zero negative-voltage reference. Its negative alpha/beta command is added to the positive command before SPWM.

The one-ISR reference delay follows the suite's existing P/Q outer-loop scheduling pattern and is acceptable at 20 kHz.

## Level 7 Data Flow

1. An angle-driven DDSRF extractor receives measured voltage/current alpha-beta vectors and the existing ramp-generator phasor.
2. Separate DDSRF channels produce decoupled `v_pos_dq`, `v_neg_dq`, `i_pos_dq`, and `i_neg_dq` through double synchronous-frame decoupling and first-order low-pass filters.
3. The common positive voltage loop consumes `v_pos_dq`; the positive current loop consumes `i_pos_dq`.
4. The existing negative voltage/current PI objects consume the DDSRF `v_neg_dq/i_neg_dq` values through a Level 7 direct-dq stepping path. Their output still uses the existing negative inverse Park transform and alpha/beta modulation summation.
5. Level 7 does not run the Level 6 100 Hz sequence-extraction notches in the active feedback paths.

The DDSRF extractor does not estimate angle or frequency and does not enable `ddsrf_pll_t`; this prevents a second PLL from competing with the Level 6/7 ramp generator.

## Controller Parameters

The common SDPE group is named `BUILD LEVEL 6/7 Common Voltage Control`. It owns:

- `GFL_LEVEL6_VD_REF_PU`, `GFL_LEVEL6_VQ_REF_PU`
- positive voltage-loop d/q gains
- circular positive current-reference limit
- Level 6 positive voltage-feedback notch Q
- Level 6 negative notch Q
- Level 6 negative current-loop gains and voltage-output limit
- Level 6 negative voltage-loop gains and current-output limit

The Level 7 group is named `BUILD LEVEL 7 DDSRF Control`. It owns:

- DDSRF low-pass cutoff frequency
- Level 7 negative current-loop gains and voltage-output limit
- Level 7 negative voltage-loop gains and current-output limit

Each Level 7 negative-loop macro defaults to the corresponding Level 6 macro expression. Changing a Level 6 value therefore changes both levels by default. Replacing a Level 7 alias with a numeric value allows independent Level 7 tuning. The positive voltage references, positive voltage PI, and positive current-reference limit remain common and are used directly by both levels.

Initial references are `vdref=0.33 pu` and `vqref=0 pu`. Initial tuning values are commissioning defaults and remain editable through SDPE.

## Isolation

- All new controller objects, declarations, initialization, dispatch, clearing, and monitoring state are guarded by `BUILD_LEVEL == 6 || BUILD_LEVEL == 7`.
- Level-specific negative sequence initialization is applied after `ctl_auto_tuning_neg_inv()` and before `ctl_init_neg_inv()`.
- Level 6 calls `ctl_enable_neg_voltage_inv()` and uses the original negative controller step.
- Level 7 selects the DDSRF direct-dq path without changing the default `inv_neg_ctrl_t` behavior used by Levels 1-5.
- Platform BUILD_LEVEL validation and option lists are extended from 1-5 to 1-7 without changing existing selected values.

## Safety And Saturation

- Positive `idref/iqref` uses circular limiting rather than independent d/q limiting.
- Voltage PI integrators are corrected to the circularly limited outputs to prevent windup.
- Negative current and voltage PI outputs retain explicit symmetric limits.
- Controller clear/disable paths clear the new filters, PI state, DDSRF state, and loop tick state.
- No automatic PWM enable, protection-threshold change, sensor mapping change, or ADC change is included.

## Verification

- Add host-side controller tests for balanced positive sequence, pure negative sequence, DDSRF cross-sequence rejection, current-reference circular limiting, and BUILD_LEVEL isolation.
- Verify generated SDPE headers contain the common and Level 7 groups with the intended macro aliases.
- Build the F280039C IRIS CCS project at BUILD_LEVEL 6 and BUILD_LEVEL 7.
- Rebuild at representative existing levels to ensure the new compile-time branches do not alter Levels 1-5.
