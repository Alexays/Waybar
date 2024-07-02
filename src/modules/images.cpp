#include "modules/images.hpp"

waybar::modules::Images::Images(const std::string &id, const Json::Value &config)
    : AModule(config, "images", id), box_(Gtk::ORIENTATION_HORIZONTAL, 0) {
  config_ = config;

  box_.set_name("images");
  box_.get_style_context()->add_class(id);
  box_.get_style_context()->add_class(MODULE_CLASS);
  event_box_.add(box_);

  size_ = config["size"].asInt();
  if (size_ == 0) {
    size_ = 16;
  }

  interval_ = config_["interval"].asInt();
  if (interval_ == 0) {
    interval_ = INT_MAX;
  }

  delayWorker();
};

void waybar::modules::Images::delayWorker() {
  thread_ = [this] {
    dp.emit();
    auto interval = std::chrono::seconds(interval_);
    thread_.sleep_for(interval);
  };
}

void waybar::modules::Images::refresh(int sig) {
  if (sig == SIGRTMIN + config_["signal"].asInt()) {
    spdlog::info("singal received {}", sig);
    thread_.wake_up();
  }
}

auto waybar::modules::Images::update() -> void {
  spdlog::info("update function run");

  // clear box_, previous css classes and memory
  if (box_.get_children().size() > 0) {
    resetBoxAndMemory();
  }

  // set new images from config script
  if (!config_["entries"].empty()) {
    setImagesData(config_["entries"]);
  } else if (!config_["exec"].empty()) {
    auto exec = util::command::exec(config_["exec"].asString(), "");
    Json::Value as_json;
    Json::Reader reader;

    if (!reader.parse(exec.out, as_json)) {
      spdlog::error("invalid json from exec {}", exec.out);
      return;
    }

    setImagesData(as_json);
  } else {
    spdlog::error("no image files provded in config");
    return;
  }

  setupAndDraw();

  // spdlog::info("children count: {}", box_.get_children().size());

  AModule::update();
};

void waybar::modules::Images::setupAndDraw() {
  for (unsigned int i = 0; i < images_data_.size(); i++) {
    images_data_[i].img = std::make_shared<Gtk::Image>();
    images_data_[i].btn = std::make_shared<Gtk::Button>();

    auto img = images_data_[i].img;
    auto data = images_data_[i];

    auto path = data.path;
    auto marker = data.marker;
    auto tooltip = data.tooltip;
    bool has_onclick = !data.on_click.empty();

    Glib::RefPtr<Gdk::Pixbuf> pixbuf;
    pixbuf = Gdk::Pixbuf::create_from_file(path, size_, size_);

    if (has_onclick) {
      auto btn = images_data_[i].btn;
      btn->set_name("button_" + path);
      btn->get_style_context()->add_class(marker);
      btn->set_tooltip_text(tooltip);
      btn->set_image(*img);
      box_.pack_start(*btn);

      btn->add_events(Gdk::BUTTON_PRESS_MASK);
      btn->signal_clicked().connect(
          sigc::bind(sigc::mem_fun(*this, &Images::handleClick), data.on_click));

      if (pixbuf) {
        btn->show_all();
        img->set(pixbuf);
        box_.get_style_context()->remove_class("empty");
      } else {
        btn->hide();
        img->clear();
        img->hide();
        box_.get_style_context()->add_class("empty");
      }
    } else {
      img->set_name(path);
      img->get_style_context()->add_class(marker);
      img->set_tooltip_text(tooltip);
      box_.pack_start(*img);
      spdlog::info("added image -> {}:{}", marker, path);

      if (pixbuf) {
        img->set(pixbuf);
        img->show();
        box_.get_style_context()->remove_class("empty");
      } else {
        img->clear();
        img->hide();
        box_.get_style_context()->add_class("empty");
      }
    }
  }
}

void waybar::modules::Images::setImagesData(const Json::Value &cfg_input) {
  for (unsigned int i = 0; i < cfg_input.size(); i++) {
    auto path = cfg_input[i]["path"];
    auto marker = cfg_input[i]["marker"];
    auto tooltip = cfg_input[i]["tooltip"];
    auto onclick = cfg_input[i]["on-click"];

    bool has_tooltip_err = !tooltip.empty() && !tooltip.isString();
    bool has_onclick_err = !onclick.empty() && !onclick.isString();

    if (!path.isString() || !marker.isString() || has_tooltip_err || has_onclick_err ||
        !Glib::file_test(path.asString(), Glib::FILE_TEST_EXISTS)) {
      spdlog::error("invalid input in images config -> {}", cfg_input[i]);
      return;
    }
    ImageData data;
    data.path = path.asString();
    data.marker = marker.asString();
    data.tooltip = !tooltip.empty() ? tooltip.asString() : "";
    data.on_click = onclick.asString();

    images_data_.push_back(data);
  }
}

void waybar::modules::Images::resetBoxAndMemory() {
  auto children = box_.get_children();
  for (auto child : children) {
    box_.remove(*child);
    spdlog::info("child removed with name -> {}", std::string(child->get_name()));
  }

  images_data_.clear();
}

void waybar::modules::Images::handleClick(const Glib::ustring &data) {
  auto msg = std::string(data);
  spdlog::info("command to be executed after clicking on image -> {}", msg);

  auto exec = util::command::exec(data, "");
  spdlog::info("onclick executed with output -> {}", exec.out);
}
