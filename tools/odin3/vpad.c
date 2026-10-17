// SPDX-License-Identifier: GPL-2.0-or-later

// vpad [name] [vendor] [product] [phys]: creates a virtual Bluetooth gamepad through /dev/uinput (writable by the
// adb shell on the Odin 3), so that multiplayer can be tested without pairing a real controller. Defaults to a
// DualShock 4 ("Wireless Controller", 054c:09cc), which Android maps with its stock key layout. Give each pad its
// own phys: Android merges input devices with identical ids and location into a single InputDevice.
// Reads commands on stdin until EOF or "quit":
//   key <a|b|x|y|l1|r1|l2|r2|start|select|l3|r3|mode> <0|1>
//   abs <lx|ly|rx|ry|lt|rt|hx|hy> <value>        (sticks 0..255 centred on 128, triggers 0..255, hat -1..1)
//   tap <key> [ms]
//   sleep <ms>
#include <fcntl.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static int fd;

static void emit(int type, int code, int value)
{
  struct input_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = type;
  ev.code = code;
  ev.value = value;
  write(fd, &ev, sizeof(ev));
}

static void sync_events() { emit(EV_SYN, SYN_REPORT, 0); }

// Codes of a DualShock 4 driven by hid-playstation, which the console maps with
// Vendor_054c_Product_09cc_Version_8100.kl (right stick on RX/RY, triggers on Z/RZ).
static const struct
{
  const char* name;
  int code;
} keys[] = {{"a", BTN_SOUTH},  {"b", BTN_EAST},       {"x", BTN_WEST},       {"y", BTN_NORTH},
            {"l1", BTN_TL},    {"r1", BTN_TR},        {"l2", BTN_TL2},       {"r2", BTN_TR2},
            {"select", BTN_SELECT}, {"start", BTN_START}, {"l3", BTN_THUMBL}, {"r3", BTN_THUMBR},
            {"mode", BTN_MODE}};

static const struct
{
  const char* name;
  int code, min, max;
} axes[] = {{"lx", ABS_X, 0, 255},  {"ly", ABS_Y, 0, 255},  {"rx", ABS_RX, 0, 255},
            {"ry", ABS_RY, 0, 255}, {"lt", ABS_Z, 0, 255},  {"rt", ABS_RZ, 0, 255},
            {"hx", ABS_HAT0X, -1, 1}, {"hy", ABS_HAT0Y, -1, 1}};

static int key_code(const char* name)
{
  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    if (!strcmp(keys[i].name, name))
      return keys[i].code;
  return -1;
}

int main(int argc, char** argv)
{
  fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (fd < 0)
  {
    perror("open /dev/uinput");
    return 1;
  }

  ioctl(fd, UI_SET_EVBIT, EV_KEY);
  for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    ioctl(fd, UI_SET_KEYBIT, keys[i].code);
  ioctl(fd, UI_SET_EVBIT, EV_ABS);

  struct uinput_user_dev dev;
  memset(&dev, 0, sizeof(dev));
  snprintf(dev.name, UINPUT_MAX_NAME_SIZE, "%s", argc > 1 ? argv[1] : "Wireless Controller");
  dev.id.bustype = BUS_BLUETOOTH;
  dev.id.vendor = argc > 2 ? strtol(argv[2], NULL, 16) : 0x054c;
  dev.id.product = argc > 3 ? strtol(argv[3], NULL, 16) : 0x09cc;
  dev.id.version = 0x8100;
  for (size_t i = 0; i < sizeof(axes) / sizeof(axes[0]); i++)
  {
    ioctl(fd, UI_SET_ABSBIT, axes[i].code);
    dev.absmin[axes[i].code] = axes[i].min;
    dev.absmax[axes[i].code] = axes[i].max;
  }
  write(fd, &dev, sizeof(dev));
  if (argc > 4)
    ioctl(fd, UI_SET_PHYS, argv[4]);
  if (ioctl(fd, UI_DEV_CREATE) < 0)
  {
    perror("UI_DEV_CREATE");
    return 1;
  }
  // Centre the sticks so that Android does not see them fully deflected.
  for (size_t i = 0; i < 4; i++)
    emit(EV_ABS, axes[i].code, 128);
  sync_events();
  printf("ready\n");
  fflush(stdout);

  char line[128], cmd[16], arg[16];
  int value;
  while (fgets(line, sizeof(line), stdin))
  {
    value = 0;
    arg[0] = 0;
    if (sscanf(line, "%15s %15s %d", cmd, arg, &value) < 1)
      continue;
    if (!strcmp(cmd, "quit"))
      break;
    if (!strcmp(cmd, "sleep"))
    {
      usleep(atoi(arg) * 1000);
    }
    else if (!strcmp(cmd, "key") || !strcmp(cmd, "tap"))
    {
      int code = key_code(arg);
      if (code < 0)
        continue;
      if (!strcmp(cmd, "key"))
      {
        emit(EV_KEY, code, value);
        sync_events();
      }
      else
      {
        emit(EV_KEY, code, 1);
        sync_events();
        usleep((value ? value : 120) * 1000);
        emit(EV_KEY, code, 0);
        sync_events();
      }
    }
    else if (!strcmp(cmd, "abs"))
    {
      for (size_t i = 0; i < sizeof(axes) / sizeof(axes[0]); i++)
        if (!strcmp(axes[i].name, arg))
          emit(EV_ABS, axes[i].code, value);
      sync_events();
    }
  }

  ioctl(fd, UI_DEV_DESTROY);
  close(fd);
  return 0;
}
