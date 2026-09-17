/**
 * @file setFgen_cli.c
 * @brief Command-line tool to configure and run a standard waveform on
 *        the NI VirtualBench Function Generator (FGEN).
 *
 * This tool is intended to be invoked from a wrapper (e.g. a MATLAB
 * function using `system()`), which passes the waveform shape and
 * signal parameters as command-line arguments.
 *
 * Usage:
 *   setFgen_cli.exe <device> <waveform> <amplitude> <offset> <frequency> <phase>
 *
 * Unlike the arbitrary-waveform tool, this program closes the FGEN
 * handle before exiting (see niVB_FGEN_Close below), but the output
 * remains configured and running on the device after the process
 * terminates.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nivirtualbench.h"

/**
 * @brief Convert a waveform shape string to the corresponding
 *        niVB_Waveform enum value.
 *
 * @param s Input string (case-insensitive). Recognized values are
 *          "square", "triangle", and "dc"; any other value (including
 *          "sine") defaults to niVB_Waveform_Sine.
 * @return The matching niVB_Waveform enum value.
 */
static niVB_Waveform strToWaveform(const char *s) {
    if (_stricmp(s, "square")   == 0) return niVB_Waveform_Square;
    if (_stricmp(s, "triangle") == 0) return niVB_Waveform_Triangle;
    if (_stricmp(s, "dc")       == 0) return niVB_Waveform_DC;
    return niVB_Waveform_Sine;
}

/**
 * @brief Entry point. Parses command-line arguments, configures a
 *        standard waveform on the VirtualBench FGEN, and starts signal
 *        generation.
 *
 * @param argc Argument count. Must be exactly 7 (executable name +
 *             device name + waveform + amplitude + offset + frequency
 *             + phase).
 * @param argv Argument vector:
 *             argv[1] - Device name (e.g. "VB8012-31D2661")
 *             argv[2] - Waveform shape ("sine", "square", "triangle", "dc")
 *             argv[3] - Amplitude, in volts, as a string
 *             argv[4] - DC offset, in volts, as a string
 *             argv[5] - Frequency, in Hz, as a string
 *             argv[6] - Phase, in degrees, as a string
 * @return 0 on success, 1 on failure (invalid arguments, or a failed
 *         VirtualBench initialization/connection call).
 *
 * @note The return codes for niVB_FGEN_ConfigureStandardWaveform and
 *       niVB_FGEN_Run are not currently checked; a failure in either
 *       call will not prevent the "success" message from being
 *       printed. Callers relying on this tool for critical
 *       configuration should inspect stderr/console output carefully.
 */
int main(int argc, char *argv[]) {
    // Expects 7 arguments: executable name + 6 parameters
    if (argc != 7) {
        fprintf(stderr, "Usage: %s <device> <waveform> <amplitude> <offset> <frequency> <phase>\n", argv[0]);
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
        fprintf(stderr, "Error: Failed to initialize the NI VirtualBench library.\n");
        return 1;
    }

    status = niVB_FGEN_Initialize(libHandle, deviceName, true, &fgenHandle);
    if (niVB_Status_Failed(status)) {
        niVB_Finalize(libHandle);
        fprintf(stderr, "Error: Failed to connect to the FGEN on device %s.\n", deviceName);
        return 1;
    }

    status = niVB_FGEN_ConfigureStandardWaveform(fgenHandle, strToWaveform(waveform),
                                                 amplitude, offset, frequency, phase);
    
    status = niVB_FGEN_Run(fgenHandle);
    
    printf("FGEN successfully configured: %s | %.2f Vpp | %.1f Hz\n", waveform, amplitude, frequency);

    // Close the handles
    niVB_FGEN_Close(fgenHandle);
    niVB_Finalize(libHandle);

    return 0; // Success
}
