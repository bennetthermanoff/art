/* -*- C++ -*-
 *
 *  This file is part of RawTherapee.
 *
 *  Copyright 2018 Alberto Griggio <alberto.griggio@gmail.com>
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  RawTherapee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef _OPENMP
#include <omp.h>
#endif

#include "labmasks.h"
#include "guidedfilter.h"
#include "sleef.c"
#include "coord.h"
#include "gauss.h"
#include "color.h"
#include "curves.h"
#include "iccstore.h"


namespace rtengine {

using procparams::AreaMask;
using procparams::LabCorrectionMask;

namespace {

#ifdef __SSE2__
void fastlin2log(float *x, float factor, float base, int w)
{
    float baseLog = 1.f / xlogf(base);
    vfloat baseLogv = F2V(baseLog);
    factor = factor * (base - 1.f);
    vfloat factorv = F2V(factor);
    vfloat onev = F2V(1.f);
    int i = 0;
    for (; i < w - 3; i += 4) {
        STVFU(x[i], xlogf(LVFU(x[i]) * factorv + onev) * baseLogv);
    }
    for (; i < w; ++i) {
        x[i] = xlogf(x[i] * factor + 1.f) * baseLog;
    }
}
#endif

bool generate_area_mask(int ox, int oy, int width, int height, array2D<float> &mask, const AreaMask &areaMask, bool enabled, float blur, bool multithread)
{
    if (!enabled || areaMask.shapes.empty() || areaMask.isTrivial()) {
        return false;
    }

    float w2 = float(width) / 2;
    float h2 = float(height) / 2;

    Coord origin(ox, oy);

    const auto inside =
        [&](int x, int y) -> bool
        {
            return (x >= 0 && x < mask.width() &&
                    y >= 0 && y < mask.height());
        };

    const int dir[] = { 1, 1, 1, -1, -1, 1, -1, -1 };

    constexpr float bgcolor = 1.f;
    constexpr float fgcolor = 1.f - bgcolor;

    // first fill with background
    array2D<float> guide(mask.width(), mask.height(), mask, 0);
    float *maskdata = mask;
    std::fill(maskdata, maskdata + (mask.width() * mask.height()), bgcolor);
    array2D<float> intersect;

    float min_feather = RT_INFINITY;

    for (const auto &area : areaMask.shapes) {
        Coord center(w2 + area.x / 100.0 * w2, h2 + area.y / 100.0 * h2);
        float area_w = area.width / 100.0 * width;
        float area_h = area.height / 100.0 * height;
    
        float a_min = area_w / 2;
        float b_min = area_h / 2;
        float r = b_min / a_min;
        float a_max = std::sqrt(2) * a_min;
        float a = a_max - area.roundness / 100.0 * (a_max - a_min);

        min_feather = std::min(a_min, b_min);

        const auto get =
            [&](int x, int y) -> Coord
            {
                PolarCoord p(Coord(x, y));
                double r, a;
                p.get(r, a);
                p.set(r, a - area.angle);
                Coord ret(p);
                ret += center;
                ret -= origin;
                return ret;
            };

        float **marr = mask;
        if (area.mode == AreaMask::Shape::INTERSECT) {
            intersect(mask.width(), mask.height());
            marr = intersect;
            float *p = intersect;
            std::fill(p, p + (mask.width() * mask.height()), bgcolor);
        }

        // draw the (bounded) ellipse
        for (int x = 0, n = int(a_min); x < n; ++x) {
            int yy = r * std::sqrt(a*a - float(x*x));
            for (int y = 0, m = std::min(yy, int(b_min)); y < m; ++y) {
                for (int d = 0; d < 4; ++d) {
                    int dx = dir[2*d], dy = dir[2*d+1];
                    Coord point = get(dx * x, dy * y);
                    for (int i = -1; i < 2; ++i) {
                        for (int j = -1; j < 2; ++j) {
                            if (inside(point.x+i, point.y+j)) {
                                switch (area.mode) {
                                case AreaMask::Shape::ADD:
                                case AreaMask::Shape::INTERSECT:
                                    marr[point.y+j][point.x+i] = fgcolor;
                                    break;
                                case AreaMask::Shape::SUBTRACT:
                                    marr[point.y+j][point.x+i] = bgcolor;
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        if (area.mode == AreaMask::Shape::INTERSECT) {
#ifdef _OPENMP
#           pragma omp parallel for if (multithread)
#endif
            for (int y = 0; y < mask.height(); ++y) {
                for (int x = 0; x < mask.width(); ++x) {
                    if (mask[y][x] == fgcolor && intersect[y][x] != fgcolor) {
                        mask[y][x] = bgcolor;
                    }
                }
            }
        }
    }

    // guided feathering and contrast
    int radius = std::max(int(areaMask.feather / 100.0 * min_feather), 1);
    guidedFilter(guide, mask, mask, radius, 1e-7, multithread);
    DiagonalCurve curve(areaMask.contrast);
    const auto contrast =
        [&curve](float x) -> float
        {
            return curve.getVal(x);
        };
#ifdef _OPENMP
#   pragma omp parallel for if (multithread)
#endif
    for (int y = 0; y < mask.height(); ++y) {
        for (int x = 0; x < mask.width(); ++x) {
            float v = LIM01(mask[y][x]);
            if (!areaMask.inverted) {
                v = 1.f - v;
            }
            mask[y][x] = contrast(v);
            assert(mask[y][x] == mask[y][x]);
        }
    }

    // and blur
    if (blur > 0.f) {
#ifdef _OPENMP
#       pragma omp parallel if (multithread)
#endif
        gaussianBlur(mask, mask, mask.width(), mask.height(), blur);
    }

    return true;
}

template <class T>
void rgb2lab(Imagefloat::Mode mode, float R, float G, float B, float &L, float &a, float &b, const T ws[3][3])
{
    switch (mode) {
    case Imagefloat::Mode::RGB:
        Color::rgb2lab(R, G, B, L, a, b, ws);
        return;
    case Imagefloat::Mode::YUV:
        Color::yuv2rgb(G, B, R, R, G, B, ws);
        Color::rgb2lab(R, G, B, L, a, b, ws);
        return;
    case Imagefloat::Mode::XYZ:
        Color::XYZ2Lab(R, G, B, L, a, b);
        return;
    case Imagefloat::Mode::LAB:
        L = G;
        a = R;
        b = B;
        return;
    }
}

} // namespace


bool generateLabMasks(Imagefloat *rgb, const std::vector<LabCorrectionMask> &masks, int offset_x, int offset_y, int full_width, int full_height, double scale, bool multithread, int show_mask_idx, std::vector<array2D<float>> *Lmask, std::vector<array2D<float>> *abmask)
{
    int n = masks.size();
    if (show_mask_idx >= n) {
        show_mask_idx = -1;
    }
    std::vector<std::unique_ptr<FlatCurve>> hmask(n);
    std::vector<std::unique_ptr<FlatCurve>> cmask(n);
    std::vector<std::unique_ptr<FlatCurve>> lmask(n);

    const int W = rgb->getWidth();
    const int H = rgb->getHeight();
    const auto mode = rgb->mode();

    const LabCorrectionMask dflt;

    const int begin_idx = max(show_mask_idx, 0);
    const int end_idx = (show_mask_idx < 0 ? n : show_mask_idx+1);
    bool has_mask = false;

    for (int i = begin_idx; i < end_idx; ++i) {
        auto &r = masks[i];
        if (!r.hueMask.empty() && r.hueMask[0] != FCT_Linear && r.hueMask != dflt.hueMask) {
            hmask[i].reset(new FlatCurve(r.hueMask, true));
            has_mask = true;
        }
        if (!r.chromaticityMask.empty() && r.chromaticityMask[0] != FCT_Linear && r.chromaticityMask != dflt.chromaticityMask) {
            cmask[i].reset(new FlatCurve(r.chromaticityMask, false));
            has_mask = true;
        }
        if (!r.lightnessMask.empty() && r.lightnessMask[0] != FCT_Linear && r.lightnessMask != dflt.lightnessMask) {
            lmask[i].reset(new FlatCurve(r.lightnessMask, false));
            has_mask = true;
        }
    }

    assert(!abmask || abmask->size() == size_t(n));
    assert(!Lmask || Lmask->size() == size_t(n));
    
    for (int i = begin_idx; i < end_idx; ++i) {
        if (abmask) {
            (*abmask)[i](W, H);
        }
        if (Lmask) {
            (*Lmask)[i](W, H);
        }
    }

    array2D<float> guide(W, H);
    TMatrix ws = ICCStore::getInstance()->workingSpaceMatrix(rgb->colorSpace());
    float wp[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            wp[i][j] = ws[i][j];
        }
    }
    
    if (has_mask) {
        // magic constant c_factor: normally chromaticity is in [0; 42000] (see color.h), but here we use the constant to match how the chromaticity pipette works (see improcfun.cc lines 4705-4706 and color.cc line 1930
        constexpr float c_factor = 327.68f / 48000.f;

#ifdef _OPENMP
#       pragma omp parallel if (multithread)
#endif
        {
#ifdef __SSE2__
            float cBuffer[W];
            float hBuffer[W];
            float lBuffer[W];
#endif
#ifdef _OPENMP
#           pragma omp for schedule(dynamic, 16)
#endif
            for (int y = 0; y < H; ++y) {
#ifdef __SSE2__
                for (int x = 0; x < W; ++x) {
                    rgb2lab(mode, rgb->r(y, x), rgb->g(y, x), rgb->b(y, x), lBuffer[x], cBuffer[x], hBuffer[x], wp);
                }
                // vectorized precalculation
                Color::Lab2Lch(cBuffer, hBuffer, cBuffer, hBuffer, W);
                fastlin2log(cBuffer, c_factor, 10.f, W);
#endif
                for (int x = 0; x < W; ++x) {
#ifdef __SSE2__
                    const float l = lBuffer[x] / 32768.f;
                    // use precalculated values
                    const float c = cBuffer[x];
                    float h = hBuffer[x];
#else
                    float l, a, b;
                    rgb2lab(mode, rgb->r(y, x), rgb->g(y, x), rgb->b(y, x), l, a, b, wp);
                    float c, h;
                    Color::Lab2Lch(a, b, c, h);
                    c = xlin2log(c * c_factor, 10.f);
#endif
                    guide[y][x] = LIM01(l);
                    h = Color::huelab_to_huehsv2(h);
                    h += 1.f/6.f; // offset the hue because we start from purple instead of red
                    if (h > 1.f) {
                        h -= 1.f;
                    }
                    h = xlin2log(h, 3.f);

                    for (int i = begin_idx; i < end_idx; ++i) {
                        auto &hm = hmask[i];
                        auto &cm = cmask[i];
                        auto &lm = lmask[i];
                        float blend = LIM01((hm ? hm->getVal(h) : 1.f) * (cm ? cm->getVal(c) : 1.f) * (lm ? lm->getVal(l) : 1.f));
                        if (Lmask) {
                            (*Lmask)[i][y][x] = blend;
                        }
                        if (abmask) {
                            (*abmask)[i][y][x] = blend;
                        }
                    }
                }
            }
        }

        for (int i = begin_idx; i < end_idx; ++i) {
            float blur = masks[i].maskBlur;
            blur = blur < 0.f ? -1.f/blur : 1.f + blur;
            int r1 = max(int(4 / scale * blur + 0.5), 1);
            int r2 = max(int(25 / scale * blur + 0.5), 1);
            if (abmask) {
                rtengine::guidedFilter(guide, (*abmask)[i], (*abmask)[i], r1, 0.001, multithread);
            }
            if (Lmask) {
                rtengine::guidedFilter(guide, (*Lmask)[i], (*Lmask)[i], r2, 0.0001, multithread);
            }
        }
    } else {
        for (int i = begin_idx; i < end_idx; ++i) {
#ifdef _OPENMP
#           pragma omp parallel for if (multithread)
#endif
            for (int y = 0; y < H; ++y) {
                if (Lmask) {
                    float *v = (*Lmask)[i][y];
                    std::fill(v, v + W, 1.f);
                }
                if (abmask) {
                    float *v = (*abmask)[i][y];
                    std::fill(v, v + W, 1.f);
                }
            }
        }
    }

    if (full_width < 0) {
        full_width = W;
    }
    if (full_height < 0) {
        full_height = H;
    }

    for (int i = begin_idx; i < end_idx; ++i) {
        if (generate_area_mask(offset_x, offset_y, full_width, full_height, guide, masks[i].areaMask, masks[i].areaEnabled, masks[i].maskBlur, multithread)) {
#ifdef _OPENMP
#           pragma omp parallel for if (multithread)
#endif
            for (int y = 0; y < H; ++y) {
                for (int x = 0; x < W; ++x) {
                    if (abmask) {
                        (*abmask)[i][y][x] *= guide[y][x];
                    }
                    if (Lmask) {
                        (*Lmask)[i][y][x] *= guide[y][x];
                    }
                }
            }
        }
    }

    if (show_mask_idx >= 0) {
        TMatrix iws = ICCStore::getInstance()->workingSpaceInverseMatrix(rgb->colorSpace());
        float iwp[3][3];
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                iwp[i][j] = iws[i][j];
            }
        }
        
        auto *smask = abmask ? abmask : Lmask;
        
#ifdef _OPENMP
        #pragma omp parallel for if (multithread)
#endif
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                auto blend = smask ? (*smask)[show_mask_idx][y][x] : 0.f;
                float l, a, b;
                rgb2lab(mode, rgb->r(y, x), rgb->g(y, x), rgb->b(y, x), l, a, b, wp);
                a = 0.f;
                b = blend * 42000.f;
                l = LIM(l + 32768.f * blend, 0.f, 32768.f);
                Color::lab2rgb(l, a, b, rgb->r(y, x), rgb->g(y, x), rgb->b(y, x), iwp);
            }
        }
        rgb->assignMode(Imagefloat::Mode::RGB);

        return false;
    }

    return true;
}


void fillPipetteLabMasks(Imagefloat *rgb, PlanarWhateverData<float>* editWhatever, LabMasksEditID id, bool multithread)
{
    TMatrix ws = ICCStore::getInstance()->workingSpaceMatrix(rgb->colorSpace());
    float wp[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            wp[i][j] = ws[i][j];
        }
    }
    const int W = rgb->getWidth();
    const int H = rgb->getHeight();
    const auto mode = rgb->mode();
    
#ifdef _OPENMP
#   pragma omp parallel for if (multithread)
#endif
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            float v = 0.f;
            float l, a, b;
            rgb2lab(mode, rgb->r(y, x), rgb->g(y, x), rgb->b(y, x), l, a, b, wp);
            switch (id) {
            case LabMasksEditID::H:
                v = Color::huelab_to_huehsv2(xatan2f(b, a));
                break;
            case LabMasksEditID::C:
                v = LIM01<float>(std::sqrt(SQR(a) + SQR(b) + 0.001f) / 48000.f);
                break;
            case LabMasksEditID::L:
                v = LIM01<float>(l / 32768.f);
                break;
            }
            editWhatever->v(y, x) = v;
        }
    }
}

} // namespace rtengine
