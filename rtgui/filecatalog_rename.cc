/** -*- C++ -*-
 *  
 *  This file is part of ART.
 *
 *  Copyright (c) 2020 Alberto Griggio <alberto.griggio@gmail.com>
 *
 *  ART is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  ART is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with ART.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <gtkmm.h>
#include "filecatalog.h"
#include "options.h"
#include "multilangmgr.h"
#include "../rtengine/imagedata.h"
#include "../rtengine/metadata.h"
#include "pathutils.h"
#include "filepanel.h"
#include "rtwindow.h"
#include <time.h>
#include <ctype.h>
#include <sstream>
#include <iomanip>


namespace {

using rtengine::FramesMetaData;
using rtengine::Exiv2Metadata;

bool is_valid_char(gunichar c)
{
#ifdef __WIN32__
    switch (c) {
    case '<':
    case '>':
    case ':':
    case '"':
    case '/':
    case '\\':
    case '|':
    case '?':
    case '*':
        return false;
    default:
        return true;
    }
#else
    return c != '/';
#endif
}


Glib::ustring make_valid(const Glib::ustring &s)
{
    Glib::ustring ret;
    for (auto c : s) {
        if (!is_valid_char(c)) {
            if (c == '/') {
                ret.push_back(gunichar(gunichar(0x2215))); // unicode "division slash" ∕
            } else {
                ret.push_back('_');
            }
        } else {
            ret.push_back(c);
        }
    }
    return ret;
}


class Pattern {
public:
    virtual ~Pattern() {}
    virtual Glib::ustring operator()(const FramesMetaData *fd, const Exiv2Metadata *md) = 0;
};


class ProgressivePattern: public Pattern {
public:
    ProgressivePattern(int &idx, size_t pad): idx_(idx), pad_(pad) {}
    
    Glib::ustring operator()(const FramesMetaData *fd, const Exiv2Metadata *md) override
    {
        auto s = std::to_string(idx_++);
        if (s.length() < pad_) {
            auto s2 = std::string(pad_ - s.length(), '0');
            s = s2 + s;
        }
        return s;
    }

private:
    int &idx_;
    size_t pad_;
};


template <class F>
class FramesDataPattern: public Pattern {
public:
    FramesDataPattern(F func): func_(func) {}
    Glib::ustring operator()(const FramesMetaData *fd, const Exiv2Metadata *md) override
    {
        return make_valid(func_(fd));
    }
private:
    F func_;
};

template <class F>
std::unique_ptr<Pattern> make_pattern(F func)
{
    return std::unique_ptr<Pattern>(new FramesDataPattern<F>(func));
}


class TagPattern: public Pattern {
public:
    TagPattern(const std::string &tag): tag_(tag) {}
    
    Glib::ustring operator()(const FramesMetaData *fd, const Exiv2Metadata *md) override
    {
        try {
            md->load();
            if (strncmp(tag_.c_str(), "Exif.", 5) == 0) {
                auto it = md->exifData().findKey(Exiv2::ExifKey(tag_));
                if (it != md->exifData().end()) {
                    return make_valid(it->toString());
                }
            } else if (strncmp(tag_.c_str(), "Iptc.", 5) == 0) {
                auto it = md->iptcData().findKey(Exiv2::IptcKey(tag_));
                if (it != md->iptcData().end()) {
                    return make_valid(it->toString());
                }
            } else if (strncmp(tag_.c_str(), "Xmp.", 4) == 0) {
                auto it = md->xmpData().findKey(Exiv2::XmpKey(tag_));
                if (it != md->xmpData().end()) {
                    return make_valid(it->toString());
                }
            }
        } catch (std::exception &e) {
            if (options.rtSettings.verbose) {
                std::cout << "TagPattern error: " << e.what() << std::endl;
            }
        }
        return "";
    }

private:
    std::string tag_;
};


struct Params {
    std::vector<std::unique_ptr<Pattern>> pattern;
    std::vector<Glib::ustring> sidecars;
    enum class Normalization {
        OFF,
        UPPERCASE,
        LOWERCASE
    };
    Normalization name_norm;
    Normalization ext_norm;
    bool allow_whitespace;
    enum class OnExistingAction {
        SKIP,
        RENAME
    };
    OnExistingAction on_existing;
    int progressive_number;

    Params() = default;
};


template <class T>
std::string tostr(T n, int digits)
{
    std::ostringstream buf;
    buf << std::setprecision(digits) << std::fixed << n;
    return buf.str();
}

/*
 * pattern syntax:
 * %f : FileNamePattern
 * %e : FileExtPattern
 * %a : DatePattern(day name abbreviated)
 * %A : DatePattern(day name full)
 * %b : DatePattern(month name abbreviated)
 * %B : DatePattern(month name full)
 * %m : DatePattern(month)
 * %Y : DatePattern(year)
 * %y : DatePattern(year 2-digits)
 * %d : DatePattern(day)
 * %C : CameraPattern
 * %M : MakePattern
 * %N : ModelPattern
 * %r : FramesDataPattern<rating>
 * %I : FramesDataPattern<ISO>
 * %F : FramesDataPattern<FNumber>
 * %L : FramesDataPattern<Lens>
 * %l : FramesDataPattern<FocalLength>
 * %E : FramesDataPattern<ExpComp>
 * %s : FramesDataPattern<ShutterSpeed>
 * %n[0-9] : ProgressivePattern
 * %T[tag] : TagPattern
 * %% : % character
 */ 
