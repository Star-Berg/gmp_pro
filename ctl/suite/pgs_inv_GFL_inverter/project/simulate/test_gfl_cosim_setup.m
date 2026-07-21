function tests = test_gfl_cosim_setup
% 仿真自动化与通道映射的静态回归测试。
tests = functiontests(localfunctions);
end

function testRequiredFilesExist(testCase)
simDir = fileparts(mfilename('fullpath'));
required = {
    'xplt/sim_control_overrides.h'
    'run_gfl_cosim.m'
    'run_gfl_validation.m'
    'VALIDATION_CHANGES.md'
};

for k = 1:numel(required)
    verifyTrue(testCase, isfile(fullfile(simDir, required{k})), ...
        sprintf('Missing required file: %s', required{k}));
end
end

function testDcChannelsAreNotSwapped(testCase)
simDir = fileparts(mfilename('fullpath'));
source = fileread(fullfile(simDir, 'xplt', 'xplt.ctl_interface.h'));

verifyNotEmpty(testCase, regexp(source, ...
    'idc_src\s*=\s*simulink_rx_buffer\.adc_result\[INV_ADC_ID_IDC\]', 'once'));
verifyNotEmpty(testCase, regexp(source, ...
    'udc_src\s*=\s*simulink_rx_buffer\.adc_result\[INV_ADC_ID_VDC\]', 'once'));
end

function testSimulationOverrides(testCase)
simDir = fileparts(mfilename('fullpath'));
source = fileread(fullfile(simDir, 'xplt', 'sim_control_overrides.h'));

verifyNotEmpty(testCase, regexp(source, ...
    '#define\s+CTRL_DCBUS_VOLTAGE\s+\(55\.0f\)', 'once'));
verifyNotEmpty(testCase, regexp(source, ...
    '#define\s+GFL_DCBUS_VOLTAGE_REF_V\s+\(55\.0f\)', 'once'));
verifyNotEmpty(testCase, regexp(source, ...
    '#define\s+GFL_GRID_VOLTAGE_PU\s+\(0\.7200f\)', 'once'));
end

function testBuckDutyIsFixedAtPoint45(testCase)
simDir = fileparts(mfilename('fullpath'));
modelFile = fullfile(simDir, 'DP_STD_MDL_DCAC_3ph_2level_gridconn.slx');
entries = unzip(modelFile, tempname);
rootXml = entries{endsWith(entries, fullfile('simulink', 'systems', 'system_root.xml'))};
source = fileread(rootXml);

verifyNotEmpty(testCase, regexp(source, ...
    '<Block BlockType="Constant" Name="D"[^>]*SID="762">.*?<P Name="Value">1350</P>', ...
    'once'));
verifyNotEmpty(testCase, regexp(source, ...
    '<P Name="CMP_MAX">3000-1</P>', 'once'));
end
