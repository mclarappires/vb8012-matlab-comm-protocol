function VB_setFgen(deviceName, varargin)
%VB_SETFGEN Configure and enable the Function Generator (FGEN) via a
%command-line executable wrapper.
%   VB_SETFGEN(DEVICENAME) configures and turns on the function
%   generator on the device identified by DEVICENAME, using default
%   waveform settings, by shelling out to an external command-line tool
%   (setFgen_cli.exe).
%
%   VB_SETFGEN(DEVICENAME, 'Name', Value, ...) allows the waveform
%   parameters to be customized using name-value pairs (see OPTIONS
%   below).
%
%   INPUTS:
%       deviceName - String identifying the target instrument/device,
%                    passed directly to the external executable.
%
%   NAME-VALUE OPTIONS:
%       Waveform  - Waveform shape, e.g. 'sine', 'square', 'triangle'
%                   (default: 'sine')
%       Amplitude - Signal amplitude, in volts (default: 2.0)
%       Offset    - DC offset, in volts (default: 0.0)
%       Frequency - Signal frequency, in Hz (default: 1000.0)
%       Phase     - Signal phase, in degrees (default: 0.0)
%
%   OUTPUTS:
%       None. Prints the success message returned by the external tool
%       on success, or throws an error (VB:SystemError) on failure.
%
%   NOTES:
%       - The external executable (setFgen_cli.exe) must be available
%         on the system PATH, or in the current working directory.
%       - Update exePath below to a full path if the executable is not
%         accessible from MATLAB's current working directory or PATH.
%
%   Example:
%       VB_setFgen('MyFgen', 'Waveform', 'square', 'Amplitude', 1.5, ...
%                   'Frequency', 5000);

    % ------------------------------------------------------------------
    % Parse name-value options with validation and defaults.
    % ------------------------------------------------------------------
    p = inputParser;
    p.CaseSensitive = false;
    addParameter(p, 'Waveform',  'sine',  @ischar);
    addParameter(p, 'Amplitude', 2.0,     @isnumeric);
    addParameter(p, 'Offset',    0.0,     @isnumeric);
    addParameter(p, 'Frequency', 1000.0,  @isnumeric);
    addParameter(p, 'Phase',     0.0,     @isnumeric);
    parse(p, varargin{:});
    opt = p.Results;

    % ------------------------------------------------------------------
    % Build the command line for the external FGEN configuration
    % executable.
    % Note: make sure 'setFgen_cli.exe' is on the MATLAB path or in the
    % same folder; update exePath to a full path otherwise.
    % ------------------------------------------------------------------
    exePath = 'setFgen_cli.exe'; % Change to full path if needed
    
    cmd = sprintf('"%s" "%s" "%s" %f %f %f %f', ...
        exePath, deviceName, opt.Waveform, opt.Amplitude, opt.Offset, opt.Frequency, opt.Phase);

    % ------------------------------------------------------------------
    % Execute the command in the system terminal.
    % ------------------------------------------------------------------
    [status, cmdout] = system(cmd);

    if status ~= 0
        error('VB:SystemError', 'Failed to configure FGEN. Console output:\n%s', cmdout);
    else
        fprintf('%s', cmdout); % Display the success message from the C executable
    end
end
