#include "SoundPlayer.hpp"

#include "FileSystem.hpp"
#include "Supervisor.hpp"
#include "i18n.hpp"
#include "utils.hpp"
#include "GamePaths.hpp"

#include "inttypes.hpp"

#ifndef __PS3__
#include <SDL.h>
#include <SDL_timer.h>
#else
#include <cell/audio.h>
#include <sys/ppu_thread.h>
#include <sys/timer.h>
#include <sys/sys_time.h>
#define SDL_GetTicks() ((u32)(sys_time_get_system_time() / 1000))
#define SDL_GetTicks64() (sys_time_get_system_time() / 1000)
#define SDL_Delay(ms) sys_timer_usleep((ms) * 1000)
#endif
#ifndef __PS3__
#include <array>
#include <cmath>
#include <cstring>
#include <vector>
#else
#include <math.h>
#include <string.h>
#include <stdlib.h>
#endif
#include <new>

// This would all be a lot easier with SDL_mixer, but SDL_mixer doesn't permit any way of doing custom
//   loop points that would be accurate to the sample like EoSD needs. So instead we get to read WAVs and
//   mix everything by hand. Yay

#define BACKGROUND_MUSIC_WAV_NUM_CHANNELS 2
#define BACKGROUND_MUSIC_WAV_SAMPLE_RATE 44100
#define BACKGROUND_MUSIC_WAV_BITS_PER_SAMPLE 16
#define BACKGROUND_MUSIC_WAV_BLOCK_ALIGN (BACKGROUND_MUSIC_WAV_BITS_PER_SAMPLE / 8 * BACKGROUND_MUSIC_WAV_NUM_CHANNELS)
#define BACKGROUND_MUSIC_WAV_BYTE_RATE (BACKGROUND_MUSIC_WAV_BLOCK_ALIGN * BACKGROUND_MUSIC_WAV_SAMPLE_RATE)
#include <iostream>
#include <string>
void soundplayerdlog(std::string msg){
    std::cout<<"soundplayer : "<<msg<<std::endl;
}

// DirectSound deals with volume by subtracting a number measured in hundredths of decibels from the source sound.
//   The scale is from 0 (no volume modification) to -10,000 (subtraction of 100 decibels, and basically silent).
//   20 decibels affects wave amplitude by a factor of 10

static const SoundBufferIdxVolume g_SoundBufferIdxVol[32] = {
    {0, -1500}, {0, -2000}, {1, -1200}, {1, -1400}, {2, -1000},  {3, -500},   {4, -500},   {5, -1700},
    {6, -1700}, {7, -1700}, {8, -1000}, {9, -1000}, {10, -1900}, {11, -1200}, {12, -900},  {5, -1500},
    {13, -900}, {14, -900}, {15, -600}, {16, -400}, {17, -1100}, {18, -900},  {5, -1800},  {6, -1800},
    {7, -1800}, {19, -300}, {20, -600}, {21, -800}, {22, -100},  {23, -500},  {24, -1000}, {25, -1000},
};
static const char *const g_SFXList[26] = {
    "data/wav/plst00.wav", "data/wav/enep00.wav",   "data/wav/pldead00.wav", "data/wav/power0.wav",
    "data/wav/power1.wav", "data/wav/tan00.wav",    "data/wav/tan01.wav",    "data/wav/tan02.wav",
    "data/wav/ok00.wav",   "data/wav/cancel00.wav", "data/wav/select00.wav", "data/wav/gun00.wav",
    "data/wav/cat00.wav",  "data/wav/lazer00.wav",  "data/wav/lazer01.wav",  "data/wav/enep01.wav",
    "data/wav/nep00.wav",  "data/wav/damage00.wav", "data/wav/item00.wav",   "data/wav/kira00.wav",
    "data/wav/kira01.wav", "data/wav/kira02.wav",   "data/wav/extend.wav",   "data/wav/timeout.wav",
    "data/wav/graze.wav",  "data/wav/powerup.wav",
};
SoundPlayer g_SoundPlayer;

SoundPlayer::SoundPlayer()
{
    // Note: memset of an std::mutex crashes on windows
    //std::memset(this, 0, sizeof(SoundPlayer));
#ifdef __PS3__
    sys_mutex_create(&this->soundBufMutex, NULL);
    this->audioPortNum = 0xFFFFFFFF;
#endif
}

#ifdef __PS3__
static void ps3_audio_thread(uint64_t arg)
{
    SoundPlayer *player = (SoundPlayer *)(uintptr_t)arg;
    player->BackgroundMusicPlayerThread();
    sys_ppu_thread_exit(0);
}
#endif

ZunResult SoundPlayer::InitializeDSound()
{
#ifdef __PS3__
    utils::Log("SoundPlayer: InitializeDSound...");
#endif
#ifndef __PS3__
    SDL_AudioSpec desiredAudio;
    SDL_AudioSpec obtainedAudio;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        goto fail;
    }

    desiredAudio.freq = 44100;
    desiredAudio.format = AUDIO_S16SYS;
    desiredAudio.channels = 2;
    desiredAudio.samples = 2048;
    desiredAudio.padding = 0;
    desiredAudio.callback = NULL;

    this->audioDev = SDL_OpenAudioDevice(NULL, 0, &desiredAudio, &obtainedAudio, 0);

    if (this->audioDev == 0)
    {
        goto fail;
    }

    this->backgroundMusicThreadHandle = std::thread(&SoundPlayer::BackgroundMusicPlayerThread, this);
