# SDPE 参数对应关系

本文只记录当前工程有效的 SDPE 改参入口。原则是：公共控制算法参数放在 `sdpe_general`，仿真工况和硬件工况分别放在各自平台层，不再把所有电压目标塞到 common 里。

## 1. 文件分层

| 层级 | 修改文件 | 作用 | 生成后看哪里 |
| --- | --- | --- | --- |
| 公共层 | `sdpe_general/sdpe_requirement.json` | 跨仿真和硬件共用的控制算法参数，例如 PLL、FDRC、电流环、Buck 环路、PF 参考等 | `src/sdpe_pgs_sinv_rc_common_settings.h` |
| 仿真平台层 | `project/simulate/sdpe_mgr/sdpe_requirement.json` | 仿真额定工况、模型参数、仿真传感器标定、仿真目标电压 | `project/simulate/sdpe_mgr/sdpe_pgs_sinv_rc_simulate_settings.h` |
| Iris 硬件平台层 | `project/f280039c_Iris_node/sdpe_mgr/sdpe_requirement.json` | 实物额定工况、硬件板卡传感器标定、硬件目标电压、保护阈值 | `project/f280039c_Iris_node/sdpe_mgr/sdpe_pgs_sinv_rc_iris_bindings.h` |

注意：`src/sdpe_pgs_sinv_rc_iris_bindings.h` 和 `src/sdpe_pgs_sinv_rc_simulate_settings.h` 是历史副本，不作为当前改参入口。硬件通过 `project/f280039c_Iris_node/xplt/ctrl_settings.h` 包含 `sdpe_mgr/sdpe_pgs_sinv_rc_iris_bindings.h`；仿真通过 `project/simulate/xplt/xplt.config.h` 包含 `../sdpe_mgr/sdpe_pgs_sinv_rc_simulate_settings.h`。

修改 SDPE 后，需要重新生成：

```bat
sdpe_general\sdpe_generate.bat
project\simulate\sdpe_mgr\sdpe_generate.bat
project\f280039c_Iris_node\sdpe_mgr\sdpe_generate.bat
```

## 2. 要改哪个电压，去哪里改

| 目标 | 仿真修改 | 硬件修改 | 当前值 | 说明 |
| --- | --- | --- | --- | --- |
| 单相整流直流母线目标 | common 层 `sdpe_general/sdpe_requirement.json` 的 `SINV_DC_BUS_REF_V` | common 层 `sdpe_general/sdpe_requirement.json` 的 `SINV_DC_BUS_REF_V` | 当前默认：`40 V` | level5 整流母线闭环只看 `SINV_DC_BUS_REF_V`。它不再别名到平台层 `CTRL_DCBUS_VOLTAGE`；后者只作为额定/基值/ready/protection 相关参数。 |
| Buck 输出目标 | `project/simulate/sdpe_mgr/sdpe_requirement.json` 的 `SINV_BUCK_OUTPUT_REF_V` | `project/f280039c_Iris_node/sdpe_mgr/sdpe_requirement.json` 的 `SINV_BUCK_OUTPUT_REF_V` | 仿真：`60 V`；硬件：`48 V` | 这是 Buck 电压环最终目标。Buck 软启动由 common 中 `SINV_BUCK_VREF_SLEW_V_S` 控制，内部参考从 0 慢慢爬到该值。 |
| 交流输入/并网额定 RMS | `project/simulate/sdpe_mgr/sdpe_requirement.json` 的 `CTRL_GRID_VOLTAGE_RMS`，并同步检查 `CTRL_VOLTAGE_BASE` | `project/f280039c_Iris_node/sdpe_mgr/sdpe_requirement.json` 的 `CTRL_GRID_VOLTAGE_RMS`，并同步检查 `CTRL_VOLTAGE_BASE` | 仿真：`36 Vrms`，`CTRL_VOLTAGE_BASE = 50.91 V`；硬件：`24 Vrms`，`CTRL_VOLTAGE_BASE = 34.0 V` | `CTRL_GRID_VOLTAGE_RMS` 是额定/标幺/保护判断用值，不等于实物输入源旋钮。仿真电压源幅值由模型初始化脚本使用 `sqrt(2) * CTRL_GRID_VOLTAGE_RMS`。 |
| 交流侧 LC 参数 | `CTRL_AC_INDUCTANCE`、`CTRL_AC_RESISTANCE`、`SINV_FILTER_CAPACITANCE_F`、`SINV_FILTER_CAP_ESR_OHM` | `CTRL_AC_INDUCTANCE`、`CTRL_AC_RESISTANCE` | 仿真见 simulate 平台层；硬件当前 `1.5 mH`、`0.1 Ω` | 换 LC 板或实物串联电感时改这里。LC 参数会影响电流环对象、并网波形和 THD，不属于传感器标定。 |
| DC bus ready 范围 | `CTRL_DCBUS_READY_MIN`、`CTRL_DCBUS_READY_MAX` | `CTRL_DCBUS_READY_MIN`、`CTRL_DCBUS_READY_MAX` | 仿真：`25~90 V`；硬件：`0.8 * CTRL_DCBUS_VOLTAGE` 到 `CTRL_PROT_VBUS_MAX` | 这是状态机允许进入工作/启动 Buck 的母线电压范围。改母线目标后要检查这个范围是否仍合理。 |
| 母线过压保护 | `CTRL_PROT_VBUS_MAX` | `CTRL_PROT_VBUS_MAX` | 仿真：`90 V`；硬件：`100 V` | 保护阈值不是目标值。提高母线目标时必须留裕量，不能让目标值贴近保护值。 |

