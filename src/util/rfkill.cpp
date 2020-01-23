#include "util/rfkill.hpp"
#include <unistd.h>
#include <stdlib.h>
#include <cstring>
#include <fcntl.h>
#include <sys/poll.h>
#include <cerrno>
#include <stdio.h>

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
    //perror("Can't open RFKILL control device");
    return;
  }

  memset(&p, 0, sizeof(p));
  p.fd = fd;
  p.events = POLLIN | POLLHUP;

  while (1) {
    n = poll(&p, 1, -1);
    if (n < 0) {
      //perror("Failed to poll RFKILL control device");
      break;
    }

    if (n == 0)
      continue;

    len = read(fd, &event, sizeof(event));
    if (len < 0) {
      //perror("Reading of RFKILL events failed");
      break;
    }

    if (len != RFKILL_EVENT_SIZE_V1) {
      //fprintf(stderr, "Wrong size of RFKILL event\n");
      continue;
    }

    if(event.type == rfkill_type_) {
      state_ = event.soft || event.hard;
      if (prev_state_ != state_) {
        prev_state_ = state_;
        break;
      }
      //ret = event.soft || event.hard;
    }
  }

  close(fd);
  return;
}


int waybar::util::Rfkill::getState() {
  return state_;
}

bool waybar::util::Rfkill::isDisabled() const {
  struct rfkill_event event;
  ssize_t len;
  int fd;
  int ret;
  ret = false;

  fd = open("/dev/rfkill", O_RDONLY);
  if (fd < 0) {
    //perror("Can't open RFKILL control device");
    return false;
  }

  if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
    //perror("Can't set RFKILL control device to non-blocking");
    close(fd);
    return false;
  }

  while(true) {
    len = read(fd, &event, sizeof(event));
    if (len < 0) {
      if (errno == EAGAIN)
        return 1;
      //perror("Reading of RFKILL events failed");
      return false;
    }

    if (len != RFKILL_EVENT_SIZE_V1) {
      //fprintf(stderr, "Wrong size of RFKILL event\n");
      return false;
    }

    if(event.type == rfkill_type_) {
      ret = event.soft || event.hard;
      break;
    }
  }

  close(fd);
  return ret;
}
