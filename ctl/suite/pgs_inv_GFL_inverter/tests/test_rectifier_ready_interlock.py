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
    def test_sdpe_exposes_enabled_active_low_ready_input(self):
        header = SDPE_HEADER.read_text(encoding="utf-8")
        requirement = json.loads(SDPE_REQUIREMENT.read_text(encoding="utf-8"))

        self.assertIn("#define GFL_RECTIFIER_READY_INPUT_ENABLE (1)", header)
        self.assertIn("#define GFL_RECTIFIER_READY_GPIO IRIS_GPIO4", header)
        self.assertIn("#define GFL_RECTIFIER_READY_ACTIVE_LEVEL (0U)", header)
        self.assertIn("#define GFL_RECTIFIER_READY_DEBOUNCE_MS (20U)", header)

        options = {item["macro"]: item for item in requirement["option_macros"]}
        self.assertEqual(options["GFL_RECTIFIER_READY_INPUT_ENABLE"]["value"], "(1)")
        self.assertEqual(options["GFL_RECTIFIER_READY_GPIO"]["value"], "IRIS_GPIO4")
        self.assertEqual(options["GFL_RECTIFIER_READY_ACTIVE_LEVEL"]["value"], "(0U)")

    def test_gpio4_name_does_not_conflict_with_physical_gpio4_pwm(self):
        three_phase_syscfg = SYSCFG.read_text(encoding="utf-8")
        single_phase_root = PROJECT_ROOT.parent / "pgs_sinv_rc" / "project" / "f280039c_Iris_node"
        single_phase_syscfg = (single_phase_root / "F280039_Iris_node.syscfg").read_text(encoding="utf-8")
        single_phase_bindings = (
            single_phase_root / "sdpe_mgr" / "sdpe_pgs_sinv_rc_iris_bindings.h"
        ).read_text(encoding="utf-8")

        for syscfg in (three_phase_syscfg, single_phase_syscfg):
            self.assertIn('epwm4.epwm.epwm_aPin.$assign                                     = "GPIO4";', syscfg)
            self.assertIn('gpio4.$name           = "IRIS_GPIO4";', syscfg)
            self.assertIn('gpio4.gpioPin.$assign = "GPIO44";', syscfg)

        self.assertIn("#define PHASE_N_BASE IRIS_EPWM4_BASE", single_phase_bindings)
        self.assertIn("#define PWM_ENABLE_PORT IRIS_GPIO1", single_phase_bindings)
        self.assertIn("#define PWM_RESET_PORT IRIS_GPIO3", single_phase_bindings)

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
