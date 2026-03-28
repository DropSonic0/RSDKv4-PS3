#include "RetroEngine.hpp"
#include "AudioPS3.hpp"
#include "Audio.hpp"
#include <cell/audio.h>
#include <sys/event.h>
#include <sys/ppu_thread.h>
#include <sys/synchronization.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define AUDIO_BLOCKS 8
#define AUDIO_CHANNELS 2
#define AUDIO_OUT_RATE 48000

// We want to buffer enough for a few hardware blocks
#define INPUT_BUFFER_SAMPLES (CELL_AUDIO_BLOCK_SAMPLES * 4)

typedef struct {
    uint32_t audio_port;
    volatile bool quit_thread;
    sys_ppu_thread_t thread;
    
    uint32_t ratio_fp;
    uint32_t pos_fp;
    
    // Aligned for safety and padded to avoid overflows
    int16_t input_buffer[INPUT_BUFFER_SAMPLES * AUDIO_CHANNELS + 32] __attribute__((aligned(16))); 
    int input_pos; // in frames
    int input_count; // in frames

    float out_tmp[CELL_AUDIO_BLOCK_SAMPLES * AUDIO_CHANNELS] __attribute__((aligned(16)));
} ps3_audio_ctx_t;

static ps3_audio_ctx_t *aud_ctx = NULL;

static void audio_event_loop(uint64_t arg) {
    ps3_audio_ctx_t *ctx = (ps3_audio_ctx_t*)(uintptr_t)arg;
    sys_event_queue_t id;
    sys_ipc_key_t key;
    sys_event_t event;

    cellAudioCreateNotifyEventQueue(&id, &key);
    cellAudioSetNotifyEventQueue(key);

    while (!ctx->quit_thread) {
        int ret = sys_event_queue_receive(id, &event, SYS_NO_TIMEOUT);
        if (ret != CELL_OK) continue;
        
        for (int i = 0; i < CELL_AUDIO_BLOCK_SAMPLES; i++) {
            while (ctx->input_pos + 1 >= ctx->input_count) {
                // Carry over the last sample pair for interpolation.
                // New audio data starts at index 8. The last samples of the previous block
                // are therefore at (INPUT_BUFFER_SAMPLES) * 2 + 6 and + 7.
                if (ctx->input_count > 0) {
                    ctx->input_buffer[6] = ctx->input_buffer[INPUT_BUFFER_SAMPLES * AUDIO_CHANNELS + 6];
                    ctx->input_buffer[7] = ctx->input_buffer[INPUT_BUFFER_SAMPLES * AUDIO_CHANNELS + 7];
                }

                // Use a short timeout for the lock to balance responsiveness and stability
                if (sys_lwmutex_lock(&audioMutex, 1000) == CELL_OK) {
                    // ProcessAudioPlayback expects sample count (frames * channels)
                    // We start at index 8 for 16-byte alignment.
                    ProcessAudioPlayback(&ctx->input_buffer[8], INPUT_BUFFER_SAMPLES * AUDIO_CHANNELS);
                    sys_lwmutex_unlock(&audioMutex);
                    ctx->input_pos = 0;
                    ctx->input_count = INPUT_BUFFER_SAMPLES + 1;
                } else {
                    // Lock busy, output silence for this hardware block and try again later
                    for (int j = i; j < CELL_AUDIO_BLOCK_SAMPLES; j++) {
                        ctx->out_tmp[j * AUDIO_CHANNELS] = 0.0f;
                        ctx->out_tmp[j * AUDIO_CHANNELS + 1] = 0.0f;
                    }
                    goto skip_resample;
                }
            }

            uint32_t fract_fp = ctx->pos_fp & 0xFFFF;
            // Base offset is 6 (carry-over samples are at 6,7; new data at 8,9...)
            int p1 = (ctx->input_pos) * AUDIO_CHANNELS + 6;
            int p2 = (ctx->input_pos + 1) * AUDIO_CHANNELS + 6;

            for (int c = 0; c < AUDIO_CHANNELS; c++) {
                int16_t s1 = ctx->input_buffer[p1 + c];
                int16_t s2 = ctx->input_buffer[p2 + c];
                // Linear interpolation in fixed point. 
                // Cast to avoid overflow before shift (s2-s1 is max 65535, fract_fp is max 65535).
                int32_t res = (int32_t)s1 + (((int64_t)(s2 - s1) * (int64_t)fract_fp) >> 16);
                ctx->out_tmp[i * AUDIO_CHANNELS + c] = (float)res * (1.0f / 32768.0f);
            }

            ctx->pos_fp += ctx->ratio_fp;
            while (ctx->pos_fp >= 0x10000) {
                ctx->pos_fp -= 0x10000;
                ctx->input_pos++;
            }
        }
        
        cellAudioAddData(ctx->audio_port, ctx->out_tmp, CELL_AUDIO_BLOCK_SAMPLES, 1.0);
skip_resample:
        (void)0;
    }

    cellAudioRemoveNotifyEventQueue(key);
    sys_ppu_thread_exit(0);
}

