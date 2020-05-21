/*
*  This file is part of RawTherapee.
*
*  Copyright (c) 2012 Oliver Duis <www.oliverduis.de>
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
#include "extprog.h"

#include <cstring>
#include <iostream>
#include <limits>
#include <thread>

#ifdef WIN32
#include <windows.h>
#include <shlobj.h>
#endif

#include "options.h"
#include "multilangmgr.h"
#include "../rtengine/utils.h"
#include "../rtengine/subprocess.h"


UserCommand::UserCommand():
    command(""),
    label(""),
    camera("^.*$"),
    extension(""),
    min_args(1),
    max_args(std::numeric_limits<size_t>::max()),
    filetype(ANY),
    match_camera(false),
    match_lens(false),
    match_shutter(false),
    match_iso(false),
    match_aperture(false),
    match_focallen(false)
{
}


bool UserCommand::matches(const std::vector<Thumbnail *> &args) const
{
    size_t n = args.size();
    if (!n || n < min_args || n > max_args) {
        return false;
    }

    auto md = args[0]->getMetaData();

    for (size_t i = 0; i < n; ++i) {
        auto mdi = args[i]->getMetaData();
        if (i > 0) {
            if (match_camera && (md->getMake() != mdi->getMake() ||
                                 md->getModel() != mdi->getModel())) {
                return false;
            }
            if (match_lens && md->getLens() != mdi->getLens()) {
                return false;
            }
            if (match_shutter && md->getShutterSpeed() != mdi->getShutterSpeed()) {
                return false;
            }
            if (match_iso && md->getISOSpeed() != mdi->getISOSpeed()) {
                return false;
            }
            if (match_aperture && md->getFNumber() != mdi->getFNumber()) {
                return false;
            }
            if (match_focallen && md->getFocalLen() != mdi->getFocalLen()) {
                return false;
            }
        }

        if (!Glib::Regex::match_simple(camera, mdi->getMake() + " " + mdi->getModel(), Glib::REGEX_CASELESS)) {
            return false;
        }
        if (filetype != ANY && (args[i]->getType() == FT_Raw) != (filetype == RAW)) {
            return false;
        }
        if (!extension.empty()) {
            auto ext = rtengine::getFileExtension(args[i]->getFileName());
            if (extension != ext.lowercase()) {
                return false;
            }
        }
    }

    return true;
}


void UserCommand::execute(const std::vector<Thumbnail *> &args) const
{
    if (args.empty()) {
        return;
    }

    std::vector<Glib::ustring> argv = rtengine::subprocess::split_command_line(command);
    
    for (auto &t : args) {
        argv.push_back(t->getFileName());
    }

    const auto doit =
        [=](bool verb) -> void
        {
            try {
                rtengine::subprocess::exec_sync(UserCommandStore::getInstance()->dir(), argv, false, nullptr, nullptr);
            } catch (rtengine::subprocess::error &exc) {
                if (verb) {
                    std::cerr << "Failed to execute \"" << command << "\": " << exc.what() << std::endl;
                }
            }
        };

    std::thread(doit, options.rtSettings.verbose).detach();
}



UserCommandStore *UserCommandStore::getInstance()
{
    static UserCommandStore instance;
    return &instance;
}


void UserCommandStore::init(const Glib::ustring &dirname)
{
    MyMutex::MyLock lock(mtx_);

    dir_ = Glib::filename_from_utf8(dirname);
    commands_.clear();

    try {
        Glib::Dir dir(dirname);
        std::vector<std::string> dirlist(dir.begin(), dir.end());
        std::sort(dirlist.begin(), dirlist.end());

        for (auto &filename : dirlist) {
            auto ext = rtengine::getFileExtension(filename).lowercase();
            if (ext != "txt") {
                continue;
            }
            
            const Glib::ustring pth = Glib::build_filename(dirname, filename);

            if (!Glib::file_test(pth, Glib::FILE_TEST_IS_REGULAR)) {
                continue;
            }

            try {
                const Glib::ustring group = "ART UserCommand";
                Glib::KeyFile kf;
                if (!kf.load_from_file(pth)) {
                    continue;
                }

                UserCommand cmd;
                if (kf.has_key(group, "Command")) {
                    cmd.command = kf.get_string(group, "Command");
                } else {
                    continue;
                }

                if (kf.has_key(group, "Label")) {
                    cmd.label = kf.get_string(group, "Label");
                } else {
                    continue;
                }

                if (kf.has_key(group, "Camera")) {
                    cmd.camera = kf.get_string(group, "Camera");
                }

                if (kf.has_key(group, "Extension")) {
                    cmd.extension = kf.get_string(group, "Extension").lowercase();
                }

                if (kf.has_key(group, "MinArgs")) {
                    cmd.min_args = kf.get_integer(group, "MinArgs");
                }

                if (kf.has_key(group, "MaxArgs")) {
                    cmd.max_args = kf.get_integer(group, "MaxArgs");
                }

                if (kf.has_key(group, "NumArgs")) {
                    cmd.min_args = cmd.max_args = kf.get_integer(group, "NumArgs");
                }

                if (kf.has_key(group, "FileType")) {
                    auto tp = kf.get_string(group, "FileType").lowercase();
                    if (tp == "raw") {
                        cmd.filetype = UserCommand::RAW;
                    } else if (tp == "nonraw") {
                        cmd.filetype = UserCommand::NONRAW;
                    } else {
                        cmd.filetype = UserCommand::ANY;
                    }
                }

                const auto getbool =
                    [&](const char *k) -> bool
                    {
                        return kf.has_key(group, k) && kf.get_boolean(group, k);
                    };
                cmd.match_camera = getbool("MatchCamera");
                cmd.match_lens = getbool("MatchLens");
                cmd.match_shutter = getbool("MatchShutter");
                cmd.match_iso = getbool("MatchISO");
                cmd.match_aperture = getbool("MatchAperture");
                cmd.match_focallen = getbool("MatchFocalLen");
            
                commands_.push_back(cmd);

                if (options.rtSettings.verbose) {
                    std::cout << "Found user command \"" << cmd.label << "\": "
                              << cmd.command << std::endl;
                }
            } catch (Glib::Exception &exc) {
                std::cout << "ERROR loading " << pth << ": " << exc.what()
                          << std::endl;
            }
        }
    } catch (Glib::Exception &exc) {
        std::cout << "ERROR scanning " << dirname << ": " << exc.what() << std::endl;
    }

    if (options.rtSettings.verbose) {
        std::cout << "Loaded " << commands_.size() << " user commands"
                  << std::endl;
    }
}


std::vector<UserCommand> UserCommandStore::getCommands(const std::vector<Thumbnail *> &sel) const
{
    std::vector<UserCommand> ret;
    for (auto &c : commands_) {
        if (c.matches(sel)) {
            ret.push_back(c);
        }
    }
    return ret;
}


namespace ExtProg {

bool spawnCommandAsync(const Glib::ustring &cmd)
{
    try {

        const auto encodedCmd = Glib::filename_from_utf8 (cmd);
        Glib::spawn_command_line_async (encodedCmd.c_str ());

        return true;

    } catch (const Glib::Exception& exception) {

        if (options.rtSettings.verbose) {
            std::cerr << "Failed to execute \"" << cmd << "\": " << exception.what() << std::endl;
        }

        return false;

    }
}


bool spawnCommandSync(const Glib::ustring &cmd)
{
    auto exitStatus = -1;

    try {

        Glib::spawn_command_line_sync (cmd, nullptr, nullptr, &exitStatus);

    } catch (const Glib::Exception& exception) {

        if (options.rtSettings.verbose) {
            std::cerr << "Failed to execute \"" << cmd << "\": " << exception.what() << std::endl;
        }

    }

    return exitStatus == 0;
}


bool openInGimp(const Glib::ustring &fileName)
{
#if defined WIN32

    auto executable = Glib::build_filename (options.gimpDir, "bin", "gimp-win-remote");
    auto success = ShellExecute( NULL, "open", executable.c_str(), fileName.c_str(), NULL, SW_SHOWNORMAL );

#elif defined __APPLE__

    // Apps should be opened using the simplest, case-insensitive form, "open -a NameOfProgram"
    // Calling the executable directly is said to often cause trouble,
    // https://discuss.pixls.us/t/affinity-photo-as-external-editor-how-to/1756/18
    auto cmdLine = Glib::ustring("open -a GIMP \'") + fileName + Glib::ustring("\'");
    auto success = spawnCommandAsync (cmdLine);

#else

    auto cmdLine = Glib::ustring("gimp \"") + fileName + Glib::ustring("\"");
    auto success = spawnCommandAsync (cmdLine);

#endif

#ifdef WIN32
    if ((uintptr_t)success > 32) {
        return true;
    }
#else
    if (success) {
        return true;
    }

#endif

#ifdef WIN32

    for (auto ver = 12; ver >= 0; --ver) {

        executable = Glib::build_filename (options.gimpDir, "bin", Glib::ustring::compose (Glib::ustring("gimp-2.%1.exe"), ver));
        auto success = ShellExecute( NULL, "open", executable.c_str(), fileName.c_str(), NULL, SW_SHOWNORMAL );

        if ((uintptr_t)success > 32) {
            return true;
        }
    }

#elif defined __APPLE__

    cmdLine = Glib::ustring("open -a GIMP-dev \'") + fileName + Glib::ustring("\'");
    success = spawnCommandAsync (cmdLine);

#else

    cmdLine = Glib::ustring("gimp-remote \"") + fileName + Glib::ustring("\"");
    success = spawnCommandAsync (cmdLine);

#endif

    return success;
}


bool openInPhotoshop(const Glib::ustring& fileName)
{
#if defined WIN32

    const auto executable = Glib::build_filename(options.psDir, "Photoshop.exe");
    const auto cmdLine = Glib::ustring("\"") + executable + Glib::ustring("\" \"") + fileName + Glib::ustring("\"");

#elif defined __APPLE__

    const auto cmdLine = Glib::ustring("open -a Photoshop \'") + fileName + Glib::ustring("\'");

#else

    const auto cmdLine = Glib::ustring("\"") + Glib::build_filename(options.psDir, "Photoshop.exe") + Glib::ustring("\" \"") + fileName + Glib::ustring("\"");

#endif

    return spawnCommandAsync (cmdLine);
}


bool openInCustomEditor(const Glib::ustring& fileName)
{
#if defined WIN32

    const auto cmdLine = Glib::ustring("\"") + options.customEditorProg + Glib::ustring("\"");
    auto success = ShellExecute( NULL, "open", cmdLine.c_str(), ('"' + fileName + '"').c_str(), NULL, SW_SHOWNORMAL );
    return (uintptr_t)success > 32;

#elif defined __APPLE__

    const auto cmdLine = options.customEditorProg + Glib::ustring(" \"") + fileName + Glib::ustring("\"");
    return spawnCommandAsync (cmdLine);

#else

    const auto cmdLine = Glib::ustring("\"") + options.customEditorProg + Glib::ustring("\" \"") + fileName + Glib::ustring("\"");
    return spawnCommandAsync (cmdLine);

#endif
}

} // namespace ExtProg
