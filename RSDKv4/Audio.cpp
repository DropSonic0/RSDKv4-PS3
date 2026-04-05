#include "RetroEngine.hpp"
#include <cmath>

int globalSFXCount = 0;
int stageSFXCount  = 0;

int masterVolume  = MAX_VOLUME;
int trackID       = -1;
int sfxVolume     = MAX_VOLUME;
int bgmVolume     = MAX_VOLUME;
bool audioEnabled = false;

bool musicEnabled = 0;
int musicStatus   = MUSIC_STOPPED;
int musicStartPos = 0;
int musicPosition = 0;
int musicRatio    = 0;
uint64_t musicLoadStartTime = 0;

#if RETRO_PLATFORM == RETRO_PS3
TrackInfo musicTracks[TRACK_COUNT] __attribute__((aligned(16)));
char sfxNames[SFX_COUNT][0x40] __attribute__((aligned(16)));
StreamFile streamFile[STREAMFILE_COUNT] __attribute__((aligned(16)));
StreamInfo streamInfo[STREAMFILE_COUNT] __attribute__((aligned(16)));
SFXInfo sfxList[SFX_COUNT] __attribute__((aligned(16)));
ChannelInfo sfxChannels[CHANNEL_COUNT] __attribute__((aligned(16)));
#else
TrackInfo musicTracks[TRACK_COUNT];
char sfxNames[SFX_COUNT][0x40];
StreamFile streamFile[STREAMFILE_COUNT];
StreamInfo streamInfo[STREAMFILE_COUNT];
SFXInfo sfxList[SFX_COUNT];
ChannelInfo sfxChannels[CHANNEL_COUNT];
#endif

int currentStreamIndex = 0;
StreamFile *streamFilePtr = NULL;
StreamInfo *streamInfoPtr = NULL;

int currentMusicTrack = -1;
int nextMusicTrack    = -1;
int nextMusicStartPos = 0;

static float tvHumPhase = 0.0f;
static uint32_t tvNoiseSeed = 0x12345678;

#if RETRO_USING_SDL1 || RETRO_USING_SDL2

#if RETRO_USING_SDL2
SDL_AudioDeviceID audioDevice;
#endif
SDL_AudioSpec audioDeviceFormat;

#define AUDIO_FREQUENCY (44100)
#define AUDIO_FORMAT    (AUDIO_S16SYS) /**< Signed 16-bit samples */
#define AUDIO_SAMPLES   (0x800)
#define AUDIO_CHANNELS  (2)

#elif RETRO_PLATFORM == RETRO_PS3

#include "AudioPS3.hpp"
#include <altivec.h>
#include <sys/synchronization.h>

sys_mutex_t audioMutex = 0;

#define AUDIO_SAMPLES   (512)
#ifndef AUDIO_FREQUENCY
#define AUDIO_FREQUENCY (44100)
#endif
#ifndef AUDIO_CHANNELS
#define AUDIO_CHANNELS  (2)
#endif

#endif

#if !RETRO_USE_ORIGINAL_CODE
#define ADJUST_VOLUME(s, v) (s = (s * v) / MAX_VOLUME)
#endif

int InitAudioPlayback()
{
#if RETRO_PLATFORM == RETRO_PS3
    static bool mutex_created = false;
    if (!mutex_created) {
        sys_mutex_attribute_t mutexAttr;
        sys_mutex_attribute_initialize(mutexAttr);
        mutexAttr.attr_recursive = SYS_SYNC_RECURSIVE;
        mutexAttr.name[0] = 0;
        int ret = sys_mutex_create(&audioMutex, &mutexAttr);
        if (ret == CELL_OK) {
            mutex_created = true;
        } else {
            PrintLog("Failed to create audio mutex: 0x%08X", ret);
        }
    }
#endif

    StopAllSfx(); //"init"

    for (int i = 0; i < STREAMFILE_COUNT; ++i) {
        if (streamFile[i].buffer == NULL) {
            streamFile[i].buffer = (byte*)memalign(16, MUSBUFFER_SIZE);
        }
        streamFile[i].vsize = 0;
        streamFile[i].vpos = 0;

        MEM_ZERO(streamInfo[i]);
    }
    for (int i = 0; i < SFX_COUNT; ++i) {
        MEM_ZERO(sfxList[i]);
    }
    for (int i = 0; i < CHANNEL_COUNT; ++i) {
        MEM_ZERO(sfxChannels[i]);
        sfxChannels[i].sfxID = -1;
    }

    streamFilePtr = NULL;
    streamInfoPtr = NULL;

#if !RETRO_USE_ORIGINAL_CODE
#if RETRO_USING_SDL1 || RETRO_USING_SDL2
    SDL_AudioSpec want;
    want.freq     = AUDIO_FREQUENCY;
    want.format   = AUDIO_FORMAT;
    want.samples  = AUDIO_SAMPLES;
    want.channels = AUDIO_CHANNELS;
    want.callback = ProcessAudioPlayback;

#if RETRO_USING_SDL2
    if ((audioDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &audioDeviceFormat, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE)) > 0) {
        audioEnabled = true;
        SDL_PauseAudioDevice(audioDevice, 0);
    }
    else {
        PrintLog("Unable to open audio device: %s", SDL_GetError());
        audioEnabled = false;
        return true; // no audio but game wont crash now
    }
#elif RETRO_USING_SDL1
    if (SDL_OpenAudio(&want, &audioDeviceFormat) == 0) {
        audioEnabled = true;
        SDL_PauseAudio(0);
    }
    else {
        PrintLog("Unable to open audio device: %s", SDL_GetError());
        audioEnabled = false;
        return true; // no audio but game wont crash now
    }
#endif // !RETRO_USING_SDL1
#elif RETRO_PLATFORM == RETRO_PS3
    if (InitPS3Audio(AUDIO_FREQUENCY, AUDIO_SAMPLES * AUDIO_CHANNELS * 4)) {
        audioEnabled = true;
    }
    else {
        PrintLog("Unable to open PS3 audio device");
        audioEnabled = false;
        return true;
    }
#endif
#endif

    LoadGlobalSfx();

    return true;
}

void LoadGlobalSfx()
{
    FileInfo info;
    FileInfo infoStore;
    char strBuffer[0x100];
    byte fileBuffer = 0;
    int fileBuffer2 = 0;

    globalSFXCount = 0;

    if (LoadFile("Data/Game/GameConfig.bin", &info)) {
        infoStore = info;

        FileRead(&fileBuffer, 1);
        FileRead(strBuffer, fileBuffer);

        FileRead(&fileBuffer, 1);
        FileRead(strBuffer, fileBuffer);

        byte buf[3];
        for (int c = 0; c < 0x60; ++c) FileRead(buf, 3);

        // Read Obect Names
        byte objectCount = 0;
        FileRead(&objectCount, 1);
        for (byte o = 0; o < objectCount; ++o) {
            FileRead(&fileBuffer, 1);
            FileRead(&strBuffer, fileBuffer);
        }

        // Read Script Paths
        for (byte s = 0; s < objectCount; ++s) {
            FileRead(&fileBuffer, 1);
            FileRead(&strBuffer, fileBuffer);
        }

        byte varCount = 0;
        FileRead(&varCount, 1);
        for (byte v = 0; v < varCount; ++v) {
            // Read Variable Name
            FileRead(&fileBuffer, 1);
            FileRead(&strBuffer, fileBuffer);

            // Read Variable Value
            FileRead(&fileBuffer2, 4);
        }

        // Read SFX
        FileRead(&fileBuffer, 1);
        globalSFXCount = fileBuffer;
        for (byte s = 0; s < globalSFXCount; ++s) { // SFX Names
            FileRead(&fileBuffer, 1);
            FileRead(&strBuffer, fileBuffer);
            strBuffer[fileBuffer] = 0;

            SetSfxName(strBuffer, s);
        }
        for (byte s = 0; s < globalSFXCount; ++s) { // SFX Paths
            FileRead(&fileBuffer, 1);
            FileRead(&strBuffer, fileBuffer);
            strBuffer[fileBuffer] = 0;

            GetFileInfo(&infoStore);
            CloseFile();
            LoadSfx(strBuffer, s);
            SetFileInfo(&infoStore);
        }

        CloseFile();

#if RETRO_USE_MOD_LOADER
        // XML SFX are now handled within LoadModConfig in a single pass
#endif
    }

    for (int i = 0; i < CHANNEL_COUNT; ++i) sfxChannels[i].sfxID = -1;
}

