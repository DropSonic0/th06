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
#ifdef __PS3__
#define PS3_NATIVE_SAMPLE_RATE 48000
#endif
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
    sys_mutex_attribute_t attr;
    sys_mutex_attribute_initialize(attr);
    attr.attr_recursive = SYS_SYNC_RECURSIVE;

    sys_mutex_create(&this->soundBufMutex, &attr);
    sys_mutex_create(&this->bgmIoMutex, &attr);
    sys_mutex_create(&this->bgmStateMutex, &attr);
    this->audioPortNum = 0xFFFFFFFF;
    this->backgroundMusic.streamCache = NULL;
    this->backgroundMusic.srcWav.fileStream = NULL;
    this->isLooping = true;
#endif
}

#ifdef __PS3__
static void ps3_audio_thread(uint64_t arg)
{
    SoundPlayer *player = (SoundPlayer *)(uintptr_t)arg;
    player->BackgroundMusicPlayerThread();
    sys_ppu_thread_exit(0);
}

static void ps3_bgm_io_thread(uint64_t arg)
{
    SoundPlayer *player = (SoundPlayer *)(uintptr_t)arg;
    while (!player->terminateFlag) {
        sys_mutex_lock(player->bgmIoMutex, 0);
        if (player->backgroundMusic.srcWav.fileStream) {
            for (int b = 0; b < 2; b++) {
                bool needsFill = false;
                sys_mutex_lock(player->bgmStateMutex, 0);
                needsFill = player->backgroundMusic.bufferBusy[b];
                sys_mutex_unlock(player->bgmStateMutex);

                if (needsFill) {
                    i16* bufPtr = player->backgroundMusic.streamCache + (b * player->backgroundMusic.streamCacheSize * 2);
                    u32 totalFramesToRead = player->backgroundMusic.streamCacheSize;
                    u32 framesRead = 0;

                    while (framesRead < totalFramesToRead) {
                        long currentFilePos = ftell(player->backgroundMusic.srcWav.fileStream);
                        long loopEndPos = (long)(player->backgroundMusic.srcWav.dataStartOffset + player->backgroundMusic.loopEnd * 4);
                        
                        if (currentFilePos >= loopEndPos) {
                            if (player->isLooping) {
                                fseek(player->backgroundMusic.srcWav.fileStream, player->backgroundMusic.srcWav.dataStartOffset + player->backgroundMusic.loopStart * 4, SEEK_SET);
                                currentFilePos = ftell(player->backgroundMusic.srcWav.fileStream);
                            } else {
                                break;
                            }
                        }

                        u32 remainingInLoop = (u32)(loopEndPos - currentFilePos) / 4;
                        u32 batch = (totalFramesToRead - framesRead < remainingInLoop) ? (totalFramesToRead - framesRead) : remainingInLoop;

                        if (batch > 0) {
                            size_t r = fread(bufPtr + framesRead * 2, 4, batch, player->backgroundMusic.srcWav.fileStream);
                            if (r > 0) {
                                for (size_t k = 0; k < r * 2; k++) {
                                    u16 val = (u16)bufPtr[framesRead * 2 + k];
                                    bufPtr[framesRead * 2 + k] = (i16)((val << 8) | (val >> 8));
                                }
                                framesRead += r;
                            } else {
                                // Error or EOF
                                break;
                            }
                        } else {
                            // Should not happen if loopEnd > loopStart
                            break;
                        }
                    }
                    
                    sys_mutex_lock(player->bgmStateMutex, 0);
                    player->backgroundMusic.streamCacheValid[b] = framesRead;
                    player->backgroundMusic.bufferBusy[b] = false;
                    sys_mutex_unlock(player->bgmStateMutex);
                }
            }
        }
        sys_mutex_unlock(player->bgmIoMutex);
        sys_timer_usleep(2000);
    }
    sys_ppu_thread_exit(0);
}
#endif