#else
    utils::Log("SoundPlayer: cellAudioInit...");
    int res_init = cellAudioInit();
    if (res_init != CELL_OK && res_init != CELL_AUDIO_ERROR_ALREADY_INIT) {
        utils::Log("SoundPlayer: cellAudioInit failed: 0x%08x", res_init);
        goto fail;
    }
    
    CellAudioPortParam portParam;
    portParam.nChannel = CELL_AUDIO_PORT_2CH;
    portParam.nBlock = 16;
    portParam.attr = CELL_AUDIO_PORTATTR_INITLEVEL;
    portParam.level = 1.0f;

    utils::Log("SoundPlayer: cellAudioPortOpen (nBlock=%d, attr=0x%llx)...", portParam.nBlock, portParam.attr);
    int res = cellAudioPortOpen(&portParam, &this->audioPortNum);
    if (res != CELL_OK)
    {
        utils::Log("SoundPlayer: cellAudioPortOpen failed: 0x%08x", res);
        goto fail;
    }

    utils::Log("SoundPlayer: cellAudioPortStart (port %d)...", this->audioPortNum);
    res = cellAudioPortStart(this->audioPortNum);
    cellAudioSetPortLevel(this->audioPortNum, 1.0f);
    if (res != CELL_OK)
    {
        utils::Log("SoundPlayer: cellAudioPortStart failed: 0x%08x", res);
        goto fail;
    }

    utils::Log("SoundPlayer: sys_ppu_thread_create...");
    this->ps3_startUs = sys_time_get_system_time() / 1000;
    this->ps3_samplesSent = 0;
    res = sys_ppu_thread_create(&this->backgroundMusicThreadHandle, ps3_audio_thread, (uint64_t)this, 1000, 128 * 1024, SYS_PPU_THREAD_CREATE_JOINABLE, "SoundThread");
    if (res != CELL_OK)
    {
        utils::Log("SoundPlayer: sys_ppu_thread_create failed: 0x%08x", res);
        goto fail;
    }
#endif

    utils::Log("SoundPlayer: InitializeDSound SUCCESS");
    GameErrorContext::Log(&g_GameErrorContext, TH_DBG_SOUNDPLAYER_INIT_SUCCESS);
    return ZUN_SUCCESS;

fail:
    GameErrorContext::Log(&g_GameErrorContext, TH_ERR_SOUNDPLAYER_FAILED_TO_INITIALIZE_OBJECT);
    return ZUN_ERROR;
}

ZunResult SoundPlayer::Release(void)
{
    this->terminateFlag = true;
#ifndef __PS3__
    this->backgroundMusicThreadHandle.join();
#else
    uint64_t exitCode;
    sys_ppu_thread_join(this->backgroundMusicThreadHandle, &exitCode);
#endif
    this->terminateFlag = false;

    StopBGM();

    for (int i = 0; i < ARRAY_SIZE_SIGNED(this->soundBuffers); i++)
    {
        if (this->soundBuffers[i].samples != NULL)
        {
            delete[] this->soundBuffers[i].samples;
            this->soundBuffers[i].samples = NULL;
            this->soundBuffers[i].isPlaying = false;
        }
    }

#ifndef __PS3__
    if (this->audioDev != 0)
    {
        SDL_CloseAudioDevice(this->audioDev);
        this->audioDev = 0;
    }
#else
    cellAudioPortStop(this->audioPortNum);
    cellAudioPortClose(this->audioPortNum);
    cellAudioQuit();
#endif

    return ZUN_SUCCESS;
}

void SoundPlayer::StopBGM()
{
    if (this->backgroundMusic.srcWav.fileStream != NULL)
    {
        // this->soundBufMutex.lock();
#ifndef __PS3__
        SDL_RWclose(this->backgroundMusic.srcWav.fileStream);
#else
        fclose(this->backgroundMusic.srcWav.fileStream);
#endif
        this->backgroundMusic.srcWav.fileStream = NULL;
        // this->soundBufMutex.unlock();

        utils::DebugPrint2("stop BGM\n");
    }
}

void SoundPlayer::FadeOut(f32 seconds)
{
    if (this->backgroundMusic.srcWav.fileStream != NULL)
    {
        this->backgroundMusic.fadeoutLen = (u32)(seconds * 44100.0f);
        this->backgroundMusic.fadeoutProgress = 0;
    }
}