bool parse_pattern(const Glib::ustring &s, Params &out)
{
    typedef const FramesMetaData *FD;
    out.pattern.clear();
    size_t prev = 0;
    size_t n = s.length();
    for (size_t i = 0; i < n; ) {
        auto c = s[i];
        if (c == '%') {
            if (prev != i) {
                auto f = s.substr(prev, i-prev);
                out.pattern.push_back(
                    make_pattern([f](FD fd) { return f; }));
            }
            if (i+1 < n) {
                i += 2;
                c = s[i-1];
                switch (c) {
                case 'f':
                    out.pattern.push_back(
                        make_pattern(
                            [](FD fd) {
                                return removeExtension(
                                    Glib::path_get_basename(fd->getFileName()));
                            }));
                    break;
                case 'e':
                    out.pattern.push_back(
                        make_pattern(
                            [](FD fd) {
                                return getExtension(fd->getFileName());
                            }));
                    break;
                case 'm':
                case 'd':
                case 'Y':
                case 'y':
                case 'a':
                case 'A':
                case 'b':
                case 'B':
                    out.pattern.push_back(
                        make_pattern(
                            [c](FD fd) {
                                char buf[256];
                                char fmt[3] = { '%', char(c), 0 };
                                auto t = fd->getDateTime();
                                strftime(buf, 255, fmt, &t);
                                return std::string(buf);
                            }));
                    break;
                case 'C':
                    out.pattern.push_back(
                        make_pattern(
                            [](FD fd) {
                                return fd->getMake() + " " + fd->getModel();
                            }));
                    break;
                case 'M':
                    out.pattern.push_back(
                        make_pattern(
                            [](FD fd) {
                                return fd->getMake();
                            }));
                    break;
                case 'N':
                    out.pattern.push_back(
                        make_pattern(
                            [](FD fd) {
                                return fd->getModel();
                            }));
                    break;
                case 'n': 
                    if (i < n && isdigit(s[i])) {
                        out.pattern.emplace_back(
                            new ProgressivePattern(out.progressive_number,
                                                   int(s[i]) - int('0')));
                        ++i;
                    } else {
                        out.pattern.emplace_back(
                            new ProgressivePattern(out.progressive_number, 0));
                    }
                    break;
                case 'T':
                    if (i < n && s[i] == '[') {
                        size_t j = i+1;
                        while (j < n && s[j] != ']') {
                            ++j;
                        }
                        if (j < n) {
                            out.pattern.emplace_back(
                                new TagPattern(s.substr(i+1, j-i-1)));
                            i = j+1;
                        } else {
                            return false;
                        }
                    } else {
                        return false;
                    }
                    break;
                case 'r':
                    out.pattern.push_back(
                        make_pattern(
                            [](FD fd) {
                                return tostr(fd->getRating(), 0);
                            }));
                    break;
                case 'I':
                    out.pattern.push_back(
                        make_pattern(
                            [](FD fd) {
                                return tostr(fd->getISOSpeed(), 0);
                            }));
                    break;
                case 'F':
                    out.pattern.push_back(
                        make_pattern(
                            [](FD fd) {
                                return fd->apertureToString(fd->getFNumber());
                            }));
                    break;
                case 'L':
                    out.pattern.push_back(
                        make_pattern(
                            [](FD fd) {
                                return fd->getLens();
                            }));
                    break;
                case 'l':
                    out.pattern.push_back(
                        make_pattern(
                            [](FD fd) {
                                return tostr(fd->getFocalLen(), 0);
                            }));
                    break;
                case 'E':
                    out.pattern.push_back(
                        make_pattern(
                            [](FD fd) {
                                return fd->expcompToString(fd->getExpComp(), false);
                            }));
                    break;
                case 's':
                    out.pattern.push_back(
                        make_pattern(
                            [](FD fd) {
                                return fd->shutterToString(fd->getShutterSpeed());
                            }));
                    break;
                case '%':
                    out.pattern.push_back(
                        make_pattern([](FD fd) { return "%"; }));
                    break;
                default:
                    return false;
                }
                prev = i;
            } else {
                return false;
            }
        } else {
            if (!is_valid_char(c)) {
                return false;
            }
            ++i;
        }
    }
    if (prev < n) {
        auto f = s.substr(prev);
        out.pattern.push_back(make_pattern([f](FD fd) { return f; }));
    }
    return !out.pattern.empty();
}


