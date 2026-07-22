function [metrics, simOut] = run_gfl_validation(varargin)
%RUN_GFL_VALIDATION 运行联合仿真，保存关键波形并计算稳态指标。

simDir = fileparts(mfilename('fullpath'));
defaultResults = fullfile(simDir, 'validation_results');

p = inputParser;
p.addParameter('ModelFile', fullfile(simDir, ...
    'DP_STD_MDL_DCAC_3ph_2level_gridconn.slx'), @(x) ischar(x) || isstring(x));
p.addParameter('Executable', fullfile(simDir, 'x64', 'Debug', ...
    'Digital_Power_simulink.exe'), @(x) ischar(x) || isstring(x));
p.addParameter('StopTime', '', @(x) ischar(x) || isstring(x) || isnumeric(x));
p.addParameter('ResultsDirectory', defaultResults, @(x) ischar(x) || isstring(x));
p.addParameter('DcBusReferenceV', 55.0, @(x) isnumeric(x) && isscalar(x) && x > 0);
p.addParameter('BuckDuty', 0.45, @(x) isnumeric(x) && isscalar(x) && x > 0 && x < 1);
p.parse(varargin{:});
opts = p.Results;

[simOut, runInfo] = run_gfl_cosim( ...
    'ModelFile', opts.ModelFile, ...
    'Executable', opts.Executable, ...
    'StopTime', opts.StopTime, ...
    'ConfigureModelFcn', @localConfigureLogging);

[tBus, vBus] = localSeries(simOut, 'gfl_dc_bus_voltage');
[tBuck, vBuck] = localSeries(simOut, 'gfl_buck_output_voltage');
[tGrid, vGrid] = localSeries(simOut, 'gfl_grid_phase_voltage');
[tCurrent, iabc] = localSeries(simOut, 'gfl_grid_current_abc');
[tPll, pll] = localSeries(simOut, 'gfl_pll_monitor');
[tIdq, idq] = localSeries(simOut, 'gfl_idq_monitor');

vBus = vBus(:, 1);
vBuck = vBuck(:, 1);
vGrid = vGrid(:, 1);
if size(iabc, 2) < 3
    error('run_gfl_validation:CurrentChannels', '三相电流日志少于 3 个通道。');
end

busSteady = localSteadyIndex(tBus);
buckSteady = localSteadyIndex(tBuck);
currentSteady = localSteadyIndex(tCurrent);

metrics = struct;
metrics.DcBusReferenceV = opts.DcBusReferenceV;
metrics.DcBusMeanV = mean(vBus(busSteady));
metrics.DcBusRippleVpp = localPeakToPeak(vBus(busSteady));
metrics.DcBusRipplePercent = 100 * metrics.DcBusRippleVpp / metrics.DcBusMeanV;
metrics.DcBusOvershootPercent = max(0, 100 * (max(vBus) - opts.DcBusReferenceV) / opts.DcBusReferenceV);
metrics.DcBusSettlingTimeS = localSettlingTime(tBus, vBus, opts.DcBusReferenceV, 0.02);
metrics.BuckDuty = opts.BuckDuty;
metrics.BuckIdealOutputV = opts.DcBusReferenceV * opts.BuckDuty;
metrics.BuckOutputMeanV = mean(vBuck(buckSteady));
metrics.BuckOutputRippleVpp = localPeakToPeak(vBuck(buckSteady));
metrics.GridPhaseVoltageRmsV = localRms(vGrid(localSteadyIndex(tGrid)));
metrics.GridPhaseCurrentRmsA = localRms(iabc(currentSteady, 1));

vaOnCurrentTime = interp1(tGrid, vGrid, tCurrent(currentSteady), 'linear', 'extrap');
ia = iabc(currentSteady, 1);
metrics.GridPowerFactorA = mean(vaOnCurrentTime .* ia) / ...
    max(eps, localRms(vaOnCurrentTime) * localRms(ia));
metrics.EstimatedThreePhasePowerW = 3 * mean(vaOnCurrentTime .* ia);
metrics.GridCurrentThdPercent = localThd(tCurrent(currentSteady), ia, 50.0);
metrics.PllErrorRmsPu = localRms(pll(localSteadyIndex(tPll), 1));
metrics.IdMeanPu = mean(idq(localSteadyIndex(tIdq), 1));
metrics.IqMeanPu = mean(idq(localSteadyIndex(tIdq), min(2, size(idq, 2))));
metrics.DcBusWithin2Percent = abs(metrics.DcBusMeanV - opts.DcBusReferenceV) <= ...
    0.02 * opts.DcBusReferenceV;
metrics.DcBusStable = metrics.DcBusWithin2Percent && ...
    metrics.DcBusRipplePercent <= 2.0 && metrics.DcBusOvershootPercent <= 10.0;

resultsDir = char(opts.ResultsDirectory);
if ~isfolder(resultsDir)
    mkdir(resultsDir);
