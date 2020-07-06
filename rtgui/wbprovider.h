/* -*- C++ -*-
 *  
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
#ifndef _WBPROVIDER_
#define _WBPROVIDER_

#include <array>
#include <vector>

class WBProvider {
public:
    virtual ~WBProvider() {}
    virtual void getAutoWB (double& temp, double& green, double equal) {}
    virtual void getCamWB (double& temp, double& green) {}
    virtual void spotWBRequested (int size) {}
    
    struct Preset {
        Glib::ustring label;
        std::array<double, 3> mult;

        Preset(const Glib::ustring &l="", const std::array<double, 3> &m={}):
            label(l), mult(m) {}
    };
    virtual std::vector<Preset> getWBPresets() const { return std::vector<Preset>(); }
};

#endif
