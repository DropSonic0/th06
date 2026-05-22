#pragma once

#ifdef __PS3__
#include "inttypes.hpp"

namespace Ps3Save
{
    void Init();
    // Loads all save files from the PS3 Save Data utility into the USRDIR.
    // Returns true on success.
    bool LoadFromNative();
    // Saves all relevant files from USRDIR to the PS3 Save Data utility.
    // Returns true on success.
    bool SaveToNative();

    void Shutdown();
}
#endif
