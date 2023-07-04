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

#include <fcntl.h>
#include <glibmm/main.h>
#include <linux/rfkill.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <cerrno>

waybar::util::Rfkill::Rfkill(const enum rfkill_type rfkill_type) : rfkill_type_(rfkill_type) {
  fd_ = open("/dev/rfkill", O_RDONLY);
  if (fd_ < 0) {
    spdlog::error("Can't open RFKILL control device");
    return;
  }
  int rc = fcntl(fd_, F_SETFL, O_NONBLOCK);
  if (rc < 0) {
    spdlog::error("Can't set RFKILL control device to non-blocking: {}", errno);
    close(fd_);
    fd_ = -1;
    return;
  }
  Glib::signal_io().connect(sigc::mem_fun(*this, &Rfkill::on_event), fd_,
                            Glib::IO_IN | Glib::IO_ERR | Glib::IO_HUP);
}

waybar::util::Rfkill::~Rfkill() {
  if (fd_ >= 0) {
    close(fd_);
  }
}

bool waybar::util::Rfkill::on_event(Glib::IOCondition cond) {
  if (cond & Glib::IO_IN) {
    struct rfkill_event event;
    ssize_t len;

    len = read(fd_, &event, sizeof(event));
    if (len < 0) {
      if (errno == EAGAIN) {
        return true;
      }
      spdlog::error("Reading of RFKILL events failed: {}", errno);
      return false;
    }

    if (static_cast<size_t>(len) < RFKILL_EVENT_SIZE_V1) {
      spdlog::error("Wrong size of RFKILL event: {} < {}", len, RFKILL_EVENT_SIZE_V1);
      return true;
    }

    if (event.type == rfkill_type_ && (event.op == RFKILL_OP_ADD || event.op == RFKILL_OP_CHANGE)) {
      state_ = event.soft || event.hard;
      on_update.emit(event);
    }
    return true;
  }
  spdlog::error("Failed to poll RFKILL control device");
  return false;
}

bool waybar::util::Rfkill::getState() const { return state_; }
