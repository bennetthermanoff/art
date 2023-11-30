/**
 *  This file is part of ART.
 *
 *  Copyright 2023 Alberto Griggio <alberto.griggio@gmail.com>
 *
 *  ART is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  ART is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with ART.  If not, see <http://www.gnu.org/licenses/>.
 */

// @ART-label: "$CTL_EQUALIZER_H"
// @ART-colorspace: "rec2020"

import "_artlib";


float hue(float r, float g, float b)
{
    return rgb2hsl(r, g, b)[0];
}


const float centers[6] = {
    hue(1, 0, 0), hue(1, 0, 1), hue(0, 0, 1),
    hue(0, 1, 1), hue(0, 1, 0), hue(1, 1, 0)
};


float gauss_sum()
{
    float res = 0;
    const float sigma2 = M_PI / 6;
    for (int i = 0; i < 6; i = i+1) {
        res = res + gauss(centers[i], sigma2, 0);
    }
    return res;
}

const float w_sum = gauss_sum();


float get_factor(float h, int red, int magenta, int blue,
                 int cyan, int green, int yellow)
{
    const float f[6] = { red, magenta, blue, cyan, green, yellow };
    float res = 0;
    const float sigma2 = M_PI / 6;
    for (int i = 0; i < 6; i = i+1) {
        res = res + f[i]/100 * gauss(centers[i], sigma2, h);
    }
    return res / w_sum;
}


const float noise = pow(2, -16);

// @ART-param: ["mode", "$CTL_TARGET", ["$TP_COLORCORRECTION_H", "$TP_COLORCORRECTION_S", "$TP_COLORCORRECTION_L"]]
// @ART-param: ["red", "$TP_COLORCORRECTION_CHANNEL_R", -100, 100, 0]
// @ART-param: ["magenta", "$CTL_MAGENTA", -100, 100, 0]
// @ART-param: ["blue", "$TP_COLORCORRECTION_CHANNEL_B", -100, 100, 0]
// @ART-param: ["cyan", "$CTL_CYAN", -100, 100, 0]
// @ART-param: ["green", "$TP_COLORCORRECTION_CHANNEL_G", -100, 100, 0]
// @ART-param: ["yellow", "$CTL_YELLOW", -100, 100, 0]

void ART_main(varying float r, varying float g, varying float b,
              output varying float rout,
              output varying float gout,
              output varying float bout,
              int mode,
              int red,
              int magenta,
              int blue,
              int cyan,
              int green,
              int yellow)
{
    float hsl[3] = rgb2hsl(r, g, b);
    float f = get_factor(hsl[0], red, magenta, blue, cyan, green, yellow);
    if (mode == 0) {
        float s = f*f;
        if (f < 0) {
            s = -s;
        }
        hsl[0] = hsl[0] + s * M_PI;
    } else if (mode == 1) {
        float s = 1 + f;
        if (s < 0) {
            s = 0;
        }
        hsl[1] = hsl[1] * s;
    } else {
        float s = hsl[1];
        if (s > 1) {
            s = 1;
        }
        s = pow(2, 10 * (f * s));
        hsl[2] = hsl[2] * s;
    }
    float rgb[3] = hsl2rgb(hsl);
    rout = rgb[0];
    gout = rgb[1];
    bout = rgb[2];
}