ZunResult SoundPlayer::LoadWav(const char *path)
{
#ifndef __PS3__
    SDL_RWops *fileStream;
#else
    FILE *fileStream;
#endif
    char idBuf[4];
    u32 riffSize;
    u32 wavDataSize;

#ifndef __PS3__
    if (this->audioDev == 0)
    {
        return ZUN_ERROR;
    }
#else
    if (this->audioPortNum == 0xFFFFFFFF)
    {
        return ZUN_ERROR;
    }
#endif

    if (g_Supervisor.cfg.playSounds == 0)
    {
        return ZUN_ERROR;
    }

    this->StopBGM();

    utils::DebugPrint2("load BGM\n");

#ifdef __ANDROID__
    std::string resolvedPath = std::string(GamePaths::GetUserPath()) + std::string(path);
    fileStream = SDL_RWFromFile(resolvedPath.c_str(), "r");
#elif defined(__PS3__)
    char resolvedPath[512];
    GamePaths::Resolve(resolvedPath, sizeof(resolvedPath), path);
    fileStream = fopen(resolvedPath, "rb");
#else
    fileStream = SDL_RWFromFile(path, "r");
#endif
    if (fileStream == NULL)
    {
#ifdef __PS3__
        utils::Log("SoundPlayer: Failed to load BGM WAV (fopen failed): %s (resolved: %s)", path, resolvedPath);
#endif
        utils::DebugPrint2("error : wav file load error %s\n", path);
        return ZUN_ERROR;
    }
#ifdef __PS3__
    utils::Log("SoundPlayer: Loaded BGM WAV: %s", path);
#endif

    // Minimum size of RIFF header and chunk info preceeding the sample data
#ifndef __PS3__
    if (SDL_RWsize(fileStream) < 44)
    {
        goto fail;
    }

    if (SDL_RWread(fileStream, idBuf, 4, 1) != 1 || std::strncmp(idBuf, "RIFF", 4) != 0)
    {
        goto fail;
    }

    riffSize = SDL_ReadLE32(fileStream);
#else
    fseek(fileStream, 0, SEEK_END);
    long fileSize = ftell(fileStream);
    fseek(fileStream, 0, SEEK_SET);
    if (fileSize < 44)
    {
        goto fail;
    }

    if (fread(idBuf, 4, 1, fileStream) != 1 || std::strncmp(idBuf, "RIFF", 4) != 0)
    {
        goto fail;
    }

    fread(&riffSize, 4, 1, fileStream);
    riffSize = utils::Swap32(riffSize);
#endif

    // Same bounds check done earlier on the total filesize
#ifndef __PS3__
    if (riffSize < 36 || riffSize > SDL_RWsize(fileStream) - 8)
    {
        goto fail;
    }

    if (SDL_RWread(fileStream, idBuf, 4, 1) != 1 || std::strncmp(idBuf, "WAVE", 4) != 0)
    {
        goto fail;
    }
#else
    if (riffSize < 36 || riffSize > fileSize - 8)
    {
        goto fail;
    }

    if (fread(idBuf, 4, 1, fileStream) != 1 || std::strncmp(idBuf, "WAVE", 4) != 0)
    {
        goto fail;
    }
#endif

    // Checks here are quite a bit less flexible than what WAV can represent. EoSD uses 44.1 kHz, stereo, 16-bit PCM
    //   so that's what we handle. We also assume that fmt and data are the only subchunks, which is definitely not
    //   a general guarantee, but it'll work fine with EoSD's WAV files.

#ifndef __PS3__
    if (SDL_RWread(fileStream, idBuf, 4, 1) != 1 || std::strncmp(idBuf, "fmt ", 4) != 0)
    {
        goto fail;
    }

    // Format subchunk size. Guaranteed 16 for PCM data
    if (SDL_ReadLE32(fileStream) != 16)
    {
        goto fail;
    }

    // Audio format. 1 represents raw PCM samples
    if (SDL_ReadLE16(fileStream) != 1)
    {
        goto fail;
    }

    // Number of channels. We expect stereo
    if (SDL_ReadLE16(fileStream) != BACKGROUND_MUSIC_WAV_NUM_CHANNELS)
    {
        goto fail;
    }

    // Sample frequency rate
    if (SDL_ReadLE32(fileStream) != BACKGROUND_MUSIC_WAV_SAMPLE_RATE)
    {
        goto fail;
    }

    // Byte rate
    if (SDL_ReadLE32(fileStream) != BACKGROUND_MUSIC_WAV_BYTE_RATE)
    {
        goto fail;
    }

    // Block alignment
    if (SDL_ReadLE16(fileStream) != BACKGROUND_MUSIC_WAV_BLOCK_ALIGN)
    {
        goto fail;
    }

    // Bits per sample
    if (SDL_ReadLE16(fileStream) != BACKGROUND_MUSIC_WAV_BITS_PER_SAMPLE)
    {
        goto fail;
    }

    if (SDL_RWread(fileStream, idBuf, 4, 1) != 1 || std::strncmp(idBuf, "data", 4) != 0)
    {
        goto fail;
    }

    wavDataSize = SDL_ReadLE32(fileStream);
#else
    if (fread(idBuf, 4, 1, fileStream) != 1 || std::strncmp(idBuf, "fmt ", 4) != 0)
    {
        goto fail;
    }

    uint32_t fmtSize; fread(&fmtSize, 4, 1, fileStream);
    fmtSize = utils::Swap32(fmtSize);
    if (fmtSize != 16) goto fail;

    uint16_t format; fread(&format, 2, 1, fileStream);
    format = utils::Swap16(format);
    if (format != 1) goto fail;

    uint16_t channels; fread(&channels, 2, 1, fileStream);
    channels = utils::Swap16(channels);
    if (channels != BACKGROUND_MUSIC_WAV_NUM_CHANNELS) goto fail;

    uint32_t sampleRate; fread(&sampleRate, 4, 1, fileStream);
    sampleRate = utils::Swap32(sampleRate);
    if (sampleRate != BACKGROUND_MUSIC_WAV_SAMPLE_RATE) goto fail;

    uint32_t byteRate; fread(&byteRate, 4, 1, fileStream);
    byteRate = utils::Swap32(byteRate);
    if (byteRate != BACKGROUND_MUSIC_WAV_BYTE_RATE) goto fail;

    uint16_t blockAlign; fread(&blockAlign, 2, 1, fileStream);
    blockAlign = utils::Swap16(blockAlign);
    if (blockAlign != BACKGROUND_MUSIC_WAV_BLOCK_ALIGN) goto fail;

    uint16_t bitsPerSample; fread(&bitsPerSample, 2, 1, fileStream);
    bitsPerSample = utils::Swap16(bitsPerSample);
    if (bitsPerSample != BACKGROUND_MUSIC_WAV_BITS_PER_SAMPLE) goto fail;

    if (fread(idBuf, 4, 1, fileStream) != 1 || std::strncmp(idBuf, "data", 4) != 0)
    {
        goto fail;
    }

    fread(&wavDataSize, 4, 1, fileStream);
    wavDataSize = utils::Swap32(wavDataSize);
#endif

    if (wavDataSize > riffSize - 44)
    {
        goto fail;
    }

    this->backgroundMusic.srcWav.samples = wavDataSize / BACKGROUND_MUSIC_WAV_BLOCK_ALIGN;

    if (this->backgroundMusic.srcWav.samples == 0)
    {
        goto fail;
    }

    this->backgroundMusic.srcWav.fileStream = fileStream;
#ifndef __PS3__
    this->backgroundMusic.srcWav.dataStartOffset = SDL_RWtell(fileStream);
#else
    this->backgroundMusic.srcWav.dataStartOffset = ftell(fileStream);
#endif
    this->backgroundMusic.loopStart = 0;
    this->backgroundMusic.loopEnd = this->backgroundMusic.srcWav.samples;
    this->backgroundMusic.fadeoutLen = 0;
    this->backgroundMusic.fadeoutProgress = 0;
    this->backgroundMusic.pos = 0;

    return ZUN_SUCCESS;

fail:
#ifdef __PS3__
    utils::Log("SoundPlayer: Failed to LoadWav %s (check WAV format?)", path);
#endif
#ifndef __PS3__
    SDL_RWclose(fileStream);
#else
    fclose(fileStream);
#endif
    return ZUN_ERROR;
}

