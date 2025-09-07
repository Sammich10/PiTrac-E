/* Copyright 2005-2011 Mark Dufour and contributors; License Expat (See LICENSE)
 */
/* Copyright (C) 2022-2025, Verdant Consultants, LLC. */


#include "colorsys.h"
#include <algorithm>

/**
 * Conversion functions between RGB and other color systems.
 *
 * This modules provides two functions for each color system ABC:
 *
 *   rgb_to_abc(r, g, b) --> a, b, c
 *   abc_to_rgb(a, b, c) --> r, g, b
 *
 * All inputs and outputs are triples of floats in the range [0.0...1.0]
 * (with the exception of I and Q, which covers a slightly larger range).
 * Inputs outside the valid range may cause exceptions or invalid outputs.
 *
 * Supported color systems:
 * RGB: Red, Green, Blue components
 * YIQ: Luminance, Chrominance (used by composite video signals)
 * HLS: Hue, Luminance, Saturation
 * HSV: Hue, Saturation, Value
 */

namespace PiTrac
{
cv::Scalar colorsys::rgb_to_yiq(const cv::Scalar &rgb)
{
    const float y = (float)(((0.3f * rgb[0]) + (0.59f * rgb[1])) + (0.11f * rgb[2]));
    const float i = (float)(((0.6f * rgb[0]) - (0.28f * rgb[1])) - (0.32f * rgb[2]));
    const float q = (float)(((0.21f * rgb[0]) - (0.52f * rgb[1])) + (0.31f * rgb[2]));

    return (cv::Scalar(y, i, q));
}

cv::Scalar colorsys::yiq_to_rgb(const cv::Scalar &yiq)
{
    const float r = (float)std::clamp((double)(yiq[0] + (0.948262f * yiq[1])) +
                                      (0.624013f * yiq[2]),
                                      0.,
                                      1.);
    const float g = (float)std::clamp((double)(yiq[0] - (0.276066f * yiq[1])) - (0.63981f * yiq[2]),
                                      0.,
                                      1.);
    const float b = (float)std::clamp((double)(yiq[0] - (1.10545f * yiq[1])) + (1.72986f * yiq[2]),
                                      0.,
                                      1.);

    return (cv::Scalar(r, g, b));
}

cv::Scalar colorsys::rgb_to_hls(const cv::Scalar &rgb)
{
    const float maxc = std::max({ rgb[0], rgb[1], rgb[2] });
    const float minc = std::min({ rgb[0], rgb[1], rgb[2] });
    const float l = ((minc + maxc) / 2.0f);

    if (minc == maxc)
    {
        return (cv::Scalar(0.0, l, 0.0));
    }

    const float s = (l <=
                     0.5) ? ((maxc - minc) / (maxc + minc)) : ((maxc - minc) /
                                                               ((2.0f - maxc) - minc));

    const float rc = ((maxc - rgb[0]) / (maxc - minc));
    const float gc = ((maxc - rgb[1]) / (maxc - minc));
    const float bc = ((maxc - rgb[2]) / (maxc - minc));

    float h;
    if (rgb[0] == maxc)
    {
        h = (bc - gc);
    }
    else if (rgb[1] == maxc)
    {
        h = ((2.0f + rc) - bc);
    }
    else
    {
        h = ((4.0f + gc) - rc);
    }
    h = fmods((h / 6.0f), 1.0);

    return (cv::Scalar(h, l, s));
}

cv::Scalar colorsys::hls_to_rgb(const cv::Scalar &hls)
{
    const float h = (float)hls[0];
    const float l = (float)hls[1];
    const float s = (float)hls[2];

    if (s == 0.0)
    {
        return (cv::Scalar(l, l, l));
    }
    const float m2 = (l <= 0.5) ? (l * (1.0f + s)) : ((l + s) - (l * s));
    const float m1 = ((2.0f * l) - m2);
    return (cv::Scalar(_v(m1, m2, (h + ONE_THIRD)), _v(m1, m2, h), _v(m1, m2, (h - ONE_THIRD))));
}

cv::Scalar colorsys::rgb_to_hsv(const cv::Scalar &rgb)
{
    const float maxc = std::max({ rgb[0], rgb[1], rgb[2] });
    const float minc = std::min({ rgb[0], rgb[1], rgb[2] });
    const float v = maxc;
    // Achromatic case (grey). Hue is undefined, so return hue 0 saturation 0
    if (minc == maxc)
    {
        return (cv::Scalar(0.0, 0.0, v));
    }
    const float s = ((maxc - minc) / maxc);
    const float rc = ((maxc - rgb[0]) / (maxc - minc));
    const float gc = ((maxc - rgb[1]) / (maxc - minc));
    const float bc = ((maxc - rgb[2]) / (maxc - minc));
    float h;
    if (rgb[0] == maxc)
    {
        h = (bc - gc);
    }
    else if (rgb[1] == maxc)
    {
        h = ((2.0f + rc) - bc);
    }
    else
    {
        h = ((4.0f + gc) - rc);
    }
    h = fmods((h / 6.0f), 1.0);

    return (cv::Scalar(h, s, v));
}

cv::Scalar colorsys::hsv_to_rgb(const cv::Scalar &hsv)
{
    const float h = hsv[0];
    const float s = hsv[1];
    const float v = hsv[2];

    if (s == 0.0)
    {
        return (cv::Scalar(v, v, v));
    }

    const int i = (int)((h * 6.0f));
    const float f = ((h * 6.0f) - i);
    const float p = (v * (1.0f - s));
    const float q = (v * (1.0f - (s * f)));
    const float t = (v * (1.0f - (s * (1.0f - f))));
    const int i2 = (int)fmods((float)i, 6.0f);

    switch(i2)
    {
        case 0:
            return (cv::Scalar(v, t, p));
        case 1:
            return (cv::Scalar(q, v, p));
        case 2:
            return (cv::Scalar(p, v, t));
        case 3:
            return (cv::Scalar(p, q, v));
        case 4:
            return (cv::Scalar(t, p, v));
        case 5:
            return (cv::Scalar(v, p, q));
        default:
            return cv::Scalar(0, 0, 0);     // should never happen
    }
}

float colorsys::_v(float m1, float m2, float hue)
{
    hue = fmods(hue, 1.0);
    if ((hue < ONE_SIXTH))
    {
        return (m1 + (((m2 - m1) * hue) * 6.0f));
    }
    if (hue < 0.5)
    {
        return m2;
    }
    if (hue < TWO_THIRD)
    {
        return (m1 + (((m2 - m1) * (TWO_THIRD - hue)) * 6.0f));
    }
    return m1;
}
}