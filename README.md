# vb8012-matlab-communication-protocol

C-based command-line interfaces for the NI VirtualBench VB-8012, designed for seamless instrument control and automated data acquisition via MATLAB system calls.

## Architecture Overview
This project acts as a bridge between MATLAB and the NI VirtualBench C API. 
* MATLAB functions execute compiled C binaries using the `system` command.
* Arbitrary waveform generation and oscilloscope data acquisition exchange data securely using temporary `.csv` files.

## Features
* **Oscilloscope (MSO):** Acquire waveforms with configurable sample rate, acquisition time, voltage range, coupling, probe attenuation, and advanced trigger settings (Internal, External, or Keep).
* **Function Generator (FGEN):** Set standard waveforms including sine, square, triangle, and DC.
* **Arbitrary Waveform Generator (AWG):** Load custom waveforms from MATLAB arrays directly into the VirtualBench memory via CSV.

## Repository Structure
* `src/getWf_cli.c`: CLI for reading analog MSO channels, handling channel normalization and triggering logic.
* `src/setFgen_cli.c`: CLI for configuring and running standard function generation.
* `src/setFgen_arb_cli.c`: CLI for arbitrary function generation, loading sample points from CSV files.
* `matlab/VB_getwf_sys.m`: MATLAB wrapper parsing input arguments and formatting the 18 parameters required for waveform acquisition.
* `matlab/VB_setFgen_sys.m`: MATLAB wrapper for setting standard FGEN parameters like amplitude, offset, frequency, and phase.
* `matlab/VB_setFgen_arb_sys.m`: MATLAB wrapper that writes matrix data to a temporary CSV for the AWG CLI to consume.

## Prerequisites
* **NI VirtualBench Driver:** Provides `nivirtualbench.h` and the required library files for linkage.
* **C Compiler:** GCC (e.g., MinGW) or MSVC to compile the command-line executables.
* **MATLAB:** To execute the `.m` wrapper scripts.

## Compilation Instructions
Ensure the NI VirtualBench C API `Include` and `Lib` folders are properly configured in your compiler's environment paths.

**Using GCC:**
```bash
gcc src/getWf_cli.c -o getWf_cli.exe -lnivirtualbench
gcc src/setFgen_cli.c -o setFgen_cli.exe -lnivirtualbench
gcc src/setFgen_arb_cli.c -o setFgen_arb_cli.exe -lnivirtualbench
```
*(If using MSVC, use `cl` and link against `nivirtualbench.lib`)*

## MATLAB Usage Examples

### 1. Acquiring a Waveform
Ensure `getWf_cli.exe` is in the MATLAB path or the current working directory.
```matlab
% Reads from channel mso/1 with a 50ms acquisition time
[w, t, t_d] = VB_getwf_sys('VB8012-XXXXX', 'mso/1', 'AcqTime', 0.05, 'VRange', 5.0);
plot(t, w);
title('Oscilloscope Acquisition');
xlabel('Time (s)');
ylabel('Voltage (V)');
```

### 2. Setting a Standard Waveform
Ensure `setFgen_cli.exe` is in the MATLAB path.
```matlab
% Outputs a 2Vpp Sine wave at 1kHz with 0V offset
VB_setFgen('VB8012-XXXXX', 'Waveform', 'sine', 'Amplitude', 2.0, 'Offset', 0.0, 'Frequency', 1000.0);
```

### 3. Uploading an Arbitrary Waveform
Ensure `setFgen_arb_cli.exe` is in the MATLAB path.
```matlab
dt = 1e-6; % 1 MHz update rate (Sample Period)
t = 0:dt:1e-3;
custom_wave = sin(2*pi*1000*t) .* exp(-1000*t); % Damped sine wave

% Sends the array to the instrument
VB_setFgen_arb_sys('VB8012-XXXXX', custom_wave, dt);
```

## License
Distributed under the MIT License. See `LICENSE` for more information.
