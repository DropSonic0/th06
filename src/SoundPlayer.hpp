#pragma once

#include "ZunResult.hpp"
#include "inttypes.hpp"
#ifndef __PS3__
#include <SDL_audio.h>
#include <SDL_rwops.h>
#include <thread>
#else
#include <cell/audio.h>
#include <sys/ppu_thread.h>
#include <sys/event.h>
#include <stdio.h>
#endif
#ifndef __PS3__
#include <atomic>
#include <mutex>
#else
#include <sys/synchronization.h>
#endif

enum SoundIdx
{
    NO_SOUND = -1,
    SOUND_SHOOT = 0,
    SOUND_1 = 1,
    SOUND_2 = 2,
    SOUND_3 = 3,
    SOUND_PICHUN = 4,
    SOUND_5 = 5,
    SOUND_BOMB_REIMARI = 6,
    SOUND_7 = 7,
    SOUND_8 = 8,
    SOUND_SHOOT_BOSS = 9,
    SOUND_SELECT = 10,
    SOUND_BACK = 11,
    SOUND_MOVE_MENU = 12,
    SOUND_BOMB_REIMU_A = 13,
    SOUND_BOMB = 14,
    SOUND_F = 15,
    SOUND_BOSS_LASER = 16,
    SOUND_BOSS_LASER_2 = 17,
    SOUND_12 = 18,
    SOUND_BOMB_MARISA_B = 19,
    SOUND_TOTAL_BOSS_DEATH = 20,
    SOUND_15 = 21,
    SOUND_16 = 22,
    SOUND_17 = 23,
    SOUND_18 = 24,
    SOUND_WTF_IS_THAT_LMAO = 25,
    SOUND_1A = 26,
    SOUND_1B = 27,
    SOUND_1UP = 28,
    SOUND_1D = 29,
    SOUND_GRAZE = 30,
    SOUND_POWERUP = 31,
};

struct SoundBufferIdxVolume
{
    i32 bufferIdx;
    i16 volume;
};

struct SoundData
{
    i16 *samples;
    u32 pos;
    u32 len;
    bool isPlaying;
};

struct WavData
{
#ifndef __PS3__
    SDL_RWops *fileStream;
#else
    FILE *fileStream;
#endif
    u32 dataStartOffset;
    u32 samples;
};

struct MusicStream
{
    WavData srcWav;
    u32 pos;
    u32 loopStart;
    u32 loopEnd;
    u32 fadeoutLen;
    u32 fadeoutProgress;
#ifdef __PS3__
    i16 *streamCache;
    u32 streamCacheSize; // in frames (per buffer)
    u32 streamCachePos;  // in frames
    u32 streamCacheValid[2]; // in frames
    u32 activeBuffer;    // 0 or 1
    bool bufferBusy[2];
    double fraction;
    i16 lastSamples[2];
    i16 nextSamples[2];
#endif
};

struct SoundPlayer
{
    SoundPlayer();

    ZunResult InitializeDSound();
    ZunResult InitSoundBuffers();
    ZunResult Release(void);

    ZunResult LoadSound(i32 idx, const char *path, f32 volumeMultiplier);
    void PlaySounds();
    void PlaySoundByIdx(SoundIdx idx);
    ZunResult PlayBGM(bool isLooping);
    void StopBGM();
    void StopBGM_NoLock();
    void FadeOut(f32 seconds);

    ZunResult LoadWav(const char *path);
    ZunResult LoadPos(const char *path);

    void BackgroundMusicPlayerThread();
    int MixAudio(u32 samples);

    SoundData soundBuffers[128];
#ifndef __PS3__
    std::mutex soundBufMutex;
    SDL_AudioDeviceID audioDev;
    std::thread backgroundMusicThreadHandle;
    std::atomic_bool terminateFlag;
#else
    sys_mutex_t soundBufMutex;
    sys_mutex_t bgmIoMutex;
    sys_mutex_t bgmStateMutex;
    uint32_t audioPortNum;
    sys_ppu_thread_t backgroundMusicThreadHandle;
    sys_ppu_thread_t bgmIoThreadHandle;
    volatile bool terminateFlag;
    sys_event_queue_t audioEventQueue;
    sys_ipc_key_t audioEventKey;
#endif
    i32 soundBuffersToPlay[3];
    MusicStream backgroundMusic;
    bool isLooping;
};

extern SoundPlayer g_SoundPlayer;