size_t readVorbis(void *mem, size_t size, size_t nmemb, void *ptr)
{
    StreamFile *file = (StreamFile *)ptr;
    if (!file || !file->buffer || file->vpos >= file->vsize)
        return 0;

    size_t n = size * nmemb;
    if (n > (size_t)(file->vsize - file->vpos))
        n = (size_t)(file->vsize - file->vpos);

    size_t elements      = size > 0 ? (n / size) : 0;
    size_t bytes_to_read = elements * size;

    if (bytes_to_read > 0) {
        memcpy(mem, &file->buffer[file->vpos], bytes_to_read);
        file->vpos += (int)bytes_to_read;
    }

    return elements;
}
int seekVorbis(void *ptr, ogg_int64_t offset, int whence)
{
    StreamFile *file = (StreamFile *)ptr;
    if (!file) return -1;

    int newPos = 0;
    switch (whence) {
        case SEEK_SET: newPos = (int)offset; break;
        case SEEK_CUR: newPos = file->vpos + (int)offset; break;
        case SEEK_END: newPos = file->vsize + (int)offset; break;
        default: return -1;
    }

    if (newPos < 0)
        newPos = 0;
    if (newPos > file->vsize)
        newPos = file->vsize;

    file->vpos = newPos;
    return 0;
}
long tellVorbis(void *ptr)
{
    StreamFile *file = (StreamFile *)ptr;
    return file->vpos;
}
int closeVorbis(void *ptr) { return 1; }

#if !RETRO_USE_ORIGINAL_CODE
#if RETRO_USING_SDL1 || RETRO_USING_SDL2 || RETRO_PLATFORM == RETRO_PS3
void ProcessMusicStream(Sint32 *stream, size_t sampleCount)
{
#if RETRO_USING_SDL1 || RETRO_USING_SDL2
    size_t bytes_wanted = sampleCount * sizeof(Sint16);
#endif
    if (!streamFilePtr || !streamInfoPtr)
        return;
    if (!streamFilePtr->vsize)
        return;
    switch (musicStatus) {
        case MUSIC_READY:
        case MUSIC_PLAYING: {
#if RETRO_USING_SDL2
            while (musicStatus == MUSIC_PLAYING && streamInfoPtr->stream && SDL_AudioStreamAvailable(streamInfoPtr->stream) < bytes_wanted) {
                // We need more samples: get some
                long bytes_read = ov_read(&streamInfoPtr->vorbisFile, (char *)streamInfoPtr->buffer, sizeof(streamInfoPtr->buffer), 0, 2, 1,
                                          &streamInfoPtr->vorbBitstream);

                if (bytes_read == 0) {
                    // We've reached the end of the file
                    if (streamInfoPtr->trackLoop) {
                        ov_pcm_seek(&streamInfoPtr->vorbisFile, streamInfoPtr->loopPoint);
                        continue;
                    }
                    else {
                        musicStatus = MUSIC_STOPPED;
                        break;
                    }
                }

                if (musicStatus != MUSIC_PLAYING
                    || (streamInfoPtr->stream && SDL_AudioStreamPut(streamInfoPtr->stream, streamInfoPtr->buffer, (int)bytes_read) == -1))
                    return;
            }

            // Now that we know there are enough samples, read them and mix them
            int bytes_done = SDL_AudioStreamGet(streamInfoPtr->stream, streamInfoPtr->buffer, (int)bytes_wanted);
            if (bytes_done == -1) {
                return;
            }
            if (bytes_done != 0)
                ProcessAudioMixing(stream, streamInfoPtr->buffer, bytes_done / sizeof(Sint16), (bgmVolume * masterVolume) / MAX_VOLUME, 0);
#elif RETRO_USING_SDL1
            size_t bytes_gotten = 0;
            byte *buffer        = (byte *)malloc(bytes_wanted);
            memset(buffer, 0, bytes_wanted);
            while (bytes_gotten < bytes_wanted) {
                // We need more samples: get some
                long bytes_read =
                    ov_read(&oggFilePtr->vorbisFile, (char *)oggFilePtr->buffer,
                            sizeof(oggFilePtr->buffer) > (bytes_wanted - bytes_gotten) ? (bytes_wanted - bytes_gotten) : sizeof(oggFilePtr->buffer),
                            0, 2, 1, &oggFilePtr->vorbBitstream);

                if (bytes_read == 0) {
                    // We've reached the end of the file
                    if (oggFilePtr->trackLoop) {
                        ov_pcm_seek(&oggFilePtr->vorbisFile, oggFilePtr->loopPoint);
                        continue;
                    }
                    else {
                        musicStatus = MUSIC_STOPPED;
                        break;
                    }
                }

                if (bytes_read > 0) {
                    memcpy(buffer + bytes_gotten, oggFilePtr->buffer, bytes_read);
                    bytes_gotten += bytes_read;
                }
                else {
                    PrintLog("Music read error: vorbis error: %d", bytes_read);
                }
            }

            if (bytes_gotten > 0) {
                SDL_AudioCVT convert;
                MEM_ZERO(convert);
                int cvtResult = SDL_BuildAudioCVT(&convert, oggFilePtr->spec.format, oggFilePtr->spec.channels, oggFilePtr->spec.freq,
                                                  audioDeviceFormat.format, audioDeviceFormat.channels, audioDeviceFormat.freq);
                if (cvtResult == 0) {
                    if (convert.len_mult > 0) {
                        convert.buf = (byte *)malloc(bytes_gotten * convert.len_mult);
                        convert.len = bytes_gotten;
                        memcpy(convert.buf, buffer, bytes_gotten);
                        SDL_ConvertAudio(&convert);
                    }
                }

                if (cvtResult == 0)
                    ProcessAudioMixing(stream, (const Sint16 *)convert.buf, bytes_gotten / sizeof(Sint16), (bgmVolume * masterVolume) / MAX_VOLUME,
                                       0);

                if (convert.len > 0 && convert.buf)
                    free(convert.buf);
            }
            if (bytes_wanted > 0)
                free(buffer);
#elif RETRO_PLATFORM == RETRO_PS3
            if (!streamInfoPtr || !streamInfoPtr->loaded) {
                musicStatus = MUSIC_STOPPED;
                break;
            }

            int channels = streamInfoPtr->vorbisFile.vi->channels;
            int frames_wanted = (int)sampleCount / 2; // sampleCount is in total samples, we want stereo frames

            static Sint16 resample_buffer[MIX_BUFFER_SAMPLES] __attribute__((aligned(16)));
            int frames_done = 0;

            while (frames_done < frames_wanted) {
                while (streamInfoPtr->inputPos >= streamInfoPtr->inputCount) {
                    if (streamInfoPtr->inputCount > 0) {
                        // Keep last sample from previous block for interpolation
                        if (channels == 1) {
                            streamInfoPtr->lastL = streamInfoPtr->buffer[streamInfoPtr->inputCount - 1];
                            streamInfoPtr->lastR = streamInfoPtr->lastL;
                        } else {
                            streamInfoPtr->lastL = streamInfoPtr->buffer[(streamInfoPtr->inputCount - 1) * 2];
                            streamInfoPtr->lastR = streamInfoPtr->buffer[(streamInfoPtr->inputCount - 1) * 2 + 1];
                        }
                    }
                    streamInfoPtr->inputPos = 0;

                    int max_samples = MIX_BUFFER_SAMPLES;
                    long bytes_read = ov_read(&streamInfoPtr->vorbisFile, (char *)streamInfoPtr->buffer, max_samples * sizeof(Sint16), &streamInfoPtr->vorbBitstream);
                    
                    if (bytes_read <= 0) {
                        if (bytes_read == 0 && streamInfoPtr->trackLoop) {
                            ov_pcm_seek(&streamInfoPtr->vorbisFile, streamInfoPtr->loopPoint);
                            streamInfoPtr->inputCount = 0;
                            continue;
                        } else {
                            musicStatus = MUSIC_STOPPED;
                            goto music_done;
                        }
                    }

                    streamInfoPtr->inputCount = (int)(bytes_read / (channels * sizeof(Sint16)));
                }

                uint32_t fract_fp = streamInfoPtr->resamplePos_fp & 0xFFFF;
                Sint16 s1L, s1R, s2L, s2R;

                if (streamInfoPtr->inputPos == 0) {
                    s1L = streamInfoPtr->lastL;
                    s1R = streamInfoPtr->lastR;
                } else {
                    if (channels == 1) {
                        s1L = streamInfoPtr->buffer[streamInfoPtr->inputPos - 1];
                        s1R = s1L;
                    } else {
                        s1L = streamInfoPtr->buffer[(streamInfoPtr->inputPos - 1) * 2];
                        s1R = streamInfoPtr->buffer[(streamInfoPtr->inputPos - 1) * 2 + 1];
                    }
                }

                if (channels == 1) {
                    s2L = streamInfoPtr->buffer[streamInfoPtr->inputPos];
                    s2R = s2L;
                } else {
                    s2L = streamInfoPtr->buffer[streamInfoPtr->inputPos * 2];
                    s2R = streamInfoPtr->buffer[streamInfoPtr->inputPos * 2 + 1];
                }

                // Vectorized interpolation would be complex here due to stream nature, 
                // but we use 64-bit to prevent overflow
                resample_buffer[frames_done * 2] = (Sint16)(s1L + (((Sint64)(s2L - s1L) * (Sint64)fract_fp) >> 16));
                resample_buffer[frames_done * 2 + 1] = (Sint16)(s1R + (((Sint64)(s2R - s1R) * (Sint64)fract_fp) >> 16));

                frames_done++;
                streamInfoPtr->resamplePos_fp += streamInfoPtr->ratio_fp;
                while (streamInfoPtr->resamplePos_fp >= 0x10000) {
                    streamInfoPtr->resamplePos_fp -= 0x10000;
                    streamInfoPtr->inputPos++;
                    if (streamInfoPtr->inputPos >= streamInfoPtr->inputCount && frames_done < frames_wanted)
                        break; // Need to refill
                }
            }

music_done:
            if (frames_done > 0) {
                ProcessAudioMixing(stream, resample_buffer, frames_done * 2, (bgmVolume * masterVolume) / MAX_VOLUME, 0);
            }
#endif

            musicPosition = (int)ov_pcm_tell(&streamInfoPtr->vorbisFile);
            break;
        }
        case MUSIC_STOPPED:
        case MUSIC_PAUSED:
        case MUSIC_LOADING:
            // dont play
            break;
    }
}