ZunResult SoundPlayer::InitializeDSound()
{
    int res;
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
    portParam.nBlock = 32;
    portParam.attr = CELL_AUDIO_PORTATTR_INITLEVEL;
    portParam.level = 1.0f;

    utils::Log("SoundPlayer: cellAudioPortOpen (nBlock=%d, attr=0x%llx)...", (int)portParam.nBlock, (unsigned long long)portParam.attr);
    res = cellAudioPortOpen(&portParam, &this->audioPortNum);
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
    this->ps3_startUs = sys_time_get_system_time();
    this->ps3_samplesSent = 0;
    this->backgroundMusic.streamCacheSize = 8192; // 8k frames (32KB) per buffer
    this->backgroundMusic.streamCache = new i16[this->backgroundMusic.streamCacheSize * 2 * 2]; // Double buffer
    this->backgroundMusic.activeBuffer = 0;
    this->backgroundMusic.bufferBusy[0] = false;
    this->backgroundMusic.bufferBusy[1] = false;
    res = sys_ppu_thread_create(&this->backgroundMusicThreadHandle, ps3_audio_thread, (uint64_t)this, 500, 128 * 1024, SYS_PPU_THREAD_CREATE_JOINABLE, "SoundThread");
    if (res != CELL_OK)
    {
        utils::Log("SoundPlayer: sys_ppu_thread_create failed: 0x%08x", res);
        goto fail;
    }
    res = sys_ppu_thread_create(&this->bgmIoThreadHandle, ps3_bgm_io_thread, (uint64_t)this, 1000, 128 * 1024, SYS_PPU_THREAD_CREATE_JOINABLE, "BgmIoThread");
    if (res != CELL_OK)
    {
        utils::Log("SoundPlayer: sys_ppu_thread_create (IO) failed: 0x%08x", res);
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
    sys_ppu_thread_join(this->bgmIoThreadHandle, &exitCode);
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

    if (this->backgroundMusic.streamCache != NULL) {
        delete[] this->backgroundMusic.streamCache;
        this->backgroundMusic.streamCache = NULL;
    }
    sys_mutex_destroy(this->bgmStateMutex);
    sys_mutex_destroy(this->bgmIoMutex);
    sys_mutex_destroy(this->soundBufMutex);
#endif

    return ZUN_SUCCESS;
}

void SoundPlayer::StopBGM()
{
#ifdef __PS3__
    // Enforce consistent lock order: bgmIoMutex -> soundBufMutex -> bgmStateMutex
    sys_mutex_lock(this->bgmIoMutex, 0);
    sys_mutex_lock(this->soundBufMutex, 0);
    sys_mutex_lock(this->bgmStateMutex, 0);
#endif
    this->StopBGM_NoLock();
#ifdef __PS3__
    sys_mutex_unlock(this->bgmStateMutex);
    sys_mutex_unlock(this->soundBufMutex);
    sys_mutex_unlock(this->bgmIoMutex);
#endif
}

void SoundPlayer::StopBGM_NoLock()
{
#ifndef __PS3__
    if (this->backgroundMusic.srcWav.fileStream != NULL)
    {
        SDL_RWclose(this->backgroundMusic.srcWav.fileStream);
        this->backgroundMusic.srcWav.fileStream = NULL;
        utils::DebugPrint2("stop BGM\n");
    }
#else
    // NOTE: bgmIoMutex must be held by caller (StopBGM or LoadWav)
    if (this->backgroundMusic.srcWav.fileStream != NULL) {
        fclose(this->backgroundMusic.srcWav.fileStream);
        this->backgroundMusic.srcWav.fileStream = NULL;
    }
    utils::DebugPrint2("stop BGM\n");
#endif
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
    int res = 0;

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

#ifdef __PS3__
    sys_mutex_lock(this->bgmIoMutex, 0);
    sys_mutex_lock(this->soundBufMutex, 0);
    sys_mutex_lock(this->bgmStateMutex, 0);
#endif

    this->StopBGM_NoLock();

#ifdef __PS3__
    sys_mutex_unlock(this->bgmStateMutex);
    sys_mutex_unlock(this->soundBufMutex);
#endif

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
        goto fail;
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
    if (fmtSize < 16) goto fail;

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
    // if (byteRate != BACKGROUND_MUSIC_WAV_BYTE_RATE) goto fail;

    uint16_t blockAlign; fread(&blockAlign, 2, 1, fileStream);
    blockAlign = utils::Swap16(blockAlign);
    if (blockAlign != BACKGROUND_MUSIC_WAV_BLOCK_ALIGN) goto fail;

    uint16_t bitsPerSample; fread(&bitsPerSample, 2, 1, fileStream);
    bitsPerSample = utils::Swap16(bitsPerSample);
    if (bitsPerSample != BACKGROUND_MUSIC_WAV_BITS_PER_SAMPLE) goto fail;

    if (fmtSize > 16) fseek(fileStream, fmtSize - 16, SEEK_CUR);

    // Skip any other chunks before "data"
    while (true) {
        if (fread(idBuf, 4, 1, fileStream) != 1) {
            utils::Log("SoundPlayer: LoadWav FAILED: reached EOF while looking for 'data' chunk");
            goto fail;
        }
        uint32_t chunkSize;
        if (fread(&chunkSize, 4, 1, fileStream) != 1) goto fail;
        chunkSize = utils::Swap32(chunkSize);

        if (std::strncmp(idBuf, "data", 4) == 0) {
            wavDataSize = chunkSize;
            break;
        }

        utils::Log("SoundPlayer: LoadWav %s: skipping chunk '%.4s' size %u", path, idBuf, chunkSize);
        fseek(fileStream, chunkSize + (chunkSize & 1), SEEK_CUR);
        if (ftell(fileStream) >= (long)fileSize) {
             utils::Log("SoundPlayer: LoadWav FAILED: reached EOF after skipping chunk");
             goto fail;
        }
    }
#endif

    utils::Log("SoundPlayer: LoadWav %s: wavDataSize=%u, riffSize=%u", path, wavDataSize, riffSize);

    if (wavDataSize > riffSize - 44)
    {
        utils::Log("SoundPlayer: LoadWav FAILED: wavDataSize > riffSize - 44");
        goto fail;
    }

    this->backgroundMusic.srcWav.samples = wavDataSize / BACKGROUND_MUSIC_WAV_BLOCK_ALIGN;
    utils::Log("SoundPlayer: LoadWav %s: samples=%u, dataStartOffset=%u", path, this->backgroundMusic.srcWav.samples, (u32)ftell(fileStream));

    if (this->backgroundMusic.srcWav.samples == 0)
    {
        goto fail;
    }

#ifndef __PS3__
    this->backgroundMusic.srcWav.fileStream = fileStream;
    this->backgroundMusic.srcWav.dataStartOffset = SDL_RWtell(fileStream);
#else
    this->backgroundMusic.srcWav.dataStartOffset = ftell(fileStream);
    this->backgroundMusic.streamCachePos = 0;
    this->backgroundMusic.streamCacheValid[0] = 0;
    this->backgroundMusic.streamCacheValid[1] = 0;
    this->backgroundMusic.activeBuffer = 0;
    this->backgroundMusic.bufferBusy[0] = true;
    this->backgroundMusic.bufferBusy[1] = true;
    this->backgroundMusic.fraction = 1.0;
    this->backgroundMusic.lastSamples[0] = 0;
    this->backgroundMusic.lastSamples[1] = 0;
    this->backgroundMusic.nextSamples[0] = 0;
    this->backgroundMusic.nextSamples[1] = 0;
#endif
    this->backgroundMusic.loopStart = 0;
    this->backgroundMusic.loopEnd = this->backgroundMusic.srcWav.samples;
    this->backgroundMusic.fadeoutLen = 0;
    this->backgroundMusic.fadeoutProgress = 0;
    this->backgroundMusic.pos = 0;
    this->isLooping = true;

#ifdef __PS3__
    sys_mutex_lock(this->soundBufMutex, 0);
    sys_mutex_lock(this->bgmStateMutex, 0);
    this->ps3_startUs = sys_time_get_system_time();
    this->ps3_samplesSent = 0;
#endif
    this->backgroundMusic.srcWav.fileStream = fileStream;
#ifdef __PS3__
    sys_mutex_unlock(this->bgmStateMutex);
    sys_mutex_unlock(this->soundBufMutex);
    sys_mutex_unlock(this->bgmIoMutex);
#endif
    return ZUN_SUCCESS;

fail:
#ifdef __PS3__
    utils::Log("SoundPlayer: Failed to LoadWav %s (check WAV format?)", path);
#endif
#ifndef __PS3__
    if (fileStream) SDL_RWclose(fileStream);
#else
    if (fileStream) fclose(fileStream);
#endif
    sys_mutex_unlock(this->bgmIoMutex);
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

    utils::Log("SoundPlayer: LoadPos %s: loopStart=%u, loopEnd=%u (max samples=%u)", path, this->backgroundMusic.loopStart, this->backgroundMusic.loopEnd, this->backgroundMusic.srcWav.samples);

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
    u32 sfxWavDataSize = 0;
    u32 sfxWavDataOffset = 0;
    u32 sfxSampleRate = 22050;
    u16 sfxChannels = 1;
    u32 srcSamples;
    double ratio;
    u8 *wavRawData;
    //soundplayerdlog("load sound 1");
#ifdef __PS3__
    sys_mutex_lock(this->soundBufMutex, 0);
#endif
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
    // Find "fmt " and "data" chunks for SFX
    u16 sfxBitsPerSample = 16;
    if (std::strncmp((char*)wavRawData, "RIFF", 4) == 0 && std::strncmp((char*)wavRawData + 8, "WAVE", 4) == 0) {
        u32 offset = 12;
        while (offset + 8 <= g_LastFileSize) {
            u32 chunkSize;
            memcpy(&chunkSize, wavRawData + offset + 4, 4);
            chunkSize = utils::Swap32(chunkSize);
            if (std::strncmp((char*)wavRawData + offset, "fmt ", 4) == 0) {
                memcpy(&sfxChannels, wavRawData + offset + 8 + 2, 2);
                sfxChannels = utils::Swap16(sfxChannels);
                memcpy(&sfxSampleRate, wavRawData + offset + 8 + 4, 4);
                sfxSampleRate = utils::Swap32(sfxSampleRate);
                memcpy(&sfxBitsPerSample, wavRawData + offset + 8 + 14, 2);
                sfxBitsPerSample = utils::Swap16(sfxBitsPerSample);
            }
            if (std::strncmp((char*)wavRawData + offset, "data", 4) == 0) {
                sfxWavDataSize = chunkSize;
                sfxWavDataOffset = offset + 8;
                break;
            }
            offset += 8 + (chunkSize + (chunkSize & 1));
        }
    }

    if (sfxWavDataOffset == 0) {
        utils::Log("SoundPlayer: LoadSound %d FAILED to find 'data' chunk in %s", idx, path);
        goto fail;
    }

    utils::Log("SoundPlayer: SFX %s: rate=%u, chan=%u, bits=%u", path, sfxSampleRate, sfxChannels, sfxBitsPerSample);

    srcSamples = sfxWavDataSize / (sfxChannels * (sfxBitsPerSample / 8));
    ratio = (double)PS3_NATIVE_SAMPLE_RATE / (double)sfxSampleRate;
    this->soundBuffers[idx].len = (u32)(srcSamples * ratio) * 2; // Stereo
    this->soundBuffers[idx].samples = new i16[this->soundBuffers[idx].len];
    
    for (u32 i = 0; i < this->soundBuffers[idx].len / 2; i++) {
        double srcPos = (double)i / ratio;
        u32 i0 = (u32)srcPos;
        u32 i1 = (i0 + 1 < srcSamples) ? i0 + 1 : i0;
        double frac = srcPos - i0;
        
        for (int ch = 0; ch < 2; ch++) {
            int srcCh = (sfxChannels == 2) ? ch : 0;
            i16 s0, s1;
            if (sfxBitsPerSample == 8) {
                s0 = (i16)((((i32)wavRawData[sfxWavDataOffset + i0 * sfxChannels + srcCh]) - 128) << 8);
                s1 = (i16)((((i32)wavRawData[sfxWavDataOffset + i1 * sfxChannels + srcCh]) - 128) << 8);
            } else {
                memcpy(&s0, wavRawData + sfxWavDataOffset + (i0 * sfxChannels + srcCh) * 2, 2);
                memcpy(&s1, wavRawData + sfxWavDataOffset + (i1 * sfxChannels + srcCh) * 2, 2);
                s0 = utils::Swap16(s0);
                s1 = utils::Swap16(s1);
            }
            this->soundBuffers[idx].samples[i * 2 + ch] = (i16)(s0 + (i32)(s1 - s0) * frac);
        }
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
#ifdef __PS3__
    sys_mutex_unlock(this->soundBufMutex);
#endif
    free(wavRawData);
    return ZUN_SUCCESS;

fail:
#ifdef __PS3__
    sys_mutex_unlock(this->soundBufMutex);
#endif
    if (wavRawData) free(wavRawData);
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


#ifndef __PS3__
    if (this->audioDev == 0 || !g_Supervisor.cfg.playSounds)
#else
    if (this->audioPortNum == 0xFFFFFFFF || !g_Supervisor.cfg.playSounds)
#endif
    {
        return;
    }

#ifdef __PS3__
    sys_mutex_lock(this->soundBufMutex, 0);
#endif

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

#ifdef __PS3__
    sys_mutex_unlock(this->soundBufMutex);
#endif
}

void SoundPlayer::PlaySoundByIdx(SoundIdx idx)
{
    u32 i;

#ifdef __PS3__
    sys_mutex_lock(this->soundBufMutex, 0);
#endif
    for (i = 0; i < ARRAY_SIZE(this->soundBuffersToPlay); i++)
    {
        if (this->soundBuffersToPlay[i] < 0)
        {
            break;
        }

        if (this->soundBuffersToPlay[i] == idx)
        {
#ifdef __PS3__
            sys_mutex_unlock(this->soundBufMutex);
#endif
            return;
        }
    }

    if (i >= 3)
    {
#ifdef __PS3__
        sys_mutex_unlock(this->soundBufMutex);
#endif
        return;
    }

    this->soundBuffersToPlay[i] = idx;
#ifdef __PS3__
    sys_mutex_unlock(this->soundBufMutex);
#endif
}

int SoundPlayer::MixAudio(u32 samples)
{
    u32 samplesMixed = 0;
    u8 playingChannels = 0;
    int i;
    f32 fadeoutMult;

#ifndef __PS3__
    std::vector<i16> finalBuffer(samples);
    std::vector<i32> mixBuffer(samples);
#else
    static i32 mixBuffer[2048] __attribute__((aligned(16)));
    u32 savedSoundPos[128];
    bool savedSoundPlaying[128];
    u32 savedBgmPos;
    double savedBgmFraction;
    u32 savedCachePos;
    u32 savedCacheValid[2];
    u32 savedActiveBuffer;
    i16 savedLastSamples[2];
    i16 savedNextSamples[2];
    int res_add;

    if (samples > 2048) samples = 2048;
    memset(mixBuffer, 0, samples * sizeof(i32));
#endif

    bool bgmActive = false;
#ifdef __PS3__
    // Quick check without full IO lock
    bgmActive = (this->backgroundMusic.srcWav.fileStream != NULL);
    sys_mutex_lock(this->soundBufMutex, 0);

    // Save state for rollback - must be done INSIDE the lock
    for(i=0; i<ARRAY_SIZE_SIGNED(this->soundBuffers); i++) {
        savedSoundPos[i] = this->soundBuffers[i].pos;
        savedSoundPlaying[i] = this->soundBuffers[i].isPlaying;
    }
    savedBgmPos = this->backgroundMusic.pos;
    savedBgmFraction = this->backgroundMusic.fraction;
    savedCachePos = this->backgroundMusic.streamCachePos;
    savedCacheValid[0] = this->backgroundMusic.streamCacheValid[0];
    savedCacheValid[1] = this->backgroundMusic.streamCacheValid[1];
    savedActiveBuffer = this->backgroundMusic.activeBuffer;
    savedLastSamples[0] = this->backgroundMusic.lastSamples[0];
    savedLastSamples[1] = this->backgroundMusic.lastSamples[1];
    savedNextSamples[0] = this->backgroundMusic.nextSamples[0];
    savedNextSamples[1] = this->backgroundMusic.nextSamples[1];
#endif

    for (i = 0; i < ARRAY_SIZE_SIGNED(this->soundBuffers); i++)
    {
        if (!this->soundBuffers[i].isPlaying) continue;
        playingChannels++;

#ifdef __PS3__
        {
            const u32 framesToMix = std::min(samples / 2, (this->soundBuffers[i].len - this->soundBuffers[i].pos) / 2);
            for (u32 j = 0; j < framesToMix; j++) {
                mixBuffer[j * 2] += this->soundBuffers[i].samples[this->soundBuffers[i].pos + j * 2];
                mixBuffer[j * 2 + 1] += this->soundBuffers[i].samples[this->soundBuffers[i].pos + j * 2 + 1];
            }
            this->soundBuffers[i].pos += framesToMix * 2;
            if (this->soundBuffers[i].pos >= this->soundBuffers[i].len) this->soundBuffers[i].isPlaying = false;
        }
#else
        {
            const u32 samplesToMix = std::min(samples / 2, this->soundBuffers[i].len - this->soundBuffers[i].pos);
            for (u32 j = 0; j < samplesToMix; j++) {
                mixBuffer[j * 2] += this->soundBuffers[i].samples[this->soundBuffers[i].pos + j];
                mixBuffer[j * 2 + 1] += this->soundBuffers[i].samples[this->soundBuffers[i].pos + j];
            }
            this->soundBuffers[i].pos += samplesToMix;
            if (this->soundBuffers[i].pos == this->soundBuffers[i].len) this->soundBuffers[i].isPlaying = false;
        }
#endif
    }

#ifndef __PS3__
    if (this->backgroundMusic.srcWav.fileStream != NULL)
    {
        bgmActive = true;
    }
#endif

    // On PS3, double check that we actually have valid data in at least one buffer
    if (bgmActive)
    {
        if (this->backgroundMusic.fadeoutLen != 0) {
            f32 fadeoutInterp = mapRange(this->backgroundMusic.fadeoutProgress, 0, this->backgroundMusic.fadeoutLen, 0, 5);
            fadeoutMult = 1.0f / ZUN_POWF(10.0f, fadeoutInterp / 2.0f);
        } else {
            fadeoutMult = 1.0f;
        }

        while (samplesMixed < samples / 2)
        {
#ifndef __PS3__
            const u32 samplesToMix = std::min((samples / 2) - samplesMixed, this->backgroundMusic.loopEnd - this->backgroundMusic.pos);
            if (samplesToMix == 0) {
                if (this->isLooping) {
                    this->backgroundMusic.pos = this->backgroundMusic.loopStart;
                    SDL_RWseek(this->backgroundMusic.srcWav.fileStream, this->backgroundMusic.srcWav.dataStartOffset + this->backgroundMusic.pos * 4, SEEK_SET);
                    continue;
                } else {
                    this->StopBGM_NoLock();
                    break;
                }
            }
            for (u32 j = 0; j < samplesToMix; j++) {
                mixBuffer[(samplesMixed + j) * 2] += ((i16)SDL_ReadLE16(this->backgroundMusic.srcWav.fileStream)) * fadeoutMult;
                mixBuffer[(samplesMixed + j) * 2 + 1] += ((i16)SDL_ReadLE16(this->backgroundMusic.srcWav.fileStream)) * fadeoutMult;
            }
            this->backgroundMusic.pos += samplesToMix;
            samplesMixed += samplesToMix;
#else
            {
                const double ratio = (double)BACKGROUND_MUSIC_WAV_SAMPLE_RATE / (double)PS3_NATIVE_SAMPLE_RATE;
                
                while (this->backgroundMusic.fraction >= 1.0) {
                    this->backgroundMusic.lastSamples[0] = this->backgroundMusic.nextSamples[0];
                    this->backgroundMusic.lastSamples[1] = this->backgroundMusic.nextSamples[1];

                    u32 activeBuf;
                    sys_mutex_lock(this->bgmStateMutex, 0);
                    activeBuf = this->backgroundMusic.activeBuffer;
                    if (this->backgroundMusic.streamCache && this->backgroundMusic.streamCachePos >= this->backgroundMusic.streamCacheValid[activeBuf]) {
                        // Current buffer exhausted, try switching
                        u32 nextBuf = 1 - activeBuf;
                        if (!this->backgroundMusic.bufferBusy[nextBuf] && this->backgroundMusic.streamCacheValid[nextBuf] > 0) {
                            this->backgroundMusic.activeBuffer = nextBuf;
                            activeBuf = nextBuf;
                            this->backgroundMusic.streamCachePos = 0;
                        }
                    }
                    
                    if (this->backgroundMusic.streamCache && this->backgroundMusic.streamCachePos < this->backgroundMusic.streamCacheValid[activeBuf]) {
                        i16* currentBuf = this->backgroundMusic.streamCache + (activeBuf * this->backgroundMusic.streamCacheSize * 2);
                        this->backgroundMusic.nextSamples[0] = currentBuf[this->backgroundMusic.streamCachePos * 2];
                        this->backgroundMusic.nextSamples[1] = currentBuf[this->backgroundMusic.streamCachePos * 2 + 1];
                        this->backgroundMusic.streamCachePos++;

                        if (this->backgroundMusic.streamCachePos >= this->backgroundMusic.streamCacheValid[activeBuf]) {
                            this->backgroundMusic.bufferBusy[activeBuf] = true;
                        }
                        sys_mutex_unlock(this->bgmStateMutex);
                        this->backgroundMusic.pos++;
                        if (this->isLooping && this->backgroundMusic.pos >= this->backgroundMusic.loopEnd) {
                            this->backgroundMusic.pos = this->backgroundMusic.loopStart;
                        }
                    } else {
                        sys_mutex_unlock(this->bgmStateMutex);
                        if (!this->isLooping && this->backgroundMusic.pos >= this->backgroundMusic.loopEnd) {
                            // StopBGM already handles its own locking safely
                            sys_mutex_unlock(this->soundBufMutex);
                            this->StopBGM();
                            sys_mutex_lock(this->soundBufMutex, 0);
                            goto bgm_done;
                        }
                        // If looping or just ran out of cache, we just have to wait for IO thread to fill buffers.
                        // We break and wait for next MixAudio call.
                        goto bgm_done;
                    }

                    this->backgroundMusic.fraction -= 1.0;
                }
                
                mixBuffer[samplesMixed * 2] += (i32)((this->backgroundMusic.lastSamples[0] + (i32)(this->backgroundMusic.nextSamples[0] - this->backgroundMusic.lastSamples[0]) * this->backgroundMusic.fraction) * fadeoutMult);
                mixBuffer[samplesMixed * 2 + 1] += (i32)((this->backgroundMusic.lastSamples[1] + (i32)(this->backgroundMusic.nextSamples[1] - this->backgroundMusic.lastSamples[1]) * this->backgroundMusic.fraction) * fadeoutMult);
                
                samplesMixed++;
                this->backgroundMusic.fraction += ratio;
            }
#endif
        }
bgm_done:

        if (this->backgroundMusic.fadeoutLen != 0) {
            this->backgroundMusic.fadeoutProgress += samplesMixed;
            if (this->backgroundMusic.fadeoutProgress >= this->backgroundMusic.fadeoutLen) {
                sys_mutex_unlock(this->soundBufMutex);
                this->StopBGM();
                sys_mutex_lock(this->soundBufMutex, 0);
            }
        }
        playingChannels++;
    }

#ifndef __PS3__
    {
        const int mixDivisor = std::max(8, (int)playingChannels);
        for (u32 i = 0; i < samples; i++) finalBuffer[i] = mixBuffer[i] / mixDivisor;
        SDL_QueueAudio(this->audioDev, finalBuffer.data(), samples * 2);
    }
    return 0;
#else
    {
    // If many channels are playing, we divide to avoid heavy clipping,
    // but we use a smaller divisor than the full channel count to keep volume punchy.
    const int mixDivisor = (playingChannels > 4) ? (playingChannels / 2) : 1;
        static float floatBuffer[2048] __attribute__((aligned(16)));
        for (u32 i = 0; i < samples; i++) {
        floatBuffer[i] = (float)mixBuffer[i] / (mixDivisor * 32768.0f);
            if (floatBuffer[i] > 1.0f) floatBuffer[i] = 1.0f;
            if (floatBuffer[i] < -1.0f) floatBuffer[i] = -1.0f;
        }
        res_add = cellAudioAdd2chData(this->audioPortNum, floatBuffer, samples / 2, 1.0f);
        if (res_add < 0) {
        // soundBufMutex is already locked by caller!
        sys_mutex_lock(this->bgmStateMutex, 0);
            for(i=0; i<ARRAY_SIZE_SIGNED(this->soundBuffers); i++) {
                this->soundBuffers[i].pos = savedSoundPos[i];
                this->soundBuffers[i].isPlaying = savedSoundPlaying[i];
            }
            this->backgroundMusic.pos = savedBgmPos;
            this->backgroundMusic.fraction = savedBgmFraction;
            this->backgroundMusic.streamCachePos = savedCachePos;
            this->backgroundMusic.streamCacheValid[0] = savedCacheValid[0];
            this->backgroundMusic.streamCacheValid[1] = savedCacheValid[1];
            this->backgroundMusic.activeBuffer = savedActiveBuffer;
        // Do not reset bufferBusy here as it's owned by the IO thread/bgmStateMutex logic
            this->backgroundMusic.lastSamples[0] = savedLastSamples[0];
            this->backgroundMusic.lastSamples[1] = savedLastSamples[1];
            this->backgroundMusic.nextSamples[0] = savedNextSamples[0];
            this->backgroundMusic.nextSamples[1] = savedNextSamples[1];
        sys_mutex_unlock(this->bgmStateMutex);
        if (res_add != CELL_AUDIO_ERROR_PORT_FULL) utils::Log("SoundPlayer: cellAudioAdd2chData failed: 0x%08x", res_add);
        sys_mutex_unlock(this->soundBufMutex);
            return res_add;
        }
    }
    sys_mutex_unlock(this->soundBufMutex);
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
    while (!this->terminateFlag)
    {

        // PS3: High precision calculation using microseconds
        u64 curUs = sys_time_get_system_time();
        // 48000 samples per second = 0.048 samples per microsecond
        // targetSamples: (time elapsed * rate) - samples sent + lead buffer (12288 frames)
        long long targetSamples = (long long)((double)(curUs - this->ps3_startUs) * 0.048) - (long long)this->ps3_samplesSent + 12288;

        // Safety reset if we drift too far (1 second = 48000 samples)
        if (targetSamples < -48000 || targetSamples > 48000) {
            utils::Log("SoundPlayer: Sync lost, resetting. targetSamples=%lld", targetSamples);
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
                // If MixAudio fails (e.g. port full), wait a bit and retry
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
