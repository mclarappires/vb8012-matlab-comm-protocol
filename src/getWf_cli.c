/**
 * @file getWf_cli.c
 * @brief Command-line tool to acquire an analog waveform from an NI
 *        VirtualBench MSO (Mixed Signal Oscilloscope) and save it to CSV.
 *
 * This tool is intended to be invoked from a wrapper (e.g. a MATLAB
 * function using `system()`), which passes all acquisition parameters
 * as command-line arguments and reads back the resulting CSV file.
 *
 * Usage:
 *   getWf_cli.exe <device> <channel> <sampleRate> <acqTime> <preTrigger>
 *                 <vRange> <vOffset> <coupling> <probe> <averages>
 *                 <triggerSource> <triggerChannel> <triggerLevel>
 *                 <triggerHysteresis> <triggerVRange> <triggerCoupling>
 *                 <triggerProbe> <outFile>
 *
 * Output CSV format: one row per sample, first column is time (seconds,
 * relative to the trigger), followed by one column per acquired channel.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "nivirtualbench.h"

/**
 * @brief Execute a VirtualBench API call and jump to Cleanup on failure.
 *
 * Assigns the result of @p function to the shared `status` variable. If
 * the assignment indicates the status changed AND the resulting status
 * represents a failure, control jumps to the `Cleanup` label so that
 * already-allocated resources are released and the error is reported.
 */
#define niVB_ErrorCheck(function) \
    if (niVB_Status_Assign(&status, (function)) && niVB_Status_Failed(status)) { \
        goto Cleanup; \
    }

/**
 * @brief Convert a coupling string ("AC"/"DC") to the corresponding
 *        niVB_MSO_Coupling enum value.
 * @param s Input string (case-insensitive). Any value other than "AC"
 *          is treated as "DC".
 * @return niVB_MSO_Coupling_AC or niVB_MSO_Coupling_DC.
 */
static niVB_MSO_Coupling strToCoupling(const char *s) {
    if (_stricmp(s, "AC") == 0) return niVB_MSO_Coupling_AC;
    return niVB_MSO_Coupling_DC;
}

/**
 * @brief Convert a probe attenuation string ("1x"/"10x") to the
 *        corresponding niVB_MSO_ProbeAttenuation enum value.
 * @param s Input string (case-insensitive). Any value other than "10x"
 *          is treated as "1x".
 * @return niVB_MSO_ProbeAttenuation_10x or niVB_MSO_ProbeAttenuation_1x.
 */
static niVB_MSO_ProbeAttenuation strToProbe(const char *s) {
    if (_stricmp(s, "10x") == 0) return niVB_MSO_ProbeAttenuation_10x;
    return niVB_MSO_ProbeAttenuation_1x;
}

/**
 * @brief Normalize a channel name into the "mso/N" format expected by
 *        the VirtualBench API.
 *
 * Accepts common shorthand input formats such as "ch1", "CH1", "1", or
 * "mso1", and converts them to "mso/1". If the input is already in the
 * "mso/N" format, it is copied through unchanged. If the channel number
 * cannot be parsed, the original input is copied through unchanged and
 * the VirtualBench API is left to report the error.
 *
 * @param input   Input channel string, in any of the accepted formats.
 * @param out     Output buffer to receive the normalized channel string.
 * @param outSize Size, in bytes, of the @p out buffer.
 */
static void normalizeChannelName(const char *input, char *out, size_t outSize) {
    const char *p = input;
    int channelNum = -1;

    /* Already in the correct "mso/N" format */
    if (_strnicmp(input, "mso/", 4) == 0) {
        strncpy(out, input, outSize - 1);
        out[outSize - 1] = '\0';
        return;
    }

    /* Skip known textual prefixes: "ch", "CH", "mso" (without slash) */
    if (_strnicmp(p, "ch", 2) == 0) {
        p += 2;
    } else if (_strnicmp(p, "mso", 3) == 0) {
        p += 3;
    }

    /* Skip any separators such as '/', '_', '-', space */
    while (*p == '/' || *p == '_' || *p == '-' || *p == ' ') p++;

    /* What remains should be the channel number */
    if (*p >= '0' && *p <= '9') {
        channelNum = atoi(p);
    }

    if (channelNum >= 0) {
        snprintf(out, outSize, "mso/%d", channelNum);
    } else {
        /* Unrecognized: pass through as-is and let the API report the error */
        strncpy(out, input, outSize - 1);
        out[outSize - 1] = '\0';
    }
}

/**
 * @brief Entry point. Parses command-line arguments, configures the
 *        VirtualBench MSO, runs one or more acquisitions (averaging if
 *        requested), and writes the resulting waveform to a CSV file.
 *
 * @param argc Argument count. Must be exactly 19 (executable name + 17
 *             configuration parameters + 1 output file path).
 * @param argv Argument vector. See file-level @ref getWf_cli.c usage
 *             comment for the expected argument order.
 * @return 0 on success, 1 on failure (invalid arguments, allocation
 *         failure, file I/O failure, or a failed VirtualBench API call).
 */
