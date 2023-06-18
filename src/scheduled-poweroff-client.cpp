/****************************************************************************
**
**   Copyright (C) 2023 P.L. Lucas <selairi@gmail.com>
**
**
** LICENSE: BSD
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of developers or companies in the above copyright and its 
**     Subsidiary(-ies) nor the names of its contributors may be used to 
**     endorse or promote products derived from this software without 
**     specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
**
****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <vector>
#include <systemd/sd-bus.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <string>
#include <fstream>
#include "configure.h"
#include <regex>

/**Removes leading and trailing blanks.
 */
std::string trim(const std::string &str){
    return regex_replace(str, std::regex("(^[ ]+)|([ ]+$)"),"");
}

static const char *interface = "local.sheduledpoweroffdaemon";
static const char *object_path = "/SheduledPowerOffDaemon";

struct DBusData {
  sd_bus_track *track_item, *track_host;
  sd_bus *bus;

  DBusData() {
    bus = nullptr;
    track_item = track_host = nullptr;
  }
};

/**Release mem from bus.
 */
static void finish(sd_bus *bus, sd_bus_slot *slot) {
  sd_bus_slot_unref(slot);
  sd_bus_unref(bus);
}

/**Show a dialog with message: "The equipment shall be switched off at %. Close this window or press accept to keep it on.".
 * The % simbol will be replaced by "hour". If the translation is available, the translation will be shown.
 */
static void show_dialog(const std::string &hour)
{  
  // Get translation from translations file
  const char *translations_file = SHARE_PATH "/translations.txt" ;
  std::string text("The equipment shall be switched off at %. Close this window or press accept to keep it on.");
  char *lang = getenv("LANG");
  if(lang != nullptr) {
    std::string lang_str(lang);
    std::ifstream in;
    in.open(translations_file);
    if(in.is_open()) {
      std::string line;
      while(getline(in, line)) {
        // Ignore comments
        if(line.length() > 0 && line[0] == '#') continue;
        // Translation sintax is lang_id=translation:
        // en=The equipment shall be switched off at %. Close this window or press accept to keep it on.
        int end = line.find("=");
        if(end != -1) {
          std::string lang_id = trim(line.substr(0, end));
          std::string translation = trim(line.substr(end + 1, -1));
          if(lang_str == lang_id || lang_str.substr(0,2) == lang_id) {
            text = translation;
          }
        }
      }
      in.close();
    }
  }

  // Show message in the dialog
  std::string message = text;
  int pos = message.find("%");
  if(pos >= 0) {
    message.replace(pos, 1, hour);
  }
  std::string command = "kdialog --msgbox '" + message + "'";
  system(command.c_str());
}

/**Catchs signal from DBus, shows dialog and stop power off if dialog is closed.
 */
static int dbus_signal_handler(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
  printf("[dbus_signal_handler]\n");
  DBusData *dbus_data = (DBusData *)userdata;
  char *arg = nullptr;
  int r = sd_bus_message_read(m, "s", &arg);
  if(r < 0) {
    fprintf(stderr, "DBus error. Failed read message %s\n", strerror(-r));
  }

  show_dialog(arg);

  sd_bus_error error = SD_BUS_ERROR_NULL;
  r = sd_bus_call_method(dbus_data->bus,
      interface,          
      object_path,       
      interface,   
      "StopPowerOff",                          
      &error,                               
      &m,                                  
      "");  
  if (r < 0) {
    fprintf(stderr, "Failed to issue method call: %s\n", error.message);
    return 1;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  DBusData dbus_data;
  for(int n = 1; n < argc; n++) {
    if(!strcmp(argv[n], "--help")) {
      printf("This is a simple scheduled-poweroff-client.\n");
      return EXIT_SUCCESS;
    }
  }

  sd_bus_slot *slot = NULL;
  sd_bus *bus = NULL;
  int r;

  r = sd_bus_open_system(&bus);
  //r = sd_bus_open_user(&bus);
  if (r < 0) {
    fprintf(stderr, "Failed to connect to system bus: %s\n", strerror(-r));
    finish(bus, slot);
    return EXIT_FAILURE;
  }

  dbus_data.bus = bus;

  int timeout_msecs = -1;
  int ret;
  bool running = true;

  r = sd_bus_match_signal(bus, &slot, 
      interface,
      object_path, 
      interface,
      "PowerOffMsg",
      dbus_signal_handler,
      &dbus_data
      );
  if(r < 0) {
    fprintf(stderr, "DBus error. Failed connect to signal: %s\n", strerror(-r));
    return EXIT_FAILURE;
  }

  while(running) {
    /* Process requests */
    r = sd_bus_process(bus, NULL);
    if (r < 0) {
      fprintf(stderr, "Failed to process bus: %s\n", strerror(-r));
      finish(bus, slot);
    }
    if (r > 0) /* we processed a request, try to process another one, right-away */
      continue;

    /* Wait for the next request to process */
    r = sd_bus_wait(bus, (uint64_t) -1);
    if (r < 0) {
      fprintf(stderr, "Failed to wait on bus: %s\n", strerror(-r));
      finish(bus, slot);
      return EXIT_FAILURE;
    }
  }

  finish(bus, slot);

  return EXIT_SUCCESS;
}
