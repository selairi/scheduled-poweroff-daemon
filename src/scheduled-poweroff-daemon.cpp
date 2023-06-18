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

static const char *interface = "local.sheduledpoweroffdaemon";
static const char *object_path = "/SheduledPowerOffDaemon";
/*
static long get_time_milliseconds()
{
  struct timespec time_aux;
  clock_gettime(CLOCK_REALTIME, &time_aux);
  return time_aux.tv_sec * 1000 + time_aux.tv_nsec / 1000000;
}
*/

enum EventType {
  NONE, SIGNAL, POWEROFF
};

struct Hour {
  int hour;
  int minutes;
};

static long get_time_for_hour_milliseconds(const Hour &hour) {
  struct timespec ts;    
  clock_gettime(CLOCK_REALTIME, &ts);
  struct tm *my_tm = localtime(&ts.tv_sec);
  time_t now = mktime(my_tm);
  my_tm->tm_sec = 0;
  my_tm->tm_min = hour.minutes;
  my_tm->tm_hour = hour.hour;
  time_t next = mktime(my_tm);
  double elapsed = difftime(next, now) * 1000;
  return (long) elapsed;
}

struct DBusData {
  sd_bus_track *track_item, *track_host;
  sd_bus *bus;
  std::vector<Hour> shutdown_hours;
  bool stop_power_off;

  DBusData() {
    bus = nullptr;
    track_item = track_host = nullptr;
    stop_power_off = false;
  }
};


/* Read shutdown hours from file
 */
static bool read_from_file(const char *filename, DBusData *dbus_data) {
  if(filename == nullptr) return false;
  FILE *in = fopen(filename, "r");
  if(in == nullptr) return false;
  char buffer[512];
  while(!feof(in)) {
    char *p = fgets(buffer, sizeof(buffer), in);
    if(p != nullptr) {
      if(p[0] != '#') { // It is not a comment
        int hour, minutes;
        sscanf(p, "%d:%d", &hour, &minutes);
        printf("%d:%d\n", hour, minutes);
        dbus_data->shutdown_hours.push_back({hour, minutes});
      }
    }
  }
  fclose(in);
  return true; 
}

static int method_stop_power_off(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
  DBusData *dbus_data = (DBusData *) userdata;
 
  dbus_data->stop_power_off = true;
  printf("Stop power off\n");

  return sd_bus_reply_method_return(m, "");
}

static int method_power_off(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
  DBusData *dbus_data = (DBusData *) userdata;
 
  printf("Power off\n");
  if(! dbus_data->stop_power_off)
    system("/sbin/shutdown -h 2 'Power off'");
  dbus_data->stop_power_off = false;

  return sd_bus_reply_method_return(m, "");
}

static int method_send_msg_power_off(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
  DBusData *dbus_data = (DBusData *) userdata;
  char *hour = nullptr;
  int r = sd_bus_message_read(m, "s", &hour);
  printf("Send power off\n");
  if(hour != nullptr) {
    printf("Hour %s\n", hour);
    sd_bus_emit_signal(dbus_data->bus, object_path, interface, "PowerOffMsg", "s", hour);
  }
  return sd_bus_reply_method_return(m, "");
}

static int get_property(sd_bus *bus, const char *path, const char *interface, const char *property, sd_bus_message *reply, void *userdata, sd_bus_error *ret_error) {
  DBusData *dbus_data = (DBusData *) userdata;
  int r;
  printf("Property: %s\n", property);
  if(! strcmp("ProtocolVersion", property))
    r = sd_bus_message_append(reply, "u", 0);
  return r;
}

