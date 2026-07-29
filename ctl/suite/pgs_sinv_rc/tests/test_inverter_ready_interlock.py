import json
import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
CTL_MAIN_C = PROJECT_ROOT / "src" / "ctl_main.c"
CTL_MAIN_H = PROJECT_ROOT / "src" / "ctl_main.h"
HARDWARE_ROOT = PROJECT_ROOT / "project" / "f280039c_Iris_node"
SDPE_HEADER = HARDWARE_ROOT / "sdpe_mgr" / "sdpe_pgs_sinv_rc_iris_bindings.h"
SDPE_REQUIREMENT = HARDWARE_ROOT / "sdpe_mgr" / "sdpe_requirement.json"
XPLT_C = HARDWARE_ROOT / "xplt" / "xplt.peripheral.c"
SYSCFG = HARDWARE_ROOT / "F280039_Iris_node.syscfg"


class InverterReadyInterlockTests(unittest.TestCase):
    def test_sdpe_exposes_enabled_active_high_gpio4_ready_output(self):
        header = SDPE_HEADER.read_text(encoding="utf-8")
        requirement = json.loads(SDPE_REQUIREMENT.read_text(encoding="utf-8"))

        self.assertIn("#define SINV_INVERTER_READY_OUTPUT_ENABLE (1)", header)
        self.assertIn("#define SINV_INVERTER_READY_GPIO IRIS_GPIO4", header)
        self.assertIn("#define SINV_INVERTER_READY_ACTIVE_LEVEL (1U)", header)
        self.assertIn("#define SINV_INVERTER_READY_STABLE_MS (100U)", header)
        self.assertIn("#define SINV_INVERTER_READY_VBUS_TOLERANCE_V (3.0f)", header)

        options = {item["macro"]: item for item in requirement["option_macros"]}
        self.assertEqual(options["SINV_INVERTER_READY_OUTPUT_ENABLE"]["value"], "(1)")
        self.assertEqual(options["SINV_INVERTER_READY_GPIO"]["value"], "IRIS_GPIO4")
        self.assertEqual(options["SINV_INVERTER_READY_ACTIVE_LEVEL"]["value"], "(1U)")

    def test_ready_requires_running_fault_free_stable_dc_bus(self):
        source = CTL_MAIN_C.read_text(encoding="utf-8", errors="ignore")
        function = re.search(
            r"ctl_update_inverter_ready_output\(.*?\n}\n",
            source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(function)
        body = function.group(0)
        self.assertIn("cia402_sm.state_word.bits.operation_enabled", body)
        self.assertIn("protection.active_errors == 0", body)
        self.assertIn("SINV_INVERTER_READY_VBUS_TOLERANCE_V", body)
        self.assertIn("SINV_INVERTER_READY_STABLE_MS", body)
        self.assertIn("vbus_ready_feedback = g_vbus_feedback_filtered", body)
        self.assertIn("xplt_set_inverter_ready_output", body)

        self.assertIn("ctl_update_inverter_ready_output();", source)

        header = CTL_MAIN_H.read_text(encoding="utf-8", errors="ignore")
        self.assertIn("void ctl_update_inverter_ready_output(void);", header)

    def test_platform_output_defaults_to_inactive_low(self):
        source = XPLT_C.read_text(encoding="utf-8", errors="ignore")
        syscfg = SYSCFG.read_text(encoding="utf-8", errors="ignore")

        self.assertIn("GPIO_writePin(SINV_INVERTER_READY_GPIO, 0U);", source)
        self.assertIn("GPIO_DIR_MODE_OUT", source)
        self.assertIn("void xplt_set_inverter_ready_output", source)
        self.assertIn("gpio4.writeInitialValue = true;", syscfg)
        self.assertIn("gpio4.initialValue      = 0;", syscfg)


if __name__ == "__main__":
    unittest.main()
