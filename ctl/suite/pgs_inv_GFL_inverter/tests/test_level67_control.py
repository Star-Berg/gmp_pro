import math
import re
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
LEVEL67_HEADER = PROJECT_ROOT / "src" / "ctl_level67_voltage.h"
CTL_MAIN_C = PROJECT_ROOT / "src" / "ctl_main.c"
CTL_MAIN_H = PROJECT_ROOT / "src" / "ctl_main.h"
COMMON_REQUIREMENT = PROJECT_ROOT / "sdpe_general" / "sdpe_requirement.json"


class IIR1:
    def __init__(self, fs, fc):
        k = math.tan(math.pi * fc / fs)
        norm = 1.0 / (k + 1.0)
        self.b0 = k * norm
        self.b1 = self.b0
        self.a1 = (k - 1.0) * norm
        self.set_state(0.0)

    def set_state(self, value):
        self.x1 = value
        self.y1 = value
        self.out = value

    def step(self, value):
        self.out = self.b0 * value + self.b1 * self.x1 - self.a1 * self.y1
        self.x1 = value
        self.y1 = self.out
        return self.out


class DDSRF:
    def __init__(self, fs=10_000.0, fc=35.355339):
        self.pos_dc = [0.0, 0.0]
        self.neg_dc = [0.0, 0.0]
        self.pos_decoupled = [0.0, 0.0]
        self.neg_decoupled = [0.0, 0.0]
        self.lpf_pos = [IIR1(fs, fc), IIR1(fs, fc)]
        self.lpf_neg = [IIR1(fs, fc), IIR1(fs, fc)]
        self.initialized = False

    def step(self, alpha, beta, theta):
        cos_theta = math.cos(theta)
        sin_theta = math.sin(theta)
        cos_2theta = cos_theta * cos_theta - sin_theta * sin_theta
        sin_2theta = 2.0 * sin_theta * cos_theta
        pos_raw = [
            alpha * cos_theta + beta * sin_theta,
            -alpha * sin_theta + beta * cos_theta,
        ]
        neg_raw = [
            alpha * cos_theta - beta * sin_theta,
            alpha * sin_theta + beta * cos_theta,
        ]

        if not self.initialized:
            self.pos_dc = pos_raw.copy()
            self.pos_decoupled = pos_raw.copy()
            self.neg_decoupled = [0.0, 0.0]
            self.neg_dc = [0.0, 0.0]
            for index in range(2):
                self.lpf_pos[index].set_state(self.pos_dc[index])
                self.lpf_neg[index].set_state(0.0)
            self.initialized = True
            return

        self.pos_decoupled = [
            pos_raw[0] - (self.neg_dc[0] * cos_2theta + self.neg_dc[1] * sin_2theta),
            pos_raw[1] - (self.neg_dc[1] * cos_2theta - self.neg_dc[0] * sin_2theta),
        ]
        self.neg_decoupled = [
            neg_raw[0] - (self.pos_dc[0] * cos_2theta - self.pos_dc[1] * sin_2theta),
            neg_raw[1] - (self.pos_dc[1] * cos_2theta + self.pos_dc[0] * sin_2theta),
        ]
        self.pos_dc = [self.lpf_pos[i].step(self.pos_decoupled[i]) for i in range(2)]
        self.neg_dc = [self.lpf_neg[i].step(self.neg_decoupled[i]) for i in range(2)]


def run_ddsrf(pos_dq, neg_dq, duration=0.5, fs=10_000.0, frequency=50.0):
    ddsrf = DDSRF(fs=fs)
    for sample in range(round(duration * fs)):
        theta = 2.0 * math.pi * frequency * sample / fs
        cos_theta = math.cos(theta)
        sin_theta = math.sin(theta)
        alpha = (
            pos_dq[0] * cos_theta
            - pos_dq[1] * sin_theta
            + neg_dq[0] * cos_theta
            + neg_dq[1] * sin_theta
        )
        beta = (
            pos_dq[0] * sin_theta
            + pos_dq[1] * cos_theta
            - neg_dq[0] * sin_theta
            + neg_dq[1] * cos_theta
        )
        ddsrf.step(alpha, beta, theta)
    return ddsrf


