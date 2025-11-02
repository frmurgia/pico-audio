/* Audio Library for Teensy, ported to RP2350
 * Copyright (c) 2014, Paul Stoffregen, paul@pjrc.com
 * RP2350 port for pico-audio library
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "play_sd_wav.h"

// States
#define STATE_STOP      0
#define STATE_PARSE1    1  // Parsing WAV file header
#define STATE_PARSE2    2
#define STATE_PARSE3    3
#define STATE_PARSE4    4
#define STATE_PARSE5    5
#define STATE_PLAY      6  // Playing audio data

void AudioPlaySdWav::begin(void)
{
	state = STATE_STOP;
	state_play = STATE_STOP;
	data_length = 0;
	total_length = 0;
}

bool AudioPlaySdWav::play(const char *filename)
{
	stop();

	// Open SD card file
	wavfile = SD.open(filename);
	if (!wavfile) {
		return false;
	}

	buffer_length = 0;
	buffer_offset = 0;
	state_play = STATE_STOP;
	data_length = 0;
	header_offset = 0;
	state = STATE_PARSE1;

	return true;
}

void AudioPlaySdWav::stop(void)
{
	__disable_irq();
	if (state != STATE_STOP) {
		state = STATE_STOP;
		__enable_irq();
		wavfile.close();
	} else {
		__enable_irq();
	}
}

bool AudioPlaySdWav::isPlaying(void)
{
	return (state != STATE_STOP);
}

uint32_t AudioPlaySdWav::positionMillis(void)
{
	if (state == STATE_STOP) return 0;
	uint32_t pos = wavfile.position();
	if (pos < 44) return 0;  // still reading header
	pos -= 44;               // skip WAV header
	if (bytes2millis == 0) return 0;
	return pos / bytes2millis;
}

uint32_t AudioPlaySdWav::lengthMillis(void)
{
	if (bytes2millis == 0) return 0;
	return total_length / bytes2millis;
}

void AudioPlaySdWav::update(void)
{
	unsigned int i, n;
	audio_block_t *left, *right;

	// Debug counter
	static uint32_t updateCount = 0;
	bool shouldDebug = ((updateCount++ % 200) == 0);

	if (shouldDebug && state != STATE_STOP) {
		Serial.print("🔧 update() state=");
		Serial.print(state);
		Serial.print(" buflen=");
		Serial.print(buffer_length);
		Serial.print(" bufoff=");
		Serial.print(buffer_offset);
		Serial.print(" datalen=");
		Serial.println(data_length);
	}

	// Only update if we're playing
	if (state == STATE_STOP) return;

	// Allocate audio blocks
	left = allocate();
	if (left == NULL) {
		if (shouldDebug) Serial.println("⚠️  Cannot allocate left block");
		return;
	}
	right = allocate();
	if (right == NULL) {
		if (shouldDebug) Serial.println("⚠️  Cannot allocate right block");
		release(left);
		return;
	}

	block_left = left;
	block_right = right;
	block_offset = 0;

	// State machine for parsing and playback
	while (1) {
		switch (state) {

		case STATE_STOP:
			goto end;

		// Parse WAV file header
		case STATE_PARSE1:
			// Read 512 byte chunks for header
			buffer_length = wavfile.read(buffer, 512);
			if (shouldDebug) {
				Serial.print("  PARSE1: Read ");
				Serial.print(buffer_length);
				Serial.println(" bytes");
			}
			if (buffer_length == 0) goto end;
			buffer_offset = 0;
			state = STATE_PARSE2;

		case STATE_PARSE2:
		case STATE_PARSE3:
		case STATE_PARSE4:
		case STATE_PARSE5:
			// Consume header data
			if (!parse_format()) goto end;
			if (state != STATE_PLAY) continue;
			// Fall through to playback

		case STATE_PLAY:
			// Fill audio blocks from WAV data
			n = buffer_length - buffer_offset;
			if (n == 0) {
				// Buffer empty, need more data from SD
				if (data_length == 0) goto end;  // End of file

				// Read next 512 byte chunk
				buffer_length = wavfile.read(buffer, 512);
				if (buffer_length == 0) goto end;
				buffer_offset = 0;
				n = buffer_length;
			}

			if (n > data_length) n = data_length;

			// Consume audio data
			if (!consume(n)) goto end;

			// Check if we filled the audio blocks
			if (block_offset >= AUDIO_BLOCK_SAMPLES) {
				transmit(block_left, 0);
				transmit(block_right, 1);
				goto end;
			}
			continue;
		}
	}

end:
	if (block_offset > 0) {
		// Fill rest with zeros
		for (i = block_offset; i < AUDIO_BLOCK_SAMPLES; i++) {
			block_left->data[i] = 0;
			block_right->data[i] = 0;
		}
		transmit(block_left, 0);
		transmit(block_right, 1);
	}
	release(left);
	release(right);
	block_left = NULL;
	block_right = NULL;
}

bool AudioPlaySdWav::consume(uint32_t size)
{
	uint32_t len;
	uint8_t lsb, msb;
	const uint8_t *p;

	p = buffer + buffer_offset;
	if (size == 0) return false;
	len = size;

	// Copy samples to audio blocks
	// Assuming 16-bit stereo PCM
	while (len >= 4 && block_offset < AUDIO_BLOCK_SAMPLES) {
		// Left channel (LSB first)
		lsb = *p++;
		msb = *p++;
		block_left->data[block_offset] = (msb << 8) | lsb;

		// Right channel (LSB first)
		lsb = *p++;
		msb = *p++;
		block_right->data[block_offset] = (msb << 8) | lsb;

		block_offset++;
		len -= 4;
	}

	buffer_offset += (size - len);
	data_length -= (size - len);

	return true;
}

bool AudioPlaySdWav::parse_format(void)
{
	uint8_t num = 0;
	uint16_t format;
	uint16_t channels;
	uint32_t rate, b_rate;
	uint16_t bits;
	uint16_t block_align;

	static bool debugPrinted = false;

	while (state != STATE_PLAY) {
		if (buffer_offset >= buffer_length) {
			if (!debugPrinted) {
				Serial.println("  parse_format: buffer_offset >= buffer_length, returning false");
				debugPrinted = true;
			}
			return false;
		}

		((uint8_t *)header)[header_offset++] = buffer[buffer_offset++];

		if (header_offset == 1 && !debugPrinted) {
			Serial.println("  parse_format: Starting to parse header");
		}

		if (header_offset < 16) continue;

		// Parse WAV header (simplified)
		if (header_offset == 16) {
			// Check RIFF header
			if (!debugPrinted) {
				Serial.print("  Header[0]=0x");
				Serial.print(header[0], HEX);
				Serial.print(" Header[2]=0x");
				Serial.println(header[2], HEX);
			}
			if (header[0] != 0x46464952) {
				Serial.println("❌ Not a RIFF file!");
				goto abort;  // "RIFF"
			}
			if (header[2] != 0x45564157) {
				Serial.println("❌ Not a WAVE file!");
				goto abort;  // "WAVE"
			}
			if (!debugPrinted) Serial.println("✓ RIFF/WAVE header OK");
			header_offset++;
		}

		if (header_offset == 17) {
			// Look for "fmt " chunk
			if (header[3] == 0x20746d66) {  // "fmt "
				header_offset++;
			} else {
				// Skip unknown chunk
				header_offset = 16;
				header[0] = header[1];
				header[1] = header[2];
				header[2] = header[3];
			}
		}

		if (header_offset >= 18) {
			// We have format chunk
			format = header[4] & 0xFFFF;
			channels = (header[4] >> 16) & 0xFFFF;
			rate = header[5];
			b_rate = header[6];
			block_align = header[7] & 0xFFFF;
			bits = (header[7] >> 16) & 0xFFFF;

			// Validate: PCM, stereo, 16-bit, 44100 Hz
			if (format != 1) goto abort;  // Must be PCM
			if (channels != 2) goto abort; // Must be stereo
			if (bits != 16) goto abort;   // Must be 16-bit

			// Calculate bytes to milliseconds conversion
			bytes2millis = b_rate / 1000;

			// Look for "data" chunk
			header_offset = 16;
			state = STATE_PARSE3;
		}
	}

	if (state == STATE_PARSE3) {
		// Look for data chunk
		if (header_offset >= 18) {
			if (header[3] == 0x61746164) {  // "data"
				data_length = header[4];
				total_length = data_length;
				state = STATE_PLAY;
				return true;
			}
		}
	}

	return false;

abort:
	stop();
	return false;
}
