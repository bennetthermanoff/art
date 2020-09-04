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
#include "histogrampanel.h"
#include "multilangmgr.h"
#include "guiutils.h"
#include "options.h"
#include <cstring>
#include <cmath>
#include "../rtengine/LUT.h"
#include "rtimage.h"
#include "../rtengine/improccoordinator.h"
#include "../rtengine/color.h"
#include "../rtengine/opthelper.h"
#include "../rtengine/iccstore.h"

using namespace rtengine;

extern Options options;


//
//
// HistogramPanel
HistogramPanel::HistogramPanel ()
{
    setExpandAlignProperties(this, true, true, Gtk::ALIGN_FILL, Gtk::ALIGN_FILL);
    set_name("HistogramPanel");

    histogramArea = Gtk::manage (new HistogramArea (this));
    setExpandAlignProperties(histogramArea, true, true, Gtk::ALIGN_FILL, Gtk::ALIGN_FILL);

    histogramRGBArea = Gtk::manage (new HistogramRGBArea ());
    setExpandAlignProperties(histogramRGBArea, true, false, Gtk::ALIGN_FILL, Gtk::ALIGN_END);
    histogramRGBArea->show();

    // connecting the two childs
    histogramArea->signal_factor_changed().connect( sigc::mem_fun(*histogramRGBArea, &HistogramRGBArea::factorChanged) );

    gfxGrid = Gtk::manage (new Gtk::Grid ());
    gfxGrid->set_orientation(Gtk::ORIENTATION_VERTICAL);
    gfxGrid->set_row_spacing(1);
    gfxGrid->set_column_spacing(1);
    histogramRGBArea->setParent(gfxGrid);
    gfxGrid->add(*histogramArea);

    if (options.histogramBar) {
        gfxGrid->add (*histogramRGBArea);
    }

    redImage   = new RTImage ("histogram-red-on-small.png");
    greenImage = new RTImage ("histogram-green-on-small.png");
    blueImage  = new RTImage ("histogram-blue-on-small.png");
    valueImage = new RTImage ("histogram-silver-on-small.png");
    chroImage  = new RTImage ("histogram-gold-on-small.png");
    rawImage   = new RTImage ("histogram-bayer-on-small.png");
    barImage   = new RTImage ("histogram-bar-on-small.png");

    redImage_g   = new RTImage ("histogram-red-off-small.png");
    greenImage_g = new RTImage ("histogram-green-off-small.png");
    blueImage_g  = new RTImage ("histogram-blue-off-small.png");
    valueImage_g = new RTImage ("histogram-silver-off-small.png");
    chroImage_g  = new RTImage ("histogram-gold-off-small.png");
    rawImage_g   = new RTImage ("histogram-bayer-off-small.png");
    barImage_g   = new RTImage ("histogram-bar-off-small.png");

    mode_images_[0] = new RTImage ("histogram-mode-linear-small.png");
    mode_images_[1] = new RTImage ("histogram-mode-logx-small.png");
    mode_images_[2] = new RTImage ("histogram-mode-logxy-small.png");
    mode_tips_[0] = M("HISTOGRAM_TOOLTIP_MODE_LINEAR");
    mode_tips_[1] = M("HISTOGRAM_TOOLTIP_MODE_LOG_X");
    mode_tips_[2] = M("HISTOGRAM_TOOLTIP_MODE_LOG_XY");

    showRed   = Gtk::manage (new Gtk::ToggleButton ());
    showGreen = Gtk::manage (new Gtk::ToggleButton ());
    showBlue  = Gtk::manage (new Gtk::ToggleButton ());
    showValue = Gtk::manage (new Gtk::ToggleButton ());
    showChro  = Gtk::manage (new Gtk::ToggleButton ());
    showRAW   = Gtk::manage (new Gtk::ToggleButton ());
    showMode  = Gtk::manage (new Gtk::Button ());
    showBAR   = Gtk::manage (new Gtk::ToggleButton ());

    showRed->set_name("histButton");
    showRed->set_can_focus(false);
    showGreen->set_name("histButton");
    showGreen->set_can_focus(false);
    showBlue->set_name("histButton");
    showBlue->set_can_focus(false);
    showValue->set_name("histButton");
    showValue->set_can_focus(false);
    showChro->set_name("histButton");
    showChro->set_can_focus(false);
    showRAW->set_name("histButton");
    showRAW->set_can_focus(false);
    showMode->set_name("histButton");
    showMode->set_can_focus(false);
    showBAR->set_name("histButton");
    showBAR->set_can_focus(false);

    showRed->set_relief (Gtk::RELIEF_NONE);
    showGreen->set_relief (Gtk::RELIEF_NONE);
    showBlue->set_relief (Gtk::RELIEF_NONE);
    showValue->set_relief (Gtk::RELIEF_NONE);
    showChro->set_relief (Gtk::RELIEF_NONE);
    showRAW->set_relief (Gtk::RELIEF_NONE);
    showMode->set_relief (Gtk::RELIEF_NONE);
    showBAR->set_relief (Gtk::RELIEF_NONE);

    showRed->set_tooltip_text   (M("HISTOGRAM_TOOLTIP_R"));
    showGreen->set_tooltip_text (M("HISTOGRAM_TOOLTIP_G"));
    showBlue->set_tooltip_text  (M("HISTOGRAM_TOOLTIP_B"));
    showValue->set_tooltip_text (M("HISTOGRAM_TOOLTIP_L"));
    showChro->set_tooltip_text  (M("HISTOGRAM_TOOLTIP_CHRO"));
    showRAW->set_tooltip_text   (M("HISTOGRAM_TOOLTIP_RAW"));
    showMode->set_tooltip_text  (M("HISTOGRAM_TOOLTIP_MODE"));
    showBAR->set_tooltip_text   (M("HISTOGRAM_TOOLTIP_BAR"));

    buttonGrid = Gtk::manage (new Gtk::Grid ());
    buttonGrid->set_orientation(Gtk::ORIENTATION_VERTICAL);
    showRed->set_active   (options.histogramRed);
    showGreen->set_active (options.histogramGreen);
    showBlue->set_active  (options.histogramBlue);
    showValue->set_active (options.histogramLuma);
    showChro->set_active  (options.histogramChroma);
    showRAW->set_active   (options.histogramRAW);
    // no showMode->set_active(), as it's not a ToggleButton
    showBAR->set_active   (options.histogramBar);

    showRed->set_image   (showRed->get_active()   ? *redImage   : *redImage_g);
    showGreen->set_image (showGreen->get_active() ? *greenImage : *greenImage_g);
    showBlue->set_image  (showBlue->get_active()  ? *blueImage  : *blueImage_g);
    showValue->set_image (showValue->get_active() ? *valueImage : *valueImage_g);
    showChro->set_image  (showChro->get_active()   ? *chroImage : *chroImage_g);
    showRAW->set_image   (showRAW->get_active()   ? *rawImage   : *rawImage_g);
    toggleButtonMode();
    showBAR->set_image   (showBAR->get_active()   ? *barImage   : *barImage_g);

    setExpandAlignProperties(showRed  , false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    setExpandAlignProperties(showGreen, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    setExpandAlignProperties(showBlue , false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    setExpandAlignProperties(showValue, false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    setExpandAlignProperties(showChro , false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    setExpandAlignProperties(showRAW  , false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    setExpandAlignProperties(showMode , false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);
    setExpandAlignProperties(showBAR  , false, false, Gtk::ALIGN_START, Gtk::ALIGN_CENTER);

    showRed->signal_toggled().connect( sigc::mem_fun(*this, &HistogramPanel::red_toggled), showRed );
    showGreen->signal_toggled().connect( sigc::mem_fun(*this, &HistogramPanel::green_toggled), showGreen );
    showBlue->signal_toggled().connect( sigc::mem_fun(*this, &HistogramPanel::blue_toggled), showBlue );
    showValue->signal_toggled().connect( sigc::mem_fun(*this, &HistogramPanel::value_toggled), showValue );
    showChro->signal_toggled().connect( sigc::mem_fun(*this, &HistogramPanel::chro_toggled), showChro );
    showRAW->signal_toggled().connect( sigc::mem_fun(*this, &HistogramPanel::raw_toggled), showRAW );
    showMode->signal_released().connect( sigc::mem_fun(*this, &HistogramPanel::mode_released), showMode );
    showBAR->signal_toggled().connect( sigc::mem_fun(*this, &HistogramPanel::bar_toggled), showBAR );

    buttonGrid->add (*showRed);
    buttonGrid->add (*showGreen);
    buttonGrid->add (*showBlue);
    buttonGrid->add (*showValue);
    buttonGrid->add (*showChro);
    buttonGrid->add (*showRAW);
    buttonGrid->add (*showMode);
    buttonGrid->add (*showBAR);

    // Put the button vbox next to the window's border to be less disturbing
    if (options.histogramPosition == 1) {
        add (*buttonGrid);
        add (*gfxGrid);
    } else {
        add (*gfxGrid);
        add (*buttonGrid);
    }

    show_all ();

    rconn = signal_size_allocate().connect( sigc::mem_fun(*this, &HistogramPanel::resized) );
}

HistogramPanel::~HistogramPanel ()
{
    delete redImage;
    delete greenImage;
    delete blueImage;
    delete valueImage;
    delete chroImage;
    delete rawImage;
    for (int i = 0; i < 3; ++i) {
        delete mode_images_[i];
    }
    delete barImage;

    delete redImage_g;
    delete greenImage_g;
    delete blueImage_g;
    delete valueImage_g;
    delete chroImage_g;
    delete rawImage_g;
    delete barImage_g;

}

void HistogramPanel::resized (Gtk::Allocation& req)
{

    histogramArea->updateBackBuffer ();
    histogramArea->queue_draw ();

    // set histogramRGBArea invalid;
    histogramRGBArea->updateBackBuffer(-1, -1, -1);
    histogramRGBArea->queue_draw ();

    // Store current height of the histogram
    options.histogramHeight = get_height();

}

void HistogramPanel::red_toggled ()
{
    showRed->set_image(showRed->get_active() ? *redImage : *redImage_g);
    rgbv_toggled();
}
void HistogramPanel::green_toggled ()
{
    showGreen->set_image(showGreen->get_active() ? *greenImage : *greenImage_g);
    rgbv_toggled();
}
void HistogramPanel::blue_toggled ()
{
    showBlue->set_image(showBlue->get_active() ? *blueImage : *blueImage_g);
    rgbv_toggled();
}
void HistogramPanel::value_toggled ()
{
    removeIfThere(showValue, valueImage, false);
    removeIfThere(showValue, valueImage_g, false);
    showValue->set_image(showValue->get_active() ? *valueImage : *valueImage_g);
    rgbv_toggled();
}
void HistogramPanel::chro_toggled ()
{
    removeIfThere(showChro, chroImage, false);
    removeIfThere(showChro, chroImage_g, false);
    showChro->set_image(showChro->get_active() ? *chroImage : *chroImage_g);
    rgbv_toggled();
}

void HistogramPanel::raw_toggled ()
{
    if (showRAW->get_active()) {
        showRAW->set_image(*rawImage);
        showValue->set_sensitive(false);
        showChro->set_sensitive(false);
    } else {
        showRAW->set_image(*rawImage_g);
        showValue->set_sensitive(true);
        showChro->set_sensitive(true);
    }

    rgbv_toggled();
}

void HistogramPanel::mode_released ()
{
    options.histogramDrawMode = (options.histogramDrawMode + 1) % 3;
    toggleButtonMode();
    rgbv_toggled();
}

void HistogramPanel::bar_toggled ()
{
    showBAR->set_image(showBAR->get_active() ? *barImage : *barImage_g);
    rgbv_toggled();
}

void HistogramPanel::rgbv_toggled ()
{
    // Update Display
    histogramArea->updateOptions (showRed->get_active(), showGreen->get_active(), showBlue->get_active(), showValue->get_active(), showChro->get_active(), showRAW->get_active(), options.histogramDrawMode);
    histogramArea->queue_draw ();

    histogramRGBArea->updateOptions (showRed->get_active(), showGreen->get_active(), showBlue->get_active(), showValue->get_active(), showChro->get_active(), showRAW->get_active(), showBAR->get_active());
    histogramRGBArea->updateBackBuffer (0, 0, 0);
    histogramRGBArea->queue_draw ();
}

void HistogramPanel::setHistRGBInvalid ()
{
    // do something to un-show vertical bars
    histogramRGBArea->updateBackBuffer(-1, -1, -1);
    histogramRGBArea->queue_draw ();
}

void HistogramPanel::pointerMoved (bool validPos, const Glib::ustring &profile, const Glib::ustring &profileW, int x, int y, int r, int g, int b, bool isRaw)
{

    if (!validPos) {
        // do something to un-show vertical bars
        histogramRGBArea->updateBackBuffer(-1, -1, -1);
    } else {
        // do something to show vertical bars
        histogramRGBArea->updateBackBuffer(r, g, b, profile, profileW);
    }
    histogramRGBArea->queue_draw ();
}

/*
 * Move the vertical button bar to the right side
 * only allowed values for align are Gtk::POS_LEFT and Gtk::POS_RIGHT
 */
void HistogramPanel::reorder (Gtk::PositionType align)
{
    if (align == Gtk::POS_LEFT) {
        gfxGrid->reference();
        removeIfThere(this, gfxGrid, false);
        add (*gfxGrid);
        gfxGrid->unreference();
    } else {
        buttonGrid->reference();
        removeIfThere(this, buttonGrid, false);
        add (*buttonGrid);
        buttonGrid->unreference();
    }
}

// DrawModeListener interface:
void HistogramPanel::toggleButtonMode ()
{
    int m = LIM(options.histogramDrawMode, 0, 2);
    showMode->set_image(*mode_images_[m]);
    showMode->set_tooltip_text(Glib::ustring::compose(M("HISTOGRAM_TOOLTIP_MODE"), mode_tips_[m]));
}

//
//
//
// HistogramScaling
HistogramScaling::HistogramScaling():
    factor(10.0)
{
    factor = options.histogram_scaling_factor;
}


double HistogramScaling::log(double vsize, double val)
{
    //double factor = 10.0; // can be tuned if necessary - higher is flatter curve
    return vsize * std::log(factor / (factor + val)) / std::log(factor / (factor + vsize));
}

//
//
//
// HistogramRGBArea
HistogramRGBArea::HistogramRGBArea () :
    val(0), r(0), g(0), b(0), valid(false),
    needRed(options.histogramRed), needGreen(options.histogramGreen), needBlue(options.histogramBlue),
    needLuma(options.histogramLuma), needChroma(options.histogramChroma), rawMode(options.histogramRAW),
    showMode(options.histogramBar), barDisplayed(options.histogramBar), parent(nullptr)
{

    get_style_context()->add_class("drawingarea");
    set_name("HistogramRGBArea");

    harih = new HistogramRGBAreaIdleHelper;
    harih->harea = this;
    harih->destroyed = false;
    harih->pending = 0;
}

HistogramRGBArea::~HistogramRGBArea ()
{
    idle_register.destroy();

    if (harih->pending) {
        harih->destroyed = true;
    } else {
        delete harih;
    }
}


Gtk::SizeRequestMode HistogramRGBArea::get_request_mode_vfunc () const
{
    return Gtk::SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

void HistogramRGBArea::get_preferred_height_vfunc (int &minimum_height, int &natural_height) const
{
    int minimumWidth = 0;
    int naturalWidth = 0;
    get_preferred_width_vfunc(minimumWidth, naturalWidth);
    get_preferred_height_for_width_vfunc (minimumWidth, minimum_height, natural_height);
}

void HistogramRGBArea::get_preferred_width_vfunc (int &minimum_width, int &natural_width) const
{
    int s = RTScalable::getScale();
    minimum_width = 60 * s;
    natural_width = 200 * s;
}

void HistogramRGBArea::get_preferred_height_for_width_vfunc (int width, int &minimum_height, int &natural_height) const
{
    int bHeight = width / 30;

    int s = RTScalable::getScale();

    if (bHeight > (10 * s)) {
        bHeight = 10 * s;
    } else if (bHeight < (5 * s)) {
        bHeight = 5 * s;
    }

    minimum_height = bHeight;
    natural_height = bHeight;
}

// unused?
void HistogramRGBArea::get_preferred_width_for_height_vfunc (int height, int &minimum_width, int &natural_width) const
{
    get_preferred_width_vfunc (minimum_width, natural_width);
}

bool HistogramRGBArea::getShow()
{
    return(showMode);
}

void HistogramRGBArea::updateBackBuffer (int r, int g, int b, const Glib::ustring &profile, const Glib::ustring &profileW)
{
    if (!get_realized () || !showMode || rawMode) {
        return;
    }

    // Mostly not necessary, but should be in some case
    GThreadLock lock; // All GUI access from idle_add callbacks or separate thread HAVE to be protected

    Glib::RefPtr<Gdk::Window> window = get_window();
    int winx, winy, winw, winh;
    window->get_geometry(winx, winy, winw, winh);

    double s = RTScalable::getScale();

    // This will create or update the size of the BackBuffer::surface
    setDrawRectangle(Cairo::FORMAT_ARGB32, 0, 0, winw, winh, true);

    if (surface)  {
        Cairo::RefPtr<Cairo::Context> cc = Cairo::Context::create(surface);
        Glib::RefPtr<Gtk::StyleContext> style = get_style_context();

        cc->set_source_rgba (0., 0., 0., 0.);
        cc->set_operator (Cairo::OPERATOR_CLEAR);
        cc->paint ();
        cc->set_operator (Cairo::OPERATOR_OVER);

        cc->set_antialias(Cairo::ANTIALIAS_NONE);
        cc->set_line_width (1.0 * s);

        if ( r != -1 && g != -1 && b != -1 ) {
            if (needRed) {
                // Red
                cc->set_source_rgb(1.0, 0.0, 0.0);
                if (options.histogramDrawMode < 2) {
                    cc->move_to(r * (winw - 1.) / 255.0 + 0.5*s, 0); // Rescaling seems needed to fit between boundaries of draw area
                    cc->line_to(r * (winw - 1.) / 255.0 + 0.5*s, winh - 0);
                } else {
                    cc->move_to(HistogramScaling::log (255, r) * (winw - 1.*s) / 255.0 + 0.5*s, 0);
                    cc->line_to(HistogramScaling::log (255, r) * (winw - 1.*s) / 255.0 + 0.5*s, winh - 0);
                }
                cc->stroke();
            }

            if (needGreen) {
                // Green
                cc->set_source_rgb(0.0, 1.0, 0.0);
                if (options.histogramDrawMode < 2) {
                    cc->move_to(g * (winw - 1.) / 255.0 + 0.5*s, 0);
                    cc->line_to(g * (winw - 1.) / 255.0 + 0.5*s, winh - 0);
                } else {
                    cc->move_to(HistogramScaling::log (255, g) * (winw - 1.) / 255.0 + 0.5*s, 0);
                    cc->line_to(HistogramScaling::log (255, g) * (winw - 1.) / 255.0 + 0.5*s, winh - 0);
                }
                cc->stroke();
            }

            if (needBlue) {
                // Blue
                cc->set_source_rgb(0.0, 0.0, 1.0);
                if (options.histogramDrawMode < 2) {
                    cc->move_to(b * (winw - 1.) / 255.0 + 0.5*s, 0);
                    cc->line_to(b * (winw - 1.) / 255.0 + 0.5*s, winh - 0);
                } else {
                    cc->move_to(HistogramScaling::log (255, b) * (winw - 1.) / 255.0 + 0.5*s, 0);
                    cc->line_to(HistogramScaling::log (255, b) * (winw - 1.) / 255.0 + 0.5*s, winh - 0);
                }
                cc->stroke();
            }

            if(needLuma || needChroma) {
                float Lab_L, Lab_a, Lab_b;
                rtengine::Color::rgb2lab01(profile, profileW, r / 255.f, g / 255.f, b / 255.f, Lab_L, Lab_a, Lab_b, options.rtSettings.HistogramWorking);

                if (needLuma) {
                    // Luma
                    cc->set_source_rgb(1.0, 1.0, 1.0);
                    if (options.histogramDrawMode < 2) {
                        cc->move_to(Lab_L * (winw - 3.*s) / 100.0 + 0.5*s, 0);
                        cc->line_to(Lab_L * (winw - 3.*s) / 100.0 + 0.5*s, winh - 0);
                    } else {
                        cc->move_to(HistogramScaling::log (100, Lab_L) * (winw - 1.) / 100.0 + 0.5*s, 0);
                        cc->line_to(HistogramScaling::log (100, Lab_L) * (winw - 1.) / 100.0 + 0.5*s, winh - 0);
                    }
                    cc->stroke();
                }

                if (needChroma) {
                    // Chroma
                    float chromaval = sqrt(Lab_a * Lab_a + Lab_b * Lab_b) / 1.8;
                    //  float chromaval = sqrt(Lab_a*Lab_a + Lab_b*Lab_b);
                    cc->set_source_rgb(0.9, 0.9, 0.0);
                    if (options.histogramDrawMode < 2) {
                        cc->move_to(chromaval * (winw - 1.) / 100.0 + 0.5*s, 0);
                        cc->line_to(chromaval * (winw - 1.) / 100.0 + 0.5*s, winh - 0);
                    } else {
                        cc->move_to(HistogramScaling::log (100, chromaval) * (winw - 1.) / 100.0 + 0.5*s, 0);
                        cc->line_to(HistogramScaling::log (100, chromaval) * (winw - 1.) / 100.0 + 0.5*s, winh - 0);
                    }
                    cc->stroke();
                }
            }
        }
    }

    setDirty(false);
}

void HistogramRGBArea::update (int valh, int rh, int  gh, int bh)
{

    if (valh) {
        val = valh;
        r = rh;
        g = gh;
        b = bh;
        valid = true;
    } else {
        valid = false;
    }

    harih->pending++;

    idle_register.add(
        [this]() -> bool
        {
            if (harih->destroyed) {
                if (harih->pending == 1) {
                    delete harih;
                } else {
                    --harih->pending;
                }

                return false;
            }

            harih->harea->updateBackBuffer(-1, -1, -1);
            harih->harea->queue_draw ();

            --harih->pending;

            return false;
        }
    );
}

void HistogramRGBArea::updateOptions (bool r, bool g, bool b, bool l, bool c, bool raw, bool bar)
{

    options.histogramRed    = needRed    = r;
    options.histogramGreen  = needGreen  = g;
    options.histogramBlue   = needBlue   = b;
    options.histogramLuma   = needLuma   = l;
    options.histogramChroma = needChroma = c;
    options.histogramRAW    = rawMode    = raw;
    options.histogramBar    = showMode   = bar;

    // Show/hide the RGB bar widget
    if (bar && !barDisplayed) {
        parent->add(*this);
        barDisplayed = true;
    } else if (!bar && barDisplayed) {
        removeIfThere(parent, this, false);
        barDisplayed = false;
    }

}

void HistogramRGBArea::on_realize ()
{

    Gtk::DrawingArea::on_realize();
    Glib::RefPtr<Gdk::Window> window = get_window();
    add_events(Gdk::BUTTON_PRESS_MASK);
}

bool HistogramRGBArea::on_draw(const ::Cairo::RefPtr< Cairo::Context> &cr)
{

    const Glib::RefPtr<Gtk::StyleContext> style = get_style_context();
    style->render_background(cr, 0, 0, get_width(), get_height());

    // on_realize & updateBackBuffer have to be called before
    if (surface) {
        if (isDirty()) { // not sure this could happen...
            updateBackBuffer(-1, -1, -1);
        }

        copySurface(cr, NULL);
    }

    style->render_frame (cr, 0, 0, get_width(), get_height());

    return true;
}

bool HistogramRGBArea::on_button_press_event (GdkEventButton* event)
{

    if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {
        // do something?
    }

    return true;
}

void HistogramRGBArea::factorChanged (double newFactor)
{
    factor = newFactor;
    options.histogram_scaling_factor = factor;
}

//
//
//
// HistogramArea
HistogramArea::HistogramArea (DrawModeListener *fml) :
    valid(false), drawMode(options.histogramDrawMode), myDrawModeListener(fml),
    oldwidth(-1), oldheight(-1),
    needRed(options.histogramRed), needGreen(options.histogramGreen), needBlue(options.histogramBlue),
    needLuma(options.histogramLuma), needChroma(options.histogramChroma), rawMode(options.histogramRAW),
    isPressed(false), movingPosition(0.0)
{

    rhist(256);
    ghist(256);
    bhist(256);
    lhist(256);
    chist(256);

    get_style_context()->add_class("drawingarea");
    set_name("HistogramArea");

    haih = new HistogramAreaIdleHelper;
    haih->harea = this;
    haih->destroyed = false;
    haih->pending = 0;
}

HistogramArea::~HistogramArea ()
{
    idle_register.destroy();

    if (haih->pending) {
        haih->destroyed = true;
    } else {
        delete haih;
    }
}

Gtk::SizeRequestMode HistogramArea::get_request_mode_vfunc () const
{
    return Gtk::SIZE_REQUEST_CONSTANT_SIZE;
}

void HistogramArea::get_preferred_height_vfunc (int &minimum_height, int &natural_height) const
{
    int s = (int)RTScalable::getScale();
    minimum_height = 100 * s;
    natural_height = 200 * s;
}

void HistogramArea::get_preferred_width_vfunc (int &minimum_width, int &natural_width) const
{

    int s = (int)RTScalable::getScale();
    minimum_width = 200 * s;
    natural_width = 400 * s;
}

void HistogramArea::get_preferred_height_for_width_vfunc (int width, int &minimum_height, int &natural_height) const
{

    minimum_height = 0;
    natural_height = 0;
}

void HistogramArea::get_preferred_width_for_height_vfunc (int height, int &minimum_width, int &natural_width) const
{
    get_preferred_width_vfunc (minimum_width, natural_width);
}

void HistogramArea::updateOptions (bool r, bool g, bool b, bool l, bool c, bool raw, int mode)
{

    options.histogramRed      = needRed    = r;
    options.histogramGreen    = needGreen  = g;
    options.histogramBlue     = needBlue   = b;
    options.histogramLuma     = needLuma   = l;
    options.histogramChroma   = needChroma = c;
    options.histogramRAW      = rawMode    = raw;
    options.histogramDrawMode = drawMode   = mode;

    updateBackBuffer ();
}

void HistogramArea::update(
    const LUTu& histRed,
    const LUTu& histGreen,
    const LUTu& histBlue,
    const LUTu& histLuma,
    const LUTu& histChroma,
    const LUTu& histRedRaw,
    const LUTu& histGreenRaw,
    const LUTu& histBlueRaw
)
{
    if (histRed) {
        rhist = histRed;
        ghist = histGreen;
        bhist = histBlue;
        lhist = histLuma;
        chist = histChroma;
        rhistRaw = histRedRaw;
        ghistRaw = histGreenRaw;
        bhistRaw = histBlueRaw;
        valid = true;
    } else {
        valid = false;
    }

    haih->pending++;

    // Can be done outside of the GUI thread
    idle_register.add(
        [this]() -> bool
        {
            if (haih->destroyed) {
                if (haih->pending == 1) {
                    delete haih;
                } else {
                    --haih->pending;
                }

                return false;
            }

            haih->harea->setDirty(true);
            haih->harea->updateBackBuffer();
            haih->harea->queue_draw();

            --haih->pending;

            return false;
        }
    );
}

void HistogramArea::updateBackBuffer ()
{
    if (!get_realized ()) {
        return;
    }

    Glib::RefPtr<Gdk::Window> window = get_window();
    int winx, winy, winw, winh;
    window->get_geometry(winx, winy, winw, winh);

    // This will create or update the size of the BackBuffer::surface
    setDrawRectangle(Cairo::FORMAT_ARGB32, 0, 0, winw, winh, true);

    Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create(surface);
    const Glib::RefPtr<Gtk::StyleContext> style = get_style_context();

    double s = RTScalable::getScale();

    // Setup drawing
    cr->set_source_rgba (0., 0., 0., 0.);
    cr->set_operator (Cairo::OPERATOR_CLEAR);
    cr->paint ();
    cr->set_operator (Cairo::OPERATOR_SOURCE);

    // Prepare drawing gridlines first
    cr->set_source_rgba (1., 1., 1., 0.25);
    cr->set_line_width (1.0 * s);
    cr->set_antialias(Cairo::ANTIALIAS_NONE);
    cr->set_line_join(Cairo::LINE_JOIN_MITER);
    cr->set_line_cap(Cairo::LINE_CAP_BUTT);
    std::valarray<double> ch_ds (1);
    ch_ds[0] = 4;
    cr->set_dash (ch_ds, 0);

    if (rawMode) {
        updateRaw(cr);
    } else {
        updateNonRaw(cr);
    }

    // Draw the frame's border
    style->render_frame(cr, 0, 0, surface->get_width(), surface->get_height());

    oldwidth = w;
    oldheight = h;

    setDirty(false);
}


void HistogramArea::updateNonRaw(Cairo::RefPtr<Cairo::Context> cr)
{
    set_has_tooltip(false);
    
    const Glib::RefPtr<Gtk::StyleContext> style = get_style_context();

    double s = RTScalable::getScale();

    // determine the number of h-gridlines based on current h
    int nrOfHGridPartitions = (int)rtengine::min (16.0, pow (2.0, floor ((h - 100) / 250) + 2));
    int nrOfVGridPartitions = 8; // always show 8 stops (lines at 1,3,7,15,31,63,127)

    // draw vertical gridlines
    if (options.histogramDrawMode == 0) {
        for (int i = 1; i < nrOfVGridPartitions; i++) {
            cr->move_to((pow(2.0,i) - 1) / 255.0 * w + 0.5, 0.);
            cr->line_to((pow(2.0,i) - 1) / 255.0 * w + 0.5, h);
            cr->stroke();
        }
    } else {
        for (int i = 1; i < nrOfVGridPartitions; i++) {
            cr->move_to(HistogramScaling::log(255, pow(2.0,i) - 1) / 255.0 * w + 0.5, 0.);
            cr->line_to(HistogramScaling::log(255, pow(2.0,i) - 1) / 255.0 * w + 0.5, h);
            cr->stroke();
        }
    }

    // draw horizontal gridlines
    if (options.histogramDrawMode != 2) {
        for (int i = 1; i < nrOfHGridPartitions; i++) {
            cr->move_to (0., i * (double)h / nrOfHGridPartitions + 0.5);
            cr->line_to (w, i * (double)h / nrOfHGridPartitions + 0.5);
            cr->stroke ();
        }
    } else {
        for (int i = 1; i < nrOfHGridPartitions; i++) {
            cr->move_to (0., h - HistogramScaling::log (h, i * (double)h / nrOfHGridPartitions) + 0.5*s);
            cr->line_to (w, h - HistogramScaling::log (h, i * (double)h / nrOfHGridPartitions) + 0.5*s);
            cr->stroke ();
        }
    }

    cr->unset_dash();

    if (valid) {
        // For RAW mode use the other hists
        LUTu& rh = rhist;
        LUTu& gh = ghist;
        LUTu& bh = bhist;

        // make copies of LUT for faster access
        unsigned int lhisttemp[256] ALIGNED16 {0}, chisttemp[256] ALIGNED16 {0}, rhtemp[256] ALIGNED16 {0}, ghtemp[256] ALIGNED16 {0}, bhtemp[256] ALIGNED16 {0};
        const int scale = 1;

        for(int i = 0; i < 256; i++) {
            if(needLuma) {
                lhisttemp[i] = lhist[i];
            }

            if(needChroma) {
                chisttemp[i] = chist[i];
            }

            if(needRed) {
                rhtemp[i] = rh[i];
            }

            if(needGreen) {
                ghtemp[i] = gh[i];
            }

            if(needBlue) {
                bhtemp[i] = bh[i];
            }
        }

        // Compute the highest point of the histogram for scaling
        // Values at far left and right end (0 and 255) are handled differently

        unsigned int histheight = 0;

        for (int i = 1; i < 255; i++) {
            if (needLuma && lhisttemp[i] > histheight) {
                histheight = lhisttemp[i];
            }

            if (needChroma && chisttemp[i] > histheight) {
                histheight = chisttemp[i];
            }

            if (needRed && rhtemp[i] > histheight) {
                histheight = rhtemp[i];
            }

            if (needGreen && ghtemp[i] > histheight) {
                histheight = ghtemp[i];
            }

            if (needBlue && bhtemp[i] > histheight) {
                histheight = bhtemp[i];
            }
        }

        int realhistheight = histheight;

        if (realhistheight < h - 2) {
            realhistheight = h - 2;
        }

        cr->set_antialias (Cairo::ANTIALIAS_SUBPIXEL);
        cr->set_line_width (1.0 * s);
        cr->set_operator (Cairo::OPERATOR_OVER);

        int ui = 0, oi = 0;

        if (needLuma && !rawMode) {
            drawCurve(cr, lhist, realhistheight, w, h);
            cr->set_source_rgba (0.65, 0.65, 0.65, 0.65);
            cr->fill ();
            drawMarks(cr, lhist, scale, w, ui, oi);
        }

        if (needChroma && !rawMode) {
            drawCurve(cr, chist, realhistheight, w, h);
            cr->set_source_rgb (0.9, 0.9, 0.);
            cr->stroke ();
            drawMarks(cr, chist, scale, w, ui, oi);
        }

        if (needRed) {
            drawCurve(cr, rh, realhistheight, w, h);
            cr->set_source_rgb (1.0, 0.0, 0.0);
            cr->stroke ();
            drawMarks(cr, rh, scale, w, ui, oi);
        }

        if (needGreen) {
            drawCurve(cr, gh, realhistheight, w, h);
            cr->set_source_rgb (0.0, 1.0, 0.0);
            cr->stroke ();
            drawMarks(cr, gh, scale, w, ui, oi);
        }

        if (needBlue) {
            drawCurve(cr, bh, realhistheight, w, h);
            cr->set_source_rgb (0.0, 0.0, 1.0);
            cr->stroke ();
            drawMarks(cr, bh, scale, w, ui, oi);
        }

    }
}


namespace {

class RawIdxHelper {
public:
    RawIdxHelper(bool logscale, unsigned int ub, unsigned int width):
        logscale_(logscale),
        ub_(ub)
    {
        incr_ = std::max(ub_ / width, 1u);
    }

    void operator()(unsigned int &cur, unsigned int &i) const
    {
        unsigned int j = i;
        i = cur;
        unsigned int next = cur;
        if (logscale_) {
            next = std::max(static_cast<unsigned int>(j * 1.3f), cur + 1u);
        } else {
            next = cur + incr_;
        }
        if (ub_ - next < next - cur) {
            next = ub_ + 1;
        }
        if (cur < ub_ && next > ub_) {
            cur = ub_;
        } else {
            cur = std::min(next, ub_+1);
        }
    }

private:
    bool logscale_;
    unsigned int ub_;
    unsigned int incr_;
};

} // namespace


void HistogramArea::updateRaw(Cairo::RefPtr<Cairo::Context> cr)
{
    const Glib::RefPtr<Gtk::StyleContext> style = get_style_context();
    double s = RTScalable::getScale();

    // determine the number of h-gridlines based on current h
    int nrOfHGridPartitions = (int)rtengine::min (16.0, pow (2.0, floor ((h - 100) / 250) + 2));
    double sz = 2;
    if (valid) {
        sz = rtengine::max(rhistRaw.getUpperBound(), ghistRaw.getUpperBound(), bhistRaw.getUpperBound());
    }

    int minval[3];
    int mincount[3];
    int maxval[3];
    int maxcount[3];
    int peakval[3];
    int peakcount[3];
    int totcount[3];
    int totsum[3];
    const char *ch[3] = {"#FF0000", "#00FF00", "#0000FF"};

    // draw vertical gridlines
    const double logmax = std::log2(sz);
    const bool logscale = options.histogramDrawMode > 0;
    
    for (double i = sz / 2.0; ; i /= 2.0) {
        double x = i / sz;
        if (logscale) {
            x = logmax + std::log2(x);
            if (x <= 0) {
                break;
            }
            x /= logmax;
        }
        x *= w;
        cr->move_to(x, 0.);
        cr->line_to(x, h);
        cr->stroke();
        if (i <= 1.0) {
            break;
        }
    }

    // draw horizontal gridlines
    if (options.histogramDrawMode == 2) {
        for (int i = 1; i < nrOfHGridPartitions; i++) {
            double y = double(i) / nrOfHGridPartitions;
            y = rtengine::log2lin(y, 10.0);
            cr->move_to(0., y * h);
            cr->line_to(w, y * h);
            cr->stroke();
        }
    } else {
        for (int i = 1; i < nrOfHGridPartitions; i++) {
            cr->move_to(0., i * (double)h / nrOfHGridPartitions + 0.5);
            cr->line_to(w, i * (double)h / nrOfHGridPartitions + 0.5);
            cr->stroke();
        }
    }

    cr->unset_dash();

    if (valid) {
        // For RAW mode use the other hists
        LUTu &rh = rhistRaw;
        LUTu &gh = ghistRaw;
        LUTu &bh = bhistRaw;

        auto ub = rtengine::max(rh.getUpperBound(), gh.getUpperBound(), bh.getUpperBound());

        const auto bin =
            [](LUTu &data, int i, int next) -> double
            {
                double val = 0.0;
                for (int j = i; j < next; ++j) {
                    val += data[j];
                }
                val /= (next - i);
                return val;
            };
        
        //const int delta = std::max(int(ub / std::max(w / 2, 1)), 1);
        //const float delta = 1.05f;
        RawIdxHelper next_raw_idx(logscale, ub, w);
        unsigned int next = 1;
        unsigned int histheight = 0;

        const unsigned int off = drawMode == 2 ? 0 : 1;
        for (unsigned int i = off; i <= ub-off;) {
            if (i < rh.getSize()) {
                double val = bin(rh, i, next);
                if (histheight < val) {
                    histheight = val;
                }
            }
            if (i < gh.getSize()) {
                double val = bin(gh, i, next);
                if (histheight < val) {
                    histheight = val;
                }
            }
            if (i < bh.getSize()) {
                double val = bin(bh, i, next);
                if (histheight < val) {
                    histheight = val;
                }
            }

            next_raw_idx(next, i);
        }

        int realhistheight = histheight;

        if (realhistheight < h - 2) {
            realhistheight = h - 2;
        }

        cr->set_antialias (Cairo::ANTIALIAS_SUBPIXEL);
        cr->set_line_width (1.0 * s);
        cr->set_operator (Cairo::OPERATOR_OVER);

        int ui = 0, oi = 0;

        if (needRed) {
            drawRawCurve(cr, rh, ub, realhistheight, w, h);
            cr->set_source_rgb(1.0, 0.0, 0.0);
            cr->stroke();
            drawMarks(cr, rh, 1.0, w, ui, oi);
        }

        if (needGreen) {
            drawRawCurve(cr, gh, ub, realhistheight, w, h);
            cr->set_source_rgb(0.0, 1.0, 0.0);
            cr->stroke();
            drawMarks(cr, gh, 1.0, w, ui, oi);
        }

        if (needBlue) {
            drawRawCurve(cr, bh, ub, realhistheight, w, h);
            cr->set_source_rgb(0.0, 0.0, 1.0);
            cr->stroke();
            drawMarks(cr, bh, 1.0, w, ui, oi);
        }

        // update stats
        for (int c = 0; c < 3; ++c) {
            minval[c] = -1;
            mincount[c] = 0;
            maxval[c] = -1;
            maxcount[c] = 0;
            peakval[c] = 0;
            peakcount[c] = 0;
            totcount[c] = 0;
            totsum[c] = 0;
        }

        const auto update =
            [&](int c, LUTu &data, unsigned int i) -> void
            {
                if (i < data.getSize()) {
                    int cnt = data[i];
                    if (cnt > 0) {
                        if (minval[c] < 0) {
                            minval[c] = i;
                            mincount[c] = cnt;
                        }
                        maxval[c] = i;
                        maxcount[c] = cnt;
                        if (peakcount[c] < cnt) {
                            peakval[c] = i;
                            peakcount[c] = cnt;
                        }
                        ++totcount[c];
                    }
                    totsum[c] += cnt;
                }
            };
        
        for (unsigned int i = 0; i <= ub; ++i) {
            update(0, rh, i);
            update(1, gh, i);
            update(2, bh, i);
        }
    }


    if (valid) {
        const auto pct =
            [](int n, int d) -> Glib::ustring
            {
                double p = double(int(double(n)/double(d) * 10000)) / 100.0;
                if (p == 0 && n > 0) {
                    return "&lt;0.01%";
                } else {
                    return Glib::ustring::compose("%1%%", p);
                }
            };
        
        set_has_tooltip(true);
        Glib::ustring tip = "";
        for (int i = 0; i < 3; ++i) {
            double ev = minval[i] >= 0 ? std::log2(maxval[i] - minval[i]) : 0;
            ev = double(int(ev * 100)) / 100.0;
            Glib::ustring m = Glib::ustring::compose(M("HISTOGRAM_RAW_STATS_TOOLTIP"), minval[i], pct(mincount[i], totsum[i]), maxval[i], pct(maxcount[i], totsum[i]), peakval[i], pct(peakcount[i], totsum[i]), ev, totcount[i], totsum[i]);
            Glib::ustring s = Glib::ustring::compose("<span font_family=\"Arial\" size=\"larger\" foreground=\"%1\">&#9632;</span> %2", ch[i], m);
            if (i > 0) {
                tip += "\n";
            }
            tip += s;
        }
        set_tooltip_markup(tip);
    } else {
        set_has_tooltip(false);
    }
}


void HistogramArea::drawRawCurve(Cairo::RefPtr<Cairo::Context> &cr,
                                 LUTu &data, unsigned int ub, double scale,
                                 int hsize, int vsize)
{
    double s = RTScalable::getScale();

    cr->set_line_width(s);
    cr->move_to(0, vsize - 1);
    scale = scale <= 0.f ? 0.001f : scale; // avoid division by zero and negative values

    unsigned int next = 1;
    const bool logscale = drawMode > 0;
    const double logmax = std::log2(ub);
    //const double ybase = std::pow(10, std::max(std::floor(std::log(scale) / std::log(10))-1, 1.0));
    const double ylogmax = std::log10(scale);

    RawIdxHelper next_raw_idx(logscale, data.getUpperBound(), hsize);

    for (unsigned int i = logscale ? 1 : 0; i < data.getSize(); ) {
        double val = 0.0;
        for (unsigned int j = i; j < next; ++j) {
            val += data[j];
        }
        val /= (next - i);
        
        val = std::min(val / scale, 1.0);

        if (drawMode == 2 && val > 0) { // scale y for log-scale
            //val = rtengine::lin2log(val, ybase);
            val = std::max((ylogmax + std::log10(val)) / ylogmax, 0.0);
        }

        double iscaled = std::min(double(i) / ub, 1.0);
        if (logscale) { // scale x for log-scale
            iscaled = logmax + std::log2(iscaled);
            if (iscaled < 0) {
                next_raw_idx(next, i);
                continue;
            }
            iscaled /= logmax;
        }

        double posX = iscaled * hsize;
        double posY = vsize - 2 + val * (4 - vsize);

        cr->line_to(posX, posY);

        next_raw_idx(next, i);
    }

    cr->line_to(hsize - 1, vsize - 1);
}


void HistogramArea::on_realize ()
{

    Gtk::DrawingArea::on_realize();
    Glib::RefPtr<Gdk::Window> window = get_window();
    add_events(Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
}

void HistogramArea::drawCurve(Cairo::RefPtr<Cairo::Context> &cr,
                              LUTu & data, double scale, int hsize, int vsize)
{
    double s = RTScalable::getScale();

    cr->set_line_width(s);
    cr->move_to (0, vsize - 1);
    scale = scale <= 0.f ? 0.001f : scale; // avoid division by zero and negative values

    for (int i = 0; i < 256; i++) {
        double val = data[i] * (double)vsize / scale;

        if (drawMode == 2) { // scale y for log-scale
            val = HistogramScaling::log ((double)vsize, val);
        }

        double iscaled = i;
        if (drawMode > 0) { // scale x for log-scale
            iscaled = HistogramScaling::log (255.0, (double)i);
        }

        double posX = (iscaled / 255.0) * (hsize - 1);
        double posY = vsize - 2 + val * (4 - vsize) / vsize;

        cr->line_to (posX, posY);
    }

    cr->line_to (hsize - 1, vsize - 1);
}

void HistogramArea::drawMarks(Cairo::RefPtr<Cairo::Context> &cr,
                              LUTu & data, double scale, int hsize, int & ui, int & oi)
{
    int s = 8 * RTScalable::getScale();

    if(data[0] > scale) {
        cr->rectangle(0, (ui++)*s, s, s);
    }

    if(data[data.getUpperBound()] > scale) {
        cr->rectangle(hsize - s, (oi++)*s, s, s);
    }

    cr->fill();
}

bool HistogramArea::on_draw(const ::Cairo::RefPtr< Cairo::Context> &cr)
{

    Glib::RefPtr<Gdk::Window> window = get_window();

    if (get_width() != oldwidth || get_height() != oldheight || isDirty ()) {
        updateBackBuffer ();
    }

    const Glib::RefPtr<Gtk::StyleContext> style = get_style_context();
    style->render_background(cr, 0, 0, get_width(), get_height());
    copySurface(cr, NULL);
    style->render_frame (cr, 0, 0, get_width(), get_height());

    return true;
}

bool HistogramArea::on_button_press_event (GdkEventButton* event)
{
    isPressed = true;
    movingPosition = event->x;

    if (event->type == GDK_2BUTTON_PRESS && event->button == 1) {

        drawMode = (drawMode + 1) % 3;
        options.histogramDrawMode = (options.histogramDrawMode + 1) % 3;

        if (myDrawModeListener) {
            myDrawModeListener->toggleButtonMode ();
        }

        updateBackBuffer ();
        queue_draw ();
    }

    return true;
}

bool HistogramArea::on_button_release_event (GdkEventButton* event)
{
    isPressed = false;
    return true;
}

bool HistogramArea::on_motion_notify_event (GdkEventMotion* event)
{
    if (isPressed)
    {
        double mod = 1 + (event->x - movingPosition) / get_width();

        factor /= mod;
        if (factor < 1.0)
            factor = 1.0;
        if (factor > 100.0)
            factor = 100.0;

        sigFactorChanged.emit(factor);

        setDirty(true);
        queue_draw ();
    }

    return true;
}

HistogramArea::type_signal_factor_changed HistogramArea::signal_factor_changed()
{
    return sigFactorChanged;
}