class Level67ControlTests(unittest.TestCase):
    def test_ddsrf_extracts_pure_positive_sequence(self):
        result = run_ddsrf((0.8, -0.1), (0.0, 0.0))
        self.assertAlmostEqual(result.pos_dc[0], 0.8, delta=2e-4)
        self.assertAlmostEqual(result.pos_dc[1], -0.1, delta=2e-4)
        self.assertLess(math.hypot(*result.neg_dc), 2e-4)

    def test_ddsrf_extracts_pure_negative_sequence(self):
        result = run_ddsrf((0.0, 0.0), (0.18, 0.07))
        self.assertLess(math.hypot(*result.pos_dc), 2e-4)
        self.assertAlmostEqual(result.neg_dc[0], 0.18, delta=2e-4)
        self.assertAlmostEqual(result.neg_dc[1], 0.07, delta=2e-4)

    def test_ddsrf_decouples_mixed_sequences(self):
        result = run_ddsrf((0.75, -0.08), (0.16, 0.06))
        self.assertAlmostEqual(result.pos_dc[0], 0.75, delta=4e-4)
        self.assertAlmostEqual(result.pos_dc[1], -0.08, delta=4e-4)
        self.assertAlmostEqual(result.neg_dc[0], 0.16, delta=4e-4)
        self.assertAlmostEqual(result.neg_dc[1], 0.06, delta=4e-4)

    def test_circular_limiter_uses_unsaturated_pi_vector(self):
        source = LEVEL67_HEADER.read_text(encoding="utf-8")
        function = re.search(
            r"ctl_step_level67_voltage_pi\(.*?\n}\n",
            source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(function)
        body = function.group(0)
        self.assertIn("pid_d->p_term + pid_d->i_term + pid_d->d_term", body)
        self.assertIn("pid_q->p_term + pid_q->i_term + pid_q->d_term", body)
        self.assertIn("ctl_sqrt(magnitude_sq)", body)
        self.assertIn("ctl_pid_clamping_correction_using_real_output(pid_d, id_ref)", body)
        self.assertIn("ctl_pid_clamping_correction_using_real_output(pid_q, iq_ref)", body)

        raw = (0.6, 0.2)
        limit = 0.3
        scale = limit / math.hypot(*raw)
        limited = (raw[0] * scale, raw[1] * scale)
        self.assertAlmostEqual(math.hypot(*limited), limit)
        self.assertAlmostEqual(limited[0] / limited[1], raw[0] / raw[1])

    def test_soft_start_ramps_applied_reference_to_target(self):
        source = LEVEL67_HEADER.read_text(encoding="utf-8")
        self.assertIn("ctl_vector2_t vdq_set_applied;", source)
        self.assertIn("ctrl_gt soft_start_gain;", source)
        self.assertIn("ctrl_gt soft_start_step;", source)

        function = re.search(
            r"ctl_step_level67_soft_start\(.*?\n}\n",
            source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(function)
        body = function.group(0)
        self.assertIn("ctrl->soft_start_gain += ctrl->soft_start_step;", body)
        self.assertIn("ctrl->soft_start_gain = float2ctrl(1.0f);", body)
        self.assertIn("ctl_mul(ctrl->vdq_set.dat[phase_d], ctrl->soft_start_gain)", body)

        voltage_pi = re.search(
            r"ctl_step_level67_voltage_pi\(.*?\n}\n",
            source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(voltage_pi)
        self.assertIn("ctl_step_level67_soft_start(ctrl);", voltage_pi.group(0))
        self.assertIn("ctrl->vdq_set_applied.dat[phase_d]", voltage_pi.group(0))

        fs = 20_000.0
        ramp_ms = 1000.0
        target = 0.7
        step = 1000.0 / (fs * ramp_ms)
        gain = 0.0
        applied = []
        for _ in range(round(fs * ramp_ms / 1000.0)):
            gain = min(1.0, gain + step)
            applied.append(target * gain)
        self.assertAlmostEqual(applied[len(applied) // 2 - 1], target * 0.5, delta=1e-6)
        self.assertAlmostEqual(applied[-1], target, delta=1e-6)

    def test_soft_start_is_configurable_and_resets_on_disable(self):
        import json

        requirement = json.loads(COMMON_REQUIREMENT.read_text(encoding="utf-8"))
        parameters = {item["macro"]: item for item in requirement["requirements"]}
        self.assertEqual(parameters["GFL_LEVEL67_SOFT_START_TIME_MS"]["binding"]["float"], "1000.0")

        header = LEVEL67_HEADER.read_text(encoding="utf-8")
        clear_function = re.search(
            r"ctl_clear_level67_voltage\(.*?\n}\n",
            header,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(clear_function)
        self.assertIn("ctl_vector2_clear(&ctrl->vdq_set_applied);", clear_function.group(0))
        self.assertIn("ctrl->soft_start_gain = 0;", clear_function.group(0))

        main_source = CTL_MAIN_C.read_text(encoding="utf-8")
        self.assertIn("GFL_LEVEL67_SOFT_START_TIME_MS", main_source)

    def test_dc_bus_filter_is_configurable_and_shared_by_display_and_feedforward(self):
        import json

        requirement = json.loads(COMMON_REQUIREMENT.read_text(encoding="utf-8"))
        parameters = {item["macro"]: item for item in requirement["requirements"]}
        self.assertEqual(parameters["GFL_LEVEL67_DCBUS_FILTER_FC_HZ"]["binding"]["float"], "300.0")

        main_source = CTL_MAIN_C.read_text(encoding="utf-8")
        self.assertIn(
            "ctl_init_filter_iir1_lpf(&inv_ctrl.filter_udc, CONTROLLER_FREQUENCY,",
            main_source,
        )
        self.assertIn("GFL_LEVEL67_DCBUS_FILTER_FC_HZ", main_source)

        main_header = CTL_MAIN_H.read_text(encoding="utf-8")
        self.assertIn("ctrl_gt dc_bus_safe = inv_ctrl.filter_udc.out;", main_header)

    def test_level67_code_is_conditionally_isolated(self):
        header = CTL_MAIN_H.read_text(encoding="utf-8")
        source = CTL_MAIN_C.read_text(encoding="utf-8")
        guard = "#if BUILD_LEVEL == 6 || BUILD_LEVEL == 7"
        self.assertIn(f'{guard}\n#include "ctl_level67_voltage.h"', header)
        self.assertIn(f"{guard}\ngfl_level67_voltage_ctrl_t voltage_ctrl;", source)
        self.assertIn("#if BUILD_LEVEL == 6", header)
        self.assertIn("#elif BUILD_LEVEL == 7", header)
        self.assertIn("#else\n        ctl_step_neg_inv_ctrl(&neg_current_ctrl);", header)

    def test_disable_preserves_legacy_current_reference(self):
        source = CTL_MAIN_C.read_text(encoding="utf-8")
        function = re.search(r"void ctl_disable_pwm\(\).*?\n}\n", source, flags=re.DOTALL)
        self.assertIsNotNone(function)
        body = function.group(0)
        self.assertIn("ctl_disable_gfl_inv(&inv_ctrl);", body)
        guarded_clear = re.search(
            r"#if BUILD_LEVEL == 6 \|\| BUILD_LEVEL == 7\s+"
            r"ctl_set_gfl_inv_current\(&inv_ctrl, 0, 0\);\s+#endif",
            body,
        )
        self.assertIsNotNone(guarded_clear)
        self.assertLess(
            guarded_clear.start(),
            body.index("ctl_clear_gfl_inv(&inv_ctrl);"),
        )
        self.assertIn("ctl_clear_level67_voltage(&voltage_ctrl);", body)


if __name__ == "__main__":
    unittest.main()