end
stamp = char(datetime('now', 'Format', 'yyyyMMdd_HHmmss'));
baseName = fullfile(resultsDir, ['gfl_validation_' stamp]);

fig = figure('Name', 'GFL AFE Validation', 'Color', 'w');
layout = tiledlayout(fig, 3, 1, 'TileSpacing', 'compact');
nexttile(layout);
plot(tBus, vBus, 'LineWidth', 1.1);
yline(opts.DcBusReferenceV, '--');
grid on; ylabel('Vdc1 (V)'); title('AFE DC bus');
nexttile(layout);
plot(tBuck, vBuck, 'LineWidth', 1.1);
yline(metrics.BuckIdealOutputV, '--');
grid on; ylabel('Vbuck (V)'); title('BUCK output, fixed duty');
nexttile(layout);
plot(tCurrent, iabc(:, 1:3), 'LineWidth', 0.9);
grid on; ylabel('Iabc (A)'); xlabel('Time (s)'); title('Grid currents');
exportgraphics(fig, [baseName '.png'], 'Resolution', 160);

metricTable = struct2table(metrics, 'AsArray', true);
writetable(metricTable, [baseName '_metrics.csv']);
save([baseName '.mat'], 'metrics', 'simOut', 'runInfo');

disp(metricTable);
fprintf('[GFL] 波形: %s.png\n', baseName);
fprintf('[GFL] 指标: %s_metrics.csv\n', baseName);
end

function localConfigureLogging(modelName)
logs = {
    'Scope2', 'gfl_dc_bus_voltage'
    'Scope1', 'gfl_buck_output_voltage'
    'Scope3', 'gfl_grid_phase_voltage'
    'Current Signal Analyzer', 'gfl_grid_current_abc'
    'Scope 4', 'gfl_pll_monitor'
    'Scope 6', 'gfl_idq_monitor'
};

for k = 1:size(logs, 1)
    block = [modelName '/' logs{k, 1}];
    if isempty(find_system(modelName, 'SearchDepth', 1, 'Name', logs{k, 1}))
        error('run_gfl_validation:MissingBlock', '模型缺少验证块: %s', block);
    end
    params = get_param(block, 'ObjectParameters');
    set_param(block, 'DataLogging', 'on', 'DataLoggingVariableName', logs{k, 2});
    if isfield(params, 'DataLoggingSaveFormat')
        set_param(block, 'DataLoggingSaveFormat', 'Structure With Time');
    end
end
end

function [time, data] = localSeries(simOut, variableName)
try
    raw = simOut.get(variableName);
catch
    error('run_gfl_validation:MissingLog', '仿真输出中没有日志变量 %s。', variableName);
end

if isa(raw, 'timeseries')
    time = raw.Time(:);
    data = squeeze(raw.Data);
elseif isstruct(raw) && isfield(raw, 'time') && isfield(raw, 'signals')
    time = raw.time(:);
    signals = raw.signals;
    data = [];
    for k = 1:numel(signals)
        data = [data, reshape(signals(k).values, numel(time), [])]; %#ok<AGROW>
    end
else
    error('run_gfl_validation:UnsupportedLog', '不支持的日志格式: %s', class(raw));
end

if isvector(data)
    data = data(:);
end
end

function index = localSteadyIndex(time)
steadyStart = max(0.8 * time(end), time(end) - 0.5);
index = time >= steadyStart;
end

function value = localRms(data)
value = sqrt(mean(data .^ 2));
end

function value = localPeakToPeak(data)
value = max(data) - min(data);
end

function settlingTime = localSettlingTime(time, data, target, tolerance)
outside = abs(data - target) > tolerance * target;
lastOutside = find(outside, 1, 'last');
if isempty(lastOutside)
    settlingTime = time(1);
elseif lastOutside == numel(time)
    settlingTime = NaN;
else
    settlingTime = time(lastOutside + 1);
end
end

function thdPercent = localThd(time, data, fundamentalHz)
time = time(:);
data = data(:) - mean(data);
sampleTime = median(diff(time));
uniformTime = (time(1):sampleTime:time(end)).';
uniformData = interp1(time, data, uniformTime, 'linear');
n = numel(uniformData);
window = 0.5 - 0.5 * cos(2 * pi * (0:n-1).' / max(1, n-1));
spectrum = abs(fft(uniformData .* window));
frequency = (0:n-1).' / (n * sampleTime);

[~, fundamentalIndex] = min(abs(frequency - fundamentalHz));
fundamental = spectrum(fundamentalIndex);
harmonicSquared = 0;
for harmonic = 2:floor((0.5 / sampleTime) / fundamentalHz)
    [~, index] = min(abs(frequency - harmonic * fundamentalHz));
    harmonicSquared = harmonicSquared + spectrum(index) ^ 2;
end
thdPercent = 100 * sqrt(harmonicSquared) / max(eps, fundamental);
end