ZunResult SoundPlayer::LoadPos(const char *path)
{
    u8 *fileData;

#ifndef __PS3__
    if (this->audioDev == 0 || g_Supervisor.cfg.playSounds == 0 || backgroundMusic.srcWav.fileStream == NULL)
#else
    if (this->audioPortNum == 0xFFFFFFFF || g_Supervisor.cfg.playSounds == 0 || backgroundMusic.srcWav.fileStream == NULL)
#endif
    {
        return ZUN_ERROR;
    }

    fileData = FileSystem::OpenPath(path, 0);

    if (fileData == NULL)
    {
        return ZUN_ERROR;
    }

#ifndef __PS3__
    this->backgroundMusic.loopStart = SDL_SwapLE32(*((u32 *)fileData));
    this->backgroundMusic.loopEnd = SDL_SwapLE32(*(u32 *)(fileData + 4));
#else
    this->backgroundMusic.loopStart = utils::Swap32(*((u32 *)fileData));
    this->backgroundMusic.loopEnd = utils::Swap32(*(u32 *)(fileData + 4));
#endif

    free(fileData);

    if (this->backgroundMusic.loopStart >= this->backgroundMusic.loopEnd ||
        this->backgroundMusic.loopEnd > this->backgroundMusic.srcWav.samples)
    {
        this->backgroundMusic.loopStart = 0;
        this->backgroundMusic.loopEnd = this->backgroundMusic.srcWav.samples;

        return ZUN_ERROR;
    }

    return ZUN_SUCCESS;
}

ZunResult SoundPlayer::InitSoundBuffers()
{
#ifdef __PS3__
    utils::Log("SoundPlayer: InitSoundBuffers...");
#endif
    //soundplayerdlog("init sound buffer");
    //soundplayerdlog("check audioDev");
#ifndef __PS3__
    if (this->audioDev == 0)
#else
    if (this->audioPortNum == 0xFFFFFFFF)
#endif
    {
        return ZUN_ERROR;
    }

    //soundplayerdlog("std::fill_n");
    std::fill_n(this->soundBuffersToPlay, ARRAY_SIZE(this->soundBuffersToPlay), -1);

    //soundplayerdlog("for loop");
    for (int idx = 0; idx < ARRAY_SIZE_SIGNED(g_SoundBufferIdxVol); idx++)
    {
        if (this->LoadSound(idx, g_SFXList[g_SoundBufferIdxVol[idx].bufferIdx],
                            1.0f / ZUN_POWF(10.0f, (float)g_SoundBufferIdxVol[idx].volume / -2000)) != ZUN_SUCCESS)
        {
#ifdef __PS3__
            utils::Log("SoundPlayer: Failed to load SFX: %s", g_SFXList[g_SoundBufferIdxVol[idx].bufferIdx]);
#endif
            GameErrorContext::Log(&g_GameErrorContext, TH_ERR_SOUNDPLAYER_FAILED_TO_LOAD_SOUND_FILE, g_SFXList[idx]);
            return ZUN_ERROR;
        }

        this->soundBuffers[idx].isPlaying = false;
        this->soundBuffers[idx].pos = 0;
    }
    //soundplayerdlog("finish");
    return ZUN_SUCCESS;
}

