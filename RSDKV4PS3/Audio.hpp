#ifndef AUDIO_H
#define AUDIO_H

#include <stdlib.h>
#include <string.h>

#define TRACK_COUNT (0x10)
#define SFX_COUNT   (0x100)
#if !RETRO_USE_ORIGINAL_CODE
#define CHANNEL_COUNT (0x10) // 4 in the original, 16 for convenience
#else
#define CHANNEL_COUNT (0x4)
#endif

#define MAX_VOLUME (100)

#if RETRO_PLATFORM == RETRO_PS3
#define MUSBUFFER_SIZE   (0x1000000)
#else
#define MUSBUFFER_SIZE   (0x200000)
#endif
#define STREAMFILE_COUNT (4)

#if RETRO_PLATFORM == RETRO_PS3
#define MIX_BUFFER_SAMPLES (2048)
#else
#define MIX_BUFFER_SAMPLES (256)
#endif

#if RETRO_USING_SDL1 || RETRO_USING_SDL2

#define LockAudioDevice()   SDL_LockAudio()
#define UnlockAudioDevice() SDL_UnlockAudio()

#elif RETRO_PLATFORM == RETRO_PS3

#include <sys/synchronization.h>
#include "ogg/ogg.h"
#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"
extern sys_lwmutex_t audioMutex;
#define LockAudioDevice()   sys_lwmutex_lock(&audioMutex, 0xFFFFFFFF)
#define UnlockAudioDevice() sys_lwmutex_unlock(&audioMutex)

#ifndef RETROENGINE_H
// Standard types and macros for Audio.hpp if RetroEngine.hpp isn't included yet
#ifndef BYTE_DEFINED
#define BYTE_DEFINED
typedef unsigned char byte;
#endif
typedef signed char sbyte;
typedef unsigned short ushort;
typedef unsigned int uint;

typedef signed short Sint16;
typedef signed int Sint32;
typedef signed long long Sint64;
typedef unsigned char Uint8;
typedef unsigned short Uint16;
typedef unsigned int Uint32;
typedef unsigned long long Uint64;

#define MEM_ZERO(x)  memset(&(x), 0, sizeof((x)))
#define MEM_ZEROP(x) memset((x), 0, sizeof(*(x)))
#endif

#else
#define LockAudioDevice()   ;
#define UnlockAudioDevice() ;
#endif

#if RETRO_PLATFORM == RETRO_PS3
#define RETRO_AUDIO_ALIGN __attribute__((aligned(16)))
#else
#define RETRO_AUDIO_ALIGN
#endif

struct RETRO_AUDIO_ALIGN TrackInfo {
    char fileName[0x40];
    int trackLoop;
    uint loopPoint;
    byte padding[8]; // Total 80 bytes
};

struct RETRO_AUDIO_ALIGN StreamInfo {
    Sint16 buffer[MIX_BUFFER_SAMPLES]; // Offset 0x0000 (2048 samples = 4096 bytes)
    OggVorbis_File vorbisFile __attribute__((aligned(16))); // Offset 0x1000
    int vorbBitstream; // Offset 0x1290 (4752)
    int trackLoop;
    uint loopPoint;
    int loaded;
#if RETRO_USING_SDL2
    SDL_AudioStream *stream;
#endif
#if RETRO_USING_SDL1
    SDL_AudioSpec spec;
#endif
#if RETRO_PLATFORM == RETRO_PS3
    double ratio;
    double resamplePos;
    Sint16 lastL;
    Sint16 lastR;
    int inputPos;
    int inputCount;
#endif
    byte padding[4724]; // Offset 0x12a0 (4768 + 4724 = 9492 bytes total)
};

struct RETRO_AUDIO_ALIGN SFXInfo {
    char name[0x40];
    Sint16 *buffer;
    size_t length;
    int loaded;
    byte padding[4]; // Make it 80 bytes (multiple of 16)
};

struct RETRO_AUDIO_ALIGN ChannelInfo {
    size_t sampleLength; // Offset 0x00
    Sint16 *samplePtr;   // Offset 0x04
    int sfxID;
    int loopSFX;
    sbyte pan;
    byte padding[15]; // Total 32 bytes on 32-bit (4+4+4+4+1+15=32)
};

struct RETRO_AUDIO_ALIGN StreamFile {
    byte *buffer;
    int vsize;
    int vpos;
    byte padding[4];
};

enum MusicStatuses {
    MUSIC_STOPPED = 0,
    MUSIC_PLAYING = 1,
    MUSIC_PAUSED  = 2,
    MUSIC_LOADING = 3,
    MUSIC_READY   = 4,
};

