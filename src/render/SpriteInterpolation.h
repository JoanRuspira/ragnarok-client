#pragma once

#include "platform/WindowsCompat.h"

class CSprRes;
struct CMotion;

// Premultiplied ARGB output, matching BlitMotionToArgb. The origins are already
// sampled attachment positions; local ACT clip transforms are interpolated here.
// Width/height are physical pixels; origins and ACT transforms remain in source
// pixels. Raster scale renders the warp itself at higher resolution, rather than
// enlarging an already blended pose. Sharp sampling retains palette boundaries.
// Sharpness is an edge gain (1..16); 4 retains the original sharp-mode appearance.
void BlitInterpolatedMotionToArgb(unsigned int* dest, int width, int height,
    float x, float y, CSprRes* sprite, const CMotion* current, const CMotion* next,
    const unsigned int* palette, float fraction, bool sharpPixels = true, int rasterScale = 1,
    int sharpness = 4);

bool ComputeInterpolatedMotionBounds(CSprRes* sprite, const CMotion* current,
    const CMotion* next, float fraction, RECT* bounds);

bool DrawInterpolatedMotionToHdc(HDC hdc, float x, float y, CSprRes* sprite,
    const CMotion* current, const CMotion* next, const unsigned int* palette, float fraction,
    bool sharpPixels = true, int sharpness = 4);

void ClearSpriteInterpolationCache();
