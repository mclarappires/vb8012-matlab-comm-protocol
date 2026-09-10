#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "nivirtualbench.h"

/* ------------------------------------------------------------------------
 * Se a chamada falhar, salta para o label Cleanup e reporta o erro.
 * ------------------------------------------------------------------------ */
#define niVB_ErrorCheck(function) \
    if (niVB_Status_Assign(&status, (function)) && niVB_Status_Failed(status)) { \
        goto Cleanup; \
    }

static niVB_MSO_Coupling strToCoupling(const char *s) {
    if (_stricmp(s, "AC") == 0) return niVB_MSO_Coupling_AC;
    return niVB_MSO_Coupling_DC;
}

static niVB_MSO_ProbeAttenuation strToProbe(const char *s) {
    if (_stricmp(s, "10x") == 0) return niVB_MSO_ProbeAttenuation_10x;
    return niVB_MSO_ProbeAttenuation_1x;
}

/* ------------------------------------------------------------------------
 * Normaliza o nome do canal para o formato esperado pela VirtualBench.
 * Aceita formatos comuns de entrada como "ch1", "CH1", "1", "mso1" -> "mso/1".
 * ------------------------------------------------------------------------ */
static void normalizeChannelName(const char *input, char *out, size_t outSize) {
    const char *p = input;
    int channelNum = -1;

    /* Já está no formato correto "mso/N" */
    if (_strnicmp(input, "mso/", 4) == 0) {
        strncpy(out, input, outSize - 1);
        out[outSize - 1] = '\0';
        return;
    }

    /* Pula prefixos textuais conhecidos: "ch", "CH", "mso" (sem barra) */
    if (_strnicmp(p, "ch", 2) == 0) {
        p += 2;
    } else if (_strnicmp(p, "mso", 3) == 0) {
        p += 3;
    }

    /* Pula eventuais separadores como '/', '_', '-', espaço */
    while (*p == '/' || *p == '_' || *p == '-' || *p == ' ') p++;

    /* O que sobrou deveria ser o número do canal */
    if (*p >= '0' && *p <= '9') {
        channelNum = atoi(p);
    }

    if (channelNum >= 0) {
        snprintf(out, outSize, "mso/%d", channelNum);
    } else {
        /* Não reconhecido: repassa como veio e deixa a API reportar o erro */
        strncpy(out, input, outSize - 1);
        out[outSize - 1] = '\0';
    }
}

