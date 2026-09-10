#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nivirtualbench.h"

// Função auxiliar para contar linhas e ler o CSV
size_t readWaveform(const char* filename, double** waveform) {
    FILE* fp = fopen(filename, "r");
    if (!fp) return 0;

    size_t count = 0;
    double temp;
    // Conta os pontos
    while (fscanf(fp, "%lf", &temp) == 1) count++;
    
    rewind(fp);
    *waveform = (double*)malloc(count * sizeof(double));
    
    for (size_t i = 0; i < count; i++) {
        fscanf(fp, "%lf", &(*waveform)[i]);
    }
    
    fclose(fp);
    return count;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: exe <DeviceName> <ArquivoCSV> <SamplePeriod>\n");
        return 1;
    }

    const char* deviceName   = argv[1];
    const char* csvFile      = argv[2];
    double samplePeriod      = atof(argv[3]);

    // 1. Lê a onda do arquivo
    double* waveform = NULL;
    size_t waveformSize = readWaveform(csvFile, &waveform);
    
    if (waveformSize == 0) {
        fprintf(stderr, "Erro ao ler o arquivo CSV ou arquivo vazio.\n");
        return 1;
    }

    // 2. Inicializa o VirtualBench
    niVB_LibraryHandle        libHandle = 0;
    niVB_FGEN_InstrumentHandle fgenHandle = 0;
    niVB_Status status;

    status = niVB_Initialize(NIVB_LIBRARY_VERSION, &libHandle);
    if (niVB_Status_Failed(status)) { free(waveform); return 1; }

    // Conecta ao FGEN (reset = true para limpar configs anteriores)
    status = niVB_FGEN_Initialize(libHandle, deviceName, true, &fgenHandle);
    if (niVB_Status_Failed(status)) { niVB_Finalize(libHandle); free(waveform); return 1; }

    // 3. Configura a onda arbitrária
    status = niVB_FGEN_ConfigureArbitraryWaveform(fgenHandle, waveform, waveformSize, samplePeriod);
    if (niVB_Status_Failed(status)) {
        fprintf(stderr, "Erro ao configurar AWG. Codigo: %d\n", status);
    } else {
        // 4. Liga a saída!
        niVB_FGEN_Run(fgenHandle);
        printf("Onda arbitraria carregada com sucesso! (%zu pontos)\n", waveformSize);
    }

    // 5. Limpeza (Não usamos FGEN_Close aqui para que o sinal continue saindo)
    // niVB_FGEN_Close(fgenHandle); 
    free(waveform);
    niVB_Finalize(libHandle);
    return 0;
}