Glib::ustring trim(const Glib::ustring &s, bool start=true, bool end=true)
{
    size_t i = 0;
    size_t n = s.length();
    while (start && isspace(s[i])) {
        ++i;
    }
    while (end && n > 0 && isspace(s[n-1])) {
        --n;
    }
    return s.substr(i, n-i);
}


bool parse_sidecars(const Glib::ustring &s, Params &out)
{
    out.sidecars.clear();
    size_t prev = 0;
    size_t n = s.length();
    for (size_t i = 0; i < n; ++i) {
        if (s[i] == ';') {
            auto e = trim(s.substr(prev, i-prev));
            if (!e.empty()) {
                out.sidecars.push_back(e);
            }
            ++i;
            prev = i;
        }
    }
    if (prev < n) {
        auto e = trim(s.substr(prev));
        if (!e.empty()) {
            out.sidecars.push_back(e);
        }
    }
    return true;
}


Glib::ustring get_new_name(Params &params, FileBrowserEntry *entry)
{
    std::unique_ptr<FramesMetaData> fd(FramesMetaData::fromFile(entry->thumbnail->getFileName()));
    Exiv2Metadata md(entry->thumbnail->getFileName());

    Glib::ustring name;
    for (auto &p : params.pattern) {
        name += (*p)(fd.get(), &md);
    }
    Glib::ustring ext = getExtension(name);
    if (!ext.empty()) {
        ext = "." + ext;
        name = removeExtension(name);
    }
    Glib::ustring ret;
    
    for (auto c : name) {
        if (!params.allow_whitespace && isspace(c)) {
            c = '_';
        }
        switch (params.name_norm) {
        case Params::Normalization::UPPERCASE:
            c = toupper(c);
            break;
        case Params::Normalization::LOWERCASE:
            c = tolower(c);
            break;
        default:
            break;
        }
        ret.push_back(c);
    }
    for (auto c : ext) {
        if (!params.allow_whitespace && isspace(c)) {
            c = '_';
        }
        switch (params.ext_norm) {
        case Params::Normalization::UPPERCASE:
            c = toupper(c);
            break;
        case Params::Normalization::LOWERCASE:
            c = tolower(c);
            break;
        default:
            break;
        }
        ret.push_back(c);
    }
    
    return ret;
}


