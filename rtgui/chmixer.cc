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
#include "chmixer.h"
#include "rtimage.h"

using namespace rtengine;
using namespace rtengine::procparams;

ChMixer::ChMixer (): FoldableToolPanel(this, "chmixer", M("TP_CHMIXER_LABEL"), false, true)
{

    imgIcon[0] = Gtk::manage (new RTImage ("circle-red-small.png"));
    imgIcon[1] = Gtk::manage (new RTImage ("circle-green-red-small.png"));
    imgIcon[2] = Gtk::manage (new RTImage ("circle-blue-red-small.png"));
    imgIcon[3] = Gtk::manage (new RTImage ("circle-red-green-small.png"));
    imgIcon[4] = Gtk::manage (new RTImage ("circle-green-small.png"));
    imgIcon[5] = Gtk::manage (new RTImage ("circle-blue-green-small.png"));
    imgIcon[6] = Gtk::manage (new RTImage ("circle-red-blue-small.png"));
    imgIcon[7] = Gtk::manage (new RTImage ("circle-green-blue-small.png"));
    imgIcon[8] = Gtk::manage (new RTImage ("circle-blue-small.png"));

    Gtk::Label* rlabel = Gtk::manage (new Gtk::Label(M("TP_CHMIXER_RED")));
    //rlabel->set_markup (Glib::ustring("\t<span foreground=\"#b00000\"><b>") + M("TP_CHMIXER_RED") + Glib::ustring(":</b></span>"));
    rlabel->set_alignment(Gtk::ALIGN_START);

    constexpr double RANGE = 500.0;
    red[0] = Gtk::manage (new Adjuster ("",   -RANGE, RANGE, 0.1, 100, imgIcon[0]));
    red[1] = Gtk::manage (new Adjuster ("", -RANGE, RANGE, 0.1, 0, imgIcon[1]));
    red[2] = Gtk::manage (new Adjuster ("",  -RANGE, RANGE, 0.1, 0, imgIcon[2]));

    Gtk::HSeparator* rsep = Gtk::manage (new Gtk::HSeparator ());

    pack_start (*rlabel);

    for (int i = 0; i < 3; i++) {
        pack_start (*red[i]);
    }

    pack_start (*rsep, Gtk::PACK_EXPAND_WIDGET, 4);

    Gtk::Label* glabel = Gtk::manage (new Gtk::Label(M("TP_CHMIXER_GREEN")));
    // glabel->set_markup (Glib::ustring("\t<span foreground=\"#0b8c21\"><b>") + M("TP_CHMIXER_GREEN") + Glib::ustring(":</b></span>"));
    glabel->set_alignment(Gtk::ALIGN_START);


    green[0] = Gtk::manage (new Adjuster ("",   -RANGE, RANGE, 0.1, 0, imgIcon[3]));
    green[1] = Gtk::manage (new Adjuster ("", -RANGE, RANGE, 0.1, 100, imgIcon[4]));
    green[2] = Gtk::manage (new Adjuster ("",  -RANGE, RANGE, 0.1, 0, imgIcon[5]));

    Gtk::HSeparator* gsep = Gtk::manage (new Gtk::HSeparator ());

    pack_start (*glabel);

    for (int i = 0; i < 3; i++) {
        pack_start (*green[i]);
    }

    pack_start (*gsep, Gtk::PACK_EXPAND_WIDGET, 4);

    Gtk::Label* blabel = Gtk::manage (new Gtk::Label(M("TP_CHMIXER_BLUE")));
    // blabel->set_markup (Glib::ustring("\t<span foreground=\"#1377d7\"><b>") + M("TP_CHMIXER_BLUE") + Glib::ustring(":</b></span>"));
    blabel->set_alignment(Gtk::ALIGN_START);
    blue[0] = Gtk::manage (new Adjuster ("",   -RANGE, RANGE, 0.1, 0, imgIcon[6]));
    blue[1] = Gtk::manage (new Adjuster ("", -RANGE, RANGE, 0.1, 0, imgIcon[7]));
    blue[2] = Gtk::manage (new Adjuster ("",  -RANGE, RANGE, 0.1, 100, imgIcon[8]));

    for (int i = 0; i < 3; i++) {
        red[i]->setAdjusterListener (this);
        green[i]->setAdjusterListener (this);
        blue[i]->setAdjusterListener (this);

        red[i]->setLogScale(10, red[i]->getValue());
        green[i]->setLogScale(10, green[i]->getValue());
        blue[i]->setLogScale(10, blue[i]->getValue());
    }

    pack_start (*blabel);

    for (int i = 0; i < 3; i++) {
        pack_start (*blue[i]);
    }

    show_all();
}

void ChMixer::read(const ProcParams* pp)
{

    disableListener ();

    setEnabled(pp->chmixer.enabled);
    
    for (int i = 0; i < 3; i++) {
        red[i]->setValue (pp->chmixer.red[i] / 10.0);
        green[i]->setValue (pp->chmixer.green[i] / 10.0);
        blue[i]->setValue (pp->chmixer.blue[i] / 10.0);
    }

    enableListener ();
}

void ChMixer::write(ProcParams* pp)
{

    for (int i = 0; i < 3; i++) {
        pp->chmixer.red[i] = red[i]->getValue() * 10;
        pp->chmixer.green[i] = green[i]->getValue() * 10;
        pp->chmixer.blue[i] = blue[i]->getValue() * 10;
    }
    pp->chmixer.enabled = getEnabled();
}

void ChMixer::setDefaults(const ProcParams* defParams)
{

    for (int i = 0; i < 3; i++) {
        red[i]->setDefault (defParams->chmixer.red[i] / 10.f);
        green[i]->setDefault (defParams->chmixer.green[i] / 10.f);
        blue[i]->setDefault (defParams->chmixer.blue[i] / 10.f);
    }
}

void ChMixer::adjusterChanged(Adjuster* a, double newval)
{

    if (listener && getEnabled()) {
        Glib::ustring descr = Glib::ustring::compose ("R=%1,%2,%3\nG=%4,%5,%6\nB=%7,%8,%9",
                              red[0]->getValue(), red[1]->getValue(), red[2]->getValue(),
                              green[0]->getValue(), green[1]->getValue(), green[2]->getValue(),
                              blue[0]->getValue(), blue[1]->getValue(), blue[2]->getValue());
        listener->panelChanged (EvChMixer, descr);
    }
}

void ChMixer::adjusterAutoToggled(Adjuster* a, bool newval)
{
}

void ChMixer::enabledChanged()
{
    if (listener) {
        if (get_inconsistent()) {
            listener->panelChanged(EvChMixer, M("GENERAL_UNCHANGED"));
        } else if (getEnabled()) {
            listener->panelChanged(EvChMixer, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvChMixer, M("GENERAL_DISABLED"));
        }
    }
}


void ChMixer::trimValues (rtengine::procparams::ProcParams* pp)
{

    for (int i = 0; i < 3; i++) {
        double r = pp->chmixer.red[i] / 10.0;
        double g = pp->chmixer.green[i] / 10.0;
        double b = pp->chmixer.blue[i] / 10.0;
        red[i]->trimValue(r);
        green[i]->trimValue(g);
        blue[i]->trimValue(b);
        pp->chmixer.red[i] = r * 10;
        pp->chmixer.green[i] = g * 10;
        pp->chmixer.blue[i] = b * 10;
    }
}
