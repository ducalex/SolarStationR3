// Fake rolling average with no buffer!
#include <float.h>

#define new_AVGr(x) {x, 0, FLT_MAX, FLT_MIN, 0.00}
#define AVGr avgr_handle_t

typedef struct {
    short nsamples;
    short count;
    float min; // All time min, not min in last nsamples
    float max; // All time max, not max in last nsamples
    float avg; // Decaying weighted average faking avg of last nsamples
} avgr_handle_t;


avgr_handle_t avgr_init(short nsamples)
{
    avgr_handle_t handle = {nsamples, 0, FLT_MAX, FLT_MIN, 0.00};

    return handle;
}


void avgr_add(avgr_handle_t *handle, float value)
{
    if (handle->count + 1 < handle->nsamples)
        handle->count++;
    handle->avg = (handle->avg * ((float)(handle->count - 1) / handle->count)) + value / handle->count;
    if (value < handle->min) handle->min = value;
    if (value > handle->max) handle->max = value;
}