#if RETRO_USING_SDL1 || RETRO_USING_SDL2
void ProcessAudioPlayback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata; // Unused

    if (!audioEnabled)
        return;

    Sint16 *output_buffer    = (Sint16 *)stream;
    size_t samples_remaining = (size_t)len / sizeof(Sint16);
#else
void ProcessAudioPlayback(Sint16 *stream, int sampleCount)
{
    if (!audioEnabled)
        return;

    Sint16 *output_buffer    = stream;
    size_t samples_remaining = (size_t)sampleCount;
#endif

    static Sint32 mix_buffer[MIX_BUFFER_SAMPLES] __attribute__((aligned(16)));
    static Sint16 sfx_temp_buffer[MIX_BUFFER_SAMPLES] __attribute__((aligned(16)));

    while (samples_remaining > 0) {
        memset(mix_buffer, 0, sizeof(mix_buffer));

        // Ensure we always do an even number of samples (stereo frames)
        // MIX_BUFFER_SAMPLES is 2048, enough for 1024 stereo frames.
        // We limit to 1024 here to avoid overflows during expansion.
        size_t samples_to_do = (samples_remaining < 1024) ? samples_remaining : 1024;
        if (samples_to_do % 2 != 0)
            samples_to_do--;
        if (samples_to_do == 0)
            break;

        LockAudioDevice();
        // Mix music
#if RETRO_USING_SDL1 || RETRO_USING_SDL2
        ProcessMusicStream(mix_buffer, samples_to_do * sizeof(Sint16));
#else
        ProcessMusicStream(mix_buffer, samples_to_do);
#endif

        // Mix SFX
        for (byte i = 0; i < CHANNEL_COUNT; ++i) {
            ChannelInfo *sfx = &sfxChannels[i];
            if (sfx->sfxID < 0 || sfx->samplePtr == NULL)
                continue;

            memset(sfx_temp_buffer, 0, sizeof(sfx_temp_buffer));

            size_t samples_done = 0;
            while (samples_done < samples_to_do) {
                size_t samples_needed = samples_to_do - samples_done;
                size_t sampleLen      = (sfx->sampleLength < samples_needed) ? sfx->sampleLength : samples_needed;

                // SFX must be frame-aligned (even number of samples)
                if (sampleLen % 2 != 0)
                    sampleLen--;

                if (sampleLen > 0 && sfx->samplePtr != NULL) {
                    memcpy(&sfx_temp_buffer[samples_done], sfx->samplePtr, sampleLen * sizeof(Sint16));

                    samples_done += sampleLen;
                    sfx->samplePtr += sampleLen;
                    sfx->sampleLength -= sampleLen;
                }

                if (sfx->sampleLength <= 1) { // 1 or 0 samples left means it's finished
                    if (sfx->loopSFX && sfxList[sfx->sfxID].length > 0 && sfxList[sfx->sfxID].buffer != NULL) {
                        sfx->samplePtr    = sfxList[sfx->sfxID].buffer;
                        sfx->sampleLength = sfxList[sfx->sfxID].length;
                    }
                    else {
                        MEM_ZEROP(sfx);
                        sfx->sfxID = -1;
                        break;
                    }
                }

                if (sampleLen == 0)
                    break; // Avoid infinite loop
            }

            if (samples_done > 0)
                ProcessAudioMixing(mix_buffer, sfx_temp_buffer, (int)samples_done, sfxVolume, sfx->pan);
        }

        // CRT TV Sound Simulation (60Hz hum and subtle static)
        // Only during 2D gameplay (ignore presentation/menus)
        if (Engine.filterMode == FILTER_TV && activeStageList != STAGELIST_PRESENTATION) {
            float dimFactor = Engine.dimMax * Engine.dimPercent;
            if (Engine.masterPaused || stageMode == STAGEMODE_PAUSED || stageMode == STAGEMODE_PAUSED_STEP) {
                dimFactor *= 0.65f;
            }

            float phase_inc = (2.0f * 3.14159265f * 60.0f) / (float)AUDIO_FREQUENCY;
            for (size_t i = 0; i < samples_to_do; i += 2) {
                // 60Hz hum - volume scales with dimming
                float hum = sinf(tvHumPhase) * (1200.0f * dimFactor);
                tvHumPhase += phase_inc;
                if (tvHumPhase > 2.0f * 3.14159265f)
                    tvHumPhase -= 2.0f * 3.14159265f;

                // White noise static - volume scales with dimming
                tvNoiseSeed        = tvNoiseSeed * 1664525 + 1013904223;
                float static_noise = ((float)(int32_t)tvNoiseSeed / 2147483647.0f) * (800.0f * dimFactor);

                Sint32 tv_sample = (Sint32)(hum + static_noise);
                mix_buffer[i] += tv_sample; // Left
                if (i + 1 < samples_to_do)
                    mix_buffer[i + 1] += tv_sample; // Right
            }
        }
        UnlockAudioDevice();

#if RETRO_PLATFORM == RETRO_PS3
        // Clamping with AltiVec
        if ((((uintptr_t)output_buffer | (uintptr_t)mix_buffer) & 15) == 0 && (samples_to_do & 7) == 0) {
            int vectors = samples_to_do / 8;
            for (int v = 0; v < vectors; v++) {
                vector signed int v_low  = vec_ld(0, mix_buffer + v * 8);
                vector signed int v_high = vec_ld(16, mix_buffer + v * 8);

                // Pack with saturation
                vector signed short v_out = vec_packs(v_low, v_high);
                vec_st(v_out, 0, output_buffer + v * 8);
            }
        }
        else {
            for (size_t i = 0; i < samples_to_do; ++i) {
                const Sint32 sample = mix_buffer[i];
                if (sample > 32767)
                    output_buffer[i] = 32767;
                else if (sample < -32768)
                    output_buffer[i] = -32768;
                else
                    output_buffer[i] = (Sint16)sample;
            }
        }
#else
        // Clamp mixed samples back to 16-bit and write them to the output buffer
        for (size_t i = 0; i < samples_to_do; ++i) {
            const Sint16 max_audioval = 32767;
            const Sint16 min_audioval = -32768;

            const Sint32 sample = mix_buffer[i];

            if (sample > max_audioval)
                output_buffer[i] = max_audioval;
            else if (sample < min_audioval)
                output_buffer[i] = min_audioval;
            else
                output_buffer[i] = (Sint16)sample;
        }
#endif
        output_buffer += samples_to_do;
        samples_remaining -= samples_to_do;
    }
}
#endif