ZunResult SoundPlayer::LoadSound(i32 idx, const char *path, f32 volumeMultiplier)
{
    //soundplayerdlog("load sound 1");
#ifndef __PS3__
    SDL_AudioCVT sampleConversionDesc;
    SDL_AudioSpec wavFormat;
    u8 *wavRawSamples;
    u32 wavRawSampleByteCount;
#else
    // Native WAV loading for PS3
    // EoSD SFX are typically 22050Hz, Mono. 
    // We need to resample to 44100Hz and handle Endianness.
    uint32_t rawSampleCount;
    i16* rawSamples;
#endif
    u8 *wavRawData;

    //soundplayerdlog("load sound 2");
    // soundBufMutex.lock();

    if (this->soundBuffers[idx].samples != NULL)
    {
        delete[] this->soundBuffers[idx].samples;
        this->soundBuffers[idx].samples = NULL;
    }

    //soundplayerdlog("load sound 3");
    wavRawData = (u8 *)FileSystem::OpenPath(path, 0);

    if (wavRawData == NULL)
    {
#ifdef __PS3__
        utils::Log("SoundPlayer: LoadSound %d FAILED to OpenPath: %s", idx, path);
#endif
        goto fail;
    }

    //soundplayerdlog("load sound 4");
#ifndef __PS3__
    if (SDL_LoadWAV_RW(SDL_RWFromConstMem(wavRawData, g_LastFileSize), 1, &wavFormat, &wavRawSamples,
                       &wavRawSampleByteCount) == NULL)
    {
        GameErrorContext::Log(&g_GameErrorContext, TH_ERR_NOT_A_WAV_FILE, path);
        goto fail;
    }

    // EoSD's sound files are all 22050 Hz, and some even use 8-bit samples. Converting them
    //   here only uses a few hundred extra kilobytes of RAM compared to the original code,
    //   but it might be worth looking into avoiding it for especially RAM-limited systems

    //soundplayerdlog("load sound 5");
    if (SDL_BuildAudioCVT(&sampleConversionDesc, wavFormat.format, wavFormat.channels, wavFormat.freq, AUDIO_S16SYS, 1,
                          44100) == 1)
    {
        sampleConversionDesc.len = wavRawSampleByteCount;
        sampleConversionDesc.buf = new u8[wavRawSampleByteCount * sampleConversionDesc.len_mult];
        std::memcpy(sampleConversionDesc.buf, wavRawSamples, wavRawSampleByteCount);

        SDL_ConvertAudio(&sampleConversionDesc);

        this->soundBuffers[idx].len = sampleConversionDesc.len_cvt / 2;
        this->soundBuffers[idx].samples = new i16[this->soundBuffers[idx].len];
        std::memcpy(this->soundBuffers[idx].samples, sampleConversionDesc.buf, sampleConversionDesc.len_cvt);

        delete[] sampleConversionDesc.buf;
    }
    else
    {
        this->soundBuffers[idx].len = wavRawSampleByteCount / 2;
        this->soundBuffers[idx].samples = new i16[this->soundBuffers[idx].len];
        std::memcpy(this->soundBuffers[idx].samples, wavRawSamples, wavRawSampleByteCount);
    }

    //soundplayerdlog("load sound 6");
    SDL_FreeWAV(wavRawSamples);
#else
    rawSampleCount = (g_LastFileSize - 44) / 2;
    this->soundBuffers[idx].len = rawSampleCount * 4; // 22050 Mono -> 44100 Stereo (4 samples per source sample)
    this->soundBuffers[idx].samples = new i16[this->soundBuffers[idx].len];
    for (uint32_t i = 0; i < rawSampleCount; i++) {
        i16 sample;
        memcpy(&sample, wavRawData + 44 + i * 2, 2);
        sample = utils::Swap16(sample);
        // Interleaved Stereo output: L, R, L, R
        this->soundBuffers[idx].samples[i*4] = sample;     // Frame 1 L
        this->soundBuffers[idx].samples[i*4 + 1] = sample; // Frame 1 R
        this->soundBuffers[idx].samples[i*4 + 2] = sample; // Frame 2 L
        this->soundBuffers[idx].samples[i*4 + 3] = sample; // Frame 2 R
    }
#endif

    for (u32 i = 0; i < this->soundBuffers[idx].len; i++)
    {
        this->soundBuffers[idx].samples[i] *= volumeMultiplier;
    }

    this->soundBuffers[idx].pos = 0;
    this->soundBuffers[idx].isPlaying = false;

#ifdef __PS3__
    utils::Log("SoundPlayer: LoadSound %d SUCCESS: %s (len: %u)", idx, path, this->soundBuffers[idx].len);
#endif

    //soundplayerdlog("load sound 7");
    // soundBufMutex.unlock();
    free(wavRawData);
    return ZUN_SUCCESS;

fail:
    // soundBufMutex.unlock();
    free(wavRawData);
    return ZUN_ERROR;
}