## 3. 改交流输入电压时为什么会牵动多个值

交流输入有两层含义，不能混在一起：

1. 实际输入电压：仿真里是 Simulink 电压源，实物里是交流源/调压器。
2. 控制器认为的额定值：SDPE 中的 `CTRL_GRID_VOLTAGE_RMS` 和 `CTRL_VOLTAGE_BASE`。

因此改交流输入电压时按下面顺序检查：

| 操作 | 必改/检查项 | 原因 |
| --- | --- | --- |
| 仿真改输入 RMS | 改 simulate 平台层 `CTRL_GRID_VOLTAGE_RMS` | `configure_sinv_models.m` 中电压源幅值使用 `sqrt(2) * CTRL_GRID_VOLTAGE_RMS`。 |
| 改额定 AC RMS | 同步检查 `CTRL_VOLTAGE_BASE ≈ CTRL_GRID_VOLTAGE_RMS * sqrt(2)` | 代码里 ADC 值、母线目标、电压保护、Buck 参考都用 `CTRL_VOLTAGE_BASE` 做标幺换算。 |
| 输入电压变化很大 | 检查 `CTRL_GRID_VMIN_PU`、`CTRL_DCBUS_READY_MIN/MAX`、`CTRL_PROT_VBUS_MAX` | PLL/PQ 低压判断、状态机启动条件和保护窗口都可能受影响。 |
| 需要同样输出功率 | 检查 `CTRL_CURRENT_LIMIT_PU`、`CTRL_P_SLEW_PU_S`、`CTRL_Q_SLEW_PU_S` | 输入电压变低时，为维持功率电流会上升；限流和斜率可能先卡住。 |
| 只是改输入源大小，没有换传感器板 | 不改 `*_SENSITIVITY` / `*_BIAS` | 传感器标定由硬件电阻、板卡增益和偏置决定，不由被测电压目标决定。 |

## 4. 当前传感器标定口径

| 信号 | 宏 | 当前口径 | bias | 说明 |
| --- | --- | --- | --- | --- |
| DC bus / Buck Vin | `CTRL_DC_VOLTAGE_SENSITIVITY`、`CTRL_DC_VOLTAGE_BIAS` | 通用半桥模块电压传感器，约 `0.02705 V/V` | `0 V` | `adc_v_bus` 和 `adc_v_buck_in` 共用。Buck 输入直接等同前级母线电压。 |
| AC/PCC 电压 | `CTRL_AC_VOLTAGE_SENSITIVITY`、`CTRL_AC_VOLTAGE_BIAS` | LC 板电压检测，按 `0.4 / 29.5 = 0.013559322 V/V` | `1.65 V` | 交流侧接在 LC 的 C 上，所以用 LC 板电压传感器。 |
| Buck 输出电压 | `CTRL_BUCK_OUTPUT_VOLTAGE_SENSITIVITY`、`CTRL_BUCK_OUTPUT_VOLTAGE_BIAS` | LC 板电压检测，按 `0.4 / 29.5 = 0.013559322 V/V` | `1.65 V` | `Vo_buck` 不能用下半桥电压传感器；下半桥电压传感器测的是自己的 DC bus。 |
| AC 电流 | `CTRL_AC_CURRENT_SENSITIVITY`、`CTRL_AC_CURRENT_BIAS` | 通用半桥模块 B5A 电流传感器 | 板卡导出值 | 方向可能与软件定义相反，实物上电后要用小电流判别，必要时改端口方向/符号处理。 |
| Buck 电感电流 | `CTRL_BUCK_CURRENT_SENSITIVITY`、`CTRL_BUCK_CURRENT_BIAS` | 通用半桥模块 B5A 电流传感器 | 板卡导出值 | 同样要实测方向。不要通过乱改 sensitivity 的正负来掩盖接线方向问题，除非确认软件就是按该极性约定。 |

标定参数只在以下情况改：

- 换了测量板或传感器板；
- 改了分压电阻、采样电阻、运放增益；
- ADC 偏置从实测结果看明显不是板卡默认值；
- 电流传感器型号从 B5A 换成其他量程。

改目标电压、输入电压范围、负载或控制目标时，不应该改传感器灵敏度。

## 5. 常见调参入口速查