#if RETRO_PLATFORM == RETRO_PS3

void ProcessAudioMixing(Sint32 *dst, const Sint16 *src, int len, int volume, sbyte pan)
{
    if (volume <= 0)
        return;

    if (volume > MAX_VOLUME)
        volume = MAX_VOLUME;

    // Fixed-point volume (Q15)
    // MAX_VOLUME is 100, so we scale to 32768
    int vol_fp = (volume << 15) / MAX_VOLUME;
    
    int panL_fp = 32768;
    int panR_fp = 32768;

    if (pan < 0) {
        panR_fp = 32768 - (abs(pan) * 32768 / 100);
    }
    else if (pan > 0) {
        panL_fp = 32768 - (abs(pan) * 32768 / 100);
    }

    // Apply volume to panning to save one multiplication in the loop
    panL_fp = (panL_fp * vol_fp) >> 15;
    panR_fp = (panR_fp * vol_fp) >> 15;

    // Use AltiVec for optimized mixing if aligned and length is sufficient
    if ((((uintptr_t)dst | (uintptr_t)src) & 15) == 0 && (len & 7) == 0) {
        int frames = len / 2;
        int vectors = frames / 4; // 4 stereo frames per vector

        // Use short to avoid overflow at 32768 (which is -32768 in signed short)
        // panL_fp/panR_fp are 0-32768. Clamp to 32767 for safety.
        int16_t pL = (int16_t)(panL_fp > 32767 ? 32767 : panL_fp);
        int16_t pR = (int16_t)(panR_fp > 32767 ? 32767 : panR_fp);

        vector signed short v_pans = { pL, pR, pL, pR, pL, pR, pL, pR };
        vector unsigned int v_shift = vec_splat_u32(15);

        for (int v = 0; v < vectors; v++) {
            // Load 8 samples (4 stereo frames)
            vector signed short v_src = vec_ld(0, src); 
            
            // Multiply samples by pans
            // vec_mule: even elements (0, 2, 4, 6) -> Left channels
            // vec_mulo: odd elements (1, 3, 5, 7) -> Right channels
            vector signed int v_mule = vec_mule(v_src, v_pans);
            vector signed int v_mulo = vec_mulo(v_src, v_pans);
            
            // Shift back to 16-bit range (Q15 * Q15 >> 15)
            v_mule = vec_sra(v_mule, v_shift);
            v_mulo = vec_sra(v_mulo, v_shift);

            // Load current destination samples (32-bit accumulators)
            vector signed int v_dst_low = vec_ld(0, dst);
            vector signed int v_dst_high = vec_ld(16, dst);

            // Re-interleave L/R back to L0, R0, L1, R1...
            vector signed int v_out_low = vec_mergeh(v_mule, v_mulo);
            vector signed int v_out_high = vec_mergel(v_mule, v_mulo);

            // Accumulate and store
            vec_st(vec_add(v_dst_low, v_out_low), 0, dst);
            vec_st(vec_add(v_dst_high, v_out_high), 16, dst);

            src += 8;
            dst += 8;
        }
    } else {
        // Fallback for unaligned or small buffers
        while (len >= 2) {
            Sint32 sampleL = *src++;
            Sint32 sampleR = *src++;

            *dst++ += (sampleL * panL_fp) >> 15;
            *dst++ += (sampleR * panR_fp) >> 15;
            len -= 2;
        }
        while (len--) {
            *dst++ += (*src++ * panL_fp) >> 15;
        }
    }
}
#else
void ProcessAudioMixing(Sint32 *dst, const Sint16 *src, int len, int volume, sbyte pan)
{
    if (volume == 0)
        return;

    if (volume > MAX_VOLUME)
        volume = MAX_VOLUME;

    float panL = 0.0;
    float panR = 0.0;
    int i      = 0;

    if (pan < 0) {
        panR = 1.0f - abs(pan / 100.0f);
        panL = 1.0f;
    }
    else if (pan > 0) {
        panL = 1.0f - abs(pan / 100.0f);
        panR = 1.0f;
    }

    while (len--) {
        Sint32 sample = *src++;
        ADJUST_VOLUME(sample, volume);

        if (pan != 0) {
            if ((i % 2) != 0) {
                sample *= panR;
            }
            else {
                sample *= panL;
            }
        }

        *dst++ += sample;

        i++;
    }
}
#endif
#endif