int main(int argc, char *argv[]) {
    /* Expects 19 arguments (exe + 17 config params + 1 output file) */
    if (argc != 19) {
        fprintf(stderr, "Insufficient parameters.\n"
                         "Usage: %s <device> <channel> <sampleRate> <acqTime> <preTrigger> "
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
    
    /* Additional trigger parameters */
    double      triggerVRange     = atof(argv[15]);
    const char* triggerCoupling   = argv[16];
    const char* triggerProbe      = argv[17];
    const char* outFile           = argv[18];

    normalizeChannelName(argv[2], channel, sizeof(channel));

    /* Determine the trigger channel for an analog trigger */
    char triggerChannel[32];
    if (strlen(trigChanArg) > 0 && _stricmp(trigChanArg, "same") != 0 && _stricmp(trigChanArg, "auto") != 0) {
        normalizeChannelName(trigChanArg, triggerChannel, sizeof(triggerChannel));
    } else {
        /* If "same" is passed, use the acquisition channel itself */
        strncpy(triggerChannel, channel, sizeof(triggerChannel) - 1);
        triggerChannel[sizeof(triggerChannel) - 1] = '\0';
    }

    /* Basic parameter validation */
    if (sampleRate <= 0.0 || acqTime <= 0.0) {
        fprintf(stderr, "Invalid timing/sampling parameters.\n");
        return 1;
    }
    if (averages < 1) averages = 1;
    if (triggerHysteresis < 0.0) {
        fprintf(stderr, "Invalid triggerHysteresis: %s\n", argv[14]);
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

    /* Auxiliary channels (mso/1 and mso/2): disabled by default, unless
     * one of them is the analog trigger channel and differs from the
     * channel actually being acquired — in that case it must remain
     * enabled for the analog trigger to work. */
    bool isInternalTrigger = (_stricmp(triggerSource, "Internal") == 0);

    {
        const char *auxChannels[2] = { "mso/1", "mso/2" };
        for (int i = 0; i < 2; i++) {
            bool isAcqChannel = (_stricmp(auxChannels[i], channel) == 0);

            /* The acquisition channel is configured with the correct
             * parameters (range, offset, probe, coupling) further below.
             * Here we only handle channels that are NOT the acquisition
             * channel. */
            if (isAcqChannel) continue;

bool isTriggerChannel = isInternalTrigger &&
                                     (_stricmp(auxChannels[i], triggerChannel) == 0);

            /* Apply the actual trigger settings on the trigger channel,
             * keeping offset at 0.0 */
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
        /* Analog trigger on the specified channel (may be the same
         * channel being acquired, or a different analog channel) */
        niVB_ErrorCheck(niVB_MSO_ConfigureAnalogEdgeTrigger(
            msoHandle, triggerChannel, niVB_EdgeWithEither_Rising,
            triggerLevel, triggerHysteresis, niVB_MSO_TriggerInstance_A));

    } else if (_stricmp(triggerSource, "External") == 0) {
        /* External trigger via the "TRIG" BNC connector */
        niVB_ErrorCheck(niVB_MSO_ConfigureDigitalEdgeTrigger(
            msoHandle, "trig", niVB_EdgeWithEither_Rising, niVB_MSO_TriggerInstance_A));
    } else if (_stricmp(triggerSource, "Keep") != 0) {
        /* Unknown value: no trigger is configured, and VirtualBench will
         * coerce this into an Immediate trigger (fires right away). We
         * warn about this so it isn't mistaken for a configured trigger
         * that "didn't work". */
        fprintf(stderr,
            "Warning: unrecognized triggerSource '%s' (use Internal, External, or Keep). "
            "No trigger will be configured; the acquisition will fire immediately.\n",
            triggerSource);
    }

    /* useAutoTrigger controls whether Run() may fire on its own
     * (immediately) if the configured trigger condition never occurs.
     * This must NOT depend on the number of averages: if the user
     * requested a real trigger (Internal or External), auto-trigger must
     * be disabled so the acquisition actually waits for the configured
     * condition. It only makes sense to enable it when no trigger is
     * configured at all (i.e. immediate firing is the desired behavior).
     *
     * Additionally, when averaging (averages > 1), auto-trigger must be
     * disabled regardless, to avoid summing acquisitions that fired at
     * different/misaligned instants. */
    bool hasRealTrigger = (_stricmp(triggerSource, "Internal") == 0) ||
                          (_stricmp(triggerSource, "External") == 0);
    bool useAutoTrigger = !hasRealTrigger && (averages <= 1);

    /* First acquisition, used to discover the exact buffer size */
    niVB_ErrorCheck(niVB_MSO_Run(msoHandle, useAutoTrigger));
    niVB_ErrorCheck(niVB_MSO_ReadAnalog(
        msoHandle, NULL, 0, &dataSizeOut, &dataStride, NULL, NULL, NULL));

   size_t actualStride = dataStride > 0 ? dataStride : 1;
    /* dataSizeOut is already the TOTAL size (all channels combined) */
    bufferCapacity = dataSizeOut > 0 ? dataSizeOut : (((size_t)(sampleRate * acqTime) + 1) * actualStride);

    buffer = (double *)malloc(bufferCapacity * sizeof(double));
    if (!buffer) {
        fprintf(stderr, "Error allocating memory for buffer.\n");
        goto Cleanup;
    }

    if (averages > 1) {
        sumBuffer = (double *)calloc(bufferCapacity, sizeof(double));
        if (!sumBuffer) {
            fprintf(stderr, "Error allocating memory for sumBuffer.\n");
            goto Cleanup;
        }
    }

    /* Read the data from the first acquisition */
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
                    "Warning: inconsistent size or stride on iteration %d. Skipping.\n", i);
                continue;
            }

            for (size_t j = 0; j < dataSizeOut; j++) sumBuffer[j] += buffer[j];
        }

        for (size_t j = 0; j < dataSizeOut; j++) buffer[j] = sumBuffer[j] / (double)averages;
    }

    /* Write the output (.csv) file */
    fp = fopen(outFile, "w");
    if (!fp) {
        fprintf(stderr, "Error creating output file: %s\n", outFile);
        goto Cleanup;
    }

{
        double dt = 1.0 / sampleRate;
        size_t numSamplesPerChannel = dataSizeOut / dataStride; /* <- Crucial correction */
        
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
            fprintf(stderr, "Error/Warning %d: %s\n", status, descrBuf);
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
