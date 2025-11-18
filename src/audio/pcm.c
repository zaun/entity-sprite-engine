#include "audio/pcm.h"
#include "core/memory_manager.h"
#include "utility/log.h"

struct EsePcm {
    // Raw PCM data (interleaved, 32-bit float samples in range [-1, 1]).
    float *samples;
    uint32_t frame_count;
    uint32_t channels;
    uint32_t sample_rate;
};

EsePcm *pcm_create(float *samples, uint32_t frame_count, uint32_t channels, uint32_t sample_rate) {
    log_assert("PCM", samples, "pcm_create called with NULL samples");
    log_assert("PCM", channels > 0, "pcm_create called with 0 channels");

    EsePcm *pcm = memory_manager.malloc(sizeof(EsePcm), MMTAG_AUDIO);
    if (!pcm) {
        memory_manager.free(samples);
        return NULL;
    }

    pcm->samples = samples;
    pcm->frame_count = frame_count;
    pcm->channels = channels;
    pcm->sample_rate = sample_rate;

    return pcm;
}

void pcm_free(EsePcm *pcm) {
    log_assert("PCM", pcm, "pcm_free called with NULL pcm");

    if (pcm->samples) {
        memory_manager.free(pcm->samples);
    }
    memory_manager.free(pcm);
}

const float *pcm_get_samples(const EsePcm *pcm) {
    log_assert("PCM", pcm, "pcm_get_samples called with NULL pcm");
    return pcm->samples;
}

uint32_t pcm_get_frame_count(const EsePcm *pcm) {
    log_assert("PCM", pcm, "pcm_get_frame_count called with NULL pcm");
    return pcm->frame_count;
}

uint32_t pcm_get_channels(const EsePcm *pcm) {
    log_assert("PCM", pcm, "pcm_get_channels called with NULL pcm");
    return pcm->channels;
}

uint32_t pcm_get_sample_rate(const EsePcm *pcm) {
    log_assert("PCM", pcm, "pcm_get_sample_rate called with NULL pcm");
    return pcm->sample_rate;
}
