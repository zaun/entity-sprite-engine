#ifndef ESE_PCM_H
#define ESE_PCM_H

#include <stdint.h>

// Forward declaration
typedef struct EsePcm EsePcm;

// Creation and destruction. `samples` is owned by the EsePcm instance and
// must be allocated with the engine memory manager. It is interleaved
// PCM32F data with `channels` channels and `frame_count` frames.
EsePcm *pcm_create(float *samples, uint32_t frame_count, uint32_t channels, uint32_t sample_rate);
void pcm_free(EsePcm *pcm);

// Raw audio data accessors
const float *pcm_get_samples(const EsePcm *pcm);
uint32_t pcm_get_frame_count(const EsePcm *pcm);
uint32_t pcm_get_channels(const EsePcm *pcm);
uint32_t pcm_get_sample_rate(const EsePcm *pcm);

#endif // ESE_PCM_H