/* The vtable of our little object, implements the net.poettering.Calculator interface */
static const sd_bus_vtable watcher_vtable[] = {
  SD_BUS_VTABLE_START(0),
  SD_BUS_METHOD("PowerOff", "", "", method_power_off, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("StopPowerOff", "", "", method_stop_power_off, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_METHOD("SendMsgPowerOff", "s", "", method_send_msg_power_off, SD_BUS_VTABLE_UNPRIVILEGED),
  SD_BUS_PROPERTY("ProtocolVersion", "u", get_property, 0, SD_BUS_VTABLE_PROPERTY_CONST),
  SD_BUS_SIGNAL("PowerOffMsg", "s", 0),
  SD_BUS_VTABLE_END
};

static void finish(sd_bus *bus, sd_bus_slot *slot) {
  sd_bus_slot_unref(slot);
  sd_bus_unref(bus);
}

int main(int argc, char *argv[]) {
  DBusData dbus_data;
  for(int n = 1; n < argc; n++) {
    if(!strcmp(argv[n], "--help")) {
      printf("This is a simple scheduled_poweroff-daemon daemon.\n");
      return EXIT_SUCCESS;
    } else {
      if(! read_from_file(argv[n], &dbus_data)) {
        fprintf(stderr, "Error reading file %s\n", argv[n]);
        return EXIT_FAILURE;
      }
    }
  }

  if(dbus_data.shutdown_hours.empty()) {
    printf("Reding from /etc/scheduled-poweroff.conf\n");
    if(! read_from_file("/etc/scheduled-poweroff.conf", &dbus_data)) {
      fprintf(stderr, "Error reading file /etc/scheduled-poweroff.conf\n");
      dbus_data.shutdown_hours.clear();
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

  r = sd_bus_add_object_vtable(bus,
      &slot,
      object_path,  /* object path */
      interface,   /* interface name */
      watcher_vtable,
      &dbus_data);
  if (r < 0) {
    fprintf(stderr, "Failed to issue method call: %s\n", strerror(-r));
    finish(bus, slot);
    return EXIT_FAILURE;
  }

  // Take a well-known service name so that clients can find us
  r = sd_bus_request_name(bus, interface, 0);
  if (r < 0) {
    fprintf(stderr, "Failed to acquire service name: %s\n", strerror(-r));
    finish(bus, slot);
    return EXIT_FAILURE;
  }

  EventType event_type = NONE;
  Hour actual_hour;
  int timeout_msecs = -1;
  int ret;
  long now_in_msecs;
  bool running = true;
  struct pollfd fds[1];
  fds[0].fd = sd_bus_get_fd(bus);
  int events = sd_bus_get_events(bus);
  if(events >= 0)
    fds[0].events = POLLIN; //sd_bus_get_events(m_bus);
  else {
    fprintf(stderr, "DBus events cannot be read.\n");
    return EXIT_FAILURE;
  }
  fds[0].revents = 0;

  while(running) {
    // Update timeout and run timeout events
    //now_in_msecs = get_time_milliseconds();
    timeout_msecs = -1;
    event_type = NONE;
    
    for(Hour hour : dbus_data.shutdown_hours) {
      int elapsed = get_time_for_hour_milliseconds(hour);
      if(elapsed < 0) // If hour is in the past, sum one day:
        elapsed += 24 * 3600 * 1000;
      if(timeout_msecs < 0 && elapsed > 0 
          ||
        timeout_msecs > 0 && elapsed > 0 && timeout_msecs > elapsed) { 
        timeout_msecs = elapsed;
        event_type = POWEROFF;
        actual_hour = hour;
      }
      // Send signal 10 minutes before the shutdown
      elapsed -= 600000;
      if(timeout_msecs < 0 && elapsed > 0 
          ||
        timeout_msecs > 0 && elapsed > 0 && timeout_msecs > elapsed) { 
        timeout_msecs = elapsed;
        event_type = SIGNAL;
        actual_hour = hour;
      }
      printf("%d\n", elapsed);
    }

    // Wait for events
    ret = poll(fds, 1, timeout_msecs);
    if(ret > 0) {
      printf("DBus event\n");
      if(fds[0].revents) {
        int r = 1;
        while(r > 0) {
          r = sd_bus_process(bus, nullptr);
          if (r < 0) {
            finish(bus, slot);
            fprintf(stderr, "DBus error. Failed to process bus: %s\n", strerror(-r));
            return EXIT_FAILURE;
          }
        }
        fds[0].events = sd_bus_get_events(bus);
        fds[0].revents = 0;
      }
    } else {
      // Timeout
      printf("Timeout event\n");
      switch(event_type) {
        case POWEROFF:
          printf("Power off\n");
          if(! dbus_data.stop_power_off) {
            //r = system("systemctl poweroff");
            r = system("/sbin/shutdown -h 2 'Power off'");
          }
          dbus_data.stop_power_off = false;
          break;
        case SIGNAL:
          printf("Signal\n");
          char buffer[16];
          sprintf(buffer, "%d:%d", actual_hour.hour, actual_hour.minutes);
          sd_bus_emit_signal(bus, object_path, interface, "PowerOffMsg", "s", buffer);
          break;
        default:
          break;
      }
    }
  }

  finish(bus, slot);

  return EXIT_SUCCESS;
}
