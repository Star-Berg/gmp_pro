import json
import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SDPE_DIRS = (
    PROJECT_ROOT / "src",
    PROJECT_ROOT / "project" / "f280039c_Iris_node" / "sdpe_mgr",
)
PERIPHERAL = (
    PROJECT_ROOT
    / "project"
    / "f280039c_Iris_node"
    / "xplt"
    / "xplt.peripheral.c"
)

EXPECTED_SENSOR_VALUES = {
    "CTRL_AC_VOLTAGE_SENSITIVITY": 0.0134704805,
    "CTRL_AC_VOLTAGE_BIAS": 1.6599712993,
    "CTRL_AC_CURRENT_SENSITIVITY": 0.1504533333,
    "CTRL_AC_CURRENT_BIAS": 1.6514133333,
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


class SensorCalibrationTests(unittest.TestCase):
    def test_sdpe_files_expose_fitted_vac_iac_values(self):
        for directory in SDPE_DIRS:
            header = (directory / "sdpe_pgs_sinv_rc_iris_bindings.h").read_text(
                encoding="utf-8"
            )
            matlab = (
                directory / "sdpe_pgs_sinv_rc_iris_bindings_matlab_init.m"
            ).read_text(encoding="utf-8")
            for name, expected in EXPECTED_SENSOR_VALUES.items():
                with self.subTest(directory=directory, macro=name):
                    self.assertAlmostEqual(
                        get_float_macro(header, name), expected, places=9
                    )
                    self.assertRegex(
                        matlab,
                        rf"(?m)^{re.escape(name)} = {expected:.10f};$",
                    )

    def test_requirement_uses_fitted_vac_iac_values(self):
        requirement = json.loads(
            (SDPE_DIRS[1] / "sdpe_requirement.json").read_text(encoding="utf-8")
        )
        items = {
            item["macro"]: item
            for item in requirement["requirements"]
            if item.get("macro") in EXPECTED_SENSOR_VALUES
        }
        self.assertEqual(set(items), set(EXPECTED_SENSOR_VALUES))
        for name, expected in EXPECTED_SENSOR_VALUES.items():
            self.assertIn("float", items[name]["binding"])
            self.assertAlmostEqual(
                float(items[name]["binding"]["float"]), expected, places=9
            )

    def test_hardware_vac_iac_channels_use_sdpe_calibration(self):
        source = PERIPHERAL.read_text(encoding="utf-8")
        for adc, sensitivity, bias, base in (
            (
                "adc_v_grid",
                "CTRL_AC_VOLTAGE_SENSITIVITY",
                "CTRL_AC_VOLTAGE_BIAS",
                "CTRL_VOLTAGE_BASE",
            ),
            (
                "adc_i_ac",
                "CTRL_AC_CURRENT_SENSITIVITY",
                "CTRL_AC_CURRENT_BIAS",
                "CTRL_CURRENT_BASE",
            ),
        ):
            block = re.search(
                rf"ctl_init_adc_channel\(&{adc},(?P<body>.*?)12, 24\);",
                source,
                flags=re.DOTALL,
            )
            self.assertIsNotNone(block, f"missing ADC initialization: {adc}")
            self.assertIn(sensitivity, block.group("body"))
            self.assertIn(bias, block.group("body"))
            self.assertIn(base, block.group("body"))


if __name__ == "__main__":
    unittest.main()
