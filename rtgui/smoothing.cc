/** -*- C++ -*-
 *  
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2018 Alberto Griggio <alberto.griggio@gmail.com>
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
#include "smoothing.h"
#include "eventmapper.h"
#include <iomanip>
#include <cmath>

using namespace rtengine;
using namespace rtengine::procparams;


//-----------------------------------------------------------------------------
// SmoothingMasksContentProvider
//-----------------------------------------------------------------------------

class SmoothingMasksContentProvider: public LabMasksContentProvider {
public:
    SmoothingMasksContentProvider(Smoothing *parent):
        parent_(parent)
    {
    }

    Gtk::Widget *getWidget() override
    {
        return parent_->box;
    }

    void getEvents(rtengine::ProcEvent &mask_list, rtengine::ProcEvent &h_mask, rtengine::ProcEvent &c_mask, rtengine::ProcEvent &l_mask, rtengine::ProcEvent &blur, rtengine::ProcEvent &show, rtengine::ProcEvent &area_mask) override
    {
        mask_list = parent_->EvList;
        h_mask = parent_->EvHueMask;
        c_mask = parent_->EvChromaticityMask;
        l_mask = parent_->EvLightnessMask;
        blur = parent_->EvMaskBlur;
        show = parent_->EvShowMask;
        area_mask = parent_->EvAreaMask;
    }

    ToolPanelListener *listener() override
    {
        if (parent_->getEnabled()) {
            return parent_->listener;
        }
        return nullptr;
    }

    void selectionChanging(int idx) override
    {
        parent_->regionGet(idx);
    }

    void selectionChanged(int idx) override
    {
        parent_->regionShow(idx);
    }

    bool addPressed() override
    {
        parent_->data.push_back(GuidedSmoothingParams::Region());
        return true;
    }

    bool removePressed(int idx) override
    {
        parent_->data.erase(parent_->data.begin() + idx);
        return true;
    }
    
    bool copyPressed(int idx) override
    {
        parent_->data.push_back(parent_->data[idx]);
        return true;
    }

    bool resetPressed() override
    {
        parent_->data = { GuidedSmoothingParams::Region() };
        parent_->labMasks->setMasks({ LabCorrectionMask() }, -1);
        return true;
    }
    
    bool moveUpPressed(int idx) override
    {
        auto r = parent_->data[idx];
        parent_->data.erase(parent_->data.begin() + idx);
        --idx;
        parent_->data.insert(parent_->data.begin() + idx, r);
        return true;
    }
    
    bool moveDownPressed(int idx) override
    {
        auto r = parent_->data[idx];
        parent_->data.erase(parent_->data.begin() + idx);
        ++idx;
        parent_->data.insert(parent_->data.begin() + idx, r);
        return true;
    }

    int getColumnCount() override
    {
        return 1;
    }
    
    Glib::ustring getColumnHeader(int col) override
    {
        return M("TP_SMOOTHING_LIST_TITLE");
    }
    
    Glib::ustring getColumnContent(int col, int row) override
    {
        auto &r = parent_->data[row];

        return Glib::ustring::compose(
            "%1 %2 [%3]", r.radius, r.epsilon, 
            r.channel == GuidedSmoothingParams::Region::Channel::LUMINANCE ? "L" :
            (r.channel == GuidedSmoothingParams::Region::Channel::CHROMINANCE ? "C" : "RGB"));
    }

    void getEditIDs(EditUniqueID &hcurve, EditUniqueID &ccurve, EditUniqueID &lcurve) override
    {
        hcurve = EUID_LabMasks_H3;
        ccurve = EUID_LabMasks_C3;
        lcurve = EUID_LabMasks_L3;
    }

private:
    Smoothing *parent_;
};


//-----------------------------------------------------------------------------
// Smoothing
//-----------------------------------------------------------------------------

Smoothing::Smoothing(): FoldableToolPanel(this, "smoothing", M("TP_SMOOTHING_LABEL"), false, true)
{
    auto m = ProcEventMapper::getInstance();
    EvEnabled = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_SMOOTHING_ENABLED");
    EvChannel = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_SMOOTHING_CHANNEL");
    EvRadius = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_SMOOTHING_RADIUS");
    EvEpsilon = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_SMOOTHING_EPSILON");

    EvList = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_SMOOTHING_LIST");
    EvHueMask = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_SMOOTHING_HUEMASK");
    EvChromaticityMask = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_SMOOTHING_CHROMATICITYMASK");
    EvLightnessMask = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_SMOOTHING_LIGHTNESSMASK");
    EvMaskBlur = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_SMOOTHING_MASKBLUR");
    EvShowMask = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_SMOOTHING_SHOWMASK");
    EvAreaMask = m->newEvent(LUMINANCECURVE, "HISTORY_MSG_SMOOTHING_AREAMASK");

    box = Gtk::manage(new Gtk::VBox());

    Gtk::HBox *hb = Gtk::manage(new Gtk::HBox());
    hb->pack_start(*Gtk::manage(new Gtk::Label(M("TP_SMOOTHING_CHANNEL") + ":")), Gtk::PACK_SHRINK, 1);
    channel = Gtk::manage(new MyComboBoxText());
    channel->append(M("TP_SMOOTHING_CHANNEL_L"));
    channel->append(M("TP_SMOOTHING_CHANNEL_C"));
    channel->append(M("TP_SMOOTHING_CHANNEL_RGB"));
    channel->set_active(2);
    channel->signal_changed().connect(sigc::mem_fun(*this, &Smoothing::channelChanged));
    hb->pack_start(*channel, Gtk::PACK_EXPAND_WIDGET, 1);
    box->pack_start(*hb, Gtk::PACK_SHRINK, 1);
    
    radius = Gtk::manage(new Adjuster(M("TP_SMOOTHING_RADIUS"), 0, 1000, 1, 0));
    radius->setLogScale(100, 0);
    radius->setAdjusterListener(this);
    box->pack_start(*radius);
    
    epsilon = Gtk::manage(new Adjuster(M("TP_SMOOTHING_EPSILON"), -10, 10, 1, 0));
    epsilon->setAdjusterListener(this);
    box->pack_start(*epsilon);
    
    radius->delay = options.adjusterMaxDelay;
    epsilon->delay = options.adjusterMaxDelay;

    labMasksContentProvider.reset(new SmoothingMasksContentProvider(this));
    labMasks = Gtk::manage(new LabMasksPanel(labMasksContentProvider.get()));
    pack_start(*labMasks, Gtk::PACK_EXPAND_WIDGET, 4);   

    show_all_children();
}


void Smoothing::read(const ProcParams *pp, const ParamsEdited *pedited)
{
    disableListener();

    setEnabled(pp->smoothing.enabled);
    data = pp->smoothing.regions;
    auto m = pp->smoothing.labmasks;
    if (data.empty()) {
        data.emplace_back(rtengine::GuidedSmoothingParams::Region());
        m.emplace_back(rtengine::LabCorrectionMask());
    }
    labMasks->updateAreaMaskDefaults(pp);
    labMasks->setMasks(m, pp->smoothing.showMask);

    if (pedited) {
        set_inconsistent(multiImage && !pedited->smoothing.enabled);
        if (pedited->smoothing.regions) {
            labMasks->setEdited(true);
        } else {
            labMasks->setEdited(false);
        }
    }

    enableListener();
}


void Smoothing::write(ProcParams *pp, ParamsEdited *pedited)
{
    pp->smoothing.enabled = getEnabled();

    regionGet(labMasks->getSelected());
    pp->smoothing.regions = data;

    labMasks->getMasks(pp->smoothing.labmasks, pp->smoothing.showMask);
    assert(pp->smoothing.regions.size() == pp->smoothing.labmasks.size());

    labMasks->updateSelected();
        
    if (pedited) {
        pedited->smoothing.enabled = !get_inconsistent();
        pedited->smoothing.regions = labMasks->getEdited();
    }
}

void Smoothing::setDefaults(const ProcParams *defParams, const ParamsEdited *pedited)
{
    radius->setDefault(defParams->smoothing.regions[0].radius);
    epsilon->setDefault(defParams->smoothing.regions[0].epsilon);
    if (pedited) {
        radius->setDefaultEditedState(pedited->smoothing.regions ? Edited : UnEdited);
        epsilon->setDefaultEditedState(pedited->smoothing.regions ? Edited : UnEdited);
    } else {
        radius->setDefaultEditedState(Irrelevant);
        epsilon->setDefaultEditedState(Irrelevant);
    }
}


void Smoothing::adjusterChanged(Adjuster* a, double newval)
{
    if (listener && getEnabled()) {
        labMasks->setEdited(true);
        
        if (a == radius) {
            listener->panelChanged(EvRadius, a->getTextValue());
        } else if (a == epsilon) {
            listener->panelChanged(EvEpsilon, a->getTextValue());
        }
    }
}


void Smoothing::enabledChanged ()
{
    if (listener) {
        if (get_inconsistent()) {
            listener->panelChanged(EvEnabled, M("GENERAL_UNCHANGED"));
        } else if (getEnabled()) {
            listener->panelChanged(EvEnabled, M("GENERAL_ENABLED"));
        } else {
            listener->panelChanged(EvEnabled, M("GENERAL_DISABLED"));
        }
    }
}


void Smoothing::setBatchMode(bool batchMode)
{
    ToolPanel::setBatchMode(batchMode);
    labMasks->setBatchMode();
}


void Smoothing::setEditProvider(EditDataProvider *provider)
{
    labMasks->setEditProvider(provider);
}


void Smoothing::procParamsChanged(
    const rtengine::procparams::ProcParams* params,
    const rtengine::ProcEvent& ev,
    const Glib::ustring& descr,
    const ParamsEdited* paramsEdited)
{
    labMasks->updateAreaMaskDefaults(params);
}


void Smoothing::updateGeometry(int fw, int fh)
{
    labMasks->updateGeometry(fw, fh);
}


void Smoothing::regionGet(int idx)
{
    if (idx < 0 || size_t(idx) >= data.size()) {
        return;
    }
    
    auto &r = data[idx];
    r.channel = GuidedSmoothingParams::Region::Channel(channel->get_active_row_number());
    r.radius = radius->getValue();
    r.epsilon = epsilon->getValue();
}


void Smoothing::regionShow(int idx)
{
    const bool disable = listener;
    if (disable) {
        disableListener();
    }

    auto &r = data[idx];
    channel->set_active(int(r.channel));
    radius->setValue(r.radius);
    epsilon->setValue(r.epsilon);
    
    if (disable) {
        enableListener();
    }
}


void Smoothing::channelChanged()
{
    if (listener && (multiImage || getEnabled()) ) {
        listener->panelChanged(EvChannel, channel->get_active_text());
    }
}