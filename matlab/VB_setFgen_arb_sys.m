function [status, cmdout] = VB_setFgen_arb_sys(deviceName, onda_volts, dt)
% VB_SETFGEN_ARB_SYS Envia uma onda arbitrária para o FGEN do VirtualBench
%
% INPUTS:
%   deviceName - String com o nome do equipamento (ex: 'VB8012-31D2661')
%   onda_volts - Vetor com os pontos da onda com amplitude já em Volts
%   dt         - Sample Period (tempo entre cada amostra em segundos)

    % Cria um arquivo CSV temporário seguro
    tempAWGFile = [tempname, '.csv'];
    
    % Garante que a onda seja um vetor coluna e salva
    writematrix(onda_volts(:), tempAWGFile);
    
    % Caminho do executável em C (certifique-se de que está na mesma pasta ou no PATH)
    exePath = 'setFgen_arb_cli.exe';
    
    % Monta o comando do sistema (%e é usado para notação científica do dt)
    cmd = sprintf('"%s" "%s" "%s" %e', exePath, deviceName, tempAWGFile, dt);
    
    % Executa o comando
    [status, cmdout] = system(cmd);
    
    % Verifica erros no nível do MATLAB
    if status ~= 0
        warning('Falha ao enviar onda arbitraria. Saída: %s', cmdout);
    end
    
    % Limpeza do arquivo temporário
    if exist(tempAWGFile, 'file')
        delete(tempAWGFile);
    end
end
