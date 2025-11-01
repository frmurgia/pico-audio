#!/usr/bin/env python3
"""
WAV to C Array Converter for RP2350
Converts WAV files to C arrays for embedding in flash memory

Usage: python3 wav2c.py input.wav output.h [array_name]
"""

import sys
import struct
import os

def read_wav_header(f):
    """Read and parse WAV file header"""
    # Read RIFF header
    riff = f.read(4)
    if riff != b'RIFF':
        raise ValueError("Not a valid WAV file (missing RIFF)")

    file_size = struct.unpack('<I', f.read(4))[0]

    wave = f.read(4)
    if wave != b'WAVE':
        raise ValueError("Not a valid WAV file (missing WAVE)")

    # Read fmt chunk
    fmt = f.read(4)
    if fmt != b'fmt ':
        raise ValueError("Missing fmt chunk")

    fmt_size = struct.unpack('<I', f.read(4))[0]
    audio_format = struct.unpack('<H', f.read(2))[0]
    num_channels = struct.unpack('<H', f.read(2))[0]
    sample_rate = struct.unpack('<I', f.read(4))[0]
    byte_rate = struct.unpack('<I', f.read(4))[0]
    block_align = struct.unpack('<H', f.read(2))[0]
    bits_per_sample = struct.unpack('<H', f.read(2))[0]

    # Skip any extra fmt data
    if fmt_size > 16:
        f.read(fmt_size - 16)

    # Find data chunk
    while True:
        chunk_id = f.read(4)
        if not chunk_id:
            raise ValueError("No data chunk found")
        chunk_size = struct.unpack('<I', f.read(4))[0]

        if chunk_id == b'data':
            break
        else:
            # Skip this chunk
            f.read(chunk_size)

    return {
        'sample_rate': sample_rate,
        'num_channels': num_channels,
        'bits_per_sample': bits_per_sample,
        'data_size': chunk_size,
        'num_samples': chunk_size // (bits_per_sample // 8)
    }

def convert_wav_to_c(input_wav, output_h, array_name=None):
    """Convert WAV file to C header with array"""

    if array_name is None:
        # Generate array name from filename
        base = os.path.splitext(os.path.basename(input_wav))[0]
        array_name = base.replace('-', '_').replace(' ', '_').replace('.', '_')

    print(f"Converting {input_wav} -> {output_h}")
    print(f"Array name: {array_name}")

    with open(input_wav, 'rb') as f:
        header = read_wav_header(f)

        print(f"Sample rate: {header['sample_rate']} Hz")
        print(f"Channels: {header['num_channels']}")
        print(f"Bits per sample: {header['bits_per_sample']}")
        print(f"Data size: {header['data_size']} bytes")
        print(f"Num samples: {header['num_samples']}")

        if header['bits_per_sample'] != 16:
            raise ValueError("Only 16-bit WAV files are supported")

        # Read all audio data
        audio_data = f.read(header['data_size'])

    # Convert to 16-bit signed samples
    num_samples = len(audio_data) // 2
    samples = struct.unpack(f'<{num_samples}h', audio_data)

    # Write C header file
    with open(output_h, 'w') as f:
        f.write(f"// Auto-generated from {os.path.basename(input_wav)}\n")
        f.write(f"// Sample rate: {header['sample_rate']} Hz\n")
        f.write(f"// Channels: {header['num_channels']}\n")
        f.write(f"// Samples: {num_samples}\n")
        f.write(f"// Size: {len(audio_data)} bytes ({len(audio_data)/1024:.1f} KB)\n\n")

        f.write(f"#ifndef _{array_name.upper()}_H\n")
        f.write(f"#define _{array_name.upper()}_H\n\n")

        f.write(f"#include <stdint.h>\n\n")

        # Audio data array
        f.write(f"const int16_t {array_name}_data[] PROGMEM = {{\n")

        # Write samples, 12 per line
        for i in range(0, len(samples), 12):
            chunk = samples[i:i+12]
            f.write("  " + ", ".join(f"{s:6d}" for s in chunk))
            if i + 12 < len(samples):
                f.write(",\n")
            else:
                f.write("\n")

        f.write("};\n\n")

        # Metadata
        f.write(f"const uint32_t {array_name}_sample_rate = {header['sample_rate']};\n")
        f.write(f"const uint16_t {array_name}_num_channels = {header['num_channels']};\n")
        f.write(f"const uint32_t {array_name}_num_samples = {num_samples};\n")
        f.write(f"const uint32_t {array_name}_size_bytes = {len(audio_data)};\n\n")

        f.write(f"#endif // _{array_name.upper()}_H\n")

    print(f"✓ Created {output_h}")
    print(f"  Array: {array_name}_data[{num_samples}]")
    print(f"  Size: {len(audio_data)/1024:.1f} KB")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 wav2c.py input.wav output.h [array_name]")
        print("\nExample:")
        print("  python3 wav2c.py kick.wav audio_kick.h kick")
        sys.exit(1)

    input_wav = sys.argv[1]
    output_h = sys.argv[2]
    array_name = sys.argv[3] if len(sys.argv) > 3 else None

    try:
        convert_wav_to_c(input_wav, output_h, array_name)
    except Exception as e:
        print(f"ERROR: {e}")
        sys.exit(1)
