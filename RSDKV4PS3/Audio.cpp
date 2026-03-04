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

sys_lwmutex_t audioMutex;

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
    StopAllSfx(); //"init"

    PrintLog("PS3: StreamInfo size=%d, SFXInfo size=%d, ChannelInfo size=%d", sizeof(StreamInfo), sizeof(SFXInfo), sizeof(ChannelInfo));

    for (int i = 0; i < STREAMFILE_COUNT; ++i) {
        if (streamFile[i].buffer == NULL) {
            streamFile[i].buffer = (byte*)memalign(16, MUSBUFFER_SIZE);
        }
        streamFile[i].vsize = 0;
        streamFile[i].vpos = 0;

        MEM_ZERO(streamInfo[i]);
        PrintLog("PS3: streamInfo[%d] at %p", i, &streamInfo[i]);
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
    static bool mutex_created = false;
    if (!mutex_created) {
        sys_lwmutex_attribute_t mutexAttr;
        sys_lwmutex_attribute_initialize(mutexAttr);
        sys_lwmutex_create(&audioMutex, &mutexAttr);
        mutex_created = true;
    }

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
        Engine.LoadXMLSoundFX();
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

    // int index = -1;
    // for (int i = 0; i < STREAMFILE_COUNT; i++) if (file == &streamFile[i]) index = i;
    // PrintLog("PS3: readVorbis[%d] size=%d nmemb=%d elements=%d bytes=%d pos=%d", index, (int)size, (int)nmemb, (int)elements, (int)bytes_to_read, file->vpos);
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
    // int index = -1;
    // for (int i = 0; i < STREAMFILE_COUNT; i++) if (file == &streamFile[i]) index = i;
    // PrintLog("PS3: seekVorbis[%d] offset=%lld whence=%d newPos=%d", index, (long long)offset, (int)whence, newPos);
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
                PrintLog("PS3: Stream no cargado (ptr=%p status=%d loaded=%d)", streamInfoPtr, musicStatus, streamInfoPtr ? streamInfoPtr->loaded : -1);
                musicStatus = MUSIC_STOPPED;
                break;
            }
            int channels = streamInfoPtr->vorbisFile.vi->channels;
            size_t samples_wanted = (size_t)sampleCount;
            size_t samples_gotten = 0;
            while (samples_gotten < samples_wanted) {
                size_t samples_to_read = (samples_wanted - samples_gotten);
                if (channels == 1) {
                    // Capping to half to ensure expansion doesn't overflow mix_buffer
                    if (samples_to_read > (MIX_BUFFER_SAMPLES / 2)) samples_to_read = (MIX_BUFFER_SAMPLES / 2);
                    samples_to_read /= 2;
                }

                if (samples_to_read > (MIX_BUFFER_SAMPLES / channels))
                    samples_to_read = (MIX_BUFFER_SAMPLES / channels);
                if (samples_to_read == 0) break;

                // Ensure we read an even number of bytes (complete samples)
                long bytes_read = ov_read(&streamInfoPtr->vorbisFile, (char *)streamInfoPtr->buffer, (int)(samples_to_read * sizeof(Sint16)), 1,
                                          2, 1, &streamInfoPtr->vorbBitstream);
                if (bytes_read > 0 && bytes_read % 2 != 0) bytes_read--; // Align to sample
                // PrintLog("ProcessMusicStream: ov_read returned %ld bytes", bytes_read);

                if (bytes_read == 0) {
                    if (streamInfoPtr->trackLoop) {
                        PrintLog("PS3: ov_read retorno 0 (looping to %u, pos=%ld, size=%ld, loop=%d)", streamInfoPtr->loopPoint, (long)ov_pcm_tell(&streamInfoPtr->vorbisFile), (long)ov_pcm_total(&streamInfoPtr->vorbisFile, -1), streamInfoPtr->trackLoop);
                        int seek_err = ov_pcm_seek(&streamInfoPtr->vorbisFile, (ogg_int64_t)streamInfoPtr->loopPoint);
                        if (seek_err != 0) {
                            PrintLog("PS3: ov_pcm_seek loop error %d", seek_err);
                            musicStatus = MUSIC_STOPPED;
                            break;
                        }
                        continue;
                    }
                    else {
                        PrintLog("PS3: ov_read retorno 0 (fin, loop=%d)", streamInfoPtr->trackLoop);
                        musicStatus = MUSIC_STOPPED;
                        break;
                    }
                }

                if (bytes_read > 0) {
                    int samples_read = (int)(bytes_read / sizeof(Sint16));
                    if (channels == 1) {
                         Sint16 *buf = streamInfoPtr->buffer;
                         for (int s = samples_read - 1; s >= 0; s--) {
                             buf[s*2 + 0] = buf[s];
                             buf[s*2 + 1] = buf[s];
                         }
                         ProcessAudioMixing(stream + samples_gotten, buf, samples_read * 2, (bgmVolume * masterVolume) / MAX_VOLUME, 0);
                         samples_gotten += samples_read * 2;
                    } else {
                         ProcessAudioMixing(stream + samples_gotten, streamInfoPtr->buffer, samples_read, (bgmVolume * masterVolume) / MAX_VOLUME, 0);
                         samples_gotten += samples_read;
                    }
                }
                else if (bytes_read < 0) {
                    PrintLog("PS3: ov_read error %ld", bytes_read);
                    // Vorbis error, stop to avoid infinite loop
                    musicStatus = MUSIC_STOPPED;
                    break;
                }
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
#else
void ProcessAudioPlayback(Sint16 *stream, int sampleCount)
#endif
{
#if RETRO_USING_SDL1 || RETRO_USING_SDL2
    (void)userdata; // Unused
#endif

    if (!audioEnabled)
        return;

    Sint16 *output_buffer = (Sint16 *)stream;

#if RETRO_USING_SDL1 || RETRO_USING_SDL2
    size_t samples_remaining = (size_t)len / sizeof(Sint16);
#else
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
        if (samples_to_do % 2 != 0) samples_to_do--; 
        if (samples_to_do == 0) break;

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
                size_t sampleLen = (sfx->sampleLength < samples_needed) ? sfx->sampleLength : samples_needed;
                
                // SFX must be frame-aligned (even number of samples)
                if (sampleLen % 2 != 0) sampleLen--;

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
                
                if (sampleLen == 0) break; // Avoid infinite loop
            }

            if (samples_done > 0)
                ProcessAudioMixing(mix_buffer, sfx_temp_buffer, (int)samples_done, sfxVolume, sfx->pan);
        }
        UnlockAudioDevice();

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
        output_buffer += samples_to_do;
        samples_remaining -= samples_to_do;
    }
}

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
    int oldStreamID = currentStreamIndex;

    LockAudioDevice();
    streamInfoPtr = NULL;
    musicStatus = MUSIC_LOADING;

    currentStreamIndex++;
    currentStreamIndex %= STREAMFILE_COUNT;

    if (streamFile[currentStreamIndex].vsize > 0) {
        musicPosition = 0;
        FreeMusInfo(false);
    }
    UnlockAudioDevice();

    FileInfo info;
    if (LoadFile(musicTracks[currentMusicTrack].fileName, &info)) {
        StreamFile *musFile = &streamFile[currentStreamIndex];
        musFile->vpos       = 0;
        musFile->vsize      = info.vfileSize;
        if (info.vfileSize > MUSBUFFER_SIZE) {
            PrintLog("PS3: Music file too large (%d > %d), truncating", info.vfileSize, MUSBUFFER_SIZE);
            musFile->vsize = MUSBUFFER_SIZE;
        }

        FileRead(streamFile[currentStreamIndex].buffer, musFile->vsize);
        CloseFile();

        PrintLog("PS3: LoadMusic - Buffer filled with %d bytes", musFile->vsize);

        LockAudioDevice();

        StreamInfo *strmInfo = &streamInfo[currentStreamIndex];
        PrintLog("PS3: LoadMusic[%d] using StreamInfo at %p", currentStreamIndex, strmInfo);

        unsigned long long samples = 0;
        ov_callbacks callbacks;

        callbacks.read_func  = readVorbis;
        callbacks.seek_func  = seekVorbis;
        callbacks.tell_func  = tellVorbis;
        callbacks.close_func = closeVorbis;

        memset(&strmInfo->vorbisFile, 0, sizeof(strmInfo->vorbisFile));
        strmInfo->loaded = false;
        int error = ov_open_callbacks(musFile, &strmInfo->vorbisFile, NULL, 0, callbacks);
        PrintLog("PS3: ov_open_callbacks[%d] returned %d", currentStreamIndex, error);
        if (error == 0) {
            strmInfo->vorbBitstream = -1;
            strmInfo->vorbisFile.vi = ov_info(&strmInfo->vorbisFile, -1);

            samples = (unsigned long long)ov_pcm_total(&strmInfo->vorbisFile, -1);
            PrintLog("PS3: ov_pcm_total = %llu samples", samples);

#if RETRO_USING_SDL2
            strmInfo->stream = SDL_NewAudioStream(AUDIO_S16, strmInfo->vorbisFile.vi->channels, (int)strmInfo->vorbisFile.vi->rate,
                                                  audioDeviceFormat.format, audioDeviceFormat.channels, audioDeviceFormat.freq);
            if (!strmInfo->stream)
                PrintLog("Failed to create stream: %s", SDL_GetError());
#endif

#if RETRO_USING_SDL1
            playbackInfo->spec.format   = AUDIO_S16;
            playbackInfo->spec.channels = playbackInfo->vorbisFile.vi->channels;
            playbackInfo->spec.freq     = (int)playbackInfo->vorbisFile.vi->rate;
#endif

            if (musicStartPos) {
                uint oldPos = (uint)ov_pcm_tell(&streamInfo[oldStreamID].vorbisFile);

                float newPos  = oldPos * ((float)musicRatio * 0.0001f); // 8,000 == 0.8, 10,000 == 1.0 (ratio / 10,000)
                musicStartPos = (int)fmod((double)newPos, (double)samples);

                int err = ov_pcm_seek(&strmInfo->vorbisFile, musicStartPos);
                PrintLog("PS3: ov_pcm_seek (start) to %d returned %d", musicStartPos, err);
            }
            musicStartPos = 0;

            strmInfo->trackLoop = musicTracks[currentMusicTrack].trackLoop;
            strmInfo->loopPoint = musicTracks[currentMusicTrack].loopPoint;
            strmInfo->loaded    = true;
            streamFilePtr       = &streamFile[currentStreamIndex];
            streamInfoPtr       = &streamInfo[currentStreamIndex];

            PrintLog("PS3: LoadMusic success - slot=%d strmInfoPtr=%p loaded=%d loop=%d pos=%d", 
                currentStreamIndex, streamInfoPtr, (int)strmInfo->loaded, (int)strmInfo->trackLoop, (int)ov_pcm_tell(&strmInfo->vorbisFile));

            musicStatus         = MUSIC_PLAYING;
            masterVolume        = MAX_VOLUME;
            trackID             = currentMusicTrack;
            currentMusicTrack   = -1;
            musicPosition       = 0;
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
        }
    }
    else {
        musicStatus = MUSIC_STOPPED;
    }
    UnlockAudioDevice();
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