bool get_params(Gtk::Window &parent, const std::vector<FileBrowserEntry *> &args, Params &out)
{
    Gtk::Dialog dialog(M("FILEBROWSER_RENAMEDLGLABEL"), parent);
    Gtk::Label lbl(M("RENAME_DIALOG_PATTERN"));
    Gtk::Entry pattern;
    Gtk::VBox vb;
    Gtk::HBox hb;
    Gtk::HBox mainhb;
    Gtk::VBox mainvb;
    double s = 1;
    int pad = 4 * s;
    
    hb.pack_start(lbl, Gtk::PACK_SHRINK, pad);
    hb.pack_start(pattern, Gtk::PACK_EXPAND_WIDGET, pad);
    vb.pack_start(hb, Gtk::PACK_SHRINK, pad);
    pattern.set_tooltip_markup(M("RENAME_DIALOG_PATTERN_TIP"));

    Gtk::HBox hb2;
    Gtk::Label lbl7(M("RENAME_DIALOG_ALLOW_WHITESPACE"));
    Gtk::CheckButton allow_whitespace("");
    hb2.pack_start(lbl7, Gtk::PACK_SHRINK, pad);
    hb2.pack_start(allow_whitespace, Gtk::PACK_SHRINK, pad);
    mainvb.pack_start(hb2, Gtk::PACK_SHRINK, pad);

    Gtk::HBox hb3;
    Gtk::Label lbl2(M("RENAME_DIALOG_NAME_NORMALIZATION"));
    hb3.pack_start(lbl2, Gtk::PACK_SHRINK, pad);
    Gtk::ComboBoxText name_norm;
    name_norm.append(M("RENAME_DIALOG_NORM_OFF"));
    name_norm.append(M("RENAME_DIALOG_NORM_UPPERCASE"));
    name_norm.append(M("RENAME_DIALOG_NORM_LOWERCASE"));
    hb3.pack_start(name_norm, Gtk::PACK_EXPAND_WIDGET, pad);
    mainvb.pack_start(hb3, Gtk::PACK_SHRINK, pad);

    Gtk::Box hb4;
    Gtk::Label lbl3(M("RENAME_DIALOG_EXT_NORMALIZATION"));
    hb4.pack_start(lbl3, Gtk::PACK_SHRINK, pad);
    Gtk::ComboBoxText ext_norm;
    ext_norm.append(M("RENAME_DIALOG_NORM_OFF"));
    ext_norm.append(M("RENAME_DIALOG_NORM_UPPERCASE"));
    ext_norm.append(M("RENAME_DIALOG_NORM_LOWERCASE"));
    hb4.pack_start(ext_norm, Gtk::PACK_EXPAND_WIDGET, pad);
    mainvb.pack_start(hb4, Gtk::PACK_SHRINK, pad);
    
    Gtk::HBox hb6;
    Gtk::Label lbl5(M("RENAME_DIALOG_ON_EXISTING"));
    hb6.pack_start(lbl5, Gtk::PACK_SHRINK, pad);
    Gtk::ComboBoxText on_existing;
    on_existing.append(M("RENAME_DIALOG_SKIP"));
    on_existing.append(M("RENAME_DIALOG_RENAME"));
    hb6.pack_start(on_existing, Gtk::PACK_EXPAND_WIDGET, pad);
    mainvb.pack_start(hb6, Gtk::PACK_SHRINK, pad);

    Gtk::HBox hb7;
    Gtk::Label lbl6(M("RENAME_DIALOG_PROGRESSIVE"));
    hb7.pack_start(lbl6, Gtk::PACK_SHRINK, pad);
    Gtk::SpinButton progressive_number;
    progressive_number.set_range(1, 1000000);
    progressive_number.set_increments(1, 1);
    progressive_number.set_value(1);
    hb7.pack_start(progressive_number, Gtk::PACK_EXPAND_WIDGET, pad);
    mainvb.pack_start(hb7, Gtk::PACK_SHRINK, pad);

    Gtk::HBox hb5;
    Gtk::Label lbl4(M("RENAME_DIALOG_SIDECARS"));
    hb5.pack_start(lbl4, Gtk::PACK_SHRINK, pad);
    Gtk::Entry sidecars;
    sidecars.set_tooltip_markup(M("RENAME_DIALOG_SIDECARS_TIP"));
    hb5.pack_start(sidecars, Gtk::PACK_EXPAND_WIDGET, pad);
    mainvb.pack_start(hb5, Gtk::PACK_SHRINK, pad);
    
    Gtk::ListViewText filelist(1);
    filelist.set_column_title(0, M("RENAME_DIALOG_FILENAMES") + " (" + std::to_string(args.size()) + ")");
    filelist.set_activate_on_single_click(true);
    for (auto &e : args) {
        filelist.append(Glib::path_get_basename(e->thumbnail->getFileName()));
    }
    if (!args.empty()) {
        Gtk::TreePath pth;
        pth.push_back(0);
        filelist.get_selection()->select(pth);
    }        
    mainhb.pack_start(filelist, Gtk::PACK_EXPAND_WIDGET, pad);
    mainhb.pack_start(mainvb, Gtk::PACK_EXPAND_WIDGET);
    vb.pack_start(mainhb, Gtk::PACK_SHRINK);

    Gtk::HBox hb8;
    Gtk::Label info;
    info.set_markup("<span size=\"large\"><b>" + M("RENAME_DIALOG_PREVIEW") + ": </b></span>");
    hb8.pack_start(info, Gtk::PACK_SHRINK, 2*pad);
    Gtk::Label empty;
    hb8.pack_start(empty, Gtk::PACK_EXPAND_WIDGET);
    vb.pack_start(hb8, Gtk::PACK_SHRINK);
    
    dialog.get_content_area()->pack_start(vb, Gtk::PACK_SHRINK, 2*pad);
    auto okbtn = dialog.add_button(M("GENERAL_OK"), 1);
    dialog.add_button(M("GENERAL_CANCEL"), 0);
    dialog.set_size_request(600, -1);
    dialog.show_all_children();


    const auto getparams =
        [&]() -> bool
        {
            bool err = false;
            Glib::ustring errmsg = "";
            Glib::ustring patternstr = pattern.get_text();
            if (!parse_pattern(patternstr, out)) {
                errmsg = M("RENAME_DIALOG_INVALID_PATTERN");
                err = true;
            } else if (!parse_sidecars(sidecars.get_text(), out)) {
                errmsg = M("RENAME_DIALOG_INVALID_SIDECARS");
                err = true;
            }

            info.set_markup("<span size=\"large\"><b>" + M("RENAME_DIALOG_PREVIEW") + ": <span foreground=\"#ff0000\">" + errmsg + "</span></b></span>");
            
            if (err) {
                okbtn->set_sensitive(false);
                return false;
            } 

            okbtn->set_sensitive(true);
            
            out.name_norm = Params::Normalization(name_norm.get_active_row_number());
            out.ext_norm = Params::Normalization(ext_norm.get_active_row_number());
            out.allow_whitespace = allow_whitespace.get_active();
            out.on_existing = Params::OnExistingAction(on_existing.get_active_row_number());
            out.progressive_number = progressive_number.get_value_as_int();

            options.renaming.pattern = patternstr;
            options.renaming.sidecars = sidecars.get_text();
            options.renaming.name_norm = name_norm.get_active_row_number();
            options.renaming.ext_norm = ext_norm.get_active_row_number();
            options.renaming.allow_whitespace = allow_whitespace.get_active();
            options.renaming.on_existing = on_existing.get_active_row_number();
            options.renaming.progressive_number = progressive_number.get_value_as_int();

            return true;
        };

    const auto on_pattern_change =
        [&]() -> void
        {
            if (getparams()) {
                auto sel = filelist.get_selected();
                if (!sel.empty()) {
                    auto entry = args[sel[0]];
                    Glib::ustring newname = get_new_name(out, entry);
                    Glib::ustring show;
                    for (auto c : newname) {
                        if (isspace(c)) {
                            show += "<span foreground=\"#E59836\">";
                            show.push_back(gunichar(9141));
                            show += "</span>";
                        } else {
                            show.push_back(c);
                        }
                    }
                    info.set_markup("<span size=\"large\"><b>" + M("RENAME_DIALOG_PREVIEW") + ": " + show + "</b></span>");
                }
            }
        };

    const auto on_file_select =
        [&](const Gtk::TreeModel::Path& path, Gtk::TreeViewColumn* column) -> void
        {
            on_pattern_change();
        };

    pattern.set_text(options.renaming.pattern);
    sidecars.set_text(options.renaming.sidecars);
    name_norm.set_active(options.renaming.name_norm);
    ext_norm.set_active(options.renaming.ext_norm);
    on_existing.set_active(options.renaming.on_existing);
    allow_whitespace.set_active(options.renaming.allow_whitespace);
    progressive_number.set_value(options.renaming.progressive_number);
    
    pattern.signal_changed().connect(sigc::slot<void>(on_pattern_change));
    name_norm.signal_changed().connect(sigc::slot<void>(on_pattern_change));
    ext_norm.signal_changed().connect(sigc::slot<void>(on_pattern_change));
    on_existing.signal_changed().connect(sigc::slot<void>(on_pattern_change));
    allow_whitespace.signal_toggled().connect(sigc::slot<void>(on_pattern_change));
    progressive_number.signal_value_changed().connect(sigc::slot<void>(on_pattern_change));
    filelist.signal_row_activated().connect(sigc::slot<void, const Gtk::TreeModel::Path &, Gtk::TreeViewColumn *>(on_file_select));

    on_pattern_change();

    return (dialog.run() == 1) && getparams();
}


