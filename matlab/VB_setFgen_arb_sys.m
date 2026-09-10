function [status, cmdout] = VB_setFgen_arb_sys(deviceName, onda_volts, dt)
%VB_SETFGEN_ARB_SYS Send an arbitrary waveform to the VirtualBench FGEN.
%   [STATUS, CMDOUT] = VB_SETFGEN_ARB_SYS(DEVICENAME, ONDA_VOLTS, DT)
%   writes the given waveform samples to a temporary CSV file and passes
%   it to an external command-line tool (setFgen_arb_cli.exe), which
%   loads the arbitrary waveform onto the function generator (FGEN) of
%   the specified VirtualBench device.
%
%   INPUTS:
%       deviceName - String identifying the target instrument, e.g.
%                    'VB8012-31D2661'.
%       onda_volts - Vector of waveform sample points, with amplitude
%                    already expressed in volts. Reshaped to a column
%                    vector before being written to file.
%       dt         - Sample period, i.e. the time between consecutive
%                    samples, in seconds.
%
%   OUTPUTS:
%       status - Numeric exit status returned by the system call
%                (0 indicates success; nonzero indicates failure).
%       cmdout - String containing the console output produced by the
%                external executable (used for diagnostics/errors).
%
%   NOTES:
%       - The external executable (setFgen_arb_cli.exe) must be
%         available on the system PATH, or in the current working
%         directory.
%       - The waveform is written to a uniquely named temporary CSV
%         file, which is deleted after the command completes,
%         regardless of success or failure.
%       - On failure (nonzero status), a warning (not an error) is
%         issued, and the temporary file is still cleaned up; the
%         caller is responsible for checking STATUS if the failure
%         should be treated as fatal.
%       - dt is passed to the external tool using scientific notation
%         ('%e') to preserve precision for very small sample periods.
%
%   Example:
%       t  = 0:1e-6:1e-3;
%       wf = 2*sin(2*pi*1000*t);
%       [status, cmdout] = VB_setFgen_arb_sys('VB8012-31D2661', wf, 1e-6);

    % ------------------------------------------------------------------
    % Create a secure, uniquely named temporary CSV file to hold the
    % waveform samples.
    % ------------------------------------------------------------------
    tempAWGFile = [tempname, '.csv'];
    
    % ------------------------------------------------------------------
    % Ensure the waveform is a column vector and write it to the
    % temporary file.
    % ------------------------------------------------------------------
    writematrix(onda_volts(:), tempAWGFile);
    
    % ------------------------------------------------------------------
    % Path to the external C executable. Make sure it is on the system
    % PATH or in the current working directory.
    % ------------------------------------------------------------------
    exePath = 'setFgen_arb_cli.exe';
    
    % ------------------------------------------------------------------
    % Build the system command. '%e' is used for dt to preserve
    % precision via scientific notation.
    % ------------------------------------------------------------------
    cmd = sprintf('"%s" "%s" "%s" %e', exePath, deviceName, tempAWGFile, dt);
    
    % ------------------------------------------------------------------
    % Execute the command.
    % ------------------------------------------------------------------
    [status, cmdout] = system(cmd);
    
    % ------------------------------------------------------------------
    % Check for errors at the MATLAB level. A nonzero status produces a
    % warning rather than an error, so the caller can decide how to
    % handle the failure using the returned STATUS/CMDOUT.
    % ------------------------------------------------------------------
    if status ~= 0
        warning('Failed to send arbitrary waveform. Output: %s', cmdout);
    end
    
    % ------------------------------------------------------------------
    % Clean up the temporary waveform file.
    % ------------------------------------------------------------------
    if exist(tempAWGFile, 'file')
        delete(tempAWGFile);
    end
end
