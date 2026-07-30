import json
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
CTL_MAIN_C = PROJECT_ROOT / "src" / "ctl_main.c"
CTL_MAIN_H = PROJECT_ROOT / "src" / "ctl_main.h"
HARDWARE_ROOT = PROJECT_ROOT / "project" / "f280039c_Iris_node"
SDPE_HEADER = HARDWARE_ROOT / "sdpe_mgr" / "sdpe_pgs_inv_gfl_iris_settings.h"
SDPE_REQUIREMENT = HARDWARE_ROOT / "sdpe_mgr" / "sdpe_requirement.json"
SDPE_MATLAB_INIT = HARDWARE_ROOT / "sdpe_mgr" / "sdpe_pgs_inv_gfl_iris_settings_matlab_init.m"
SYSCFG = HARDWARE_ROOT / "F280039_Iris_node.syscfg"
XPLT_C = HARDWARE_ROOT / "xplt" / "xplt.peripheral.c"


class KeyboardRunControlTests(unittest.TestCase):
    def test_sdpe_no_longer_exposes_rectifier_ready_interlock(self):
        header = SDPE_HEADER.read_text(encoding="utf-8")
        requirement = json.loads(SDPE_REQUIREMENT.read_text(encoding="utf-8"))

        self.assertNotIn("GFL_RECTIFIER_READY", header)
        option_macros = {item["macro"] for item in requirement["option_macros"]}
        requirement_macros = {item["macro"] for item in requirement["requirements"]}
        self.assertFalse(any(name.startswith("GFL_RECTIFIER_READY") for name in option_macros))
        self.assertFalse(any(name.startswith("GFL_RECTIFIER_READY") for name in requirement_macros))
        self.assertNotIn(
            "GFL_RECTIFIER_READY",
            SDPE_MATLAB_INIT.read_text(encoding="utf-8"),
        )

    def test_gpio4_is_not_used_by_the_run_control(self):
        source = XPLT_C.read_text(encoding="gbk")
        header = CTL_MAIN_H.read_text(encoding="utf-8")
        syscfg = SYSCFG.read_text(encoding="utf-8")

        self.assertNotIn("GFL_RECTIFIER_READY", source)
        self.assertNotIn("xplt_get_rectifier_ready_input", source)
        self.assertNotIn("cia402_send_cmd", source)
        self.assertNotIn("ctl_rectifier_ready", header)
        self.assertNotIn('"IRIS_GPIO4"', syscfg)

    def test_keyboard_request_directly_drives_cia402_command(self):
        source = CTL_MAIN_C.read_text(encoding="utf-8", errors="ignore")

        self.assertIn("void ctl_set_run_request(fast_gt enable)", source)
        self.assertIn("CIA402_CMD_ENABLE_OPERATION", source)
        self.assertIn("CIA402_CMD_DISABLE_VOLTAGE", source)
        self.assertNotIn("ctl_update_rectifier_ready_command", source)
        self.assertIn("cia402_sm.flag_enable_control_word = 0", source)


if __name__ == "__main__":
    unittest.main()