void get_targets(Params &params, FileBrowserEntry *entry,
                 std::vector<std::pair<Glib::ustring, Glib::ustring>> &out)
{
    out.clear();
    
    auto fn = entry->thumbnail->getFileName();
    auto dir = Glib::path_get_dirname(fn);
    auto name = Glib::path_get_basename(fn);
    auto newname = get_new_name(params, entry);
    auto newpath = Glib::build_filename(dir, newname);
    if (Glib::file_test(newpath, Glib::FILE_TEST_EXISTS)) {
        if (params.on_existing == Params::OnExistingAction::RENAME) {
            auto bn = removeExtension(newname);
            auto ext = getExtension(newname);
            if (!ext.empty()) {
                ext = "." + ext;
            }
            for (int i = 1; ; ++i) {
                auto nn = bn + "_" + std::to_string(i) + ext;
                newpath = Glib::build_filename(dir, nn);
                if (!Glib::file_test(newpath, Glib::FILE_TEST_EXISTS)) {
                    newname = nn;
                    break;
                }
            }
        } else {
            return; // skip
        }
    }
    out.push_back(std::make_pair(fn, newpath));
    auto pf = options.getParamFile(fn);
    if (Glib::file_test(pf, Glib::FILE_TEST_EXISTS)) {
        out.push_back(std::make_pair(pf, options.getParamFile(newpath)));
    }
    auto xmp = options.getXmpSidecarFile(fn);
    if (Glib::file_test(xmp, Glib::FILE_TEST_EXISTS)) {
        out.push_back(std::make_pair(xmp, options.getXmpSidecarFile(newpath)));
    }
    if (!params.sidecars.empty()) {
        auto base_fn = removeExtension(fn);
        auto base_newpath = removeExtension(newpath);
        for (auto &s : params.sidecars) {
            Glib::ustring orig_sidename;
            Glib::ustring new_sidename;
            if (s[0] == '+') {
                orig_sidename = fn + "." + s.substr(1);
                new_sidename = newpath + "." + s.substr(1);
            } else {
                orig_sidename = base_fn + "." + s;
                new_sidename = base_newpath + "." + s;
            }
            if (Glib::file_test(orig_sidename, Glib::FILE_TEST_EXISTS)) {
                out.push_back(std::make_pair(orig_sidename, new_sidename));
            }
        }
    }
}

} // namespace


void FileCatalog::renameRequested(const std::vector<FileBrowserEntry *> &args)
{
    Params params;
    if (get_params(getToplevelWindow(this), args, params)) {
        std::vector<std::pair<Glib::ustring, Glib::ustring>> torename;
        for (auto e : args) {
            get_targets(params, e, torename);
            bool first = true;
            for (auto &p : torename) {
                if (::g_rename(p.first.c_str(), p.second.c_str()) == 0) {
                    if (first) {
                        cacheMgr->renameEntry(p.first, e->thumbnail->getMD5(), p.second);
                    }
                } else {
                    filepanel->getParent()->error(Glib::ustring::compose(M("RENAME_DIALOG_ERROR"), p.first, p.second));
                }
                first = false;
            }
        }
        reparseDirectory();
    }
}
    