#include "util/rfkill.hpp"
#include <unistd.h>
#include <stdlib.h>
#include <cstring>
#include <fcntl.h>
#include <cerrno>

bool waybar::util::isDisabled(enum rfkill_type rfkill_type) {
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

    if(event.type == rfkill_type) {
      ret = event.soft || event.hard;
      break;
    }
  }

  close(fd);
  return ret;
}
