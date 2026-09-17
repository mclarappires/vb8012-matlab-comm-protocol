/**
 * @file setFgen_arb_cli.c
 * @brief Command-line tool to load and run an arbitrary waveform on the
 *        NI VirtualBench Function Generator (FGEN).
 *
 * This tool is intended to be invoked from a wrapper (e.g. a MATLAB
 * function using `system()`), which writes the waveform samples to a
 * CSV file and passes its path along with the target device name and
 * sample period as command-line arguments.
 *
 * Usage:
 *   setFgen_arb_cli.exe <DeviceName> <CsvFile> <SamplePeriod>
 *
 * Input CSV format: one floating-point sample value per line
 * (whitespace-separated values are also accepted, since parsing is
 * done with fscanf("%lf")).
 *
 * Note: the FGEN output is intentionally left running after this
 * program exits (see step 5 below), so the signal continues to be
 * generated after the process terminates.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nivirtualbench.h"

/**
 * @brief Read a waveform from a CSV file into a dynamically allocated
 *        array of doubles.
 *
 * The file is scanned twice: once to count the number of numeric
 * values present, and once to actually read them, after allocating a
 * buffer of the exact required size.
 *
 * @param filename Path to the input CSV file.
 * @param waveform Output parameter; on success, set to point to a
 *                 newly allocated array containing the waveform
 *                 samples. Caller is responsible for freeing this
 *                 memory. Left unset if the file cannot be opened.
 * @return Number of samples read (0 if the file could not be opened
 *         or contains no valid numeric values).
 */
size_t readWaveform(const char* filename, double** waveform) {
    FILE* fp = fopen(filename, "r");
    if (!fp) return 0;

    size_t count = 0;
    double temp;
    // Count the number of data points
    while (fscanf(fp, "%lf", &temp) == 1) count++;
    
    rewind(fp);
    *waveform = (double*)malloc(count * sizeof(double));
    
    for (size_t i = 0; i < count; i++) {
        fscanf(fp, "%lf", &(*waveform)[i]);
    }
    
    fclose(fp);
    return count;
}

/**
 * @brief Entry point. Reads a waveform from a CSV file, loads it onto
 *        the VirtualBench FGEN as an arbitrary waveform, and starts
 *        signal generation.
 *
 * @param argc Argument count. Must be exactly 4 (executable name +
 *             device name + CSV file path + sample period).
 * @param argv Argument vector:
 *             argv[1] - Device name (e.g. "VB8012-31D2661")
 *             argv[2] - Path to the CSV file containing waveform samples
 *             argv[3] - Sample period, in seconds, as a string
 * @return 0 on success, 1 on failure (invalid arguments, CSV read
 *         failure, or a failed VirtualBench initialization call).
 *
 * @note If waveform configuration fails, an error is reported but the
 *       program still exits with code 0, since initialization
 *       succeeded up to that point; only initialization-level failures
 *       return 1. Callers checking for success should also inspect the
 *       printed output, not just the exit code.
 */
int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <DeviceName> <CsvFile> <SamplePeriod>\n", argv[0]);
        return 1;
    }

    const char* deviceName   = argv[1];
    const char* csvFile      = argv[2];
    double samplePeriod      = atof(argv[3]);

    // 1. Read the waveform from the file
    double* waveform = NULL;
    size_t waveformSize = readWaveform(csvFile, &waveform);
    
    if (waveformSize == 0) {
        fprintf(stderr, "Error reading CSV file, or file is empty.\n");
        return 1;
    }

    // 2. Initialize VirtualBench
    niVB_LibraryHandle        libHandle = 0;
    niVB_FGEN_InstrumentHandle fgenHandle = 0;
    niVB_Status status;

    status = niVB_Initialize(NIVB_LIBRARY_VERSION, &libHandle);
    if (niVB_Status_Failed(status)) { free(waveform); return 1; }

    // Connect to the FGEN (reset = true clears any previous configuration)
    status = niVB_FGEN_Initialize(libHandle, deviceName, true, &fgenHandle);
    if (niVB_Status_Failed(status)) { niVB_Finalize(libHandle); free(waveform); return 1; }

    // 3. Configure the arbitrary waveform
    status = niVB_FGEN_ConfigureArbitraryWaveform(fgenHandle, waveform, waveformSize, samplePeriod);
    if (niVB_Status_Failed(status)) {
        fprintf(stderr, "Error configuring AWG. Code: %d\n", status);
    } else {
        // 4. Turn on the output!
        niVB_FGEN_Run(fgenHandle);
        printf("Arbitrary waveform successfully loaded! (%zu points)\n", waveformSize);
    }

    // 5. Cleanup (we do NOT call FGEN_Close here, so the signal keeps
    //    being generated after this process exits)
    // niVB_FGEN_Close(fgenHandle); 
    free(waveform);
    niVB_Finalize(libHandle);
    return 0;
}
