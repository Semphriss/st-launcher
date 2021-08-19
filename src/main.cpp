//  SuperTux Launcher - A simple launcher interface for SuperTux
//  Copyright (C) 2021 Semphris <semphris@protonmail.com>
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#if defined(unix) || defined(__unix__) || defined(__unix)
#define UNIX 1
#endif

#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#ifdef UNIX
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "curl/curl.h"
#include "SDL.h"
#include "SDL_image.h"
#include "SDL_ttf.h"
#include "portable-file-dialogs.h"

#include "ui/button_image.hpp"
#include "ui/button_label.hpp"
#include "ui/container_scroll.hpp"
#include "ui/listbox.hpp"
#include "ui/scrollbar.hpp"
#include "ui/textbox.hpp"
#include "util/log.hpp"
#include "video/gl/gl_window.hpp"
#include "video/sdl/sdl_window.hpp"
#include "video/drawing_context.hpp"
#include "video/font.hpp"
#include "video/renderer.hpp"
#include "video/window.hpp"

#define OS "x64-linux"

#define CRASH_URL "https://supertux.semphris.com/upload_crash"
#define VERSIONS_URL "http://supertux.semphris.com/versions/" OS

void
create_dir(const char* path)
{
#ifdef UNIX
    int mkdir_result = mkdir(path, 0700);
    if (mkdir_result && mkdir_result != EEXIST && mkdir_result != -1)
    {
      log_warn << "Could not create user folder '" << path << "': Error "
               << mkdir_result << std::endl;
    }
#endif
}

std::string
upload_crash(const char* path)
{
  std::string contents;
  std::ifstream in(path, std::ios::in | std::ios::binary);

  if (!in)
    return "Could not open log file";

  std::ostringstream sstr;
  sstr << in.rdbuf();
  contents = sstr.str();
  in.close();

  CURL *curl;
  CURLcode res;

  struct curl_httppost *formpost = NULL;
  struct curl_httppost *lastptr = NULL;
  struct curl_slist *headerlist = NULL;
  static const char buf[] =  "Expect:";

  FILE *devnull = fopen("/dev/null", "w+");

  curl_global_init(CURL_GLOBAL_ALL);

  curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "cache-control:",
               CURLFORM_COPYCONTENTS, "no-cache", CURLFORM_END);
  curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "content-type:",
               CURLFORM_COPYCONTENTS, "multipart/form-data", CURLFORM_END);
  curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "logs", CURLFORM_BUFFER,
               "data", CURLFORM_BUFFERPTR, contents.data(),
               CURLFORM_BUFFERLENGTH, contents.size(), CURLFORM_END);
  curl = curl_easy_init();

  headerlist = curl_slist_append(headerlist, buf);
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, CRASH_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, devnull);
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    curl_formfree(formpost);
    curl_slist_free_all(headerlist);
  }

  fclose(devnull);

  return (curl && (res == CURLE_OK)) ? "" : curl_easy_strerror(res);
}

static size_t donwload_size_so_far = 0;
static double donwload_size_total = 0;
static Window* window_to_draw_on = nullptr;

size_t
dont_write(void*, size_t size, size_t nmemb, void*) {
  return size * nmemb;
}

size_t
write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  size_t written;
  written = fwrite(ptr, size, nmemb, stream);
  donwload_size_so_far += written;

  if (window_to_draw_on)
  {
    auto& w = *window_to_draw_on;
    w.get_renderer().start_draw();
    auto& t = w.load_texture("../data/images/background.png");
    w.get_renderer().draw_texture(t, t.get_size(), Rect(0, 0, 640, 400), 0.f, Color(.3f, .3f, .3f), Renderer::Blend::BLEND);
    w.get_renderer().draw_text("Downloading file... (This might take a while)", Vector(320, 200), Rect(0, 0, 640, 400), Renderer::TextAlign::CENTER, "../data/fonts/Roboto-Regular.ttf", 16, Color(1.f, 1.f, 1.f), Renderer::Blend::BLEND);
    w.get_renderer().draw_text(std::to_string(floor(double(donwload_size_so_far) / 65536.0) / 16.0) + " Mb / " + std::to_string(floor(donwload_size_total / 65536.0) / 16.0) + " Mb", Vector(320, 230), Rect(0, 0, 640, 400), Renderer::TextAlign::CENTER, "../data/fonts/Roboto-Regular.ttf", 14, Color(1.f, 1.f, 1.f), Renderer::Blend::BLEND);
    w.get_renderer().end_draw();
  }

  return written;
}