extern int globalSFXCount;
extern int stageSFXCount;

extern int masterVolume;
extern int trackID;
extern int sfxVolume;
extern int bgmVolume;
extern bool audioEnabled;

extern bool musicEnabled;
extern int musicStatus;
extern int musicStartPos;
extern int musicPosition;
extern int musicRatio;
extern TrackInfo musicTracks[TRACK_COUNT];

extern int currentStreamIndex;
#if RETRO_PLATFORM == RETRO_PS3
extern StreamFile streamFile[STREAMFILE_COUNT] __attribute__((aligned(16)));
extern StreamInfo streamInfo[STREAMFILE_COUNT] __attribute__((aligned(16)));
#else
extern StreamFile streamFile[STREAMFILE_COUNT];
extern StreamInfo streamInfo[STREAMFILE_COUNT];
#endif
extern StreamFile *streamFilePtr;
extern StreamInfo *streamInfoPtr;

#if RETRO_PLATFORM == RETRO_PS3
extern SFXInfo sfxList[SFX_COUNT] __attribute__((aligned(16)));
#else
extern SFXInfo sfxList[SFX_COUNT];
#endif
extern char sfxNames[SFX_COUNT][0x40];

#if RETRO_PLATFORM == RETRO_PS3
extern ChannelInfo sfxChannels[CHANNEL_COUNT] __attribute__((aligned(16)));
#else
extern ChannelInfo sfxChannels[CHANNEL_COUNT];
#endif

#if RETRO_USING_SDL1 || RETRO_USING_SDL2
extern SDL_AudioSpec audioDeviceFormat;
#endif

int InitAudioPlayback();
void LoadGlobalSfx();

#if !RETRO_USE_ORIGINAL_CODE
// These functions did exist, but with different signatures
void ProcessMusicStream(Sint32 *stream, size_t bytes_wanted);
#if RETRO_USING_SDL1 || RETRO_USING_SDL2
void ProcessAudioPlayback(void *data, Uint8 *stream, int len);
#else
void ProcessAudioPlayback(Sint16 *stream, int sampleCount);
#endif
void ProcessAudioMixing(Sint32 *dst, const Sint16 *src, int len, int volume, sbyte pan);
#endif

#if !RETRO_USE_ORIGINAL_CODE
inline void FreeMusInfo(bool Lock = true)
{
    if (Lock) LockAudioDevice();

#if RETRO_USING_SDL2
    if (streamInfo[currentStreamIndex].stream)
        SDL_FreeAudioStream(streamInfo[currentStreamIndex].stream);
    streamInfo[currentStreamIndex].stream = NULL;
#endif

    if (streamInfo[currentStreamIndex].loaded) {
        ov_clear(&streamInfo[currentStreamIndex].vorbisFile);
        streamInfo[currentStreamIndex].loaded = false;
    }

#if RETRO_USING_SDL2
    streamInfo[currentStreamIndex].stream = nullptr;
#endif

    if (Lock) UnlockAudioDevice();
}
#endif

void LoadMusic(void *userdata);
void SetMusicTrack(const char *filePath, byte trackID, bool loop, uint loopPoint);
void SwapMusicTrack(const char *filePath, byte trackID, uint loopPoint, uint ratio);
bool PlayMusic(int track, int musStartPos);
inline void StopMusic(bool setStatus)
{
    if (setStatus)
        musicStatus = MUSIC_STOPPED;
    musicPosition = 0;

#if !RETRO_USE_ORIGINAL_CODE
    FreeMusInfo(true);
#endif
}

void LoadSfx(char *filePath, byte sfxID);
void PlaySfx(int sfx, bool loop);
inline void StopSfx(int sfx)
{
    LockAudioDevice();
    for (int i = 0; i < CHANNEL_COUNT; ++i) {
        if (sfxChannels[i].sfxID == sfx) {
            MEM_ZERO(sfxChannels[i]);
            sfxChannels[i].sfxID = -1;
        }
    }
    UnlockAudioDevice();
}
void SetSfxAttributes(int sfx, int loopCount, sbyte pan);

void SetSfxName(const char *sfxName, int sfxID);

