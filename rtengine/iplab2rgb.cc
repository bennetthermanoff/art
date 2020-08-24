/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
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
#include "rtengine.h"
#include "improcfun.h"
#include <glibmm.h>
#include "iccstore.h"
#include "iccmatrices.h"
#include "../rtgui/options.h"
#include "settings.h"
#include "curves.h"
#include "alignedbuffer.h"
#include "color.h"


namespace rtengine {

extern const Settings* settings;

namespace {

inline void copyAndClampLine(const float *src, unsigned char *dst, const int W)
{
    for (int j = 0; j < W * 3; ++j) {
        dst[j] = uint16ToUint8Rounded(CLIP(src[j] * MAXVALF));
    }
}


inline void copyAndClamp(const Imagefloat *src, unsigned char *dst, const double rgb_xyz[3][3], bool multiThread)
{
    const int W = src->getWidth();
    const int H = src->getHeight();

#ifdef _OPENMP
        #pragma omp parallel for schedule(dynamic,16) if (multiThread)
#endif
    for (int i = 0; i < H; ++i) {
        float* rL = src->g.ptrs[i];
        float* ra = src->r.ptrs[i];
        float* rb = src->b.ptrs[i];
        int ix = i * 3 * W;

        float R, G, B;
        float x_, y_, z_;

        for (int j = 0; j < W; ++j) {
            Color::Lab2XYZ(rL[j], ra[j], rb[j], x_, y_, z_ );
            Color::xyz2rgb(x_, y_, z_, R, G, B, rgb_xyz);

            dst[ix++] = uint16ToUint8Rounded(Color::gamma2curve[CLIP(R)]);
            dst[ix++] = uint16ToUint8Rounded(Color::gamma2curve[CLIP(G)]);
            dst[ix++] = uint16ToUint8Rounded(Color::gamma2curve[CLIP(B)]);
        }
    }
}

} // namespace

// Used in ImProcCoordinator::updatePreviewImage  (rtengine/improccoordinator.cc)
//         Crop::update                           (rtengine/dcrop.cc)
//         Thumbnail::processImage                (rtengine/rtthumbnail.cc)
//
// If monitorTransform, divide by 327.68 then apply monitorTransform (which can integrate soft-proofing)
// otherwise divide by 327.68, convert to xyz and apply the sRGB transform, before converting with gamma2curve
void ImProcFunctions::lab2monitorRgb(Imagefloat *img, Image8* image)
{
    img->setMode(Imagefloat::Mode::LAB, multiThread);
    image->allocate(img->getWidth(), img->getHeight());
    
    if (monitorTransform) {

        const int W = img->getWidth();
        const int H = img->getHeight();
        unsigned char * data = image->data;

        // cmsDoTransform is relatively expensive
#ifdef _OPENMP
        #pragma omp parallel firstprivate(img, data, W, H)
#endif
        {
            AlignedBuffer<float> pBuf(3 * W);
            AlignedBuffer<float> mBuf(3 * W);

            AlignedBuffer<float> gwBuf1;
            AlignedBuffer<float> gwBuf2;

            if (gamutWarning) {
                gwBuf1.resize(3 * W);
                gwBuf2.resize(3 * W);
            }

            float *buffer = pBuf.data;
            float *outbuffer = mBuf.data;

#ifdef _OPENMP
            #pragma omp for schedule(dynamic,16)
#endif

            for (int i = 0; i < H; i++) {

                const int ix = i * 3 * W;
                int iy = 0;

                float* rL = img->g(i);
                float* ra = img->r(i);
                float* rb = img->b(i);

                for (int j = 0; j < W; j++) {
                    buffer[iy++] = rL[j] / 327.68f;
                    buffer[iy++] = ra[j] / 327.68f;
                    buffer[iy++] = rb[j] / 327.68f;
                }

                cmsDoTransform (monitorTransform, buffer, outbuffer, W);
                copyAndClampLine(outbuffer, data + ix, W);

                if (gamutWarning) {
                    gamutWarning->markLine(image, i, buffer, gwBuf1.data, gwBuf2.data);
                }
            }
        } // End of parallelization
    } else {
        copyAndClamp(img, image->data, sRGB_xyz, multiThread);
    }
}



// Used in ImProcCoordinator::updatePreviewImage  (rtengine/improccoordinator.cc)
//         Crop::update                           (rtengine/dcrop.cc)
//
// Generate an Image8
//
// If output profile used, divide by 327.68 then apply the "profile" profile (eventually with a standard gamma)
// otherwise divide by 327.68, convert to xyz and apply the RGB transform, before converting with gamma2curve
Image8* ImProcFunctions::lab2rgb(Imagefloat *img, int cx, int cy, int cw, int ch, const procparams::ColorManagementParams &icm, bool consider_histogram_settings)
{
    img->setMode(Imagefloat::Mode::LAB, multiThread);
    //gamutmap(lab);

    if (cx < 0) {
        cx = 0;
    }

    if (cy < 0) {
        cy = 0;
    }

    const int W = img->getWidth();
    const int H = img->getHeight();

    if (cx + cw > W) {
        cw = W - cx;
    }

    if (cy + ch > H) {
        ch = H - cy;
    }

    Image8* image = new Image8(cw, ch);
    Glib::ustring profile;

    cmsHPROFILE oprof = nullptr;

    if (settings->HistogramWorking && consider_histogram_settings) {
        profile = icm.workingProfile;
    } else {
        profile = icm.outputProfile;

        if (icm.outputProfile.empty() || icm.outputProfile == ColorManagementParams::NoICMString) {
            profile = "sRGB";
        }
        oprof = ICCStore::getInstance()->getProfile(profile);
    }


    if (oprof) {
        cmsHPROFILE oprofG = oprof;
        cmsUInt32Number flags = cmsFLAGS_NOOPTIMIZE | cmsFLAGS_NOCACHE;

        if (icm.outputBPC) {
            flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;
        }

        lcmsMutex->lock();
        cmsHPROFILE LabIProf  = cmsCreateLab4Profile(nullptr);
        cmsHTRANSFORM hTransform = cmsCreateTransform (LabIProf, TYPE_Lab_DBL, oprofG, TYPE_RGB_FLT, icm.outputIntent, flags);  // NOCACHE is important for thread safety
        cmsCloseProfile(LabIProf);
        lcmsMutex->unlock();

        unsigned char *data = image->data;

        // cmsDoTransform is relatively expensive
#ifdef _OPENMP
        #pragma omp parallel
#endif
        {
            AlignedBuffer<double> pBuf(3 * cw);
            AlignedBuffer<float> oBuf(3 * cw);
            double *buffer = pBuf.data;
            float *outbuffer = oBuf.data;
            int condition = cy + ch;

#ifdef _OPENMP
#           pragma omp for firstprivate(img) schedule(dynamic,16)
#endif

            for (int i = cy; i < condition; i++) {
                const int ix = i * 3 * cw;
                int iy = 0;
                float* rL = img->g(i);
                float* ra = img->r(i);
                float* rb = img->b(i);

                for (int j = cx; j < cx + cw; j++) {
                    buffer[iy++] = rL[j] / 327.68f;
                    buffer[iy++] = ra[j] / 327.68f;
                    buffer[iy++] = rb[j] / 327.68f;
                }

                cmsDoTransform (hTransform, buffer, outbuffer, cw);
                copyAndClampLine(outbuffer, data + ix, cw);
            }
        } // End of parallelization

        cmsDeleteTransform(hTransform);

        if (oprofG != oprof) {
            cmsCloseProfile(oprofG);
        }
    } else {
        const auto xyz_rgb = ICCStore::getInstance()->workingSpaceInverseMatrix(profile);
        copyAndClamp(img, image->data, xyz_rgb, multiThread);
    }

    return image;
}


/** @brief Convert the final Lab image to the output RGB color space
 *
 * Used in processImage   (rtengine/simpleprocess.cc)
 *
 * Provide a pointer to a 7 floats array for "ga" (uninitialized ; this array will be filled with the gamma values) if you want
 * to use the custom gamma scenario. Those gamma values will correspond to the ones of the chosen standard output profile
 * (Prophoto if non standard output profile given)
 *
 * If "ga" is NULL, then we're considering standard gamma with the chosen output profile.
 *
 * Generate an Image16
 *
 * If a custom gamma profile can be created, divide by 327.68, convert to xyz and apply the custom gamma transform
 * otherwise divide by 327.68, convert to xyz and apply the sRGB transform, before converting with gamma2curve
 */
Imagefloat* ImProcFunctions::lab2rgbOut(Imagefloat *img, int cx, int cy, int cw, int ch, const procparams::ColorManagementParams &icm)
{
    img->setMode(Imagefloat::Mode::LAB, multiThread);
    const int W = img->getWidth();
    const int H = img->getHeight();
    
    if (cx < 0) {
        cx = 0;
    }

    if (cy < 0) {
        cy = 0;
    }

    if (cx + cw > W) {
        cw = W - cx;
    }

    if (cy + ch > H) {
        ch = H - cy;
    }

    Imagefloat* image = new Imagefloat(cw, ch);
    cmsHPROFILE oprof = ICCStore::getInstance()->getProfile(icm.outputProfile);

    if (oprof) {
        cmsUInt32Number flags = cmsFLAGS_NOOPTIMIZE | cmsFLAGS_NOCACHE;

        if (icm.outputBPC) {
            flags |= cmsFLAGS_BLACKPOINTCOMPENSATION;
        }

        lcmsMutex->lock();
        cmsHPROFILE iprof = cmsCreateLab4Profile(nullptr);
        cmsHTRANSFORM hTransform = cmsCreateTransform(iprof, TYPE_Lab_FLT, oprof, TYPE_RGB_FLT, icm.outputIntent, flags);
        lcmsMutex->unlock();

        image->ExecCMSTransform(hTransform, img, cx, cy);
        cmsDeleteTransform(hTransform);
    } else if (icm.outputProfile != procparams::ColorManagementParams::NoProfileString) {
        
#ifdef _OPENMP
        #pragma omp parallel for schedule(dynamic,16) if (multiThread)
#endif

        for (int i = cy; i < cy + ch; i++) {
            float R, G, B;
            float* rL = img->g(i);
            float* ra = img->r(i);
            float* rb = img->b(i);

            for (int j = cx; j < cx + cw; j++) {

                float fy = (Color::c1By116 * rL[j]) / 327.68f + Color::c16By116; // (L+16)/116
                float fx = (0.002f * ra[j]) / 327.68f + fy;
                float fz = fy - (0.005f * rb[j]) / 327.68f;
                float LL = rL[j] / 327.68f;

                float x_ = 65535.0f * Color::f2xyz(fx) * Color::D50x;
                //float y_ = 65535.0 * Color::f2xyz(fy);
                float z_ = 65535.0f * Color::f2xyz(fz) * Color::D50z;
                float y_ = (LL > (float)Color::epskap) ? 65535.0f * fy * fy * fy : 65535.0f * LL / (float)Color::kappa;

                Color::xyz2srgb(x_, y_, z_, R, G, B);

                image->r(i - cy, j - cx) = Color::gamma2curve[CLIP(R)];
                image->g(i - cy, j - cx) = Color::gamma2curve[CLIP(G)];
                image->b(i - cy, j - cx) = Color::gamma2curve[CLIP(B)];
            }
        }
    } else {
        img->copyTo(image);
        image->setMode(Imagefloat::Mode::RGB, multiThread);
    }

    return image;
}

} // namespace rtengine