ZunResult SoundPlayer::PlayBGM(bool isLooping)
{
    utils::DebugPrint2("play BGM\n");

    if (this->backgroundMusic.srcWav.fileStream == NULL)
    {
        return ZUN_ERROR;
    }

    //    res = this->backgroundMusic->Reset();
    //    if (FAILED(res))
    //    {
    //        return ZUN_ERROR;
    //    }
    //
    //    buffer = this->backgroundMusic->GetBuffer(0);
    //    res = this->backgroundMusic->FillBufferWithSound(buffer, isLooping);
    //    if (FAILED(res))
    //    {
    //        return ZUN_ERROR;
    //    }
    //    res = this->backgroundMusic->Play(0, DSBPLAY_LOOPING);
    //    if (FAILED(res))
    //    {
    //        return ZUN_ERROR;
    //    }
    utils::DebugPrint2("comp\n");
    this->isLooping = isLooping;
    return ZUN_SUCCESS;
}

void SoundPlayer::PlaySounds()
{
    i32 idx;
    i32 sndBufIdx;

#ifdef __PS3__
    static int log_counter = 0;
    if (this->soundBuffersToPlay[0] >= 0 && ++log_counter >= 1) { 
        utils::Log("SoundPlayer: PlaySounds commit [%d, %d, %d]", this->soundBuffersToPlay[0], this->soundBuffersToPlay[1], this->soundBuffersToPlay[2]); 
        log_counter = 0; 
    }
#endif

#ifndef __PS3__
    if (this->audioDev == 0 || !g_Supervisor.cfg.playSounds)
#else
    if (this->audioPortNum == 0xFFFFFFFF || !g_Supervisor.cfg.playSounds)
#endif
    {
        return;
    }

    // soundBufMutex.lock();

    for (idx = 0; idx < ARRAY_SIZE_SIGNED(this->soundBuffersToPlay); idx++)
    {
        if (this->soundBuffersToPlay[idx] < 0)
        {
            break;
        }

        sndBufIdx = this->soundBuffersToPlay[idx];
        this->soundBuffersToPlay[idx] = -1;

        if (this->soundBuffers[sndBufIdx].samples == NULL)
        {
            continue;
        }

        this->soundBuffers[sndBufIdx].pos = 0;
        this->soundBuffers[sndBufIdx].isPlaying = true;
    }

    // soundBufMutex.unlock();
}

void SoundPlayer::PlaySoundByIdx(SoundIdx idx)
{
    u32 i;

#ifdef __PS3__
    utils::Log("SoundPlayer: PlaySoundByIdx(%d)", idx);
#endif

    for (i = 0; i < ARRAY_SIZE(this->soundBuffersToPlay); i++)
    {
        if (this->soundBuffersToPlay[i] < 0)
        {
            break;
        }

        if (this->soundBuffersToPlay[i] == idx)
        {
            return;
        }
    }

    if (i >= 3)
    {
        return;
    }

    this->soundBuffersToPlay[i] = idx;
}