void LoadMusic(void *userdata)
{
    PrintLog("LoadMusic: Processing loop started (track: %d)", currentMusicTrack);
    while (currentMusicTrack >= 0) {
        int oldStreamID = currentStreamIndex;

        LockAudioDevice();
        streamInfoPtr = NULL;
        musicStatus   = MUSIC_LOADING;

        currentStreamIndex++;
        currentStreamIndex %= STREAMFILE_COUNT;

        if (streamFile[currentStreamIndex].vsize > 0) {
            musicPosition = 0;
            FreeMusInfo(false);
        }
        UnlockAudioDevice();

#if RETRO_PLATFORM == RETRO_PS3
        uint64_t startTime = GetSystemTime();
        uint64_t openTime, readTime, decodeTime;
#endif

        FileInfo info;
#if RETRO_PLATFORM == RETRO_PS3
        PrintLog("Music Thread: Locking Reader for %s", musicTracks[currentMusicTrack].fileName);
        LockReader();
#endif
        if (LoadFile(musicTracks[currentMusicTrack].fileName, &info)) {
#if RETRO_PLATFORM == RETRO_PS3
            openTime = GetSystemTime();
#endif
            StreamFile *musFile = &streamFile[currentStreamIndex];
            musFile->vpos       = 0;
            musFile->vsize      = info.vfileSize;
            if (info.vfileSize > MUSBUFFER_SIZE) {
                musFile->vsize = MUSBUFFER_SIZE;
            }

            FileRead(streamFile[currentStreamIndex].buffer, musFile->vsize);
            CloseFile();
#if RETRO_PLATFORM == RETRO_PS3
            UnlockReader();
            PrintLog("Music Thread: Reader Unlocked");
#endif

#if RETRO_PLATFORM == RETRO_PS3
            readTime = GetSystemTime();
#endif

            LockAudioDevice();

            StreamInfo *strmInfo = &streamInfo[currentStreamIndex];

            unsigned long long samples = 0;
            ov_callbacks callbacks;

            callbacks.read_func  = readVorbis;
            callbacks.seek_func  = seekVorbis;
            callbacks.tell_func  = tellVorbis;
            callbacks.close_func = closeVorbis;

            memset(&strmInfo->vorbisFile, 0, sizeof(strmInfo->vorbisFile));
            strmInfo->loaded = false;
            int error        = ov_open_callbacks(musFile, &strmInfo->vorbisFile, NULL, 0, callbacks);
            if (error == 0) {
                strmInfo->vorbBitstream = -1;
                strmInfo->vorbisFile.vi = ov_info(&strmInfo->vorbisFile, -1);
                if (!strmInfo->vorbisFile.vi) {
                    PrintLog("Music load FAILED: ov_info returned NULL");
                    ov_clear(&strmInfo->vorbisFile);
                    musicStatus = MUSIC_STOPPED;
                    currentMusicTrack = -1;
                    UnlockAudioDevice();
                    continue;
                }

                samples = (unsigned long long)ov_pcm_total(&strmInfo->vorbisFile, -1);

#if RETRO_USING_SDL2
                strmInfo->stream = SDL_NewAudioStream(AUDIO_S16, strmInfo->vorbisFile.vi->channels, (int)strmInfo->vorbisFile.vi->rate,
                                                      audioDeviceFormat.format, audioDeviceFormat.channels, audioDeviceFormat.freq);
                if (!strmInfo->stream)
                    PrintLog("Failed to create stream: %s", SDL_GetError());
#endif

#if RETRO_USING_SDL1
                strmInfo->spec.format   = AUDIO_S16;
                strmInfo->spec.channels = strmInfo->vorbisFile.vi->channels;
                strmInfo->spec.freq     = (int)strmInfo->vorbisFile.vi->rate;
#endif

#if RETRO_PLATFORM == RETRO_PS3
                strmInfo->ratio_fp    = (uint32_t)(((double)strmInfo->vorbisFile.vi->rate / (double)AUDIO_FREQUENCY) * 65536.0);
                strmInfo->resamplePos_fp = 0;
                strmInfo->lastL       = 0;
                strmInfo->lastR       = 0;
                strmInfo->inputPos    = 0;
                strmInfo->inputCount  = 0;
#endif

                if (musicStartPos) {
                    uint oldPos = 0;
                    if (streamInfo[oldStreamID].loaded)
                        oldPos = (uint)ov_pcm_tell(&streamInfo[oldStreamID].vorbisFile);

                    float newPos  = oldPos * ((float)musicRatio * 0.0001f); // 8,000 == 0.8, 10,000 == 1.0 (ratio / 10,000)
                    musicStartPos = (int)fmod((double)newPos, (double)samples);

                    ov_pcm_seek(&strmInfo->vorbisFile, musicStartPos);
                }
                musicStartPos = 0;

                strmInfo->trackLoop = musicTracks[currentMusicTrack].trackLoop;
                strmInfo->loopPoint = musicTracks[currentMusicTrack].loopPoint;
                strmInfo->loaded    = true;
                streamFilePtr       = &streamFile[currentStreamIndex];
                streamInfoPtr       = &streamInfo[currentStreamIndex];

                musicStatus       = MUSIC_PLAYING;
                masterVolume      = MAX_VOLUME;
                trackID           = currentMusicTrack;
                currentMusicTrack = -1;
                musicPosition     = 0;

#if RETRO_PLATFORM == RETRO_PS3
                decodeTime = GetSystemTime();
                PrintLog("Music load took %llu us (Open: %llu, Read: %llu, Decode: %llu, Total: %llu)", decodeTime - musicLoadStartTime,
                         openTime - startTime, readTime - openTime, decodeTime - readTime, decodeTime - startTime);
#endif
                UnlockAudioDevice();
            }
            else {
                musicStatus = MUSIC_STOPPED;
                PrintLog("Failed to load vorbis! error: %d", error);
                switch (error) {
                    default: PrintLog("Vorbis open error: Unknown (%d)", error); break;
                    case OV_EREAD: PrintLog("Vorbis open error: A read from media returned an error"); break;
                    case OV_ENOTVORBIS: PrintLog("Vorbis open error: Bitstream does not contain any Vorbis data"); break;
                    case OV_EVERSION: PrintLog("Vorbis open error: Vorbis version mismatch"); break;
                    case OV_EBADHEADER: PrintLog("Vorbis open error: Invalid Vorbis bitstream header"); break;
                    case OV_EFAULT: PrintLog("Vorbis open error: Internal logic fault; indicates a bug or heap / stack corruption"); break;
                }
                currentMusicTrack = -1;
                UnlockAudioDevice();
            }
        }
        else {
#if RETRO_PLATFORM == RETRO_PS3
            UnlockReader();
            PrintLog("Music Thread: Reader Unlocked (Load FAILED)");
#endif
            musicStatus       = MUSIC_STOPPED;
            currentMusicTrack = -1;
        }

        if (nextMusicTrack >= 0) {
            LockAudioDevice();
            currentMusicTrack = nextMusicTrack;
            musicStartPos     = nextMusicStartPos;
            nextMusicTrack    = -1;
            nextMusicStartPos = 0;
            UnlockAudioDevice();
        }
    }
}

void SetMusicTrack(const char *filePath, byte trackID, bool loop, uint loopPoint)
{
    LockAudioDevice();
    TrackInfo *track = &musicTracks[trackID];
    StrCopy(track->fileName, "Data/Music/");
    StrAdd(track->fileName, filePath);
    track->trackLoop = loop;
    track->loopPoint = loopPoint;
    UnlockAudioDevice();
}

void SwapMusicTrack(const char *filePath, byte trackID, uint loopPoint, uint ratio)
{
    if (StrLength(filePath) <= 0) {
        StopMusic(true);
    }
    else {
        LockAudioDevice();
        TrackInfo *track = &musicTracks[trackID];
        StrCopy(track->fileName, "Data/Music/");
        StrAdd(track->fileName, filePath);
        track->trackLoop = true;
        track->loopPoint = loopPoint;
        musicRatio       = ratio;
        UnlockAudioDevice();
        PlayMusic(trackID, 1);
    }
}

#if RETRO_PLATFORM == RETRO_PS3
static volatile bool musicThreadRunning = false;
void LoadMusic_Thread(uint64_t arg) {
    PrintLog("Music Thread: Started");
    LoadMusic(NULL);
    PrintLog("Music Thread: Finished");
    musicThreadRunning = false;
    sys_ppu_thread_exit(0);
}
#endif

