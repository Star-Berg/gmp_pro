# Three-phase converter: grid-following and Level 6 stand-alone voltage control

**English** | [简体中文](README_CN.md)

This suite implements a three-phase, two-level grid-following converter and a PC-simulation-only stand-alone voltage-control extension. Its shared controller source is used by F280039C Iris, LaunchXL-F280049C, PC simulation, and STM32G431 projects. The existing C2000 stages are hardware-validated; `BUILD_LEVEL=6` must remain in simulation until its protection, sensing, and gains are validated on the target hardware.

## Control stages

- `BUILD_LEVEL=1`: open-loop voltage generation.
- `BUILD_LEVEL=2`: islanded current-loop validation.
- `BUILD_LEVEL=3`: grid synchronization with PLL and sequence-current control.
- `BUILD_LEVEL=4`: decoupling, damping, and lead compensation.
- `BUILD_LEVEL=5`: complete active/reactive-power control.
- `BUILD_LEVEL=6`: stand-alone alpha/beta QPR voltage control for the `resload` model.

Level 6 treats 24–36 V as line-to-line RMS, quantized in 0.5 V steps, and supports 20–100 Hz in 1 Hz steps. It uses load-current feed-forward plus a `C*dv/dt` capacitor-current estimate because the current simulation ADC bus does not expose bridge-side inductor currents. Frequency changes preserve phase continuity and retune both QPR loops. See the [Chinese guide](README_CN.md) for the runtime API, monitor map, limitations, and validation requirements.

The suite uses a two-layer SDPE model: common control settings are kept in `sdpe_general/`, while sampling, PWM, protection, and board mappings remain target-specific. The validated hardware combination includes the Helios three-phase GaN inverter and Harmonia LC filter.

Grid-connected commissioning involves hazardous voltage and energy. Complete isolated low-voltage tests, polarity checks, protection tests, and the lower build levels before connection to a live grid. See the [Chinese guide](README_CN.md) for detailed configuration and validation notes.
