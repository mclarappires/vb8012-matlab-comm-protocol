function [w, t, t_d] = VB_getwf_sys(deviceName, channel, varargin)
% VB_GETWF Adquire forma de onda via executável de linha de comando.
if isnumeric(channel)
        channelStr = sprintf('mso/%d', channel);
else
        channelStr = channel;
end
    p = inputParser;
    p.CaseSensitive = false;
    addParameter(p, 'NumPoints',         10000,   @isnumeric);
    addParameter(p, 'AcqTime',           0.05,    @isnumeric);
    addParameter(p, 'PreTrigger',        0.01,    @isnumeric);
    addParameter(p, 'VRange',            5.0,     @isnumeric);
    addParameter(p, 'VOffset',           0.0,     @isnumeric);
    addParameter(p, 'Coupling',          'DC',    @ischar);
    addParameter(p, 'Probe',             '1x',    @ischar);
    addParameter(p, 'MaxRetries',        5,       @isnumeric);
    addParameter(p, 'Averages',          1,       @isnumeric);
    addParameter(p, 'TriggerSource',     'Keep',  @ischar);
    addParameter(p, 'TriggerChannel',    '',      @(x) ischar(x) || isnumeric(x)); % NOVO: Canal de disparo
    addParameter(p, 'TriggerLevel',      0.1,     @isnumeric);
    addParameter(p, 'TriggerHysteresis', 0.05,    @isnumeric);
    addParameter(p, 'TriggerVRange',     5.0,     @isnumeric);
    addParameter(p, 'TriggerCoupling',   'DC',    @ischar);
    addParameter(p, 'TriggerProbe',      '1x',    @ischar);
    parse(p, varargin{:});
    opt = p.Results;
    acqTime = opt.AcqTime;
    numPoints = opt.NumPoints;
    calculatedSampleRate = numPoints / acqTime;
    maxHardwareRate = 1e9;
if calculatedSampleRate > maxHardwareRate
        warning('VB:SampleRateLimit', 'Limitando taxa para %.0f MS/s.', maxHardwareRate/1e6);
        calculatedSampleRate = maxHardwareRate;
        acqTime = numPoints / calculatedSampleRate;
end

triggerSourceRaw = opt.TriggerSource;
isKnownSourceKeyword = any(strcmpi(triggerSourceRaw, {'Internal', 'External', 'Keep'}));

if ~isKnownSourceKeyword
    % TriggerSource nao e uma palavra-chave conhecida: assume que o
    % usuario quis dizer "trigger interno neste canal".
    if isnumeric(opt.TriggerChannel) && ~isempty(opt.TriggerChannel)
        trigChanStr = sprintf('mso/%d', opt.TriggerChannel);
    elseif ischar(opt.TriggerChannel) && ~isempty(opt.TriggerChannel)
        % TriggerChannel foi informado explicitamente: tem prioridade.
        trigChanStr = opt.TriggerChannel;
    else
        trigChanStr = triggerSourceRaw;
    end
    triggerSourceStr = 'Internal';
else
    triggerSourceStr = triggerSourceRaw;
    if isnumeric(opt.TriggerChannel) && ~isempty(opt.TriggerChannel)
        trigChanStr = sprintf('mso/%d', opt.TriggerChannel);
    elseif ischar(opt.TriggerChannel) && ~isempty(opt.TriggerChannel)
        trigChanStr = opt.TriggerChannel;
    else
        trigChanStr = 'same'; % Se nao for informado, usa o mesmo canal de aquisicao
    end
end
    tempDataFile = [tempname, '.csv'];
    exePath = 'getWf_cli.exe';

    cmd = sprintf('"%s" "%s" "%s" %f %f %f %f %f "%s" "%s" %d "%s" "%s" %f %f %f "%s" "%s" "%s"', ...
        exePath, deviceName, channelStr, calculatedSampleRate, acqTime, ...
        opt.PreTrigger, opt.VRange, opt.VOffset, opt.Coupling, opt.Probe, ...
        opt.Averages, triggerSourceStr, trigChanStr, opt.TriggerLevel, ...
        opt.TriggerHysteresis, opt.TriggerVRange, opt.TriggerCoupling, opt.TriggerProbe, tempDataFile);
% Loop de aquisição
    acquired = false;
    attempt  = 0;
while ~acquired
        attempt = attempt + 1;
        [status, cmdout] = system(cmd);
if status == 0 && exist(tempDataFile, 'file')
try
                data = readmatrix(tempDataFile);
if ~isempty(data)
                    t = data(:, 1)';
                    w = data(:, 2)';
                    acquired = true;
else
                    error('Buffer vazio retornado no CSV.');
end
catch ME
                fprintf('Falha ao ler dados (%d/%d): %s\n', attempt, opt.MaxRetries, ME.message);
end
else
            fprintf('Erro na chamada de sistema (%d/%d): %s\n', attempt, opt.MaxRetries, cmdout);
end
if ~acquired
if attempt >= opt.MaxRetries
if exist(tempDataFile, 'file'), delete(tempDataFile); end
                error('VB:MaxRetries', 'Falha ao adquirir após %d tentativas.', opt.MaxRetries);
end
            pause(0.5);
end
end
% Limpeza
if exist(tempDataFile, 'file')
        delete(tempDataFile);
end
if ~isempty(w)
        w(end) = [];
        t(end) = [];
end
    t_d = t;
    t = t + opt.PreTrigger;
end
