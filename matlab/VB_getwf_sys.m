function [w, t, t_d] = VB_getwf_sys(deviceName, channel, varargin)
%VB_GETWF_SYS Acquire a waveform via a command-line executable wrapper.
%   [W, T, T_D] = VB_GETWF_SYS(DEVICENAME, CHANNEL) acquires a waveform
%   from the device identified by DEVICENAME on the given CHANNEL by
%   shelling out to an external command-line tool (getWf_cli.exe). The
%   tool writes the acquired samples to a temporary CSV file, which is
%   then read back into MATLAB.
%
%   [W, T, T_D] = VB_GETWF_SYS(DEVICENAME, CHANNEL, 'Name', Value, ...)
%   allows acquisition parameters to be customized using name-value
%   pairs (see OPTIONS below).
%
%   INPUTS:
%       deviceName - String identifying the target instrument/device,
%                    passed directly to the external executable.
%       channel    - Acquisition channel. Can be:
%                       * numeric  -> converted to 'mso/<channel>'
%                       * string   -> used as-is (e.g. a raw channel path)
%
%   NAME-VALUE OPTIONS:
%       NumPoints         - Number of samples to acquire (default: 10000)
%       AcqTime           - Total acquisition time window, in seconds
%                            (default: 0.05)
%       PreTrigger        - Pre-trigger time, in seconds (default: 0.01).
%                            Added back to the returned time vector T.
%       VRange            - Vertical voltage range of the acquisition
%                            channel (default: 5.0)
%       VOffset           - Vertical voltage offset (default: 0.0)
%       Coupling          - Input coupling, e.g. 'DC' or 'AC'
%                            (default: 'DC')
%       Probe             - Probe attenuation factor, e.g. '1x', '10x'
%                            (default: '1x')
%       MaxRetries        - Maximum number of acquisition attempts before
%                            giving up (default: 5)
%       Averages          - Number of waveform averages (default: 1)
%       TriggerSource     - Trigger source keyword: 'Internal', 'External',
%                            or 'Keep' (default: 'Keep', i.e. do not
%                            change the instrument's current trigger
%                            source). If a value other than these three
%                            keywords is passed, it is treated as
%                            shorthand for "use an internal trigger on
%                            this channel" (see NOTES below).
%       TriggerChannel    - Channel used for triggering. Can be numeric
%                            (converted to 'mso/<channel>') or a string.
%                            If omitted, the trigger channel defaults to
%                            'same' (i.e. the acquisition channel) when
%                            TriggerSource is a known keyword.
%       TriggerLevel      - Trigger level, in volts (default: 0.1)
%       TriggerHysteresis - Trigger hysteresis, in volts (default: 0.05)
%       TriggerVRange     - Voltage range of the trigger channel
%                            (default: 5.0)
%       TriggerCoupling   - Trigger channel coupling (default: 'DC')
%       TriggerProbe      - Trigger channel probe attenuation
%                            (default: '1x')
%
%   OUTPUTS:
%       w   - Acquired waveform samples (row vector)
%       t   - Time vector, in seconds, shifted by PreTrigger so that
%             t = 0 corresponds to the trigger event
%       t_d - Raw ("device") time vector as read from the CSV, before
%             the PreTrigger shift is applied
%
%   NOTES:
%       - The requested sample rate is computed as NumPoints / AcqTime.
%         If this exceeds the hardware limit (1 GS/s), the sample rate
%         is clamped and AcqTime is recalculated accordingly, with a
%         warning (VB:SampleRateLimit).
%       - The external executable (getWf_cli.exe) must be available on
%         the system PATH, or in the current working directory.
%       - Acquisition is retried up to MaxRetries times if the system
%         call fails or the returned CSV cannot be read / is empty.
%       - The last sample of both W and T is discarded before returning,
%         to drop a known trailing artifact/placeholder value produced
%         by the external tool.
%
%   Example:
%       [w, t] = VB_getwf_sys('MyScope', 1, 'NumPoints', 5000, ...
%                              'AcqTime', 0.02, 'TriggerLevel', 0.5);

% ------------------------------------------------------------------
% Resolve the acquisition channel string expected by the CLI tool.
% Numeric channels are mapped to the 'mso/<n>' convention; string
% channels are passed through unchanged.
% ------------------------------------------------------------------
if isnumeric(channel)
        channelStr = sprintf('mso/%d', channel);
else
        channelStr = channel;
