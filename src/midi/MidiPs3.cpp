#include "midi/MidiPs3.hpp"
#include "utils.hpp"
#include "ZunMath.hpp"
#include <math.h>

#define PS3_NATIVE_SAMPLE_RATE 48000

MidiDevice::MidiDevice()
{
    for (int i = 0; i < 64; i++) {
        this->voices[i].active = false;
    }
}

MidiDevice::~MidiDevice()
{
}

ZunResult MidiDevice::Close()
{
    for (int i = 0; i < 64; i++) {
        this->voices[i].active = false;
    }
    return ZUN_SUCCESS;
}

bool MidiDevice::OpenDevice(u32 uDeviceId)
{
    return true;
}

bool MidiDevice::SendShortMsg(u8 midiStatus, u8 firstByte, u8 secondByte)
{
    u8 opcode = midiStatus & 0xF0;
    // u8 channel = midiStatus & 0x0F;

    if (opcode == 0x90 && secondByte > 0) { // Note On
        for (int i = 0; i < 64; i++) {
            if (!this->voices[i].active) {
                this->voices[i].active = true;
                this->voices[i].note = firstByte;
                this->voices[i].velocity = secondByte;
                this->voices[i].phase = 0.0f;
                float freq = 440.0f * powf(2.0f, (float)(firstByte - 69) / 12.0f);
                this->voices[i].phaseInc = freq / (float)PS3_NATIVE_SAMPLE_RATE;
                break;
            }
        }
    } else if (opcode == 0x80 || (opcode == 0x90 && secondByte == 0)) { // Note Off
        for (int i = 0; i < 64; i++) {
            if (this->voices[i].active && this->voices[i].note == firstByte) {
                this->voices[i].active = false;
            }
        }
    }
    return true;
}

bool MidiDevice::SendLongMsg(const u8 *buf, u32 len)
{
    return true;
}

void MidiDevice::Render(i32 *buffer, u32 samples)
{
    static bool hasProducedSound = false;
    for (u32 i = 0; i < samples / 2; i++) {
        float mix = 0.0f;
        for (int v = 0; v < 64; v++) {
            if (this->voices[v].active) {
                // Square wave synthesizer
                float val = (this->voices[v].phase < 0.5f) ? 1.0f : -1.0f;
                mix += val * ((float)this->voices[v].velocity / 127.0f) * 0.1f;
                
                this->voices[v].phase += this->voices[v].phaseInc;
                if (this->voices[v].phase >= 1.0f) this->voices[v].phase -= 1.0f;
            }
        }
        i32 sample = (i32)(mix * 32767.0f);
        if (sample != 0 && !hasProducedSound) {
            utils::DebugPrint2("PS3 MIDI: Produced non-zero sample!");
            hasProducedSound = true;
        }
        buffer[i * 2] += sample;
        buffer[i * 2 + 1] += sample;
    }
}
