#pragma once

// This number has to be incremented whenever the PP3 file format is modified or the behaviour of a tool changes
#define PPVERSION 1008
#define PPVERSION_AEXP 301 //value of PPVERSION when auto exposure algorithm was modified

/*
  Log of version changes
  1008  2020-02-19
        parametric masks reorganization
  1007  2020-01-17
        added HSL mode to color correction
  1006  2019-12-26
        logenc: changed detail to preserveLocalContrast
  1005  2019-12-24
        added individual channel sliders for slope,offset,power in ColorCorrectionParams
  1004  2019-11-28
        added PPI and Unit to ResizeParams
  1003  2019-11-25
        increased sensitivity of ColorCorrectionParams.Offset
  1002  2019-10-20
        ToneEqualizer.Detail --> ToneEqualizer.Regularization
  1001  2019-10-06
        AreaMaskInverted --> MaskInverted
  1000  2019-07-29
        Bumped to 1000 for ART
   350  2019-07-07
        split ToneCurveParams into ExposureParams,
        BrightnessContrastSaturationParams and ToneCurveParams
   349  2019-01-14
        changed logenc.base to logenc.targetGray
   348  2018-12-30
        local smoothing
        color correction separate from color toning
   347  2018-12-13
        masks in CBDL
   346  2018-12-07
        new denoise parameters
   345  2018-10-21
        dual demosaic auto contrast threshold
   344  2018-10-04
        added Lab/RGB color space selection for shadows/highlights
   343  2018-09-06
        raw auto ca correction avoid colour shift
   342  2018-09-05
        raw auto ca correction iterations
   341  2018-07-22
        [ICM] enhanced custom output profile
   340  2018-07-08
        store whether curve is from histogram matching
   339  2018-07-04
        added allowUpscaling to ResizeParams
   338  2018-06-15
        increased precision for the channel mixer
   337  2018-06-13
        new scales for the LabGrid color toning parameters
   336  2018-06-01
        new demosaic method combobox for pixelshift
   335  2018-05-30
        new contrast adjuster in Bayer process tool
   334  2018-05-13
        new contrast threshold adjuster in Microcontrast tool
   333  2018-04-26
        new Shadows/Highlights tool
   332  2018-04-18
        changed pixelShiftEperIso calculation
   331  2018-02-14
        changed wavelet.Lmethod to int
   330  2018-01-20
        Added 'Auto-matched Tone Curve' button, performing histogram matching
   329  2017-09-12
        Added 'Enabled' flag for Channel Mixer, RGB Curves, HSV Equalizer and L*a*b* Adjustments
   328  2017-11-22
        Fix wrong type of ff_clipControl
   327  2017-09-15
        [Profiled Lens Correction] Added Lensfun
   326  2015-07-26
        [Exposure] Added 'Perceptual' tone curve mode
   325  2015-07-23
        [Exposure] [RGB Curves] [B&W] Normalized RGB pipeline curve gammas to sRGB (before it was a mix between sRGB and 1.0 and depended on file format)
   323  2015-10-05
        [Exposure] Added 'Luminance' tone curve mode
   322  2015-01-31
        [Wavelet] new tool using wavelet levels
   321  2014-08-17
        [Film Simulation] new  tool using HALDCLUT files
   320  2014-07-02  (yes, same version number... this is an error due to a wrong version number set in comment of previous change)
        New [RAW Bayer] and [RAW X-Trans] sections, with some parameters transferred from [RAW] to [RAW Bayer]
   320  2014-03-29
        [ColorToning] new tool for color toning
   319  2014-02-11
        Hue skin for Contrast by detail levels
   318  2014-02-10
        Vignetting Correction bug makes hard transitions for positive Amount values, Issue 2241
   317  2014-01-19
        changes to behaviour of LC curve, Issue 2209
   315  2013-12-12
        add LH et HH curve to lab mode
   313  2013-11-19
        add CL curve to lab mode
   312  2013-11-08
        added numerous changes to [channel mixer]
   311  2013-11-07
        [Gradient] new tool (gradient/graduated filter
        [PCVignette] new tool (vignette filter)
   310  2013-09-16
        Defringing /Threshold - changed calculation, issue 1801
   307  2013-03-16
        [Perspective] Horizontal and Vertical changed from int to double
        added  [Directional Pyramid Denoising] Method, Redchro, Bluechro
        added [RGB Curves] LumaMode
 */