std::string
fetch_file(std::string url, const char* path)
{
  CURL *curl;
  FILE *fp;
  CURLcode res;

  donwload_size_so_far = 0;
  donwload_size_total = 0;

  curl = curl_easy_init();
  if(curl) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HEADER, 1);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dont_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, nullptr);
    // FIXME: That's a security issue
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false);
    res = curl_easy_perform(curl);
  
    if(!res) {
      /* check the size */
      double cl;
      res = curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &cl);
      if(!res) {
        donwload_size_total = cl;
      }
    }
  }

  curl = curl_easy_init();
  if (curl) {
    fp = fopen(path, "wb");
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    // FIXME: That's a security issue
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(fp);
  }

  return (curl && (res == CURLE_OK)) ? "" : curl_easy_strerror(res);
}

// TODO: The obvious
std::vector<std::tuple<std::string, std::vector<std::tuple<std::string, std::string>>>>
get_installs(const char* path)
{
  std::string line;
  std::ifstream myfile;
  myfile.open(path);

  if(!myfile.is_open()) {
    return {{"Custom", {}}};
  }

  std::vector<std::tuple<std::string, std::vector<std::tuple<std::string, std::string>>>> r;
  while(std::getline(myfile, line)) {
    if (line.size() < 3)
      continue;

    if (line.at(0) == '#')
    {
      if (line.at(1) != ' ')
        continue;

      r.push_back({line.substr(2), {}});
    }
    else
    {
      if (line.find(": ") == std::string::npos || line.size() < line.find(": ") + 3 || line.find(": ") == 0 || line.find(":") < line.find(": "))
        continue;

      if (r.size() < 1)
        continue;

      auto l = std::make_tuple<std::string, std::string>(line.substr(0, line.find(':')), line.substr(line.find(':') + 2));
      std::get<1>(r.back()).push_back(l);
    }
  }

  if (r.empty() || std::get<0>(r.back()) != "Custom")
  {
    r.push_back({"Custom", {}});
  }

  return std::move(r);
}

void
save(std::string path, const std::vector<std::tuple<std::string, std::vector<std::tuple<std::string, std::string>>>>& installs)
{
  std::ofstream out(path);

  out << "Text above the first category (line starting with #) will be ignored."
         " Text that\nis not properly formatted will also be safely ignored.\n"
         "\nFormat for categories: Pound + space + name\nExample:              "
         " # My Category Name\n\nFormat for versions:   label + colon + space +"
         " path (label may not have colons)\nExample:               v0.0.0: "
         "https://example.org/download/version-0-0-0.zip\n\nGiven that the "
         "first non-empty line below starts with a pound+space pair, any\nline "
         "that contains colons will be interpreted as a valid version. Be "
         "careful\nif you write comments below them!\n\n";

  for (const auto& category : installs)
  {
    out << "\n# " << std::get<0>(category) << "\n";
    for (const auto& version : std::get<1>(category))
    {
      out << std::get<0>(version) << ": " << std::get<1>(version) << "\n";
    }
  }

  out << std::endl; // Both a newline and a flush
  out.close();
}