bool PlayMusic(int track, int musStartPos)
{
    if (!audioEnabled)
        return false;

    if (track < 0 || track >= TRACK_COUNT) {
        PrintLog("PlayMusic: INVALID track %d requested", track);
        StopMusic(true);
        currentMusicTrack = -1;
        return false;
    }

    PrintLog("PlayMusic: Track %d requested (pos: %d, file: %s)", track, musStartPos, musicTracks[track].fileName);

    if (musicTracks[track].fileName[0]) {
        if (musicStatus != MUSIC_LOADING && !musicThreadRunning) {
            musicStartPos     = musStartPos;
            currentMusicTrack = track;
            musicStatus       = MUSIC_LOADING;
#if RETRO_PLATFORM == RETRO_PS3
            PrintLog("PlayMusic: Starting load thread for track %d", track);
            musicLoadStartTime = GetSystemTime();
            musicThreadRunning = true;
            sys_ppu_thread_t thread;
            int ret = sys_ppu_thread_create(&thread, LoadMusic_Thread, 0, 1500, 0x40000, 0, "MusicLoadThread");
            if (ret != CELL_OK) {
                PrintLog("PlayMusic: Thread creation FAILED (0x%08X), loading synchronously", ret);
                musicThreadRunning = false;
                LoadMusic(NULL);
            } else {
                PrintLog("PlayMusic: Load thread created successfully (Priority: 1500, Stack: 256KB)");
            }
#else
            LoadMusic(NULL);
#endif
            return true;
        }
        else {
            nextMusicTrack    = track;
            nextMusicStartPos = musStartPos;
            PrintLog("Music queued: track %d (Status: %d, ThreadRunning: %d)", track, musicStatus, (int)musicThreadRunning);
            return true;
        }
    }
    else {
        StopMusic(true);
    }

    return false;
}

void SetSfxName(const char *sfxName, int sfxID)
{
    int sfxNameID   = 0;
    int soundNameID = 0;
    while (sfxName[sfxNameID]) {
        if (sfxName[sfxNameID] != ' ')
            sfxNames[sfxID][soundNameID++] = sfxName[sfxNameID];
        ++sfxNameID;
    }
    sfxNames[sfxID][soundNameID] = 0;
    PrintLog("Set SFX (%d) name to: %s", sfxID, sfxName);
}

