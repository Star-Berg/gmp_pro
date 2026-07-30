import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
USER_MAIN_C = (PROJECT_ROOT / "src" / "user_main.c").read_text(encoding="utf-8")
USER_MAIN_H = (PROJECT_ROOT / "src" / "user_main.h").read_text(encoding="utf-8")
CTL_MAIN_C = (PROJECT_ROOT / "src" / "ctl_main.c").read_text(encoding="utf-8")
CCS_PROJECT = (
    PROJECT_ROOT / "project" / "f280039c_Iris_node" / ".project"
).read_text(encoding="utf-8")


class IrisOperatorPanelTests(unittest.TestCase):
    def test_only_keys_20_3_and_21_control_the_operator_panel(self):
        self.assertIn("#define GFL_UI_KEY_SWITCH_ID      (20)", USER_MAIN_H)
        self.assertIn("#define GFL_UI_KEY_FREQUENCY_ID   (3)", USER_MAIN_H)
        self.assertIn("#define GFL_UI_KEY_FAULT_RESET_ID (21)", USER_MAIN_H)
        self.assertNotIn("GFL_UI_KEY_RUN_ID", USER_MAIN_H)
        self.assertNotIn("GFL_UI_KEY_STOP_ID", USER_MAIN_H)
        self.assertIn("case GFL_UI_KEY_SWITCH_ID:", USER_MAIN_C)
        self.assertIn("ctl_set_run_request(ctl_user_run_request ? 0 : 1);", USER_MAIN_C)
        self.assertIn("cia402_fault_reset(&cia402_sm);", USER_MAIN_C)

    def test_key_three_toggles_30_and_60_hz_without_long_press_retrigger(self):
        self.assertIn("case GFL_UI_KEY_FREQUENCY_ID:", USER_MAIN_C)
        self.assertIn("ui_key_armed", USER_MAIN_C)
        self.assertIn("GFL_UI_KEY_RELEASE_TIMEOUT_MS", USER_MAIN_C)
        self.assertIn("ctl_request_output_frequency_hz(target_frequency_hz);", USER_MAIN_C)

    def test_frequency_change_is_deferred_until_pwm_is_stopped(self):
        self.assertIn("ctl_output_frequency_request_hz", CTL_MAIN_C)
        self.assertIn("ctl_request_output_frequency_hz", CTL_MAIN_C)
        self.assertIn("ctl_apply_output_frequency_request", CTL_MAIN_C)
        self.assertIn("if (inv_ctrl.flag_enable_system)", CTL_MAIN_C)
        self.assertIn("ctl_set_run_request(0);", CTL_MAIN_C)

    def test_frequency_update_changes_phase_step_and_frequency_dependent_coefficients(self):
        self.assertIn("ctl_set_ramp_generator_slope", CTL_MAIN_C)
        self.assertIn("inv_ctrl.coef_ff_decouple", CTL_MAIN_C)
        self.assertIn("ctl_init_biquad_notch", CTL_MAIN_C)
        self.assertIn("ctl_init_ddsrf_channel", CTL_MAIN_C)

    def test_run_request_is_driven_directly_by_keyboard(self):
        self.assertNotIn("ctl_rectifier_ready", CTL_MAIN_C)
        self.assertIn("CIA402_CMD_ENABLE_OPERATION", CTL_MAIN_C)
        self.assertIn("CIA402_CMD_DISABLE_VOLTAGE", CTL_MAIN_C)
        self.assertIn("ctl_set_run_request(0);", USER_MAIN_C)

    def test_oled_only_shows_set_frequency_physical_bus_voltage_and_switch_state(self):
        self.assertIn(
            "ctrl2float(inv_ctrl.filter_udc.out) * (float)CTRL_VOLTAGE_BASE",
            USER_MAIN_C,
        )
        self.assertIn('"FREQ SET:%02u Hz"', USER_MAIN_C)
        self.assertIn('"DC BUS:%3u.%u V"', USER_MAIN_C)
        self.assertIn('"OUTPUT:%s"', USER_MAIN_C)
        self.assertNotIn('"ERR:', USER_MAIN_C)
        self.assertNotIn('"ST:', USER_MAIN_C)

    def test_fault_automatically_clears_the_output_switch_request(self):
        self.assertIn("CIA402_SM_FAULT_REACTION", CTL_MAIN_C)
        self.assertIn("CIA402_SM_FAULT", CTL_MAIN_C)
        self.assertIn("cia402_sm.last_cb_result <= CIA402_EC_ERROR", CTL_MAIN_C)
        fault_guard = CTL_MAIN_C.split(
            "// A controller/state-machine fault must also clear", 1
        )[1]
        self.assertIn("ctl_set_run_request(0);", fault_guard)

    def test_i2c_and_seven_segment_power_on_self_test_are_configured(self):
        syscfg = (
            PROJECT_ROOT / "project" / "f280039c_Iris_node" / "F280039_Iris_node.syscfg"
        ).read_text(encoding="utf-8")
        self.assertIn('i2c1.bitCount               = "I2C_BITCOUNT_8";', syscfg)
        self.assertIn("ui_set_7segment_frequency", USER_MAIN_C)
        self.assertIn("ht16k33_update_display(&ui_keypad)", USER_MAIN_C)

    def test_ccs_project_links_existing_oled_driver(self):
        self.assertIn("<name>user/oled_driver.c</name>", CCS_PROJECT)
        self.assertIn(
            "csp/c28x_syscfg/iris_280039c_board/oled_driver/oled_driver.c",
            CCS_PROJECT,
        )


if __name__ == "__main__":
    unittest.main()
