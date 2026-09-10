function VB_setFgen(deviceName, varargin)
% VB_SETFGEN Configura e liga o Gerador de Funções (FGEN) via comando de sistema.

    p = inputParser;
    p.CaseSensitive = false;
    addParameter(p, 'Waveform',  'sine',  @ischar);
    addParameter(p, 'Amplitude', 2.0,     @isnumeric);
    addParameter(p, 'Offset',    0.0,     @isnumeric);
    addParameter(p, 'Frequency', 1000.0,  @isnumeric);
    addParameter(p, 'Phase',     0.0,     @isnumeric);
    parse(p, varargin{:});
    opt = p.Results;

    % Formata o comando para o sistema operacional
    % Nota: Certifique-se de que 'setFgen_cli.exe' está no path do MATLAB ou na mesma pasta
    exePath = 'setFgen_cli.exe'; % Altere para o caminho completo se necessário
    
    cmd = sprintf('"%s" "%s" "%s" %f %f %f %f', ...
        exePath, deviceName, opt.Waveform, opt.Amplitude, opt.Offset, opt.Frequency, opt.Phase);

    % Executa o comando no terminal do sistema
    [status, cmdout] = system(cmd);

    if status ~= 0
        error('VB:SystemError', 'Falha ao configurar FGEN. Saída do console:\n%s', cmdout);
    else
        fprintf('%s', cmdout); % Mostra a mensagem de sucesso do C
    end
end