#pragma once

#include <stdint.h>

/* Layout consumed by 0x821dcfe8 and forwarded to 0x821da938. */
typedef struct Ace6Viewport {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    float min_depth;
    float max_depth;
} Ace6Viewport;

#if defined(__cplusplus)
static_assert(sizeof(Ace6Viewport) == 24);
#else
_Static_assert(sizeof(Ace6Viewport) == 24, "Ace6Viewport size");
#endif
