#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nivirtualbench.h"

// Função auxiliar para conversão de string para enum de forma de onda
static niVB_Waveform strToWaveform(const char *s) {
    if (_stricmp(s, "square")   == 0) return niVB_Waveform_Square;
    if (_stricmp(s, "triangle") == 0) return niVB_Waveform_Triangle;
    if (_stricmp(s, "dc")       == 0) return niVB_Waveform_DC;
    return niVB_Waveform_Sine;
}

int main(int argc, char *argv[]) {
    // Espera 7 argumentos: Nome do .exe + 6 parâmetros
    if (argc != 7) {
        fprintf(stderr, "Uso: setFgen_cli <device> <waveform> <amplitude> <offset> <frequency> <phase>\n");
        return 1;
    }

    const char* deviceName = argv[1];
    const char* waveform   = argv[2];
    double amplitude       = atof(argv[3]);
    double offset          = atof(argv[4]);
    double frequency       = atof(argv[5]);
    double phase           = atof(argv[6]);

    niVB_LibraryHandle         libHandle = 0;
    niVB_FGEN_InstrumentHandle fgenHandle = 0;
    niVB_Status status;

    status = niVB_Initialize(NIVB_LIBRARY_VERSION, &libHandle);
    if (niVB_Status_Failed(status)) {
        fprintf(stderr, "Erro: Falha ao inicializar a biblioteca NI VirtualBench.\n");
        return 1;
    }

    status = niVB_FGEN_Initialize(libHandle, deviceName, true, &fgenHandle);
    if (niVB_Status_Failed(status)) {
        niVB_Finalize(libHandle);
        fprintf(stderr, "Erro: Falha ao conectar ao FGEN do dispositivo %s.\n", deviceName);
        return 1;
    }

    status = niVB_FGEN_ConfigureStandardWaveform(fgenHandle, strToWaveform(waveform),
                                                 amplitude, offset, frequency, phase);
    
    status = niVB_FGEN_Run(fgenHandle);
    
    printf("FGEN Configurado com sucesso: %s | %.2f Vpp | %.1f Hz\n", waveform, amplitude, frequency);

    // Fecha os handles
    niVB_FGEN_Close(fgenHandle);
    niVB_Finalize(libHandle);

    return 0; // Sucesso
}