int SoundPlayer::MixAudio(u32 samples)
{
#ifdef __PS3__
    static int mix_enter_log = 0;
    if (++mix_enter_log >= 300) {
        utils::Log("SoundPlayer: MixAudio enter, samples=%u", samples);
        mix_enter_log = 0;
    }
#endif
#ifndef __PS3__
    std::vector<i16> finalBuffer(samples);
    std::vector<i32> mixBuffer(samples);
    u8 playingChannels = 0;
#else
    static i32 mixBuffer[2048] __attribute__((aligned(16)));
    if (samples > 2048) samples = 2048;
    memset(mixBuffer, 0, samples * sizeof(i32));
    u8 playingChannels = 0;

    // Save state for rollback
    u32 savedSoundPos[ARRAY_SIZE(this->soundBuffers)];
    bool savedSoundPlaying[ARRAY_SIZE(this->soundBuffers)];
    for(int i=0; i<ARRAY_SIZE_SIGNED(this->soundBuffers); i++) {
        savedSoundPos[i] = this->soundBuffers[i].pos;
        savedSoundPlaying[i] = this->soundBuffers[i].isPlaying;
    }
    u32 savedBgmPos = this->backgroundMusic.pos;
    long savedFilePos = -1;
    if (this->backgroundMusic.srcWav.fileStream) {
        savedFilePos = ftell(this->backgroundMusic.srcWav.fileStream);
    }
#endif

    // this->soundBufMutex.lock();

    for (int i = 0; i < ARRAY_SIZE_SIGNED(this->soundBuffers); i++)
    {
        if (!this->soundBuffers[i].isPlaying)
        {
            continue;
        }

        playingChannels++;

#ifdef __PS3__
        // Source samples are interleaved stereo (PS3 LoadSound upsamples them)
        const u32 framesToMix = std::min(samples / 2, (this->soundBuffers[i].len - this->soundBuffers[i].pos) / 2);

        for (u32 j = 0; j < framesToMix; j++)
        {
            mixBuffer[j * 2] += this->soundBuffers[i].samples[this->soundBuffers[i].pos + j * 2];
            mixBuffer[j * 2 + 1] += this->soundBuffers[i].samples[this->soundBuffers[i].pos + j * 2 + 1];
        }

        this->soundBuffers[i].pos += framesToMix * 2;

        if (this->soundBuffers[i].pos >= this->soundBuffers[i].len)
#else
        // Sounds are all mono, so we need to duplicate each sample for stereo output
        const u32 samplesToMix = std::min(samples / 2, this->soundBuffers[i].len - this->soundBuffers[i].pos);

        for (u32 j = 0; j < samplesToMix; j++)
        {
            mixBuffer[j * 2] += this->soundBuffers[i].samples[this->soundBuffers[i].pos + j];
            mixBuffer[j * 2 + 1] += this->soundBuffers[i].samples[this->soundBuffers[i].pos + j];
        }

        this->soundBuffers[i].pos += samplesToMix;

        if (this->soundBuffers[i].pos == this->soundBuffers[i].len)
#endif
        {
            this->soundBuffers[i].isPlaying = false;
        }
    }

    if (this->backgroundMusic.srcWav.fileStream != NULL)
    {
        u32 samplesMixed = 0;
        f32 fadeoutMult;

        if (this->backgroundMusic.fadeoutLen != 0)
        {
            f32 fadeoutInterp =
                mapRange(this->backgroundMusic.fadeoutProgress, 0, this->backgroundMusic.fadeoutLen, 0, 5);
            fadeoutMult = 1.0f / ZUN_POWF(10.0f, fadeoutInterp / 2.0f);
        }
        else
        {
            fadeoutMult = 1.0f;
        }

        while (samplesMixed < samples / 2)
        {
            const u32 samplesToMix =
                std::min((samples / 2) - samplesMixed, this->backgroundMusic.loopEnd - this->backgroundMusic.pos);

#ifndef __PS3__
            for (u32 j = 0; j < samplesToMix; j++)
            {
                mixBuffer[(samplesMixed + j) * 2] +=
                    ((i16)SDL_ReadLE16(this->backgroundMusic.srcWav.fileStream)) * fadeoutMult;
                mixBuffer[(samplesMixed + j) * 2 + 1] +=
                    ((i16)SDL_ReadLE16(this->backgroundMusic.srcWav.fileStream)) * fadeoutMult;
            }
            this->backgroundMusic.pos += samplesToMix;
            samplesMixed += samplesToMix;
#else
            // PS3 Optimization: Use a buffer to read multiple samples at once
            static int16_t readBuffer[2048];
            u32 framesToRead = (samplesToMix > 1024) ? 1024 : samplesToMix;
            size_t itemsRead = fread(readBuffer, 2, framesToRead * 2, this->backgroundMusic.srcWav.fileStream);
            u32 framesRead = itemsRead / 2;
            
            for (u32 j = 0; j < framesRead; j++)
            {
                mixBuffer[(samplesMixed + j) * 2] += (int16_t)utils::Swap16(readBuffer[j * 2]) * fadeoutMult;
                mixBuffer[(samplesMixed + j) * 2 + 1] += (int16_t)utils::Swap16(readBuffer[j * 2 + 1]) * fadeoutMult;
            }
            
            this->backgroundMusic.pos += framesRead;
            samplesMixed += framesRead;
            
            if (framesRead == 0 && framesToRead > 0) {
                // Unexpected end of file or error
                break;
            }
#endif

            if (this->backgroundMusic.pos >= this->backgroundMusic.loopEnd)
            {
                if (this->isLooping)
                {
                    this->backgroundMusic.pos = this->backgroundMusic.loopStart;
#ifndef __PS3__
                    SDL_RWseek(this->backgroundMusic.srcWav.fileStream,
                               this->backgroundMusic.srcWav.dataStartOffset + this->backgroundMusic.pos * 4, SEEK_SET);
#else
                    fseek(this->backgroundMusic.srcWav.fileStream,
                               this->backgroundMusic.srcWav.dataStartOffset + this->backgroundMusic.pos * 4, SEEK_SET);
#endif
                }
                else
                {
#ifndef __PS3__
                    SDL_RWclose(this->backgroundMusic.srcWav.fileStream);
#else
                    fclose(this->backgroundMusic.srcWav.fileStream);
#endif
                    this->backgroundMusic.srcWav.fileStream = NULL;

                    break;
                }
            }
        }

        if (this->backgroundMusic.fadeoutLen != 0)
        {
            this->backgroundMusic.fadeoutProgress += samplesMixed;

            if (this->backgroundMusic.fadeoutProgress >= this->backgroundMusic.fadeoutLen)
            {
#ifndef __PS3__
                SDL_RWclose(this->backgroundMusic.srcWav.fileStream);
#else
                fclose(this->backgroundMusic.srcWav.fileStream);
#endif
                this->backgroundMusic.srcWav.fileStream = NULL;
            }
        }

        playingChannels++;
    }

    // this->soundBufMutex.unlock();

    // DirectSound supports playing from an arbitrary number of buffers at once, but that's kind of
    //   difficult to get right as it turns out. Instead we use 8 as an assumption of the
    //   max number of channels that could possibly be playing at once. If more channels end up in use,
    //   the input volume of each channel will start scaling down, which isn't correct, but would
    //   likely be imperceptible with that many channels anyway.

#ifndef __PS3__
    const int mixDivisor = std::max(8, (int)playingChannels);
#else
    // PS3: Dynamic mix divisor for better volume
    const int mixDivisor = std::max(2, (int)playingChannels);
#endif

#ifndef __PS3__
    for (u32 i = 0; i < samples; i++)
    {
        // Integer division like this doesn't get optimized at all by the compiler. If it becomes
        //   a problem, it could be a good idea to convert to float, or to do the division as
        //   fixed point multiplication by the inverse of mixDivisor, depending on what's faster
        //   on any particular platform
        finalBuffer[i] = mixBuffer[i] / mixDivisor;
    }

    SDL_QueueAudio(this->audioDev, finalBuffer.data(), samples * 2);
    return 0;
#else
    static float floatBuffer[2048] __attribute__((aligned(16)));
    if (samples > 2048) samples = 2048;

    for (u32 i = 0; i < samples; i++)
    {
        floatBuffer[i] = (float)mixBuffer[i] / (mixDivisor * 32768.0f);
    }
    int res_add = cellAudioAdd2chData(this->audioPortNum, floatBuffer, samples / 2, 1.0f);

    static int log_counter = 0;
    static int first_mixes = 0;
    if (first_mixes < 10 || ++log_counter >= 300) {
        float peak = 0;
        if (playingChannels > 0) {
            for (u32 i = 0; i < samples; i++) if (fabsf(floatBuffer[i]) > peak) peak = fabsf(floatBuffer[i]);
        }
        utils::Log("SoundPlayer: Mix (chans=%d, peak=%.3f, res=0x%x)", playingChannels, peak, res_add);
        log_counter = 0;
        if (first_mixes < 10) first_mixes++;
    }

    if (res_add < 0) {
        if (res_add != CELL_AUDIO_ERROR_PORT_FULL) {
            utils::Log("SoundPlayer: cellAudioAdd2chData(port=%d, samples=%d) failed: 0x%08x", this->audioPortNum, samples/2, res_add);
        }
        // Rollback
        for(int i=0; i<ARRAY_SIZE_SIGNED(this->soundBuffers); i++) {
            this->soundBuffers[i].pos = savedSoundPos[i];
            this->soundBuffers[i].isPlaying = savedSoundPlaying[i];
        }
        this->backgroundMusic.pos = savedBgmPos;
        if (savedFilePos != -1) {
            fseek(this->backgroundMusic.srcWav.fileStream, savedFilePos, SEEK_SET);
        }
        return res_add;
    }
    return 0;
#endif
}

