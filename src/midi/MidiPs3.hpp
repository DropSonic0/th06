#pragma once

#include "ZunResult.hpp"
#include "inttypes.hpp"

// PS3 MIDI renderer using software synthesis (placeholder for now)

struct MidiPs3Voice
{
    bool active;
    u8 note;
    u8 velocity;
    float phase;
    float phaseInc;
};

struct MidiDevice
{
  public:
    MidiDevice();
    ~MidiDevice();

    ZunResult Close();
    bool OpenDevice(u32 uDeviceId);
    bool SendShortMsg(u8 midiStatus, u8 firstByte, u8 secondByte);
    bool SendLongMsg(const u8 *buf, u32 len);

    void Render(i32 *buffer, u32 samples);

  private:
    MidiPs3Voice voices[64];
};
