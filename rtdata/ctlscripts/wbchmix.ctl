// @ART-label: "Temperature/Primaries Correction"
// @ART-colorspace: "rec2020"

import "_artlib";


float[3] temp_to_xy(float temp)
{
    float T = temp;
    if (temp < 1667) {
        T = 1667;
    } else if (temp > 25000) {
        T = 25000;
    }
    const float T1 = 1e3 / T;
    const float T2 = T1 * T1;
    const float T3 = T2 * T1;
    float x;
    float y;
    if (T <= 4000) {
        x = -0.2661239 * T3 - 0.2343589 * T2 + 0.8776956 * T1 + 0.179910;
    } else {
        x = -3.0258469 * T3 + 2.1070379 * T2 + 0.2226347 * T1 + 0.24039;
    }
    const float x2 = x * x;
    const float x3 = x2 * x;
    if (T <= 2222) {
        y = -1.1063814 * x3 - 1.34811020 * x2 + 2.18555832 * x - 0.20219683;
    } else if (T <= 4000) {
        y = -0.9549476 * x3 - 1.37418593 * x2 + 2.09137015 * x - 0.16748867;
    } else {
        y = 3.0817580 * x3 - 5.87338670 * x2 + 3.75112997 * x - 0.37001483;
    }
    float res[3] = {x, y, 1.0 - x - y};
    return res;
}


float[3] temp_tint_to_xy(float temp, float tint)
{
    float xy[3] = temp_to_xy(temp);
    float x = xy[0];
    float y = xy[1];
    const float x2 = x * x;
    float f;
    if (temp <= 2222) {
        f = -1.1063814 * 3 * x2 - 1.34811020 * 2 * x + 2.18555832;
    } else if (temp <= 4000) {
        f = -0.9549476 * 3 * x2 - 1.37418593 * 2 * x + 2.09137015;
    } else {
        f = 3.0817580 * 3 * x2 - 5.87338670 * 2 * x + 3.75112997;
    }
    if (f == 0) {
        y = y + tint;
    } else {
        const float m = -1.0 / f;
        const float q = y - m * x;
        const float angle = atan(m);
        x = x + cos(angle) * tint;
        y = y + sin(angle) * tint;
    }
    
    float res[3] = { x, y, 1.0 - x - y };
    return res;
}


const float red[3] = { 1, 0, 0 };
const float green[3] = { 0, 1, 0 };
const float blue[3] = { 0, 0, 1 };
const float red_xy[3] = rgb2xy(red);
const float green_xy[3] = rgb2xy(green);
const float blue_xy[3] = rgb2xy(blue);


float[3] tweak_xy(float xy[3], float hue, float sat, float hrange, float srange)
{
    float x = xy[0] - D50_xy[0];
    float y = xy[1] - D50_xy[1];
    float radius = hypot(x, y) * (1.0 + sat * srange);
    float angle = atan2(y, x) + hue * hrange;
    float xx = radius * cos(angle) + D50_xy[0];
    float yy = radius * sin(angle) + D50_xy[1];
    float res[3] = { xx, yy, 1 - xx - yy };
    return res;
}


float[3][3] get_matrix(float rhue, float rsat, float ghue, float gsat,
                       float bhue, float bsat, float temp, float tint)
{
    float temp_k = 5000;
    if (temp > 0) {
        temp_k = temp_k + 100 * temp;
    } else {
        temp_k = temp_k + 32 * temp;
    }
    const float white[3] = temp_tint_to_xy(temp_k, tint / 1000);
    const float M[3][3] =
        matrix_from_primaries(red_xy, green_xy, blue_xy, white);
    const float N[3][3] = matrix_from_primaries(
        tweak_xy(red_xy, rhue / 100, rsat / 100, 0.47, 0.3),
        tweak_xy(green_xy, ghue / 100, gsat / 100, 0.63, 0.5),
        tweak_xy(blue_xy, bhue / 100, bsat / 100, 0.47, 0.5),
        D50_xy);
    const float res[3][3] = mult_f33_f33(invert_f33(M), N);
    return res;
}


// @ART-param: ["temp", "$TP_WBALANCE_TEMPERATURE", -100, 100, 0, 0.1]
// @ART-param: ["tint", "$TP_WBALANCE_GREEN", -100, 100, 0, 0.1]
// @ART-param: ["rhue", "Red hue", -100, 100, 0, 1, "$TP_CHMIXER_MODE_PRIMARIES_CHROMA"]
// @ART-param: ["rsat", "Red saturation", -100, 100, 0, 1, "$TP_CHMIXER_MODE_PRIMARIES_CHROMA"]
// @ART-param: ["ghue", "Green hue", -100, 100, 0, 1, "$TP_CHMIXER_MODE_PRIMARIES_CHROMA"]
// @ART-param: ["gsat", "Green saturation", -100, 100, 0, 1, "$TP_CHMIXER_MODE_PRIMARIES_CHROMA"]
// @ART-param: ["bhue", "Blue hue", -100, 100, 0, 1, "$TP_CHMIXER_MODE_PRIMARIES_CHROMA"]
// @ART-param: ["bsat", "Blue saturation", -100, 100, 0, 1, "$TP_CHMIXER_MODE_PRIMARIES_CHROMA"]

void ART_main(varying float r, varying float g, varying float b,
              output varying float rout,
              output varying float gout,
              output varying float bout,
              float temp, float tint,
              float rhue, float rsat,
              float ghue, float gsat,
              float bhue, float bsat)
{
    float M[3][3] = get_matrix(rhue, rsat, ghue, gsat, bhue, bsat, temp, tint);
    float rgb[3] = { r, g, b };
    rgb = mult_f3_f33(rgb, M);
    rout = rgb[0];
    gout = rgb[1];
    bout = rgb[2];
}
