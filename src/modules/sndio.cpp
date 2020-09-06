#include "modules/sndio.hpp"
#include <algorithm>
#include <cstdlib>
#include <poll.h>
#include <fmt/format.h>

namespace waybar::modules {

void ondesc(void *arg, struct sioctl_desc *d, int curval) {
  auto self = static_cast<Sndio*>(arg);
  if (d == NULL) {
    // d is NULL when the list is done
    return;
  }
  self->set_desc(d, curval);
}

void onval(void *arg, unsigned int addr, unsigned int val) {
  auto self = static_cast<Sndio*>(arg);
  self->put_val(addr, val);
}

Sndio::Sndio(const std::string &id, const Json::Value &config)
    : ALabel(config, "sndio", id, "{volume}%"),
      hdl_(nullptr),
      pfds_(0),
      addr_(0),
      volume_(0),
      old_volume_(0),
      maxval_(0),
      muted_(false) {
  hdl_ = sioctl_open(SIO_DEVANY, SIOCTL_READ | SIOCTL_WRITE, 0);
  if (hdl_ == nullptr) {
    throw std::runtime_error("sioctl_open() failed.");
  }

  if(sioctl_ondesc(hdl_, ondesc, this) == 0) {
    throw std::runtime_error("sioctl_ondesc() failed.");
  }

  sioctl_onval(hdl_, onval, this);

  pfds_.reserve(sioctl_nfds(hdl_));

  event_box_.show();

  event_box_.add_events(Gdk::SCROLL_MASK | Gdk::SMOOTH_SCROLL_MASK | Gdk::BUTTON_PRESS_MASK);
  event_box_.signal_scroll_event().connect(
    sigc::mem_fun(*this, &Sndio::handleScroll));
  event_box_.signal_button_press_event().connect(
    sigc::mem_fun(*this, &Sndio::handleToggle));

  thread_ = [this] {
    dp.emit();

    int nfds = sioctl_pollfd(hdl_, pfds_.data(), POLLIN);
    if (nfds == 0) {
      throw std::runtime_error("sioctl_pollfd() failed.");
    }
    while (poll(pfds_.data(), nfds, -1) < 0) {
      if (errno != EINTR) {
        throw std::runtime_error("poll() failed.");
      }
    }

    int revents = sioctl_revents(hdl_, pfds_.data());
    if (revents & POLLHUP) {
      throw std::runtime_error("disconnected!");
    }
  };
}

Sndio::~Sndio() {
  sioctl_close(hdl_);
}

auto Sndio::update() -> void {
  auto format = format_;
  unsigned int vol = 100. * static_cast<double>(volume_) / static_cast<double>(maxval_);

  if (volume_ == 0) {
    label_.get_style_context()->add_class("muted");
  } else {
    label_.get_style_context()->remove_class("muted");
  }

  label_.set_markup(fmt::format(format,
                                fmt::arg("volume", vol),
                                fmt::arg("raw_value", volume_)));

  ALabel::update();
}

auto Sndio::set_desc(struct sioctl_desc *d, unsigned int val) -> void {
  std::string name{d->func};
  std::string node_name{d->node0.name};

  if (name == "level" && node_name == "output" && d->type == SIOCTL_NUM) {
    // store addr for output.level value, used in put_val
    addr_ = d->addr;
    maxval_ = d->maxval;
    volume_ = val;
  }
}

auto Sndio::put_val(unsigned int addr, unsigned int val) -> void {
  if (addr == addr_) {
    volume_ = val;
  }
}

bool Sndio::handleScroll(GdkEventScroll *e) {
  // change the volume only when no user provided
  // events are configured
  if (config_["on-scroll-up"].isString() || config_["on-scroll-down"].isString()) {
    return AModule::handleScroll(e);
  }

  auto dir = AModule::getScrollDir(e);
  if (dir == SCROLL_DIR::NONE) {
    return true;
  }

  int step = 5;
  if (config_["scroll-step"].isInt()) {
    step = config_["scroll-step"].asInt();
  }

  int new_volume = volume_;
  if (muted_) {
    new_volume = old_volume_;
  }

  if (dir == SCROLL_DIR::UP) {
    new_volume += step;
  } else if (dir == SCROLL_DIR::DOWN) {
    new_volume -= step;
  }
  new_volume = std::clamp(new_volume, 0, static_cast<int>(maxval_));

  // quits muted mode if volume changes
  muted_ = false;

  sioctl_setval(hdl_, addr_, new_volume);

  return true;
}

bool Sndio::handleToggle(GdkEventButton* const& e) {
  // toggle mute only when no user provided events are configured
  if (config_["on-click"].isString()) {
    return AModule::handleToggle(e);
  }

  muted_ = !muted_;
  if (muted_) {
    // store old volume to be able to restore it later
    old_volume_ = volume_;
    sioctl_setval(hdl_, addr_, 0);
  } else {
    sioctl_setval(hdl_, addr_, old_volume_);
  }

  return true;
}

} /* namespace waybar::modules */
