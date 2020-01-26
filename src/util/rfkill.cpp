#include "util/rfkill.hpp"
#include <linux/rfkill.h>
#include <unistd.h>
#include <stdlib.h>
#include <cstring>
#include <fcntl.h>
#include <sys/poll.h>
#include <cerrno>
#include <stdexcept>

waybar::util::Rfkill::Rfkill(const enum rfkill_type rfkill_type)
  : rfkill_type_(rfkill_type) {
}

void waybar::util::Rfkill::waitForEvent() {
  struct rfkill_event event;
  struct pollfd p;
  ssize_t len;
  int fd, n;

  fd = open("/dev/rfkill", O_RDONLY);
  if (fd < 0) {
    throw std::runtime_error("Can't open RFKILL control device");
    return;
  }

  memset(&p, 0, sizeof(p));
  p.fd = fd;
  p.events = POLLIN | POLLHUP;

  while (1) {
    n = poll(&p, 1, -1);
    if (n < 0) {
      throw std::runtime_error("Failed to poll RFKILL control device");
      break;
    }

    if (n == 0)
      continue;

    len = read(fd, &event, sizeof(event));
    if (len < 0) {
      throw std::runtime_error("Reading of RFKILL events failed");
      break;
    }

    if (len != RFKILL_EVENT_SIZE_V1) {
      throw std::runtime_error("Wrong size of RFKILL event");
      continue;
    }

    if(event.type == rfkill_type_ && event.op == RFKILL_OP_CHANGE) {
      state_ = event.soft || event.hard;
      break;
    }
  }

  close(fd);
  return;
}


int waybar::util::Rfkill::getState() const {
  return state_;
}