| 想调什么 | 优先改哪里 |
| --- | --- |
| level5 母线目标从 40 V 改到 70 V | common 层 `SINV_DC_BUS_REF_V`；同时检查平台层 `CTRL_PROT_VBUS_MAX` 和 `CTRL_DCBUS_READY_MIN/MAX` |
| 改额定/基准母线电压 | 对应平台层 `CTRL_DCBUS_VOLTAGE`；它影响 ready/protection/基值相关配置，不再自动改变 level5 母线闭环目标 |
| Buck 输出从 48 V 改到 60 V | 对应平台层 `SINV_BUCK_OUTPUT_REF_V` |
| Buck 输出上升太快/超调 | common 层 `SINV_BUCK_VREF_SLEW_V_S`，必要时再看 Buck 电压环/电流环参数 |
| 仿真交流输入从 36 Vrms 改到 24 Vrms | simulate 平台层 `CTRL_GRID_VOLTAGE_RMS`，同步改 `CTRL_VOLTAGE_BASE ≈ 24*sqrt(2)=33.94`，再重新运行模型初始化或重新打开模型 |
| 实物交流输入从 24 Vrms 改到 36 Vrms | 先调实物交流源；若这是新额定工况，再改 Iris 平台层 `CTRL_GRID_VOLTAGE_RMS` 和 `CTRL_VOLTAGE_BASE ≈ 50.91` |
| PF 从 1 改成 0.8 | level6 使用：common 层 `SINV_POWER_FACTOR_REF`；方向不对时改平台层 `SINV_POWER_FACTOR_Q_SIGN`；幅值偏差再用 common 层 `SINV_POWER_FACTOR_Q_GAIN` 微调 |
| 并网电流 THD/碎波形 | common 层电流环、PLL、FDRC 参数；如果刚换 LC，先确认平台层 `CTRL_AC_INDUCTANCE` 和传感器标定正确 |


## 6. BUILD_LEVEL 分层补充：Buck/Boost 后移

当前分层口径：

- `BUILD_LEVEL = 5`：回到原始单相变流器整流母线闭环，`Q_ref = 0`，不执行 Buck/Boost PWM。用于先验证单相变流器五级链路。
- `BUILD_LEVEL = 6`：在 level5 整流母线闭环基础上加入 PF-derived Q 和后级 Buck 控制。只有 level6 才执行 Buck 正向降压 PWM。
- `BUILD_LEVEL = 7`：低压侧 DC 源经 Buck 半桥反向 Boost 到 DC bus，单相全桥再按 signed P/Q 做并网电流控制。只有 level7 才执行 Boost PWM。

| 想调什么 | 改哪里 | 说明 |
| --- | --- | --- |
| level6 Buck 输出目标 | 对应平台层 `SINV_BUCK_OUTPUT_REF_V` | level6 下 Buck 电压环目标，控制对象是 `adc_v_buck_out`。level1~5 不会执行 Buck PWM。 |
| level6 PF | common 层 `SINV_POWER_FACTOR_REF` / `SINV_POWER_FACTOR_Q_GAIN`，平台层 `SINV_POWER_FACTOR_Q_SIGN` | level6 才使用 PF-derived Q；level5 原始整流固定 `Q_ref = 0`。 |
| level7 并网额定电压 | 对应平台层 `SINV_LEVEL7_GRID_VOLTAGE_RMS` | 这是 level7 的额定/目标 RMS，用于仿真电网源和 RMS 保护窗口；PLL 仍然使用实测 `Vac`，不应拿实时电网波动去动态改这个目标值。 |
| level7 Boost 后的 DC bus 目标 | 对应平台层 `SINV_LEVEL7_DC_BUS_REF_V` | level7 下 DCDC 级的输出目标，控制对象是 `adc_v_bus`。 |
| level7 Boost 低压侧 DC 源 | 对应平台层 `SINV_LEVEL7_BOOST_INPUT_REF_V` | 仿真时外部低压 DC 源设为这个值；控制代码中它也作为 Boost 前馈的名义输入值。 |
| level7 Boost 母线软启动速度 | 对应平台层 `SINV_LEVEL7_BOOST_VBUS_SLEW_V_S` | 内部母线参考从 0 按该斜率爬到 `SINV_LEVEL7_DC_BUS_REF_V`，用于避免启动超调。 |
| level7 Boost 延迟启动时间 | 对应平台层 `SINV_LEVEL7_BOOST_START_DELAY_MS` | 低压侧输入存在且 CiA402 operation-enabled 后，再延迟这么久开始 Boost PWM。 |
| level7 并网有功/无功 | common 层 `SINV_LEVEL7_ACTIVE_POWER_REF_PU`、`SINV_LEVEL7_REACTIVE_POWER_REF_PU` | level7 并网侧直接使用 signed P/Q 指令。实物方向可能受电压/电流传感器极性影响，上电后用小功率确认 P/Q 符号。 |

level7 仿真接线口径：`adc_v_buck_out` 在 level7 中表示 Boost 低压输入侧电压，`adc_v_bus` 表示 Boost 输出母线电压，Buck/Boost 电感电流仍走 `adc_i_buck`。如果模型或实物半桥 PWM 比较值对应的是另一只管子的占空比，表现通常是母线升不起来或 duty 方向反，此时应只在 level7 的 PWM 极性/compare 输出处修正，不要改外环参数来硬凑。
