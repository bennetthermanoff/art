/* -*- C++ -*-
 *
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2019 Alberto Griggio <alberto.griggio@gmail.com>
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

#include <stdio.h>
#include <glib/gstdio.h>
#include <iostream>
#include <unistd.h>

#include "metadata.h"
#include "settings.h"
#include "../rtgui/version.h"
#include "../rtgui/pathutils.h"


namespace rtengine {

extern const Settings *settings;


namespace {

Exiv2::Image::AutoPtr open_exiv2(const Glib::ustring &fname)
{
#if defined WIN32 && defined EXV_UNICODE_PATH
    auto *ws = g_utf8_to_utf16(fname.c_str(), -1, NULL, NULL, NULL);
    std::wstring wfname(reinterpret_cast<wchar_t *>(ws));
    g_free(ws);
    auto image = Exiv2::ImageFactory::open(wfname);
#else
    auto image = Exiv2::ImageFactory::open(Glib::filename_from_utf8(fname));
#endif
    return image;
}


inline bool check_exit_ok(int exit_status)
{
#ifdef WIN32
    return exit_status == 0;
#else
    return WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == 0;
#endif
}


Glib::ustring exiftool_base_dir;
#ifdef WIN32
  const Glib::ustring exiftool_default = "exiftool.exe";
#else
  const Glib::ustring exiftool_default = "exiftool";
#endif


Exiv2::Image::AutoPtr exiftool_import(const Glib::ustring &fname, const std::exception &exc)
{
    Glib::ustring exiftool = settings->exiftool_path;
    if (exiftool == exiftool_default) {
        Glib::ustring e = Glib::build_filename(exiftool_base_dir, exiftool);
        if (Glib::file_test(e, Glib::FILE_TEST_EXISTS)) {
            exiftool = e;
        }
    }
        
    std::string templ = Glib::build_filename(Glib::get_tmp_dir(), Glib::ustring::format("ART-exiftool-%1-XXXXXX", Glib::path_get_basename(fname)));
    int fd = Glib::mkstemp(templ);
    if (fd < 0) {
        throw exc;
    }
    std::string outname = templ + ".xmp";
    int exit_status = -1;
    std::vector<std::string> argv = {
        exiftool,
        "-TagsFromFile",
        fname,
        "-xmp:all<all",       
        outname
    };
    Glib::spawn_sync("", argv, Glib::SPAWN_DEFAULT|Glib::SPAWN_SEARCH_PATH, Glib::SlotSpawnChildSetup(), nullptr, nullptr, &exit_status);
    close(fd);
    g_remove(templ.c_str());
    if (!check_exit_ok(exit_status)) {
        if (Glib::file_test(outname, Glib::FILE_TEST_EXISTS)) {
            g_remove(outname.c_str());
        }
        throw exc;
    }
    try {
        auto image = Exiv2::ImageFactory::open(outname);
        image->readMetadata();
        auto &exif = image->exifData();
        auto &xmp = image->xmpData();
        const auto set_from =
            [&](const char *src, const char *dst) -> void
            {
                auto dk = Exiv2::ExifKey(dst);
                auto pos = exif.findKey(dk);
                if (pos == exif.end() || !pos->size()) {
                    auto sk = Exiv2::XmpKey(src);
                    auto it = xmp.findKey(sk);
                    if (it != xmp.end() && it->size()) {
                        exif[dst] = it->toString();
                    }
                }
            };
        set_from("Xmp.exifEX.LensModel", "Exif.Photo.LensModel");
        xmp.clear();
        g_remove(outname.c_str());
        return image;
    } catch (Exiv2::AnyError &) {
        if (Glib::file_test(outname, Glib::FILE_TEST_EXISTS)) {
            g_remove(outname.c_str());
        }
        throw exc;
    }
    return Exiv2::Image::AutoPtr();
}

} // namespace


Exiv2Metadata::Exiv2Metadata():
    src_(""),
    merge_xmp_(false),
    image_(nullptr)
{
}


Exiv2Metadata::Exiv2Metadata(const Glib::ustring &path):
    src_(path),
    merge_xmp_(settings->metadata_xmp_sync != Settings::MetadataXmpSync::NONE),
    image_(nullptr)
{
}


Exiv2Metadata::Exiv2Metadata(const Glib::ustring &path, bool merge_xmp_sidecar):
    src_(path),
    merge_xmp_(merge_xmp_sidecar),
    image_(nullptr)
{
}


void Exiv2Metadata::load() const
{
    if (!src_.empty() && !image_.get()) {
        try {
            auto img = open_exiv2(src_);
            image_.reset(img.release());
            image_->readMetadata();
        } catch (std::exception &exc) {
            auto img = exiftool_import(src_, exc);
            image_.reset(img.release());
        }

        if (merge_xmp_) {
            do_merge_xmp(image_.get());
        }
    }
}


void Exiv2Metadata::do_merge_xmp(Exiv2::Image *dst) const
{
    try { 
        auto xmp = getXmpSidecar(src_);
        Exiv2::ExifData exif;
        Exiv2::IptcData iptc;
        Exiv2::moveXmpToIptc(xmp, iptc);
        Exiv2::moveXmpToExif(xmp, exif);

        for (auto &datum : exif) {
            dst->exifData()[datum.key()] = datum;
        }
        for (auto &datum : iptc) {
            dst->iptcData()[datum.key()] = datum;
        }
        for (auto &datum : xmp) {
            dst->xmpData()[datum.key()] = datum;
        }
    } catch (Exiv2::AnyError &exc) {
        if (settings->verbose) {
            std::cerr << "Error loading metadata from XMP sidecar: "
                      << exc.what() << std::endl;
        }
    }
}


void Exiv2Metadata::saveToImage(const Glib::ustring &path) const
{
    auto dst = open_exiv2(path);
    dst->readMetadata();
    if (image_.get()) {
        dst->setMetadata(*image_);
        if (merge_xmp_) {
            do_merge_xmp(dst.get());
        }
        remove_unwanted(dst.get());
    } else {
        dst->setExifData(exif_data_);
        dst->setIptcData(iptc_data_);
        dst->setXmpData(xmp_data_);
    }

    dst->exifData()["Exif.Image.Software"] = RTNAME " " RTVERSION;
    import_exif_pairs(dst->exifData());
    import_iptc_pairs(dst->iptcData());
    dst->writeMetadata();    
}


void Exiv2Metadata::remove_unwanted(Exiv2::Image *dst) const
{
    static const std::vector<std::string> keys = {
        "Exif.Image.Orientation",
        "Exif.Image2.JPEGInterchangeFormat",
        "Exif.Image2.JPEGInterchangeFormatLength"
    };
    for (auto &k : keys) {
        auto it = dst->exifData().findKey(Exiv2::ExifKey(k));
        if (it != dst->exifData().end()) {
            dst->exifData().erase(it);
        }
    }
    Exiv2::ExifThumb thumb(dst->exifData());
    thumb.erase();
}


void Exiv2Metadata::import_exif_pairs(Exiv2::ExifData &out) const
{
    for (auto &p : exif_) {
        try {
            out[p.first] = p.second;
        } catch (Exiv2::AnyError &exc) {}
    }
}


void Exiv2Metadata::import_iptc_pairs(Exiv2::IptcData &out) const
{
    for (auto &p : iptc_) {
        try {
            auto &v = p.second;
            if (v.size() >= 1) {
                out[p.first] = v[0];
                for (size_t j = 1; j < v.size(); ++j) {
                    Exiv2::Iptcdatum d(Exiv2::IptcKey(p.first));
                    d.setValue(v[j]);
                    out.add(d);
                }
            }
        } catch (Exiv2::AnyError &exc) {}
    }
}


void Exiv2Metadata::saveToXmp(const Glib::ustring &path) const
{
    Exiv2::XmpData xmp;
    Exiv2::copyExifToXmp(exifData(), xmp);
    Exiv2::copyIptcToXmp(iptcData(), xmp);
    for (auto &datum : xmpData()) {
        xmp[datum.key()] = datum;
    }
    Exiv2::ExifData exif;
    Exiv2::IptcData iptc;
    import_exif_pairs(exif);
    import_iptc_pairs(iptc);
    Exiv2::copyExifToXmp(exif, xmp);
    Exiv2::copyIptcToXmp(iptc, xmp);

    std::string data;
    bool err = false;
    if (Exiv2::XmpParser::encode(data, xmp, Exiv2::XmpParser::omitPacketWrapper|Exiv2::XmpParser::useCompactFormat) != 0) {
        err = true;
    } else {
        FILE *out = g_fopen(path.c_str(), "wb");
        if (!out || fputs(data.c_str(), out) == EOF) {
            err = true;
        }
        if (out) {
            fclose(out);
        }
    }

    class Error: public Exiv2::AnyError {
    public:
        Error(const std::string &msg): msg_(msg) {}
        const char *what() const throw() { return msg_.c_str(); }
        int code() const throw() { return 0; }

    private:
        std::string msg_;
    };
    if (err) {
        throw Error("error saving XMP sidecar " + path);
    }
}


Glib::ustring Exiv2Metadata::xmpSidecarPath(const Glib::ustring &path)
{
    Glib::ustring fn = path;
    if (settings->xmp_sidecar_style == Settings::XmpSidecarStyle::STD) {
        fn = removeExtension(fn);
    }
    return fn + ".xmp";
}


Exiv2::XmpData Exiv2Metadata::getXmpSidecar(const Glib::ustring &path)
{
    Exiv2::XmpData ret;
    auto fname = xmpSidecarPath(path);
    if (Glib::file_test(fname, Glib::FILE_TEST_EXISTS)) {
        auto image = open_exiv2(fname);
        image->readMetadata();
        ret = image->xmpData();
    }
    return ret;
}


void Exiv2Metadata::init(const Glib::ustring &base_dir)
{
    exiftool_base_dir = base_dir;
    Exiv2::XmpParser::initialize();
}


void Exiv2Metadata::cleanup()
{
    Exiv2::XmpParser::terminate();
}

} // namespace rtengine