bool PlayMusic(int track, int musStartPos)
{
    if (!audioEnabled)
        return false;

    PrintLog("PS3: PlayMusic track=%d fileName=%s", track, musicTracks[track].fileName);

    if (musicTracks[track].fileName[0]) {
        if (musicStatus != MUSIC_LOADING) {
            if (track < 0 || track >= TRACK_COUNT) {
                StopMusic(true);
                currentMusicTrack = -1;
                return false;
            }
            musicStartPos     = musStartPos;
            currentMusicTrack = track;
            musicStatus       = MUSIC_LOADING;
            LoadMusic(NULL);
            return true;
        }
        else {
            PrintLog("WARNING music tried to play while music was loading!");
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

    if (LoadFile(fullPath, &info)) {
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
            // Native WAV loader for PS3
            byte *wavData = new byte[info.vfileSize];
            FileRead(wavData, info.vfileSize);
            CloseFile();

            // Simple WAV parser (RIFF, fmt, data)
            struct WAVHeader {
                char riff[4];
                uint32_t fileSize;
                char wave[4];
                char fmt[4];
                uint32_t fmtLen;
                uint16_t format;
                uint16_t channels;
                uint32_t sampleRate;
                uint32_t byteRate;
                uint16_t blockAlign;
                uint16_t bitsPerSample;
            } *header = (WAVHeader *)wavData;

            if (strncmp(header->riff, "RIFF", 4) == 0 && strncmp(header->wave, "WAVE", 4) == 0) {
                // Find 'data' chunk
                byte *ptr = wavData + 12;
                while (ptr < wavData + info.vfileSize - 8) {
                    if (strncmp((char *)ptr, "data", 4) == 0) {
                        uint32_t dataLen = *(uint32_t *)(ptr + 4);
                        ptr += 8;
                        
                        int srcChannels = header->channels;
                        int srcRate = header->sampleRate;
                        int sampleCount = (int)(dataLen / 2); // Sint16 samples in file
                        
                        // We want 44100Hz Stereo
                        int rateMul = (srcRate <= 22050) ? 2 : 1;
                        int dstSampleCount = (sampleCount / srcChannels) * 2 * rateMul;
                        
                        Sint16 *buffer = (Sint16 *)malloc(dstSampleCount * sizeof(Sint16));
                        Sint16 *src = (Sint16 *)ptr;
                        
                        for (int s = 0; s < dstSampleCount / 2; s++) {
                            int srcIdx = (s / rateMul) * srcChannels;
                            Sint16 valL = src[srcIdx];
                            // Swap endianness (WAV is LE, PS3 is BE)
                            valL = (Sint16)(((valL << 8) & 0xFF00) | ((valL >> 8) & 0x00FF));
                            
                            Sint16 valR = (srcChannels > 1) ? src[srcIdx + 1] : valL;
                            if (srcChannels > 1)
                                valR = (Sint16)(((valR << 8) & 0xFF00) | ((valR >> 8) & 0x00FF));
                            
                            buffer[s * 2 + 0] = valL;
                            buffer[s * 2 + 1] = valR;
                        }
                        
                        LockAudioDevice();
                        StrCopy(sfxList[sfxID].name, filePath);
                        sfxList[sfxID].buffer = buffer;
                        sfxList[sfxID].length = (size_t)dstSampleCount;
                        sfxList[sfxID].loaded = true;
                        UnlockAudioDevice();
                        break;
                    }
                    uint32_t chunkSize = *(uint32_t *)(ptr + 4);
                    ptr += 8 + chunkSize;
                }
            }
            delete[] wavData;
        }
        else if (type == 'o') {
            OggVorbis_File vf;
            vorbis_info *vinfo;
            int bitstream = -1;
            long samples;
            int read = 0;

            // Load OGG into a temporary structure instead of sharing with music slots
            PrintLog("PS3: Loading OGG SFX %s", filePath);
            StreamFile *sfxFile = (StreamFile*)malloc(sizeof(StreamFile));
            if (!sfxFile) return;
            sfxFile->buffer = (byte*)memalign(16, info.vfileSize);
            if (!sfxFile->buffer) { free(sfxFile); return; }

            sfxFile->vpos = 0;
            sfxFile->vsize = (int)info.vfileSize;
            FileRead(sfxFile->buffer, info.vfileSize);
            CloseFile();

            ov_callbacks callbacks;
            callbacks.read_func  = readVorbis;
            callbacks.seek_func  = seekVorbis;
            callbacks.tell_func  = tellVorbis;
            callbacks.close_func = closeVorbis;

            int error = ov_open_callbacks(sfxFile, &vf, NULL, 0, callbacks);
            if (error != 0) {
                if (sfxFile->buffer) free(sfxFile->buffer);
                free(sfxFile);
                PrintLog("failed to load ogg sfx %s error %d!", filePath, error);
                return;
            }

            vinfo = ov_info(&vf, -1);
            samples = (long)ov_pcm_total(&vf, -1);

            int channels = vinfo->channels;
            int srcRate = vinfo->rate;
            int rateMul = (srcRate <= 22050) ? 2 : 1;
            
            // Always convert to stereo (2 channels) for the engine's mixer
            // Add a safety margin to prevent overflow
            size_t maxSamples = (size_t)(samples * 2 * rateMul) + 2048; 
            Sint16 *audioBuf = (Sint16 *)memalign(16, maxSamples * sizeof(Sint16));
            if (!audioBuf) {
                if (sfxFile->buffer) free(sfxFile->buffer);
                free(sfxFile);
                ov_clear(&vf);
                return;
            }
            
            // Temporary buffer for decoding
            int decodeBufSize = 4096; // bytes
            Sint16 *decodeBuf = (Sint16 *)memalign(16, decodeBufSize);
            if (!decodeBuf) {
                free(audioBuf);
                if (sfxFile->buffer) free(sfxFile->buffer);
                free(sfxFile);
                ov_clear(&vf);
                return;
            }

            size_t samples_written = 0;
            size_t max_expansion_per_read = (decodeBufSize / 2) * 2 * rateMul; // max stereo samples produced per read
            while (samples_written + max_expansion_per_read < maxSamples && (read = (int)ov_read(&vf, (char *)decodeBuf, decodeBufSize, 1, 2, 1, &bitstream)) > 0) {
                int samples_read_per_channel = read / (channels * 2);
                if (samples_read_per_channel == 0) break;

                Sint16 *src = decodeBuf;
                Sint16 *dst = audioBuf + samples_written;

                for (int s = 0; s < samples_read_per_channel; s++) {
                    Sint16 valL = src[s * channels];
                    Sint16 valR = (channels > 1) ? src[s * channels + 1] : valL;
                    
                    for (int r = 0; r < rateMul; r++) {
                        dst[0] = valL;
                        dst[1] = valR;
                        dst += 2;
                    }
                }
                samples_written += (samples_read_per_channel * 2 * rateMul);
            }
            if (read < 0) {
                free(audioBuf);
                if (decodeBuf) free(decodeBuf);
                if (sfxFile->buffer) free(sfxFile->buffer);
                free(sfxFile);
                ov_clear(&vf);
                PrintLog("failed to read ogg sfx %s error %d!", filePath, read);
                return;
            }

            ov_clear(&vf);
            if (decodeBuf) free(decodeBuf);
            if (sfxFile->buffer) free(sfxFile->buffer);
            free(sfxFile);

            LockAudioDevice();
            StrCopy(sfxList[sfxID].name, filePath);
            sfxList[sfxID].buffer = audioBuf;
            sfxList[sfxID].length = samples_written;
            sfxList[sfxID].loaded = true;
            UnlockAudioDevice();
        }
        else {
            CloseFile();
            PrintLog("Sfx format not supported!");
        }
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
}
void PlaySfx(int sfx, bool loop)
{
    if (sfx < 0 || sfx >= SFX_COUNT) return;
    if (!sfxList[sfx].loaded || !sfxList[sfx].buffer) return;

    // PrintLog("PS3: PlaySfx id=%d (%s) loop=%d", sfx, sfxList[sfx].name, (int)loop);

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