end

    % ------------------------------------------------------------------
    % Parse name-value options with validation and defaults.
    % ------------------------------------------------------------------
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
    addParameter(p, 'TriggerChannel',    '',      @(x) ischar(x) || isnumeric(x)); % Trigger channel override
    addParameter(p, 'TriggerLevel',      0.1,     @isnumeric);
    addParameter(p, 'TriggerHysteresis', 0.05,    @isnumeric);
    addParameter(p, 'TriggerVRange',     5.0,     @isnumeric);
    addParameter(p, 'TriggerCoupling',   'DC',    @ischar);
    addParameter(p, 'TriggerProbe',      '1x',    @ischar);
    parse(p, varargin{:});
    opt = p.Results;

    % ------------------------------------------------------------------
    % Compute the sample rate implied by NumPoints/AcqTime and clamp it
    % to the hardware's maximum supported rate, adjusting AcqTime to
    % remain consistent with the (possibly clamped) sample rate.
    % ------------------------------------------------------------------
    acqTime = opt.AcqTime;
    numPoints = opt.NumPoints;
    calculatedSampleRate = numPoints / acqTime;
    maxHardwareRate = 1e9;
if calculatedSampleRate > maxHardwareRate
        warning('VB:SampleRateLimit', 'Limiting sample rate to %.0f MS/s.', maxHardwareRate/1e6);
        calculatedSampleRate = maxHardwareRate;
        acqTime = numPoints / calculatedSampleRate;
end

% ------------------------------------------------------------------
% Resolve the trigger source and trigger channel strings passed to
% the CLI tool.
%
% TriggerSource may be one of the known keywords ('Internal',
% 'External', 'Keep'). If the caller passes something else (e.g. a
% channel name/number as a convenience shorthand), it is interpreted
% as "use an internal trigger on this channel", and that raw value is
% used as the trigger channel unless TriggerChannel was also given
% explicitly (which takes priority).
% ------------------------------------------------------------------
triggerSourceRaw = opt.TriggerSource;
isKnownSourceKeyword = any(strcmpi(triggerSourceRaw, {'Internal', 'External', 'Keep'}));

if ~isKnownSourceKeyword
    % TriggerSource is not one of the recognized keywords: treat it as
    % shorthand for "internal trigger on this channel".
    if isnumeric(opt.TriggerChannel) && ~isempty(opt.TriggerChannel)
        trigChanStr = sprintf('mso/%d', opt.TriggerChannel);
    elseif ischar(opt.TriggerChannel) && ~isempty(opt.TriggerChannel)
        % TriggerChannel was given explicitly: it takes priority.
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
        trigChanStr = 'same'; % Not specified: default to the acquisition channel
    end
end

    % ------------------------------------------------------------------
    % Build the command line for the external acquisition executable.
    % Results are written to a temporary CSV file, whose path is passed
    % as the final argument.
    % ------------------------------------------------------------------
    tempDataFile = [tempname, '.csv'];
    exePath = 'getWf_cli.exe';

    cmd = sprintf('"%s" "%s" "%s" %f %f %f %f %f "%s" "%s" %d "%s" "%s" %f %f %f "%s" "%s" "%s"', ...
        exePath, deviceName, channelStr, calculatedSampleRate, acqTime, ...
        opt.PreTrigger, opt.VRange, opt.VOffset, opt.Coupling, opt.Probe, ...
        opt.Averages, triggerSourceStr, trigChanStr, opt.TriggerLevel, ...
        opt.TriggerHysteresis, opt.TriggerVRange, opt.TriggerCoupling, opt.TriggerProbe, tempDataFile);

% ------------------------------------------------------------------
% Acquisition loop: run the external executable and attempt to read
% the resulting CSV. Retries on system-call failure, missing file,
% unreadable data, or an empty buffer, up to opt.MaxRetries times.
% ------------------------------------------------------------------
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
                fprintf('Failed to read data (attempt %d/%d): %s\n', attempt, opt.MaxRetries, ME.message);
end
else
            fprintf('System call error (attempt %d/%d): %s\n', attempt, opt.MaxRetries, cmdout);
end
if ~acquired
if attempt >= opt.MaxRetries
if exist(tempDataFile, 'file'), delete(tempDataFile); end
                error('VB:MaxRetries', 'Failed to acquire waveform after %d attempts.', opt.MaxRetries);
end
            pause(0.5);
end
end

% ------------------------------------------------------------------
% Clean up the temporary CSV file now that its contents have been
% loaded into memory.
% ------------------------------------------------------------------
if exist(tempDataFile, 'file')
        delete(tempDataFile);
end

% ------------------------------------------------------------------
% Drop the trailing sample from both waveform and time vectors. This
% removes a known artifact/placeholder value appended by the external
% tool and not part of the actual acquired data.
% ------------------------------------------------------------------
if ~isempty(w)
        w(end) = [];
        t(end) = [];
end

    % ------------------------------------------------------------------
    % Return the raw device time base (t_d) alongside the pre-trigger-
    % shifted time vector (t), so that t = 0 aligns with the trigger.
    % ------------------------------------------------------------------
    t_d = t;
    t = t + opt.PreTrigger;
end
