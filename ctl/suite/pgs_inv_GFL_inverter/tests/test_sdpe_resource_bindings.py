import json
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
SDPE_MGR = PROJECT_ROOT / "project" / "f280039c_Iris_node" / "sdpe_mgr"
REQUIREMENT = json.loads(
    (SDPE_MGR / "sdpe_requirement.json").read_text(encoding="utf-8")
)
GENERATED_HEADER = (SDPE_MGR / "sdpe_pgs_inv_gfl_iris_settings.h").read_text(
    encoding="utf-8"
)


class SdpeResourceBindingTests(unittest.TestCase):
    def setUp(self):
        self.options = {
            item["macro"]: item for item in REQUIREMENT["option_macros"]
        }

    def test_pwm_and_gate_driver_resources_are_sdpe_options(self):
        expected = {
            "PHASE_U_BASE": "IRIS_EPWM1_BASE",
            "PHASE_V_BASE": "IRIS_EPWM2_BASE",
            "PHASE_W_BASE": "IRIS_EPWM3_BASE",
            "PWM_ENABLE_PORT": "IRIS_GPIO5",
            "PWM_RESET_PORT": "IRIS_GPIO3",
        }

        for macro, value in expected.items():
            self.assertIn(macro, self.options)
            self.assertEqual(value, self.options[macro]["value"])

        self.assertEqual(
            "iris_f280039c_node.IRIS_EPWM",
            self.options["PHASE_U_BASE"]["options_preset"],
        )
        self.assertEqual(
            "iris_f280039c_node.IRIS_GPIO",
            self.options["PWM_ENABLE_PORT"]["options_preset"],
        )

    def test_all_runtime_adc_reads_are_sdpe_options(self):
        signal_macros = (
            "INV_UA",
            "INV_UB",
            "INV_UC",
            "INV_IA",
            "INV_IB",
            "INV_IC",
            "INV_UU",
            "INV_UV",
            "INV_UW",
            "INV_IU",
            "INV_IV",
            "INV_IW",
            "INV_VBUS",
            "INV_IBUS",
        )

        for macro in signal_macros:
            self.assertIn(macro, self.options)
            self.assertIn(f"{macro}_RESULT_BASE", self.options)
            self.assertEqual(
                "iris_f280039c_node.ADC_CHANNEL_INDEX",
                self.options[macro]["options_preset"],
            )
            self.assertEqual(
                "iris_f280039c_node.ADC_RESULT_REG",
                self.options[f"{macro}_RESULT_BASE"]["options_preset"],
            )

    def test_generated_header_contains_each_resource_once(self):
        for macro in (
            "PHASE_U_BASE",
            "PHASE_V_BASE",
            "PHASE_W_BASE",
            "PWM_ENABLE_PORT",
            "PWM_RESET_PORT",
            "INV_UA_RESULT_BASE",
            "INV_VBUS_RESULT_BASE",
        ):
            self.assertEqual(1, GENERATED_HEADER.count(f"#define {macro} "))


if __name__ == "__main__":
    unittest.main()
