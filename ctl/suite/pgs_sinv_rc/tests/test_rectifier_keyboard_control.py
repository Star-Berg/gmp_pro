import json
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
CTL_MAIN_C = PROJECT_ROOT / "src" / "ctl_main.c"
CTL_MAIN_H = PROJECT_ROOT / "src" / "ctl_main.h"
USER_MAIN_C = PROJECT_ROOT / "src" / "user_main.c"
HARDWARE_ROOT = PROJECT_ROOT / "project" / "f280039c_Iris_node"
SDPE_HEADER = HARDWARE_ROOT / "sdpe_mgr" / "sdpe_pgs_sinv_rc_iris_bindings.h"
SDPE_REQUIREMENT = HARDWARE_ROOT / "sdpe_mgr" / "sdpe_requirement.json"
XPLT_C = HARDWARE_ROOT / "xplt" / "xplt.peripheral.c"
XPLT_H = HARDWARE_ROOT / "xplt" / "xplt.peripheral.h"
SYSCFG = HARDWARE_ROOT / "F280039_Iris_node.syscfg"


class RectifierKeyboardControlTests(unittest.TestCase):
    def test_keyboard_sdpe_defaults_to_50v_and_exposes_two_profiles(self):
        header = SDPE_HEADER.read_text(encoding="utf-8")
        requirement = json.loads(SDPE_REQUIREMENT.read_text(encoding="utf-8"))
        options = {item["macro"]: item for item in requirement["option_macros"]}

        self.assertIn("#define SINV_KEYBOARD_CONTROL_ENABLE (1)", header)
        self.assertIn("#define SINV_KEYBOARD_SW1_KEY_ID (8U)", header)
        self.assertIn("#define SINV_KEYBOARD_SW2_KEY_ID (9U)", header)
        self.assertIn("#define SINV_KEYBOARD_SW3_KEY_ID (10U)", header)
        self.assertIn("#define SINV_KEYBOARD_VBUS_REF_0_V (50.0f)", header)
        self.assertIn("#define SINV_KEYBOARD_VBUS_REF_1_V (60.0f)", header)
        self.assertIn("#define SINV_KEYBOARD_DEFAULT_VBUS_PROFILE (0U)", header)
        self.assertIn("#define SINV_KEYBOARD_SCAN_PERIOD_MS (50U)", header)
        self.assertIn("#define SINV_KEYBOARD_RELEASE_TIMEOUT_MS (200U)", header)

        self.assertEqual(options["SINV_KEYBOARD_CONTROL_ENABLE"]["value"], "(1)")
        self.assertEqual(options["SINV_KEYBOARD_SW3_KEY_ID"]["value"], "(10U)")
        self.assertEqual(options["SINV_KEYBOARD_VBUS_REF_0_V"]["value"], "(50.0f)")
        self.assertEqual(options["SINV_KEYBOARD_VBUS_REF_1_V"]["value"], "(60.0f)")
        self.assertEqual(options["SINV_KEYBOARD_DEFAULT_VBUS_PROFILE"]["value"], "(0U)")

    def test_keyboard_sw1_switches_cia402_and_sw2_switches_vbus_reference(self):
        source = USER_MAIN_C.read_text(encoding="utf-8", errors="ignore")

        self.assertIn("ht16k33_read_keys", source)
        self.assertIn("SINV_KEYBOARD_SW1_KEY_ID", source)
        self.assertIn("cia402_sm.current_state == CIA402_SM_FAULT", source)
        self.assertIn("CIA402_CMD_FAULT_RESET", source)
        self.assertIn("CIA402_CMD_ENABLE_OPERATION", source)
        self.assertIn("CIA402_CMD_DISABLE_VOLTAGE", source)
        self.assertIn("SINV_KEYBOARD_SW2_KEY_ID", source)
        self.assertIn("g_vbus_ref_user", source)
        self.assertIn("SINV_KEYBOARD_VBUS_REF_0_V", source)
        self.assertIn("SINV_KEYBOARD_VBUS_REF_1_V", source)
        self.assertIn('{"rectifier_key", tsk_rectifier_keyboard, SINV_KEYBOARD_SCAN_PERIOD_MS',
                      source)

    def test_sw1_can_cancel_an_enable_request_while_startup_is_waiting(self):
        source = USER_MAIN_C.read_text(encoding="utf-8", errors="ignore")
        keyboard_source = source[source.index("gmp_task_status_t tsk_rectifier_keyboard"):
                                 source.index("// External declaration for slow protection task")]

        self.assertIn("static fast_gt g_rectifier_run_requested = 0;", source)
        self.assertIn("g_rectifier_run_requested ^= 1U;", keyboard_source)
        self.assertIn("if (g_rectifier_run_requested)", keyboard_source)
        self.assertIn("cia402_send_cmd(&cia402_sm, CIA402_CMD_ENABLE_OPERATION);", keyboard_source)
        self.assertIn("cia402_send_cmd(&cia402_sm, CIA402_CMD_DISABLE_VOLTAGE);", keyboard_source)
        self.assertNotIn("cia402_sm.state_word.bits.operation_enabled", keyboard_source)

    def test_sw3_cancels_startup_or_resets_fault(self):
        source = USER_MAIN_C.read_text(encoding="utf-8", errors="ignore")
        keyboard_source = source[source.index("gmp_task_status_t tsk_rectifier_keyboard"):
                                 source.index("// External declaration for slow protection task")]

        self.assertIn("key_id == SINV_KEYBOARD_SW3_KEY_ID", keyboard_source)
        self.assertIn("g_rectifier_run_requested = 0;", keyboard_source)
        self.assertIn("cia402_sm.current_state == CIA402_SM_FAULT", keyboard_source)
        self.assertIn("CIA402_CMD_FAULT_RESET", keyboard_source)
        self.assertIn("CIA402_CMD_DISABLE_VOLTAGE", keyboard_source)

    def test_display_shows_output_state_and_physical_vbus_reference(self):
        source = USER_MAIN_C.read_text(encoding="utf-8", errors="ignore")

        self.assertIn("rectifier_display_update", source)
        self.assertIn("cia402_sm.state_word.bits.operation_enabled", source)
        self.assertIn("cia402_sm.current_state == CIA402_SM_FAULT", source)
        self.assertIn("else if (g_rectifier_run_requested)", source)
        self.assertIn("ctrl2float(g_vbus_ref_user) * CTRL_VOLTAGE_BASE", source)
        self.assertIn("ht16k33_update_display", source)
        self.assertIn("dev->display_ram[ram_index]", source)

    def test_removed_ready_gpio_has_no_source_or_syscfg_residue(self):
        source = CTL_MAIN_C.read_text(encoding="utf-8", errors="ignore")
        header = CTL_MAIN_H.read_text(encoding="utf-8", errors="ignore")
        sdpe = SDPE_HEADER.read_text(encoding="utf-8")
        requirement = SDPE_REQUIREMENT.read_text(encoding="utf-8")
        xplt_c = XPLT_C.read_text(encoding="utf-8", errors="ignore")
        xplt_h = XPLT_H.read_text(encoding="utf-8", errors="ignore")
        syscfg = SYSCFG.read_text(encoding="utf-8", errors="ignore")

        for text in (source, header, sdpe, requirement, xplt_c, xplt_h, syscfg):
            self.assertNotIn("SINV_INVERTER_READY", text)
            self.assertNotIn("ctl_update_inverter_ready_output", text)
            self.assertNotIn("xplt_set_inverter_ready_output", text)

        self.assertNotIn('gpio4.direction       = "GPIO_DIR_MODE_OUT";', syscfg)
        self.assertNotIn("gpio4.writeInitialValue = true;", syscfg)
        self.assertNotIn("gpio4.initialValue      = 0;", syscfg)


if __name__ == "__main__":
    unittest.main()
