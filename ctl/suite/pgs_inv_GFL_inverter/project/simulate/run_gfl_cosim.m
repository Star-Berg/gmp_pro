function [simOut, runInfo] = run_gfl_cosim(varargin)
%RUN_GFL_COSIM 后台启动控制器 EXE，并运行当前 GFL Simulink 模型。

simDir = fileparts(mfilename('fullpath'));
defaultModel = fullfile(simDir, 'DP_STD_MDL_DCAC_3ph_2level_gridconn.slx');
defaultExe = fullfile(simDir, 'x64', 'Debug', 'Digital_Power_simulink.exe');

p = inputParser;
p.addParameter('ModelFile', defaultModel, @(x) ischar(x) || isstring(x));
p.addParameter('Executable', defaultExe, @(x) ischar(x) || isstring(x));
p.addParameter('StopTime', '', @(x) ischar(x) || isstring(x) || isnumeric(x));
p.addParameter('ConfigureModelFcn', [], @(x) isempty(x) || isa(x, 'function_handle'));
p.addParameter('KeepControllerRunning', false, @(x) islogical(x) && isscalar(x));
p.addParameter('CloseModel', true, @(x) islogical(x) && isscalar(x));
p.parse(varargin{:});
opts = p.Results;

modelFile = char(opts.ModelFile);
exeFile = char(opts.Executable);
if ~isfile(modelFile)
    error('run_gfl_cosim:MissingModel', '找不到 Simulink 模型: %s', modelFile);
end
if ~isfile(exeFile)
    error('run_gfl_cosim:MissingExecutable', ...
        '找不到控制器 EXE: %s\n请先生成 gmp_src_mgr/gmp_src，并编译 Debug|x64。', exeFile);
end

[~, modelName] = fileparts(modelFile);
wasLoaded = bdIsLoaded(modelName);
try
    if ~wasLoaded
        load_system(modelFile);
    end
catch cause
    if contains(cause.message, '较新版本') || contains(lower(cause.message), 'newer version')
        error('run_gfl_cosim:ModelVersion', ...
            '模型由更高版本 Simulink 保存。当前主模型需要 R2025a 或更高版本: %s', modelFile);
    end
    rethrow(cause);
end

try
    if ~isempty(opts.StopTime)
        set_param(modelName, 'StopTime', char(string(opts.StopTime)));
    end
    if ~isempty(opts.ConfigureModelFcn)
        opts.ConfigureModelFcn(modelName);
    end

    process = localStartController(exeFile, simDir);
catch cause
    if opts.CloseModel && ~wasLoaded && bdIsLoaded(modelName)
        close_system(modelName, 0);
    end
    rethrow(cause);
end

cleanup = onCleanup(@() localCleanup(process, modelName, wasLoaded, opts));

fprintf('[GFL] 控制器 PID %d 已后台启动。\n', process.Id);
fprintf('[GFL] 开始仿真模型 %s。\n', modelName);
simOut = sim(modelName, 'ReturnWorkspaceOutputs', 'on');

runInfo = struct;
runInfo.ModelFile = modelFile;
runInfo.Executable = exeFile;
runInfo.ControllerProcessId = double(process.Id);
runInfo.StopTime = get_param(modelName, 'StopTime');
runInfo.ControllerKeptRunning = opts.KeepControllerRunning;
end

function process = localStartController(exeFile, workingDirectory)
startInfo = System.Diagnostics.ProcessStartInfo;
startInfo.FileName = exeFile;
startInfo.WorkingDirectory = workingDirectory;
startInfo.UseShellExecute = false;
startInfo.CreateNoWindow = true;

process = System.Diagnostics.Process;
process.StartInfo = startInfo;
if ~process.Start()
    error('run_gfl_cosim:ControllerStartFailed', '控制器进程启动失败: %s', exeFile);
end

pause(0.35);
if process.HasExited
    error('run_gfl_cosim:ControllerExited', ...
        '控制器进程提前退出，退出码 %d。请检查 network.json 和端口占用。', process.ExitCode);
end
end

function localCleanup(process, modelName, wasLoaded, opts)
if ~opts.KeepControllerRunning && ~isempty(process)
    try
        if ~process.HasExited
            process.Kill();
            process.WaitForExit(2000);
        end
    catch cause
        warning('run_gfl_cosim:CleanupFailed', '结束控制器进程失败: %s', cause.message);
    end
end

if opts.CloseModel && ~wasLoaded && bdIsLoaded(modelName)
    close_system(modelName, 0);
end
end