int main(int argc, char *argv[]) {
    /* Espera 19 argumentos (exe + 17 configs + 1 saida) */
    if (argc != 19) {
        fprintf(stderr, "Parametros insuficientes.\n"
                         "Uso: %s <device> <channel> <sampleRate> <acqTime> <preTrigger> "
                         "<vRange> <vOffset> <coupling> <probe> <averages> <triggerSource> "
                         "<triggerChannel> <triggerLevel> <triggerHysteresis> <triggerVRange> "
                         "<triggerCoupling> <triggerProbe> <outFile>\n", argv[0]);
        return 1;
    }

    const char* deviceName        = argv[1];
    char        channel[32];
    double      sampleRate        = atof(argv[3]);
    double      acqTime           = atof(argv[4]);
    double      preTrigger        = atof(argv[5]);
    double      vRange            = atof(argv[6]);
    double      vOffset           = atof(argv[7]);
    const char* coupling          = argv[8];
    const char* probe             = argv[9];
    int         averages          = atoi(argv[10]);
    const char* triggerSource     = argv[11];
    const char* trigChanArg       = argv[12];
    double      triggerLevel      = atof(argv[13]);
    double      triggerHysteresis = atof(argv[14]);
    
    /* Novos parametros do trigger */
    double      triggerVRange     = atof(argv[15]);
    const char* triggerCoupling   = argv[16];
    const char* triggerProbe      = argv[17];
    const char* outFile           = argv[18];

    normalizeChannelName(argv[2], channel, sizeof(channel));

    /* Determina o canal de disparo do trigger analógico */
    char triggerChannel[32];
    if (strlen(trigChanArg) > 0 && _stricmp(trigChanArg, "same") != 0 && _stricmp(trigChanArg, "auto") != 0) {
        normalizeChannelName(trigChanArg, triggerChannel, sizeof(triggerChannel));
    } else {
        /* Se passar "same", usa o próprio canal medido */
        strncpy(triggerChannel, channel, sizeof(triggerChannel) - 1);
        triggerChannel[sizeof(triggerChannel) - 1] = '\0';
    }

    /* Validações básicas */
    if (sampleRate <= 0.0 || acqTime <= 0.0) {
        fprintf(stderr, "Parametros de tempo/amostragem invalidos.\n");
        return 1;
    }
    if (averages < 1) averages = 1;
    if (triggerHysteresis < 0.0) {
        fprintf(stderr, "triggerHysteresis invalido: %s\n", argv[14]);
        return 1;
    }

    niVB_Status                status     = niVB_Status_Success;
    niVB_LibraryHandle         libHandle  = 0;
    niVB_MSO_InstrumentHandle  msoHandle  = 0;

    double  *buffer      = NULL;
    double  *sumBuffer   = NULL;
    FILE    *fp          = NULL;

    size_t  dataSizeOut  = 0;
    size_t  dataStride   = 0;
    size_t  bufferCapacity = 0;
    niVB_Timestamp tsStart, tsEnd;
    niVB_MSO_TriggerReason triggerReason;

    int exitCode = 1;

    niVB_ErrorCheck(niVB_Initialize(NIVB_LIBRARY_VERSION, &libHandle));
    niVB_ErrorCheck(niVB_MSO_Initialize(libHandle, deviceName, false, &msoHandle));

    /* Canais auxiliares (mso/1 e mso/2): por padrao ficam desligados, exceto se
     * um deles for o canal de trigger analogico e for diferente do canal que
     * esta sendo efetivamente aquisitado — nesse caso ele precisa continuar
     * ligado para que o trigger analogico funcione. */
    bool isInternalTrigger = (_stricmp(triggerSource, "Internal") == 0);

    {
        const char *auxChannels[2] = { "mso/1", "mso/2" };
        for (int i = 0; i < 2; i++) {
            bool isAcqChannel = (_stricmp(auxChannels[i], channel) == 0);

            /* O canal de aquisicao sera configurado logo abaixo com os
             * parametros corretos (range, offset, probe, coupling). Aqui so
             * tratamos os canais que NAO sao o canal aquisitado. */
            if (isAcqChannel) continue;

bool isTriggerChannel = isInternalTrigger &&
                                     (_stricmp(auxChannels[i], triggerChannel) == 0);

            /* Aplica os valores reais no canal de trigger, e mantém 0.0 de offset */
            niVB_ErrorCheck(niVB_MSO_ConfigureAnalogChannel(
                msoHandle, auxChannels[i], isTriggerChannel, triggerVRange, 0.0,
                strToProbe(triggerProbe), strToCoupling(triggerCoupling)));
        }
    }

    niVB_ErrorCheck(niVB_MSO_ConfigureAnalogChannel(
        msoHandle, channel, true, vRange, vOffset,
        strToProbe(probe), strToCoupling(coupling)));

    niVB_ErrorCheck(niVB_MSO_ConfigureTiming(
        msoHandle, sampleRate, acqTime, preTrigger, niVB_MSO_SamplingMode_Sample));

    if (_stricmp(triggerSource, "Internal") == 0) {
        /* Trigger analógico no canal especificado (pode ser o mesmo ou outro canal analógico) */
        niVB_ErrorCheck(niVB_MSO_ConfigureAnalogEdgeTrigger(
            msoHandle, triggerChannel, niVB_EdgeWithEither_Rising,
            triggerLevel, triggerHysteresis, niVB_MSO_TriggerInstance_A));

    } else if (_stricmp(triggerSource, "External") == 0) {
        /* Trigger externo via BNC "TRIG" */
        niVB_ErrorCheck(niVB_MSO_ConfigureDigitalEdgeTrigger(
            msoHandle, "trig", niVB_EdgeWithEither_Rising, niVB_MSO_TriggerInstance_A));
    } else if (_stricmp(triggerSource, "Keep") != 0) {
        /* Valor desconhecido: nao configuramos nenhum trigger, e a VirtualBench
         * vai coagir isso para trigger Immediate (dispara na hora). Avisamos
         * para que isso nao seja confundido com um trigger configurado que
         * "nao funcionou". */
        fprintf(stderr,
            "Aviso: triggerSource '%s' nao reconhecido (use Internal, External ou Keep). "
            "Nenhum trigger sera configurado; a aquisicao disparara imediatamente.\n",
            triggerSource);
    }

    /* useAutoTrigger controla se o Run() pode disparar sozinho (imediatamente)
     * quando a condicao de trigger configurada nao ocorrer. Isso NAO deve
     * depender do numero de medias: se o usuario pediu um trigger real
     * (Internal ou External), o auto-trigger deve ficar desativado para que
     * a aquisicao realmente espere pela condicao configurada. Ele so faz
     * sentido ficar ligado quando nao ha nenhum trigger configurado (ou seja,
     * disparo imediato é o comportamento desejado).
     *
     * Alem disso, quando estamos tirando medias (averages > 1), o auto-trigger
     * precisa ficar desativado de qualquer forma, para nao somar aquisicoes
     * disparadas em instantes diferentes/desalinhados. */
    bool hasRealTrigger = (_stricmp(triggerSource, "Internal") == 0) ||
                          (_stricmp(triggerSource, "External") == 0);
    bool useAutoTrigger = !hasRealTrigger && (averages <= 1);

    /* Primeira aquisição para descobrir o tamanho exato do buffer */
    niVB_ErrorCheck(niVB_MSO_Run(msoHandle, useAutoTrigger));
    niVB_ErrorCheck(niVB_MSO_ReadAnalog(
        msoHandle, NULL, 0, &dataSizeOut, &dataStride, NULL, NULL, NULL));

   size_t actualStride = dataStride > 0 ? dataStride : 1;
    /* dataSizeOut já é o tamanho TOTAL (todos os canais somados) */
    bufferCapacity = dataSizeOut > 0 ? dataSizeOut : (((size_t)(sampleRate * acqTime) + 1) * actualStride);

    buffer = (double *)malloc(bufferCapacity * sizeof(double));
    if (!buffer) {
        fprintf(stderr, "Erro ao alocar memoria para buffer.\n");
        goto Cleanup;
    }

    if (averages > 1) {
        sumBuffer = (double *)calloc(bufferCapacity, sizeof(double));
        if (!sumBuffer) {
            fprintf(stderr, "Erro ao alocar memoria para sumBuffer.\n");
            goto Cleanup;
        }
    }

    /* Lê os dados da primeira aquisição */
    niVB_ErrorCheck(niVB_MSO_ReadAnalog(
        msoHandle, buffer, bufferCapacity, &dataSizeOut, &dataStride,
        &tsStart, &tsEnd, &triggerReason));

 if (averages > 1) {
        for (size_t j = 0; j < dataSizeOut; j++) sumBuffer[j] += buffer[j];

        for (int i = 1; i < averages; i++) {
            size_t thisSize = 0, thisStride = 0;

            niVB_ErrorCheck(niVB_MSO_Run(msoHandle, useAutoTrigger));
            niVB_ErrorCheck(niVB_MSO_ReadAnalog(
                msoHandle, buffer, bufferCapacity, &thisSize, &thisStride,
                &tsStart, &tsEnd, &triggerReason));

            if (thisSize != dataSizeOut || thisStride != dataStride) {
                fprintf(stderr,
                    "Aviso: tamanho ou stride inconsistente na iteracao %d. Ignorando.\n", i);
                continue;
            }

            for (size_t j = 0; j < dataSizeOut; j++) sumBuffer[j] += buffer[j];
        }

        for (size_t j = 0; j < dataSizeOut; j++) buffer[j] = sumBuffer[j] / (double)averages;
    }

    /* Gravação no arquivo de saída (.csv) */
    fp = fopen(outFile, "w");
    if (!fp) {
        fprintf(stderr, "Erro ao criar arquivo de saida: %s\n", outFile);
        goto Cleanup;
    }

{
        double dt = 1.0 / sampleRate;
        size_t numSamplesPerChannel = dataSizeOut / dataStride; /* <- Correção crucial */
        
        for (size_t i = 0; i < numSamplesPerChannel; i++) {
            double t = -preTrigger + (double)i * dt;
            
            fprintf(fp, "%.9f", t);
            
            for (size_t c = 0; c < dataStride; c++) {
                fprintf(fp, ",%.9f", buffer[(i * dataStride) + c]);
            }
            fprintf(fp, "\n");
        }
    }

    exitCode = 0;

Cleanup:
    if (status != niVB_Status_Success && libHandle) {
        size_t descrSize = 0;
        niVB_GetErrorDescription(libHandle, status, niVB_Language_CurrentThreadLocale, NULL, 0, &descrSize);
        if (descrSize != 0) {
            char *descrBuf = (char*)malloc(descrSize * sizeof(char));
            niVB_GetErrorDescription(libHandle, status, niVB_Language_CurrentThreadLocale, descrBuf, descrSize, NULL);
            fprintf(stderr, "Erro/Aviso %d: %s\n", status, descrBuf);
            free(descrBuf);
        }
    }

    if (fp) fclose(fp);
    free(sumBuffer);
    free(buffer);

    if (msoHandle) niVB_MSO_Close(msoHandle);
    if (libHandle)  niVB_Finalize(libHandle);

    return exitCode;
}