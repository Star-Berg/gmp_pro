import json
import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
CTL_MAIN_C = PROJECT_ROOT / "src" / "ctl_main.c"
HARDWARE_ROOT = PROJECT_ROOT / "project" / "f280039c_Iris_node"
SDPE_HEADER = HARDWARE_ROOT / "sdpe_mgr" / "sdpe_pgs_inv_gfl_iris_settings.h"
SDPE_REQUIREMENT = HARDWARE_ROOT / "sdpe_mgr" / "sdpe_requirement.json"
SYSCFG = HARDWARE_ROOT / "F280039_Iris_node.syscfg"
XPLT_C = HARDWARE_ROOT / "xplt" / "xplt.peripheral.c"


class RectifierReadyInterlockTests(unittest.TestCase):
    def test_sdpe_exposes_disabled_active_low_ready_input(self):
        header = SDPE_HEADER.read_text(encoding="utf-8")
        requirement = json.loads(SDPE_REQUIREMENT.read_text(encoding="utf-8"))

        self.assertIn("#define GFL_RECTIFIER_READY_INPUT_ENABLE (0)", header)
        self.assertIn("#define GFL_RECTIFIER_READY_GPIO IRIS_GPIO4", header)
        self.assertIn("#define GFL_RECTIFIER_READY_ACTIVE_LEVEL (0U)", header)
        self.assertIn("#define GFL_RECTIFIER_READY_DEBOUNCE_MS (20U)", header)

        options = {item["macro"]: item for item in requirement["option_macros"]}
        self.assertEqual(options["GFL_RECTIFIER_READY_INPUT_ENABLE"]["value"], "(0)")
        self.assertEqual(options["GFL_RECTIFIER_READY_GPIO"]["value"], "IRIS_GPIO4")
        self.assertEqual(options["GFL_RECTIFIER_READY_ACTIVE_LEVEL"]["value"], "(0U)")

    def test_input_uses_internal_pullup(self):
        syscfg = SYSCFG.read_text(encoding="utf-8")
        self.assertIn('gpio4.padConfig      = "PULLUP";', syscfg)

        source = XPLT_C.read_text(encoding="utf-8", errors="ignore")
        self.assertIn("GPIO_PIN_TYPE_PULLUP", source)
        self.assertIn("GPIO_DIR_MODE_IN", source)
        self.assertIn("fast_gt xplt_get_rectifier_ready_input", source)

    def test_low_level_enables_after_debounce_and_high_disables_immediately(self):
        source = CTL_MAIN_C.read_text(encoding="utf-8", errors="ignore")
        function = re.search(
            r"ctl_update_rectifier_ready_command\(.*?\n}\n",
            source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(function)
        body = function.group(0)
        self.assertIn("GFL_RECTIFIER_READY_ACTIVE_LEVEL", body)
        self.assertIn("GFL_RECTIFIER_READY_DEBOUNCE_MS", body)
        self.assertIn("CIA402_CMD_ENABLE_OPERATION", body)
        self.assertIn("CIA402_CMD_DISABLE_VOLTAGE", body)
        self.assertIn("cia402_sm.flag_enable_control_word = 0", source)


if __name__ == "__main__":
    unittest.main()
