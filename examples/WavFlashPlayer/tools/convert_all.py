#!/usr/bin/env python3
"""
Convert all WAV files in a directory to C arrays

Usage: python3 convert_all.py input_directory output_directory
"""

import sys
import os
from wav2c import convert_wav_to_c

def convert_directory(input_dir, output_dir):
    """Convert all WAV files in directory"""

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    wav_files = [f for f in os.listdir(input_dir) if f.lower().endswith('.wav')]

    if not wav_files:
        print(f"No WAV files found in {input_dir}")
        return

    print(f"Found {len(wav_files)} WAV files")
    print("=" * 60)

    total_size = 0
    converted = []

    for i, wav_file in enumerate(sorted(wav_files), 1):
        input_path = os.path.join(input_dir, wav_file)

        # Generate array name from filename
        base = os.path.splitext(wav_file)[0]
        array_name = f"audio_{base.replace('-', '_').replace(' ', '_').replace('.', '_')}"

        output_filename = f"audio_{base}.h"
        output_path = os.path.join(output_dir, output_filename)

        print(f"\n[{i}/{len(wav_files)}] {wav_file}")

        try:
            convert_wav_to_c(input_path, output_path, array_name)
            size = os.path.getsize(input_path)
            total_size += size
            converted.append({
                'name': wav_file,
                'array': array_name,
                'header': output_filename,
                'size': size
            })
        except Exception as e:
            print(f"  ERROR: {e}")

    # Generate master header file
    master_header = os.path.join(output_dir, "audio_data.h")
    print(f"\n{'=' * 60}")
    print(f"Generating master header: {master_header}")

    with open(master_header, 'w') as f:
        f.write("// Auto-generated master header for all audio files\n\n")
        f.write("#ifndef _AUDIO_DATA_H\n")
        f.write("#define _AUDIO_DATA_H\n\n")

        # Include all individual headers
        for item in converted:
            f.write(f'#include "{item["header"]}"\n')

        f.write("\n// Audio file count\n")
        f.write(f"#define NUM_AUDIO_FILES {len(converted)}\n\n")

        # Create array of pointers to audio data
        f.write("// Array of pointers to audio data\n")
        f.write("const int16_t* const audio_files[NUM_AUDIO_FILES] PROGMEM = {\n")
        for i, item in enumerate(converted):
            f.write(f"  {item['array']}_data")
            if i < len(converted) - 1:
                f.write(",\n")
            else:
                f.write("\n")
        f.write("};\n\n")

        # Array of sample counts
        f.write("// Array of sample counts\n")
        f.write("const uint32_t audio_num_samples[NUM_AUDIO_FILES] PROGMEM = {\n")
        for i, item in enumerate(converted):
            f.write(f"  {item['array']}_num_samples")
            if i < len(converted) - 1:
                f.write(",\n")
            else:
                f.write("\n")
        f.write("};\n\n")

        # Array of channel counts
        f.write("// Array of channel counts\n")
        f.write("const uint16_t audio_num_channels[NUM_AUDIO_FILES] PROGMEM = {\n")
        for i, item in enumerate(converted):
            f.write(f"  {item['array']}_num_channels")
            if i < len(converted) - 1:
                f.write(",\n")
            else:
                f.write("\n")
        f.write("};\n\n")

        # Array of filenames
        f.write("// Array of original filenames\n")
        f.write("const char* const audio_filenames[NUM_AUDIO_FILES] PROGMEM = {\n")
        for i, item in enumerate(converted):
            f.write(f'  "{item["name"]}"')
            if i < len(converted) - 1:
                f.write(",\n")
            else:
                f.write("\n")
        f.write("};\n\n")

        f.write("#endif // _AUDIO_DATA_H\n")

    print(f"\n{'=' * 60}")
    print(f"Conversion complete!")
    print(f"Files converted: {len(converted)}")
    print(f"Total size: {total_size / 1024:.1f} KB ({total_size / 1024 / 1024:.2f} MB)")
    print(f"\nOutput directory: {output_dir}")
    print(f"Master header: {master_header}")
    print(f"\n⚠️  WARNING: RP2350 has 16MB flash total")
    if total_size > 15 * 1024 * 1024:
        print(f"   Your files ({total_size / 1024 / 1024:.1f} MB) may not fit!")
        print(f"   Consider reducing quality or using fewer files.")
    else:
        print(f"   Your files ({total_size / 1024 / 1024:.1f} MB) should fit.")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 convert_all.py input_directory output_directory")
        print("\nExample:")
        print("  python3 convert_all.py ~/my_wav_files ./audio_data")
        sys.exit(1)

    input_dir = sys.argv[1]
    output_dir = sys.argv[2]

    if not os.path.isdir(input_dir):
        print(f"ERROR: {input_dir} is not a directory")
        sys.exit(1)

    try:
        convert_directory(input_dir, output_dir)
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