void LoadSfx(char *filePath, byte sfxID)
{
    if (!audioEnabled || sfxID >= SFX_COUNT)
        return;

    FileInfo info;
    char fullPath[0x80];

    StrCopy(fullPath, "Data/SoundFX/");
    StrAdd(fullPath, filePath);

#if RETRO_PLATFORM == RETRO_PS3
    LockReader();
#endif
    if (LoadFile(fullPath, &info)) {
        if (sfxList[sfxID].loaded && sfxList[sfxID].buffer) {
            free(sfxList[sfxID].buffer);
            sfxList[sfxID].buffer = NULL;
            sfxList[sfxID].loaded = false;
        }

#if !RETRO_USE_ORIGINAL_CODE
        byte type = fullPath[StrLength(fullPath) - 3];
#if RETRO_USING_SDL1 || RETRO_USING_SDL2
        if (type == 'w') {
            byte *sfx = new byte[info.vfileSize];
            FileRead(sfx, info.vfileSize);
            CloseFile();

            SDL_RWops *src = SDL_RWFromMem(sfx, info.vfileSize);
            if (src == NULL) {
                PrintLog("Unable to open sfx: %s", info.fileName);
            }
            else {
                SDL_AudioSpec wav_spec;
                uint wav_length;
                byte *wav_buffer;
                SDL_AudioSpec *wav = SDL_LoadWAV_RW(src, 0, &wav_spec, &wav_buffer, &wav_length);

                SDL_RWclose(src);
                delete[] sfx;
                if (wav == NULL) {
                    PrintLog("Unable to read sfx: %s", info.fileName);
                }
                else {
                    SDL_AudioCVT convert;
                    if (SDL_BuildAudioCVT(&convert, wav->format, wav->channels, wav->freq, audioDeviceFormat.format, audioDeviceFormat.channels,
                                          audioDeviceFormat.freq)
                        > 0) {
                        convert.buf = (byte *)malloc(wav_length * convert.len_mult);
                        convert.len = wav_length;
                        memcpy(convert.buf, wav_buffer, wav_length);
                        SDL_ConvertAudio(&convert);

                        LockAudioDevice();
                        StrCopy(sfxList[sfxID].name, filePath);
                        sfxList[sfxID].buffer = (Sint16 *)convert.buf;
                        sfxList[sfxID].length = convert.len_cvt / sizeof(Sint16);
                        sfxList[sfxID].loaded = true;
                        UnlockAudioDevice();
                        SDL_FreeWAV(wav_buffer);
                    }
                    else { // this causes errors, actually
                        PrintLog("Unable to read sfx: %s (error: %s)", info.fileName, SDL_GetError());
                        sfxList[sfxID].loaded = false;
                        SDL_FreeWAV(wav_buffer);
                        // LockAudioDevice()
                        // StrCopy(sfxList[sfxID].name, filePath);
                        // sfxList[sfxID].buffer = (Sint16 *)wav_buffer;
                        // sfxList[sfxID].length = wav_length / sizeof(Sint16);
                        // sfxList[sfxID].loaded = false;
                        // UnlockAudioDevice()
                    }
                }
            }
        }
        else if (type == 'o') {
            // ogg sfx :(
            OggVorbis_File vf;
            ov_callbacks callbacks = OV_CALLBACKS_NOCLOSE;
            vorbis_info *vinfo;
            byte *buf;
            SDL_AudioSpec spec;
            int bitstream = -1;
            long samplesize;
            long samples;
            int read, toRead;

            currentStreamIndex++;
            currentStreamIndex %= STREAMFILE_COUNT;

            StreamFile *sfxFile = &streamFile[currentStreamIndex];
            sfxFile->vpos       = 0;
            sfxFile->vsize      = info.vfileSize;
            if (info.vfileSize > MUSBUFFER_SIZE)
                sfxFile->vsize = MUSBUFFER_SIZE;

            FileRead(streamFile[currentStreamIndex].buffer, sfxFile->vsize);
            CloseFile();

            callbacks.read_func  = readVorbis;
            callbacks.seek_func  = seekVorbis;
            callbacks.tell_func  = tellVorbis;
            callbacks.close_func = closeVorbis;

            // GetFileInfo(&info);
            int error = ov_open_callbacks(sfxFile, &vf, NULL, 0, callbacks);
            if (error != 0) {
                ov_clear(&vf);
                PrintLog("failed to load ogg sfx!");
                return;
            }

            vinfo = ov_info(&vf, -1);

            byte *audioBuf = NULL;
            uint audioLen  = 0;
            memset(&spec, 0, sizeof(SDL_AudioSpec));

            spec.format   = AUDIO_S16;
            spec.channels = vinfo->channels;
            spec.freq     = (int)vinfo->rate;
            spec.samples  = 4096; /* buffer size */

            samples = (long)ov_pcm_total(&vf, -1);

            audioLen = spec.size = (Uint32)(samples * spec.channels * 2);
            audioBuf             = (byte *)malloc(audioLen);
            buf                  = audioBuf;
            toRead               = audioLen;

            for (read = (int)ov_read(&vf, (char *)buf, toRead, 0, 2, 1, &bitstream); read > 0;
                 read = (int)ov_read(&vf, (char *)buf, toRead, 0, 2, 1, &bitstream)) {
                if (read < 0) {
                    free(audioBuf);
                    ov_clear(&vf);
                    PrintLog("failed to read ogg sfx!");
                    return;
                }
                toRead -= read;
                buf += read;
            }

            ov_clear(&vf); // clears & closes vorbis file

            /* Don't return a buffer that isn't a multiple of samplesize */
            samplesize = ((spec.format & 0xFF) / 8) * spec.channels;
            audioLen &= ~(samplesize - 1);

            SDL_AudioCVT convert;
            if (SDL_BuildAudioCVT(&convert, spec.format, spec.channels, spec.freq, audioDeviceFormat.format, audioDeviceFormat.channels,
                                  audioDeviceFormat.freq)
                > 0) {
                convert.buf = (byte *)malloc(audioLen * convert.len_mult);
                convert.len = audioLen;
                memcpy(convert.buf, audioBuf, audioLen);
                SDL_ConvertAudio(&convert);

                LockAudioDevice();
                StrCopy(sfxList[sfxID].name, filePath);
                sfxList[sfxID].buffer = (Sint16 *)convert.buf;
                sfxList[sfxID].length = convert.len_cvt / sizeof(Sint16);
                sfxList[sfxID].loaded = true;
                UnlockAudioDevice();
                free(audioBuf);
            }
            else {
                LockAudioDevice();
                StrCopy(sfxList[sfxID].name, filePath);
                sfxList[sfxID].buffer = (Sint16 *)audioBuf;
                sfxList[sfxID].length = audioLen / sizeof(Sint16);
                sfxList[sfxID].loaded = true;
                UnlockAudioDevice();
            }
        }
#endif
#if RETRO_PLATFORM == RETRO_PS3
        if (type == 'w') {
            // Native WAV loader for PS3 (Big Endian)
            byte *wavData = new byte[info.vfileSize];
            FileRead(wavData, info.vfileSize);
            CloseFile();

#define READ_LE32(p) ((uint32_t)(p)[0] | ((uint32_t)(p)[1] << 8) | ((uint32_t)(p)[2] << 16) | ((uint32_t)(p)[3] << 24))
#define READ_LE16(p) ((uint16_t)(p)[0] | ((uint16_t)(p)[1] << 8))

            if (strncmp((char *)wavData, "RIFF", 4) == 0 && strncmp((char *)(wavData + 8), "WAVE", 4) == 0) {
                byte *ptr = wavData + 12;
                int srcChannels = 0;
                int srcRate = 0;
                int bitsPerSample = 0;
                
                // Find 'fmt ' and 'data' chunks
                while (ptr < wavData + info.vfileSize - 8) {
                    if (strncmp((char *)ptr, "fmt ", 4) == 0) {
                        srcChannels = READ_LE16(ptr + 10);
                        srcRate = READ_LE32(ptr + 12);
                        bitsPerSample = READ_LE16(ptr + 22);
                    }
                    else if (strncmp((char *)ptr, "data", 4) == 0) {
                        uint32_t dataLen = READ_LE32(ptr + 4);
                        ptr += 8;
                        
                        if (srcChannels > 0 && (bitsPerSample == 16 || bitsPerSample == 8)) {
                            int sampleSize = bitsPerSample / 8;
                            int srcFrameCount = (int)(dataLen / (srcChannels * sampleSize));
                            
                            // We want 44100Hz Stereo
                            if (srcRate == 44100 && srcChannels == 2 && bitsPerSample == 16) {
                                // Fast path for 44100Hz Stereo 16-bit
                                Sint16 *buffer = (Sint16 *)memalign(16, srcFrameCount * 2 * sizeof(Sint16));
                                for (int i = 0; i < srcFrameCount * 2; ++i) {
                                    buffer[i] = (Sint16)READ_LE16(ptr + i * 2);
                                }
                                LockAudioDevice();
                                StrCopy(sfxList[sfxID].name, filePath);
                                sfxList[sfxID].buffer = buffer;
                                sfxList[sfxID].length = (size_t)(srcFrameCount * 2);
                                sfxList[sfxID].loaded = true;
                                UnlockAudioDevice();
                            } else {
                                double ratio = (double)srcRate / (double)AUDIO_FREQUENCY;
                                int dstFrameCount = (int)(srcFrameCount / ratio);
                                int dstSampleCount = dstFrameCount * 2;
                                
                                Sint16 *buffer = (Sint16 *)memalign(16, dstSampleCount * sizeof(Sint16));
                                uint32_t ratio_fp = (uint32_t)(ratio * 65536.0);
                                
                                for (int f = 0; f < dstFrameCount; f++) {
                                    uint64_t pos_fp = (uint64_t)f * ratio_fp;
                                    int i = (int)(pos_fp >> 16);
                                    uint32_t fract_fp = (uint32_t)(pos_fp & 0xFFFF);
                                    
                                    if (i + 1 >= srcFrameCount) {
                                        // Last frame
                                        byte *samplePtr = ptr + (i * srcChannels * sampleSize);
                                        Sint16 valL = (bitsPerSample == 16) ? (Sint16)READ_LE16(samplePtr) : (Sint16)((samplePtr[0] - 128) << 8);
                                        Sint16 valR = (srcChannels > 1) ? ((bitsPerSample == 16) ? (Sint16)READ_LE16(samplePtr + sampleSize) : (Sint16)((samplePtr[1] - 128) << 8)) : valL;
                                        buffer[f * 2 + 0] = valL;
                                        buffer[f * 2 + 1] = valR;
                                        continue;
                                    }

                                    byte *p1 = ptr + (i * srcChannels * sampleSize);
                                    byte *p2 = ptr + ((i + 1) * srcChannels * sampleSize);

                                    Sint16 s1L = (bitsPerSample == 16) ? (Sint16)READ_LE16(p1) : (Sint16)((p1[0] - 128) << 8);
                                    Sint16 s1R = (srcChannels > 1) ? ((bitsPerSample == 16) ? (Sint16)READ_LE16(p1 + sampleSize) : (Sint16)((p1[1] - 128) << 8)) : s1L;
                                    Sint16 s2L = (bitsPerSample == 16) ? (Sint16)READ_LE16(p2) : (Sint16)((p2[0] - 128) << 8);
                                    Sint16 s2R = (srcChannels > 1) ? ((bitsPerSample == 16) ? (Sint16)READ_LE16(p2 + sampleSize) : (Sint16)((p2[1] - 128) << 8)) : s2L;

                                    buffer[f * 2 + 0] = (Sint16)(s1L + (((int64_t)(s2L - s1L) * (int64_t)fract_fp) >> 16));
                                    buffer[f * 2 + 1] = (Sint16)(s1R + (((int64_t)(s2R - s1R) * (int64_t)fract_fp) >> 16));
                                }
                                
                                LockAudioDevice();
                                StrCopy(sfxList[sfxID].name, filePath);
                                sfxList[sfxID].buffer = buffer;
                                sfxList[sfxID].length = (size_t)dstSampleCount;
                                sfxList[sfxID].loaded = true;
                                UnlockAudioDevice();
                            }
                        }
                        break;
                    }
                    uint32_t chunkSize = READ_LE32(ptr + 4);
                    ptr += 8 + ((chunkSize + 1) & ~1);
                }
            }
            delete[] wavData;
        }
        else if (type == 'o') {
            OggVorbis_File *vf = (OggVorbis_File *)memalign(16, sizeof(OggVorbis_File));
            if (!vf) return;
            memset(vf, 0, sizeof(OggVorbis_File));

            vorbis_info *vinfo;
            int bitstream = -1;
            long samples;
            int read = 0;

            // Load OGG into a temporary structure instead of sharing with music slots
            StreamFile *sfxFile = (StreamFile*)memalign(16, sizeof(StreamFile));
            if (!sfxFile) {
                free(vf);
                return;
            }
            memset(sfxFile, 0, sizeof(StreamFile));

            sfxFile->buffer = (byte*)memalign(16, info.vfileSize);
            if (!sfxFile->buffer) {
                free(sfxFile);
                free(vf);
                return;
            }

            sfxFile->vpos = 0;
            sfxFile->vsize = (int)info.vfileSize;
            FileRead(sfxFile->buffer, info.vfileSize);
            CloseFile();

            ov_callbacks callbacks;
            callbacks.read_func  = readVorbis;
            callbacks.seek_func  = seekVorbis;
            callbacks.tell_func  = tellVorbis;
            callbacks.close_func = closeVorbis;

            int error = ov_open_callbacks(sfxFile, vf, NULL, 0, callbacks);
            if (error != 0) {
                if (sfxFile->buffer) free(sfxFile->buffer);
                free(sfxFile);
                free(vf);
                PrintLog("failed to load ogg sfx!");
                return;
            }

            vinfo = ov_info(vf, -1);
            if (!vinfo) {
                ov_clear(vf);
                free(vf);
                if (sfxFile->buffer) free(sfxFile->buffer);
                free(sfxFile);
                return;
            }
            samples = (long)ov_pcm_total(vf, -1);

            int channels = vinfo->channels;
            int srcRate = vinfo->rate;
            
            // Always convert to stereo (2 channels) for the engine's mixer
            double ratio = (double)srcRate / (double)AUDIO_FREQUENCY;
            int dstFrameCount = (int)(samples / ratio);
            size_t maxSamples = (size_t)(dstFrameCount * 2) + 16; 
            
            Sint16 *audioBuf = (Sint16 *)memalign(16, maxSamples * sizeof(Sint16));
            if (!audioBuf) {
                ov_clear(vf);
                free(vf);
                if (sfxFile->buffer) free(sfxFile->buffer);
                free(sfxFile);
                return;
            }
            
            if (channels == 2 && srcRate == 44100) {
                // Optimized path for 44100Hz Stereo (common case)
                size_t samples_written = 0;
                while (samples_written < maxSamples) {
                    read = (int)ov_read(vf, (char *)(audioBuf + samples_written), (int)((maxSamples - samples_written) * 2), &bitstream);
                    if (read <= 0) break;
                    samples_written += (read / 2);
                }
                
                LockAudioDevice();
                StrCopy(sfxList[sfxID].name, filePath);
                sfxList[sfxID].buffer = audioBuf;
                sfxList[sfxID].length = samples_written;
                sfxList[sfxID].loaded = true;
                UnlockAudioDevice();
            } else {
                // Linear Resampler for OGG SFX
                Sint16 *srcBuf = (Sint16 *)memalign(16, (samples + 1) * channels * sizeof(Sint16));
                if (!srcBuf) {
                    free(audioBuf);
                    ov_clear(vf);
                    free(vf);
                    if (sfxFile->buffer) free(sfxFile->buffer);
                    free(sfxFile);
                    return;
                }
                
                long samples_read = 0;
                while (samples_read < samples) {
                    read = (int)ov_read(vf, (char *)(srcBuf + samples_read * channels), (int)((samples - samples_read) * channels * 2), &bitstream);
                    if (read <= 0) break;
                    samples_read += (read / (channels * 2));
                }
                
                uint32_t ratio_fp = (uint32_t)(ratio * 65536.0);
                for (int f = 0; f < dstFrameCount; f++) {
                    uint64_t pos_fp = (uint64_t)f * ratio_fp;
                    int i = (int)(pos_fp >> 16);
                    uint32_t fract_fp = (uint32_t)(pos_fp & 0xFFFF);
                    
                    Sint16 s1L, s1R, s2L, s2R;
                    s1L = srcBuf[i * channels];
                    s1R = (channels > 1) ? srcBuf[i * channels + 1] : s1L;
                    
                    if (i + 1 < samples_read) {
                        s2L = srcBuf[(i + 1) * channels];
                        s2R = (channels > 1) ? srcBuf[(i + 1) * channels + 1] : s2L;
                    } else {
                        s2L = s1L;
                        s2R = s1R;
                    }
                    
                    audioBuf[f * 2 + 0] = (Sint16)(s1L + (((int64_t)(s2L - s1L) * (int64_t)fract_fp) >> 16));
                    audioBuf[f * 2 + 1] = (Sint16)(s1R + (((int64_t)(s2R - s1R) * (int64_t)fract_fp) >> 16));
                }
                
                free(srcBuf);
                
                LockAudioDevice();
                StrCopy(sfxList[sfxID].name, filePath);
                sfxList[sfxID].buffer = audioBuf;
                sfxList[sfxID].length = dstFrameCount * 2;
                sfxList[sfxID].loaded = true;
                UnlockAudioDevice();
            }
            if (read < 0) {
                free(audioBuf);
                if (sfxFile->buffer) free(sfxFile->buffer);
                free(sfxFile);
                ov_clear(vf);
                free(vf);
                PrintLog("failed to read ogg sfx!");
#if RETRO_PLATFORM == RETRO_PS3
                UnlockReader();
#endif
                return;
            }

            ov_clear(vf);
            free(vf);
            if (sfxFile->buffer) free(sfxFile->buffer);
            free(sfxFile);
        }
        else {
            CloseFile();
            PrintLog("Sfx format not supported!");
        }
#if RETRO_PLATFORM == RETRO_PS3
        UnlockReader();
#endif
#else
#if !RETRO_USING_SDL1 && !RETRO_USING_SDL2
        else {
            // wtf lol
            CloseFile();
            PrintLog("Sfx format not supported!");
        }
#endif
#endif
#endif
    }
    else {
#if RETRO_PLATFORM == RETRO_PS3
        UnlockReader();
#endif
    }
}
void PlaySfx(int sfx, bool loop)
{
    if (sfx < 0 || sfx >= SFX_COUNT) return;
    if (!sfxList[sfx].loaded || !sfxList[sfx].buffer) return;

    LockAudioDevice();
    int sfxChannelID = -1;
    for (int c = 0; c < CHANNEL_COUNT; ++c) {
        if (sfxChannels[c].sfxID == sfx || sfxChannels[c].sfxID == -1) {
            sfxChannelID = c;
            break;
        }
    }

    if (sfxChannelID >= 0) {
        ChannelInfo *sfxInfo  = &sfxChannels[sfxChannelID];
        sfxInfo->sfxID        = sfx;
        sfxInfo->samplePtr    = sfxList[sfx].buffer;
        sfxInfo->sampleLength = sfxList[sfx].length;
        sfxInfo->loopSFX      = loop;
        sfxInfo->pan          = 0;
    }
    UnlockAudioDevice();
}
void SetSfxAttributes(int sfx, int loopCount, sbyte pan)
{
    LockAudioDevice();
    int sfxChannel = -1;
    for (int i = 0; i < CHANNEL_COUNT; ++i) {
        if (sfxChannels[i].sfxID == sfx) {
            sfxChannel = i;
            break;
        }
    }
    if (sfxChannel == -1) {
        UnlockAudioDevice();
        return; // wasn't found
    }

    ChannelInfo *sfxInfo = &sfxChannels[sfxChannel];
    sfxInfo->loopSFX     = loopCount == -1 ? sfxInfo->loopSFX : loopCount;
    sfxInfo->pan         = pan;
    sfxInfo->sfxID       = sfx;
    UnlockAudioDevice();
}