bool InitPS3Audio(uint32_t samplerate, uint32_t buffersize) {
    (void)buffersize;
    aud_ctx = (ps3_audio_ctx_t*)calloc(1, sizeof(*aud_ctx));
    if (!aud_ctx) return false;

    cellAudioInit();

    CellAudioPortParam params;
    params.nChannel = AUDIO_CHANNELS;
    params.nBlock = AUDIO_BLOCKS;
    params.attr = 0;

    if (cellAudioPortOpen(&params, &aud_ctx->audio_port) != CELL_OK) {
        cellAudioQuit();
        free(aud_ctx);
        aud_ctx = NULL;
        return false;
    }

    aud_ctx->ratio_fp = (uint32_t)(((double)samplerate / (double)AUDIO_OUT_RATE) * 65536.0);
    aud_ctx->pos_fp = 0;
    aud_ctx->input_pos = 0;
    aud_ctx->input_count = 0;
    aud_ctx->quit_thread = false;

    memset(aud_ctx->input_buffer, 0, sizeof(aud_ctx->input_buffer));
    
    // Initial fill (using index 8 for alignment)
    if (sys_lwmutex_lock(&audioMutex, 1000000) == CELL_OK) {
        ProcessAudioPlayback(&aud_ctx->input_buffer[8], INPUT_BUFFER_SAMPLES * AUDIO_CHANNELS);
        sys_lwmutex_unlock(&audioMutex);
        aud_ctx->input_count = INPUT_BUFFER_SAMPLES + 1;
    }

    // Prime the buffers with silence to trigger initial events
    float silence[CELL_AUDIO_BLOCK_SAMPLES * AUDIO_CHANNELS];
    memset(silence, 0, sizeof(silence));
    for (int i = 0; i < AUDIO_BLOCKS; i++) {
        cellAudioAddData(aud_ctx->audio_port, silence, CELL_AUDIO_BLOCK_SAMPLES, 1.0);
    }

    cellAudioPortStart(aud_ctx->audio_port);
    
    sys_ppu_thread_create(&aud_ctx->thread, audio_event_loop, (uint64_t)(uintptr_t)aud_ctx, 500, 1048576, SYS_PPU_THREAD_CREATE_JOINABLE, "AudioThread");

    return true;
}

void WritePS3Audio(const audio_sample_t* buffer, uint32_t samples) {
    (void)buffer; (void)samples;
}

uint32_t GetPS3AudioFreeSamples() {
    return 0;
}

void ExitPS3Audio() {
    if (!aud_ctx) return;

    aud_ctx->quit_thread = true;
    uint64_t exit_code;
    sys_ppu_thread_join(aud_ctx->thread, &exit_code);

    cellAudioPortStop(aud_ctx->audio_port);
    cellAudioPortClose(aud_ctx->audio_port);
    cellAudioQuit();
    
    free(aud_ctx);
    aud_ctx = NULL;
}