// EoSD originally just used this function to manage the streaming of the music WAV file.
//   We also use it to mix and queue audio, since we have to do that manually and doing it
//   in a thread keeps sound running continuously, even if the main thread runs into lag
void SoundPlayer::BackgroundMusicPlayerThread()
{
#ifdef __PS3__
    utils::Log("SoundPlayer: BackgroundMusicPlayerThread start...");
    u64 lastDiagLog = 0;
    while (!this->terminateFlag)
    {
        // PS3: High precision calculation using microseconds
        u64 curUs = sys_time_get_system_time() / 1000;
        i32 targetSamples = (i32)((curUs - this->ps3_startUs) * 441 / 10000) - (i32)this->ps3_samplesSent;

        if (curUs - lastDiagLog > 1000000) {
            utils::Log("SoundPlayer: Thread alive, curUs=%llu, samplesSent=%llu, targetSamples=%d", curUs, this->ps3_samplesSent, targetSamples);
            lastDiagLog = curUs;
        }

        // Safety reset if we drift too far (1 second = 44100 samples)
        if (targetSamples < -44100 || targetSamples > 44100) {
            utils::Log("SoundPlayer: Sync lost, resetting. targetSamples=%d", targetSamples);
            this->ps3_startUs = curUs;
            this->ps3_samplesSent = 0;
            targetSamples = 0;
        }
        
        while (targetSamples >= 256)
        {
            if (this->MixAudio(256 * 2) == 0) {
                this->ps3_samplesSent += 256;
                targetSamples -= 256;
            } else {
                break;
            }
        }
        sys_timer_usleep(1000);
    }
#else
    SDL_PauseAudioDevice(this->audioDev, 0);

    u32 latencyLimit = 14700; // ~5 frames
    u64 samplesSent = 0;
    u64 startTick = SDL_GetTicks64();

    while (!this->terminateFlag)
    {
        u64 curTicks = SDL_GetTicks64();

        // Keep slightly more than 1 frame's worth of samples in the audio buffer at all times
        i32 targetSamples = (curTicks - startTick) * 44.100 - samplesSent + 1024;

        // Quick and dirty checks to keep audio latency low
        if (SDL_GetQueuedAudioSize(this->audioDev) > latencyLimit)
        {
            latencyLimit += 2940; // 1 frame
            samplesSent += targetSamples;
            targetSamples = 0;
        }
        else if (targetSamples > 1024)
        {
            samplesSent += targetSamples - 1024;
            targetSamples = 1024;
        }

        if (targetSamples > 0)
        {
            this->MixAudio(targetSamples * 2);
            samplesSent += targetSamples;
        }

        SDL_Delay(5);
    }
#endif
}
