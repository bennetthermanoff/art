/** -*- C++ -*-
 *  
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2017 Alberto Griggio <alberto.griggio@gmail.com>
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

// adapted from the "color correction" module of Darktable. Original copyright follows
/*
    copyright (c) 2009--2010 johannes hanika.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <gtkmm.h>
#include "toolpanel.h"
#include "eventmapper.h"


class ColorWheelArea: public Gtk::DrawingArea, public BackBuffer {
public:
    ColorWheelArea(rtengine::ProcEvent evt, const Glib::ustring &msg, bool enable_low=true);

    void getParams(double &x, double &y) const;
    void setParams(double x, double y, bool notify);
    void setDefault(double x, double y, double s);
    void setEdited(bool yes);
    bool getEdited() const;
    void reset(bool toInitial);
    void setListener(ToolPanelListener *l);

    void setScale(double s, bool notify);
    double getScale() const;

    bool on_draw(const ::Cairo::RefPtr<Cairo::Context> &crf) override;
    void on_style_updated() override;
    bool on_button_press_event(GdkEventButton *event) override;
    bool on_button_release_event(GdkEventButton *event) override;
    bool on_motion_notify_event(GdkEventMotion *event) override;
    Gtk::SizeRequestMode get_request_mode_vfunc() const override;
    void get_preferred_width_vfunc(int &minimum_width, int &natural_width) const override;
    void get_preferred_height_for_width_vfunc(int width, int &minimum_height, int &natural_height) const override;

private:
    rtengine::ProcEvent evt;
    Glib::ustring evtMsg;
    
    double low_a;
    double x_;
    double low_b;
    double y_;

    double defaultLow_a;
    double default_x_;
    double defaultLow_b;
    double default_y_;

    ToolPanelListener *listener;
    bool edited;
    bool point_active_;
    bool is_dragged_;
    bool lock_angle_;
    bool lock_radius_;
    sigc::connection delayconn;
    static const int inset = 5;

    double scale;
    double defaultScale;

    bool notifyListener();
    void getLitPoint();
};


class ColorWheel: public Gtk::HBox {
public:
    ColorWheel(rtengine::ProcEvent evt, const Glib::ustring &msg);

    void getParams(double &x, double &y, double &s) const;
    void setParams(double x, double y, double s, bool notify);
    void setDefault(double x, double y, double s);
    void setEdited(bool yes) { grid.setEdited(yes); }
    bool getEdited() const { return grid.getEdited(); }
    void reset(bool toInitial) { grid.reset(toInitial); }
    void setListener(ToolPanelListener *l) { grid.setListener(l); }

private:
    bool resetPressed(GdkEventButton *event);
    void scaleChanged();

    ColorWheelArea grid;
    Gtk::VScale *scale;
    sigc::connection scaleconn;
    sigc::connection timerconn;
};