#if !RETRO_USE_ORIGINAL_CODE
// Helper Funcs
inline bool PlaySfxByName(const char *sfx, sbyte loopCnt)
{
    char buffer[0x40];
    int pos = 0;
    while (*sfx) {
        if (*sfx != ' ')
            buffer[pos++] = *sfx;
        sfx++;
    }
    buffer[pos] = 0;

    for (int s = 0; s < globalSFXCount + stageSFXCount; ++s) {
        if (StrComp(sfxNames[s], buffer)) {
            PlaySfx(s, loopCnt);
            return true;
        }
    }
    return false;
}
inline bool StopSFXByName(const char *sfx)
{
    char buffer[0x40];
    int pos = 0;
    while (*sfx) {
        if (*sfx != ' ')
            buffer[pos++] = *sfx;
        sfx++;
    }
    buffer[pos] = 0;

    for (int s = 0; s < globalSFXCount + stageSFXCount; ++s) {
        if (StrComp(sfxNames[s], buffer)) {
            StopSfx(s);
            return true;
        }
    }
    return false;
}
#endif

inline void SetMusicVolume(int volume)
{
    if (volume < 0)
        volume = 0;
    if (volume > MAX_VOLUME)
        volume = MAX_VOLUME;

    masterVolume = volume;
}

inline void SetGameVolumes(int bgmVol, int sfxVol)
{
    bgmVolume = bgmVol;
    sfxVolume = sfxVol;

    if (bgmVolume < 0)
        bgmVolume = 0;
    if (bgmVolume > MAX_VOLUME)
        bgmVolume = MAX_VOLUME;

    if (sfxVolume < 0)
        sfxVolume = 0;
    if (sfxVolume > MAX_VOLUME)
        sfxVolume = MAX_VOLUME;
}

inline bool PauseSound()
{
    if (musicStatus == MUSIC_PLAYING) {
        musicStatus = MUSIC_PAUSED;
        return true;
    }
    return false;
}

inline void ResumeSound()
{
    if (musicStatus == MUSIC_PAUSED)
        musicStatus = MUSIC_PLAYING;
}

inline void StopAllSfx()
{
#if !RETRO_USE_ORIGINAL_CODE
    LockAudioDevice();
#endif

    for (int i = 0; i < CHANNEL_COUNT; ++i) {
        sfxChannels[i].sfxID = -1;
        sfxChannels[i].samplePtr = NULL;
        sfxChannels[i].sampleLength = 0;
    }

#if !RETRO_USE_ORIGINAL_CODE
    UnlockAudioDevice();
#endif
}
inline void ReleaseGlobalSfx()
{
    LockAudioDevice();
    for (int i = 0; i < CHANNEL_COUNT; ++i) {
        if (sfxChannels[i].sfxID >= 0 && sfxChannels[i].sfxID < globalSFXCount) {
            sfxChannels[i].sfxID = -1;
            sfxChannels[i].samplePtr = NULL;
            sfxChannels[i].sampleLength = 0;
        }
    }

    for (int i = globalSFXCount - 1; i >= 0; --i) {
        if (sfxList[i].loaded) {
            StrCopy(sfxList[i].name, "");
            StrCopy(sfxNames[i], "");
            if (sfxList[i].buffer)
                free(sfxList[i].buffer);

            sfxList[i].buffer = NULL;
            sfxList[i].length = 0;
            sfxList[i].loaded = false;
        }
    }

    globalSFXCount = 0;
    UnlockAudioDevice();
}
inline void ReleaseStageSfx()
{
    LockAudioDevice();
    for (int i = 0; i < CHANNEL_COUNT; ++i) {
        if (sfxChannels[i].sfxID >= globalSFXCount) {
            sfxChannels[i].sfxID = -1;
            sfxChannels[i].samplePtr = NULL;
            sfxChannels[i].sampleLength = 0;
        }
    }

    for (int i = (stageSFXCount + globalSFXCount) - 1; i >= globalSFXCount; --i) {
        if (sfxList[i].loaded) {
            StrCopy(sfxList[i].name, "");
            StrCopy(sfxNames[i], "");
            if (sfxList[i].buffer)
                free(sfxList[i].buffer);

            sfxList[i].buffer = NULL;
            sfxList[i].length = 0;
            sfxList[i].loaded = false;
        }
    }

    stageSFXCount = 0;
    UnlockAudioDevice();
}

inline void ReleaseAudioDevice()
{
    StopMusic(true);
    StopAllSfx();
    ReleaseStageSfx();
    ReleaseGlobalSfx();

    LockAudioDevice();
    for (int i = 0; i < STREAMFILE_COUNT; i++) {
        if (streamFile[i].buffer) free(streamFile[i].buffer);
        streamFile[i].buffer = NULL;
    }
    UnlockAudioDevice();
}

#endif // !AUDIO_H