int
main()
{
  try
  {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
      throw std::runtime_error("Could not init SDL: " + std::string(SDL_GetError()));
    }

    if (!IMG_Init(IMG_INIT_PNG))
    {
      throw std::runtime_error("Could not init IMG: " + std::string(IMG_GetError()));
    }

    if (TTF_Init())
    {
      throw std::runtime_error("Could not init TTF: " + std::string(TTF_GetError()));
    }

    auto* path = SDL_GetPrefPath("SuperTux","stlauncher");
    create_dir(path);
    create_dir((std::string(path) + "/userdirs/").c_str());
    create_dir((std::string(path) + "/installs/").c_str());

    SDLWindow w(Size(640.f, 400.f), true);
    w.set_title("SuperTux Launcher");
    w.set_bordered(false);
    window_to_draw_on = &w;

    SDL_SetWindowHitTest(w.get_sdl_window(),
      [](SDL_Window* win, const SDL_Point* area, void* /* data */) {
        return (area->y < 20 && area->x < 600) ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
      }, nullptr);

    Control::ThemeSet t;
    memset(&t, 0, sizeof(t));
    t.active.font = "../data/fonts/Roboto-Regular.ttf";
    t.active.fontsize = 18;
    t.active.fg_blend = Renderer::Blend::BLEND;
    t.active.bg_blend = Renderer::Blend::BLEND;
    t.active.fg_color = Color(0.f, 0.f, 0.f);
    t.active.top.padding = 3.f;
    t.active.left.padding = 3.f;
    t.active.right.padding = 3.f;
    t.active.bottom.padding = 3.f;
    t.disabled = t.active;
    t.focus = t.active;
    t.hover = t.active;
    t.normal = t.active;
    t.active.bg_color = Color(1.f, 1.f, 1.f);
    t.disabled.bg_color = Color(.7f, .7f, .75f);
    t.focus.bg_color = Color(.85f, .85f, .9f);
    t.hover.bg_color = Color(.9f, .9f, .95f);
    t.normal.bg_color = Color(.75f, .75f, .8f);
    Control::ThemeSet t2 = t;
    t2.active.bg_color = Color(0.f, 0.f, 0.f);
    t2.disabled.bg_color = Color(0.f, 0.f, 0.f);
    t2.focus.bg_color = Color(0.f, 0.f, 0.f);
    t2.hover.bg_color = Color(0.f, 0.f, 0.f);
    t2.normal.bg_color = Color(0.f, 0.f, 0.f);
    t2.active.fg_color = Color(1.f, 1.f, 1.f);
    t2.disabled.fg_color = Color(.7f, .7f, .75f);
    t2.focus.fg_color = Color(.85f, .85f, .9f);
    t2.hover.fg_color = Color(.9f, .9f, .95f);
    t2.normal.fg_color = Color(.8f, .8f, .85f);
    Control::ThemeSet t3 = t2;
    t3.active.bg_color = Color(1.f, 1.f, 1.f, 0.3f);
    t3.active.fontsize = 14;
    t3.disabled.bg_color = Color(1.f, 1.f, 1.f, 0.f);
    t3.disabled.fontsize = 14;
    t3.focus.bg_color = Color(1.f, 1.f, 1.f, 0.2f);
    t3.focus.fontsize = 14;
    t3.hover.bg_color = Color(1.f, 1.f, 1.f, 0.1f);
    t3.hover.fontsize = 14;
    t3.normal.bg_color = Color(1.f, 1.f, 1.f, 0.f);
    t3.normal.fontsize = 14;

    bool quit = false;

    Container c_always(false, 1, Rect(0, 0, 640, 400), t, nullptr);
    Container c_mainmenu(false, 1, Rect(0, 0, 640, 400), t, nullptr);
    Container c_newversion(false, 1, Rect(0, 0, 640, 400), t, nullptr);
    Container c_download(false, 1, Rect(0, 0, 640, 400), t, nullptr);
    Container* active = &c_mainmenu;

    auto& l = c_mainmenu.add<Listbox<std::string>>(25.f, t2, 10, Rect(120, 80, 520, 260), t);
    auto installs_list = get_installs((std::string(path) + "/installs.txt").c_str());
    for (const auto& c : installs_list)
    {
      for (const auto& i : std::get<1>(c))
      {
        l.add_item(std::get<0>(i), std::get<1>(i));
      }
    }
    auto& l_dnl = c_download.add<Listbox<std::string>>(25.f, t2, 10, Rect(120, 80, 520, 260), t);

    c_download.add<ButtonLabel>("Download", [&w, &l_dnl, &path, &installs_list, &active, &c_mainmenu, &l](int btn){
      if (!l_dnl.get_selected_item() || l_dnl.get_selected_label().empty())
      {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Please select a SuperTux version.", w.get_sdl_window());
        return;
      }

      std::string file_url = *l_dnl.get_selected_item();
      std::string install_path = std::string(path) + "/installs/" + l_dnl.get_selected_label() + "/" + file_url.substr(file_url.find_last_of('/'));
      create_dir((std::string(path) + "/installs/" + l_dnl.get_selected_label()).c_str());
      fetch_file(file_url, install_path.c_str());

      std::get<1>(installs_list.back()).push_back(std::make_tuple(l_dnl.get_selected_label(), install_path));
      save(std::string(path) + "/installs.txt", installs_list);
#ifdef UNIX
      chmod(install_path.c_str(), (mode_t) 0700);
#endif
      l.add_item(l_dnl.get_selected_label(), install_path);
      active = &c_mainmenu;
    }, 31, true, 1, Rect(160, 270, 480, 310), t);

    c_download.add<ButtonLabel>("Cancel", [&active, &c_mainmenu](int btn){
      active = &c_mainmenu;
    }, 31, true, 1, Rect(160, 325, 480, 355), t);

    c_mainmenu.add<ButtonLabel>("Play SuperTux", [&w, &quit, &path, &l](int btn){
      if (!l.get_selected_item() || l.get_selected_label().empty())
      {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Please select a SuperTux version.", w.get_sdl_window());
        return;
      }

      int iv = system(("\"" + *l.get_selected_item() + "\" --version > \"" + std::string(path) + "/console.log\" 2>&1").c_str());
      if (iv)
      {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not detect SuperTux version. Is this a SuperTux executable?", w.get_sdl_window());
        return;
      }

      w.set_visible(false);

      std::string userdir(std::string(path) + "/userdirs/" + l.get_selected_label());
      create_dir(userdir.c_str());

      int i = system(("\"" + *l.get_selected_item() + "\" --userdir \"" + userdir + "\" >> \"" + std::string(path) + "/console.log\" 2>&1").c_str());
      if (i)
      {
        const SDL_MessageBoxButtonData msg_btns[] = {
          { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Don't send" },
          { 0                                      , 1, "Open log file" },
          { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 2, "Send report" },
        };

        const SDL_MessageBoxData msg = {
          SDL_MESSAGEBOX_INFORMATION,
          NULL,
          "Oops!",
          "It looks like SuperTux crashed.\n\nDo you want to send the logs to "
                                    "the developers to help them fix this bug?",
          SDL_arraysize(msg_btns),
          msg_btns,
          NULL
        };

        int resp;

        if (SDL_ShowMessageBox(&msg, &resp))
        {
          log_error << "Could not show error report dialog" << std::endl;
          return;
        }

        switch(resp)
        {
          case 1:
            system(("xdg-open \"" + std::string(path) + "/console.log\"").c_str());
            break;

          case 2:
          {
            auto m = upload_crash((std::string(path) + "/console.log").c_str());
            if (m.empty())
            {
              SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                                       "Report uploaded",
                                       "The crash logs have successfully been "
                                       "uploaded. Please notify the developers "
                                       "about the crash.",
                                       NULL);
            }
            else
            {
              SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                       "Error",
                                       ("The crash logs could not be uploaded. "
                                        "The error is: " + m).c_str(),
                                       NULL);
            }
            break;
          }

          default:
            break;
        }

        if (!quit)
        {
          w.set_visible(true);
        }
      }
      else
      {
        quit = true;
      }
    }, 31, true, 1, Rect(160, 270, 480, 310), t);

    c_always.add<ButtonLabel>("_", [&w](int /* btn */){
      w.set_status(Window::Status::MINIMIZED);
    }, 1, true, 101, Rect(600, 0, 620, 20), t3);
    c_always.add<ButtonLabel>("X", [&quit](int /* btn */){
      quit = true;
    }, 1, true, 101, Rect(620, 0, 640, 20), t3);

    c_mainmenu.add<ButtonLabel>("Check for new versions", [&path, &w, &active, &c_download, &l_dnl](int btn){
      auto r = fetch_file(VERSIONS_URL, (std::string(path) + "/versions.txt").c_str());
      if (!r.empty())
      {
        log_error << "Could not fetch versions from '" VERSIONS_URL "': " << r << std::endl;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", ("Could not fetch list of versions: " + r).c_str(), w.get_sdl_window());
      }
      else
      {
        l_dnl.clear_items();
        auto versions = get_installs((std::string(path) + "/versions.txt").c_str());
        for (const auto& c : versions)
        {
          for (const auto& i : std::get<1>(c))
          {
            l_dnl.add_item(std::get<0>(i), std::get<1>(i));
          }
        }
        active = &c_download;
      }
    }, 31, true, 1, Rect(160, 320, 480, 360), t);

    c_mainmenu.add<ButtonImage>("../data/images/plus-solid.png",
      ButtonImage::Scaling::CONTAIN, [&active, &c_newversion](int /* btn */){
        active = &c_newversion;
      }, 1, true, 1, Rect(120, 50, 140, 70), t);

    c_mainmenu.add<ButtonImage>("../data/images/minus-solid.png",
      ButtonImage::Scaling::CONTAIN, [&l, &w, &installs_list, &path](int /* btn */){
        if (!l.get_selected_item() || l.get_selected_label().empty())
        {
          SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Please select a SuperTux version.", w.get_sdl_window());
          return;
        }

        const SDL_MessageBoxButtonData msg_btns[] = {
          { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No" },
          { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes" },
        };

        const SDL_MessageBoxData msg = {
          SDL_MESSAGEBOX_INFORMATION,
          w.get_sdl_window(),
          "Detaching version",
          "This will remove the version from the list. It will NOT uninstall\n"
          "the game from this PC.\n\n"
          "This is intended for versions that aren't managed by the launcher;\n"
          "if you also want to uninstall that version, please use the trash can\n"
          "button instead.\n\n"
          "Do you want to remove this version from the list without uninstalling it?",
          SDL_arraysize(msg_btns),
          msg_btns,
          NULL
        };

        int resp;

        if (SDL_ShowMessageBox(&msg, &resp))
        {
          log_error << "Could not show error report dialog" << std::endl;
          return;
        }

        switch(resp)
        {
          case 1:
            for (auto& category : installs_list)
            {
              auto& list = std::get<1>(category);
              bool erased = false;
              for (auto it = list.begin(); it != list.end(); it++)
              {
                if (std::get<0>(*it) == l.get_selected_label() && std::get<1>(*it) == *l.get_selected_item())
                {
                  list.erase(it);
                  erased = true;
                  break;
                }
              }
              if (erased)
                break;
            }
            
            l.remove_item(l.get_selected_index());
            save(std::string(path) + "/installs.txt", installs_list);
            break;

          default:
            break;
        }

      }, 1, true, 1, Rect(150, 50, 170, 70), t);

    c_mainmenu.add<ButtonImage>("../data/images/trash-alt-solid.png",
      ButtonImage::Scaling::CONTAIN, [&l, &w, &installs_list, &path](int /* btn */){
        if (!l.get_selected_item() || l.get_selected_label().empty())
        {
          SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Please select a SuperTux version.", w.get_sdl_window());
          return;
        }

        const SDL_MessageBoxButtonData msg_btns[] = {
          { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No" },
          { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes" },
        };

        const SDL_MessageBoxData msg = {
          SDL_MESSAGEBOX_INFORMATION,
          w.get_sdl_window(),
          "Deleting version",
          "This will delete all files associated with the given version.\n\n"
          "ALL PROGRESS, LEVELS AND SAVE DATA ASSOCIATED WITH THIS VERSION WILL\n"
          "BE DELETED AS WELL. If the game has been installed manually, it will\n"
          "not be uninstalled.\n\n"
          "Do you want to completely remove this version from your system?",
          SDL_arraysize(msg_btns),
          msg_btns,
          NULL
        };

        int resp;

        if (SDL_ShowMessageBox(&msg, &resp))
        {
          log_error << "Could not show error report dialog" << std::endl;
          return;
        }

        switch(resp)
        {
          case 1:
            system(("rm -r \"" + std::string(path) + "/installs/" + l.get_selected_label() + "\" \"" + std::string(path) + "/userdirs/" + l.get_selected_label() + "\"").c_str());

            for (auto& category : installs_list)
            {
              auto& list = std::get<1>(category);
              bool erased = false;
              for (auto it = list.begin(); it != list.end(); it++)
              {
                if (std::get<0>(*it) == l.get_selected_label() && std::get<1>(*it) == *l.get_selected_item())
                {
                  list.erase(it);
                  erased = true;
                  break;
                }
              }
              if (erased)
                break;
            }

            l.remove_item(l.get_selected_index());
            save(std::string(path) + "/installs.txt", installs_list);
            break;

          default:
            break;
        }
      }, 1, true, 1, Rect(180, 50, 200, 70), t);

    auto& new_label = c_newversion.add<Textbox>(1, Rect(160, 120, 480, 150), t);
    std::string new_path = "";
    c_newversion.add<ButtonLabel>("Select SuperTux version", [&new_path](int){
      auto files = pfd::open_file("Select SuperTux version",
#ifdef _WIN32
                                  "C:\\",
#else
                                  "/",
#endif
                                  { "All Files", "*" },
                                  pfd::opt::none).result();
      if (files.size() >= 1)
      {
        new_path = files[0];
      }
    }, 1, true, 1, Rect(160, 190, 480, 220), t);

    c_newversion.add<ButtonLabel>("Cancel", [&active, &c_mainmenu](int /* btn */){
      active = &c_mainmenu;
    }, 1, true, 1, Rect(160, 270, 350, 300), t);

    c_newversion.add<ButtonLabel>("Add", [&active, &c_mainmenu, &l, &new_label, &new_path, &installs_list, &path](int /* btn */){
      if (new_label.get_contents().empty() || new_path.empty())
        return;

      l.add_item(new_label.get_contents(), new_path);
      std::get<1>(installs_list.back()).push_back(std::make_tuple(new_label.get_contents(), new_path));
      save(std::string(path) + "/installs.txt", installs_list);
      active = &c_mainmenu;
    }, 1, true, 1, Rect(370, 270, 480, 300), t);

    while (!quit)
    {
      SDL_Event e;
      while (SDL_PollEvent(&e))
      {
        switch (e.type)
        {
          case SDL_QUIT:
            quit = true;
            break;

          default:
            break;
        }

        c_always.event(e);
        active->event(e);

        if (quit)
          break;
      }

      if (quit)
        break;

      if (w.get_visible())
      {
        DrawingContext dc(w.get_renderer());
        auto& t = w.load_texture("../data/images/background.png");
        dc.draw_texture(t, t.get_size(), w.get_size(), 0.f, Color(1.f, 1.f, 1.f), Renderer::Blend::BLEND, -1);
        dc.draw_filled_rect(Rect(0, 0, 640, 20), Color(.15f, .15f, .15f, .9f), Renderer::Blend::BLEND, 100);
        dc.draw_filled_rect(Rect(0, 0, 640, 21), Color(.15f, .15f, .15f, .2f), Renderer::Blend::BLEND, 100);
        dc.draw_filled_rect(Rect(0, 0, 640, 22), Color(.15f, .15f, .15f, .2f), Renderer::Blend::BLEND, 100);
        dc.draw_filled_rect(Rect(0, 0, 640, 23), Color(.15f, .15f, .15f, .1f), Renderer::Blend::BLEND, 100);
        dc.draw_filled_rect(Rect(0, 0, 640, 24), Color(.15f, .15f, .15f, .1f), Renderer::Blend::BLEND, 100);
        dc.draw_text("SuperTux Launcher version 0.0.1", Vector(5, 10), Renderer::TextAlign::MID_LEFT,
                    "../data/fonts/SuperTux-Medium.ttf", 12, Color(1.f, 1.f, 1.f), Renderer::Blend::BLEND, 101);
        c_always.draw(dc);
        active->draw(dc);
        dc.render();
        dc.clear();
      }

      c_always.update(0.015f);
      active->update(0.015f);

      SDL_Delay(15);
    }
  }
  catch (std::exception& e)
  {
    log_fatal << "Unhandled exception: " << e.what() << std::endl;
    Font::flush_fonts();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 1;
  }
  catch (...)
  {
    log_fatal << "Unhandled error" << std::endl;
    Font::flush_fonts();
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 1;
  }

  Font::flush_fonts();
  TTF_Quit();
  IMG_Quit();
  SDL_Quit();
  return 0;
}
