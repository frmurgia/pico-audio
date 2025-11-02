#!/usr/bin/env python3
"""
WAV to PROGMEM Converter
Converts WAV files to C++ header files with PROGMEM arrays

⚠️  WARNING: This creates HUGE binaries!
    - A 1MB WAV file = 1MB added to your binary
    - RP2350 has only 16MB flash total
    - Your binary must be < 16MB
    - You have 98MB of WAV files → THIS WON'T WORK!

✅  Better alternatives:
    - Use LittleFS (FlashWavPlayer) - 14MB, easy to update
    - Use SD card with SDIO - unlimited size, 10MB/s
    - Use SD card with SPI - unlimited size, 2MB/s

Only use PROGMEM for:
    - Very small files (<100KB)
    - 1-2 files max
    - When you need absolute maximum speed
"""

import sys
import struct
import os

def wav_to_header(wav_path, var_name=None):
    """
    Convert WAV file to C++ header with PROGMEM array

    Args:
        wav_path: Path to WAV file
        var_name: Variable name (default: filename without extension)

    Returns:
        String containing C++ header code
    """

    if not os.path.exists(wav_path):
        raise FileNotFoundError(f"WAV file not found: {wav_path}")

    file_size = os.path.getsize(wav_path)
    file_size_mb = file_size / (1024 * 1024)

    print(f"Processing: {wav_path}")
    print(f"Size: {file_size_mb:.2f} MB")

    if file_size > 16 * 1024 * 1024:
        print("⚠️  WARNING: File too large for RP2350 flash (>16MB)!")
        return None

    if file_size > 2 * 1024 * 1024:
        print("⚠️  WARNING: File very large - binary will be huge!")
        response = input("Continue? (yes/no): ")
        if response.lower() != 'yes':
            return None

    # Default variable name from filename
    if var_name is None:
        var_name = os.path.splitext(os.path.basename(wav_path))[0]
        var_name = var_name.replace('-', '_').replace(' ', '_')

    # Read WAV file
    with open(wav_path, 'rb') as f:
        wav_data = f.read()

    # Parse WAV header
    if wav_data[:4] != b'RIFF' or wav_data[8:12] != b'WAVE':
        raise ValueError("Invalid WAV file format")

    # Find format chunk
    fmt_offset = wav_data.find(b'fmt ')
    if fmt_offset == -1:
        raise ValueError("No format chunk found")

    fmt_size = struct.unpack('<I', wav_data[fmt_offset+4:fmt_offset+8])[0]
    fmt_data = wav_data[fmt_offset+8:fmt_offset+8+fmt_size]

    audio_format = struct.unpack('<H', fmt_data[0:2])[0]
    num_channels = struct.unpack('<H', fmt_data[2:4])[0]
    sample_rate = struct.unpack('<I', fmt_data[4:8])[0]
    bits_per_sample = struct.unpack('<H', fmt_data[14:16])[0]

    print(f"Format: {sample_rate}Hz, {bits_per_sample}-bit, {num_channels} channel(s)")

    if audio_format != 1:
        print("⚠️  WARNING: Not PCM format - may not work correctly")

    # Generate header file
    header = []
    header.append("// Auto-generated WAV file array")
    header.append(f"// Source: {os.path.basename(wav_path)}")
    header.append(f"// Size: {file_size_mb:.2f} MB ({file_size} bytes)")
    header.append(f"// Format: {sample_rate}Hz, {bits_per_sample}-bit, {num_channels}ch")
    header.append("")
    header.append("#ifndef WAV_DATA_H")
    header.append("#define WAV_DATA_H")
    header.append("")
    header.append("#include <Arduino.h>")
    header.append("")
    header.append(f"// WAV data array - stored in FLASH memory")
    header.append(f"const uint8_t {var_name}_data[] PROGMEM = {{")

    # Write data in chunks of 12 bytes per line
    bytes_per_line = 12
    for i in range(0, len(wav_data), bytes_per_line):
        chunk = wav_data[i:i+bytes_per_line]
        hex_values = ', '.join(f'0x{b:02X}' for b in chunk)
        header.append(f"  {hex_values},")

    header.append("};")
    header.append("")
    header.append(f"const uint32_t {var_name}_size = {file_size};")
    header.append("")
    header.append("// Metadata")
    header.append(f"const uint32_t {var_name}_sample_rate = {sample_rate};")
    header.append(f"const uint16_t {var_name}_bits_per_sample = {bits_per_sample};")
    header.append(f"const uint16_t {var_name}_num_channels = {num_channels};")
    header.append("")
    header.append("#endif // WAV_DATA_H")
    header.append("")

    return '\n'.join(header)

def main():
    if len(sys.argv) < 2:
        print("WAV to PROGMEM Converter")
        print()
        print("Usage:")
        print("  python wav_to_progmem.py input.wav [output.h] [variable_name]")
        print()
        print("Examples:")
        print("  python wav_to_progmem.py track1.wav")
        print("  python wav_to_progmem.py track1.wav track1_data.h")
        print("  python wav_to_progmem.py track1.wav track1_data.h my_audio")
        print()
        print("⚠️  WARNING: Creates HUGE binaries!")
        print("    Better use LittleFS (see FlashWavPlayer example)")
        print()
        sys.exit(1)

    wav_path = sys.argv[1]

    # Output path
    if len(sys.argv) >= 3:
        output_path = sys.argv[2]
    else:
        base_name = os.path.splitext(os.path.basename(wav_path))[0]
        output_path = f"{base_name}_data.h"

    # Variable name
    if len(sys.argv) >= 4:
        var_name = sys.argv[3]
    else:
        var_name = None

    try:
        header_code = wav_to_header(wav_path, var_name)

        if header_code is None:
            print("Conversion cancelled")
            sys.exit(1)

        with open(output_path, 'w') as f:
            f.write(header_code)

        print()
        print(f"✓ Header file created: {output_path}")
        print()
        print("Usage in Arduino sketch:")
        print(f"  #include \"{os.path.basename(output_path)}\"")
        print()
        print("To play the audio:")
        print("  1. Copy data from PROGMEM to RAM buffer")
        print("  2. Parse WAV header")
        print("  3. Feed samples to AudioPlayQueue")
        print()
        print("⚠️  Remember: This adds significant size to your binary!")

    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
