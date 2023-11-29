// @ART-label: "Equalizer by Hue"
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

// @ART-param: ["mode", "Target", ["Hue", "Saturation", "Lightness"]]
// @ART-param: ["red", "Red", -100, 100, 0]
// @ART-param: ["magenta", "Magenta", -100, 100, 0]
// @ART-param: ["blue", "Blue", -100, 100, 0]
// @ART-param: ["cyan", "Cyan", -100, 100, 0]
// @ART-param: ["green", "Green", -100, 100, 0]
// @ART-param: ["yellow", "Yellow", -100, 100, 0]

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
