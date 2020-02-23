/* https://git.kernel.org/pub/scm/linux/kernel/git/jberg/rfkill.git/
 *
 * Copyright 2009 Johannes Berg <johannes@sipsolutions.net>
 * Copyright 2009 Marcel Holtmann <marcel@holtmann.org>
 * Copyright 2009 Tim Gardner <tim.gardner@canonical.com>
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

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


bool waybar::util::Rfkill::getState() const {
  return state_;
}
