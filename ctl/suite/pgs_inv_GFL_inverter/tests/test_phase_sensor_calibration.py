import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SETTINGS = (
    PROJECT_ROOT
    / "project"
    / "f280039c_Iris_node"
    / "sdpe_mgr"
    / "sdpe_pgs_inv_gfl_iris_settings.h"
)
PERIPHERAL = (
    PROJECT_ROOT
    / "project"
    / "f280039c_Iris_node"
    / "xplt"
    / "xplt.peripheral.c"
)


EXPECTED_SENSOR_VALUES = {
    "CTRL_GRID_VOLTAGE_A_SENSITIVITY": 0.0133760665,
    "CTRL_GRID_VOLTAGE_A_BIAS": 1.6608797884,
    "CTRL_GRID_VOLTAGE_B_SENSITIVITY": 0.0134189286,
    "CTRL_GRID_VOLTAGE_B_BIAS": 1.6542464286,
    "CTRL_GRID_VOLTAGE_C_SENSITIVITY": 0.0134290098,
    "CTRL_GRID_VOLTAGE_C_BIAS": 1.6549623557,
    "CTRL_GRID_CURRENT_A_SENSITIVITY": 0.1503832007,
    "CTRL_GRID_CURRENT_A_BIAS": 1.6531446197,
    "CTRL_GRID_CURRENT_B_SENSITIVITY": 0.1501000000,
    "CTRL_GRID_CURRENT_B_BIAS": 1.6534500000,
    "CTRL_GRID_CURRENT_C_SENSITIVITY": 0.1503476190,
    "CTRL_GRID_CURRENT_C_BIAS": 1.6539083333,
}


def get_float_macro(source, name):
    match = re.search(
        rf"^#define\s+{re.escape(name)}\s+\((-?[0-9.]+)f\)$",
        source,
        flags=re.MULTILINE,
    )
    if match is None:
        raise AssertionError(f"missing float macro: {name}")
    return float(match.group(1))


class PhaseSensorCalibrationTests(unittest.TestCase):
    def test_sdpe_exposes_fitted_abc_sensor_values(self):
        source = SETTINGS.read_text(encoding="utf-8")
        for name, expected in EXPECTED_SENSOR_VALUES.items():
            self.assertAlmostEqual(get_float_macro(source, name), expected, places=9)

    def test_hardware_adc_uses_independent_abc_parameters(self):
        source = PERIPHERAL.read_text(encoding="gbk")
        for adc, base, quantity in (
            ("vabc", "CTRL_VOLTAGE_BASE", "VOLTAGE"),
            ("iabc", "CTRL_CURRENT_BASE", "CURRENT"),
        ):
            for phase in "ABC":
                gain = (
                    rf"{adc}\.gain\[phase_{phase}\]\s*=\s*ctl_gain_calc_generic\("
                    rf"CTRL_ADC_VOLTAGE_REF,\s*CTRL_GRID_{quantity}_{phase}_SENSITIVITY,\s*{base}\);"
                )
                bias = (
                    rf"{adc}\.bias\[phase_{phase}\]\s*=\s*ctl_bias_calc_via_Vref_Vbias\("
                    rf"CTRL_ADC_VOLTAGE_REF,\s*CTRL_GRID_{quantity}_{phase}_BIAS\);"
                )
                self.assertRegex(source, gain)
                self.assertRegex(source, bias)


if __name__ == "__main__":
    unittest.main()
