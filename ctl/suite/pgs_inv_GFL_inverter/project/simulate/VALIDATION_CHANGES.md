# GFL AFE 联合仿真准备与变更

## 当前目标

- 三相电网：28 Vrms 线电压，模型 `dq0 to abc` 的 d 轴输入为 22.86 V。
- AFE 中间直流母线：55 V 闭环。
- BUCK：固定比较值 1350，基准 3000，即占空比 0.45；暂不加 BUCK 电压环。
- 理想 BUCK 输出约为 `55 x 0.45 = 24.75 V`，本阶段只观察其平均值与纹波。

## 控制链

```text
Vdc1_ref(55 V) - Vdc1
        -> 直流母线 PI
        -> 负有功功率给定（P < 0 表示整流）
        -> 现有 P/Q 环
        -> Id/Iq 给定
        -> dq 电流环
        -> 三相 PWM
```

外层 `src/ctl_main.c` 已包含上述母线环。本次不修改该文件，只对 PC 仿真提供 55 V 标定覆盖。

## 文件变更

| 文件 | 类型 | 内容 |
|---|---|---|
| `xplt/sim_control_overrides.h` | 新增 | PC 仿真专用 55 V 母线基值、55 V 参考和 0.7200 pu 电网电压 |
| `xplt/xplt.peripheral.h` | 1 行修改 | 在生成的 SDPE 设置完整加载后引入仿真覆盖 |
| `run_gfl_cosim.m` | 新增 | 后台启动 EXE、运行模型、异常或结束时关闭进程，不保存模型 |
| `run_gfl_validation.m` | 新增 | 临时启用 Scope 日志，计算指标并保存 PNG/CSV/MAT |
| `test_gfl_cosim_setup.m` | 新增 | 检查文件、标定、BUCK 固定占空比和 DC 通道映射 |
| `.gitignore` | 1 行修改 | 忽略自动验证输出目录 |
| `VALIDATION_CHANGES.md` | 新增 | 本说明和变更清单 |

`project/simulate/xplt/xplt.ctl_interface.h` 已经是正确映射：

```c
idc_src = simulink_rx_buffer.adc_result[INV_ADC_ID_IDC];
udc_src = simulink_rx_buffer.adc_result[INV_ADC_ID_VDC];
```

因此该文件不需要再次修改。物理板平台本次未改。

## 使用

先生成 GMP 仿真源并编译 `Debug|x64`，保证以下文件存在：

```text
x64/Debug/Digital_Power_simulink.exe
```

在 MATLAB 中切换到本目录后运行：

```matlab
[simOut, runInfo] = run_gfl_cosim;
[metrics, simOut] = run_gfl_validation;
```

验证结果写入 `validation_results/`。脚本不会保存对 `.slx` 的临时日志设置。

## 版本说明

当前主模型由 Simulink R2025a 保存。R2024b 会在加载时明确报版本不兼容；随附的
`.slx.r2024b` 可加载，但不包含当前新增的 BUCK 电路，不能替代主模型完成本次验证。
