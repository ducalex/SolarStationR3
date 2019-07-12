// Fake rolling average with no buffer!
#define AVGr(x) {x, 0, 0.00, 0.00, 0.00, 0.00}
#define AVGr_t avgr_handle_t

typedef struct {
    unsigned int nsamples;
    unsigned int count;
    float min; // All time min, not min in last nsamples
    float max; // All time max, not max in last nsamples
    float avg; // Decaying weighted average faking avg of last nsamples
    float val; // Last added value
} avgr_handle_t;


avgr_handle_t avgr_init(unsigned int nsamples)
{
    avgr_handle_t handle = AVGr(nsamples);
    return handle;
}


void avgr_add(avgr_handle_t *handle, float value)
{
    if (handle->count + 1 < handle->nsamples)
        handle->count++;
    handle->avg = (handle->avg * ((float)(handle->count - 1) / handle->count)) + value / handle->count;
    handle->val = value;
    if (value < handle->min || handle->count == 1) handle->min = value;
    if (value > handle->max || handle->count == 1) handle->max = value;
}
