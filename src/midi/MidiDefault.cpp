#include "MidiDefault.hpp"
#include "GameErrorContext.hpp"
#include "i18n.hpp"

#if !defined(_WIN32) && !defined(LIBASOUND_MIDI_SUPPORT) && !defined(__PS3__)
MidiDevice::MidiDevice()
{
    printedWarning = false;
}

MidiDevice::~MidiDevice()
{
}

bool MidiDevice::OpenDevice(u32 uDeviceId)
{
    (void)uDeviceId;

    if (!printedWarning)
    {
        g_GameErrorContext.Log(TH_ERR_NO_MIDI_SUPPORT);
        printedWarning = true;
    }

    return true;
}

ZunResult MidiDevice::Close()
{
    return ZUN_SUCCESS;
}

bool MidiDevice::SendLongMsg(const u8 *buf, u32 len)
{
    return true;
}

bool MidiDevice::SendShortMsg(u8 midiStatus, u8 firstByte, u8 secondByte)
{
    return true;
}
#endif
