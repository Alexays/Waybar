#include "include/modules/gammastep.hpp"

using namespace waybar::modules;

GammaButton::GammaButton(const Json::Value& config) : 
	Gtk::ToggleButton((Glib::ustring)(SUN_STRING)),
	config_(config) {

	this->signal_toggled().connect(
		sigc::mem_fun(*this, &GammaButton::handle_toggled)
	);
}

GammaButton::~GammaButton() {

}

void GammaButton::handle_toggled() {

	if (this->get_label() == (Glib::ustring)(SUN_STRING)) {
		this->set_label((Glib::ustring)(MOON_STRING));
		system(this->command_start.c_str());
		spdlog::info("gammastep activated");
	} 
	else {
		this->set_label((Glib::ustring)(SUN_STRING));
		system(this->command_reset.c_str());
		spdlog::info("gammastep killed");
	}

}

void Settings::set_margins(const Json::Value& config) {
	struct settings_margins margins_;

	if (config["margin-top"].isUInt() || config["margin-right"].isUInt() ||
      	config["margin-bottom"].isUInt() || config["margin-left"].isUInt()) {
		margins_ = {
        config["margin-top"].isUInt() ? config["margin-top"].asUInt() : 0,
        config["margin-right"].isUInt() ? config["margin-right"].asUInt() : 0,
        config["margin-bottom"].isUInt() ? config["margin-bottom"].asUInt() : 0,
        config["margin-left"].isUInt() ? config["margin-left"].asUInt() : 0
		};

    } else if (config["margin"].isString()) {
		std::istringstream iss(config["margin"].asString());
		std::vector<std::string> margins{std::istream_iterator<std::string>(iss), {}};
		try {
			if (margins.size() == 1) {
				auto gaps = (uint)std::stoul(margins[0], nullptr, 10);
				margins_ = {.top = gaps, .right = gaps, .bottom = gaps, .left = gaps};
			}
			if (margins.size() == 2) {
				auto vertical_margins = (uint)std::stoul(margins[0], nullptr, 10);
				auto horizontal_margins = (uint)std::stoul(margins[1], nullptr, 10);
				margins_ = {.top = vertical_margins,
							.right = horizontal_margins,
							.bottom = vertical_margins,
							.left = horizontal_margins};
			}
			if (margins.size() == 3) {
				auto horizontal_margins = (uint)std::stoul(margins[1], nullptr, 10);
				margins_ = {.top = (uint)std::stoul(margins[0], nullptr, 10),
							.right = horizontal_margins,
							.bottom = (uint)std::stoul(margins[2], nullptr, 10),
							.left = horizontal_margins};
			}
			if (margins.size() == 4) {
				margins_ = {.top = (uint)std::stoul(margins[0], nullptr, 10),
							.right = (uint)std::stoul(margins[1], nullptr, 10),
							.bottom = (uint)std::stoul(margins[2], nullptr, 10),
							.left = (uint)std::stoul(margins[3], nullptr, 10)};
			}
		} catch (...) {
			spdlog::warn("Invalid margins: {}", config["margin"].asString());
		}
	} else if (config["margin"].isUInt()) {
		auto gaps = config["margin"].asUInt();
		margins_ = {.top = gaps, .right = gaps, .bottom = gaps, .left = gaps};
	}

	GtkWindow *c_window_ = window_.gobj();

	gtk_layer_set_margin(c_window_, GTK_LAYER_SHELL_EDGE_TOP, margins_.top);
	gtk_layer_set_margin(c_window_, GTK_LAYER_SHELL_EDGE_RIGHT, margins_.right);
	gtk_layer_set_margin(c_window_, GTK_LAYER_SHELL_EDGE_BOTTOM, margins_.bottom);
	gtk_layer_set_margin(c_window_, GTK_LAYER_SHELL_EDGE_LEFT, margins_.left);
}

void Settings::set_anchors(const Json::Value& config) {
	struct settings_anchors anchors_;

	if (config["anchor"].isString()) {
		std::istringstream iss(config["anchor"].asString());
		std::vector<std::string> anchors{std::istream_iterator<std::string>(iss), {}};
		try {
			if (anchors.size() == 1) {
				auto gaps = (bool)std::stoi(anchors[0], nullptr, 10);
				anchors_ = {.top = gaps, .right = gaps, .bottom = gaps, .left = gaps};
			}
			if (anchors.size() == 2) {
				auto vertical_anchors = (bool)std::stoi(anchors[0], nullptr, 10);
				auto horizontal_anchors = (bool)std::stoi(anchors[1], nullptr, 10);
				anchors_ = {.top = vertical_anchors,
							.right = horizontal_anchors,
							.bottom = vertical_anchors,
							.left = horizontal_anchors};
			}
			if (anchors.size() == 3) {
				auto horizontal_anchors = (bool)std::stoi(anchors[1], nullptr, 10);
				anchors_ = {.top = (bool)std::stoi(anchors[0], nullptr, 10),
							.right = horizontal_anchors,
							.bottom = (bool)std::stoi(anchors[2], nullptr, 10),
							.left = horizontal_anchors};
			}
			if (anchors.size() == 4) {
				anchors_ = {.top = (bool)std::stoi(anchors[0], nullptr, 10),
							.right = (bool)std::stoi(anchors[1], nullptr, 10),
							.bottom = (bool)std::stoi(anchors[2], nullptr, 10),
							.left = (bool)std::stoi(anchors[3], nullptr, 10)};
			}
		} catch (...) {
			spdlog::warn("Invalid anchors: {}", config["anchor"].asString());
		}
	}

	GtkWindow *c_window_ = window_.gobj();

	gtk_layer_set_anchor (c_window_, GTK_LAYER_SHELL_EDGE_TOP, anchors_.top);
	gtk_layer_set_anchor (c_window_, GTK_LAYER_SHELL_EDGE_RIGHT, anchors_.right);
	gtk_layer_set_anchor (c_window_, GTK_LAYER_SHELL_EDGE_BOTTOM, anchors_.bottom);
	gtk_layer_set_anchor (c_window_, GTK_LAYER_SHELL_EDGE_LEFT, anchors_.left);
}

Settings::Settings(const Json::Value& config) :
	app_(Gtk::Application::create("gamma.setting", Gio::APPLICATION_FLAGS_NONE)),
	box_(Gtk::Box(Gtk::ORIENTATION_VERTICAL, 0)),
	label_("Gamma Settings"),
	config_(config) {

	GtkWindow *c_window_ = window_.gobj();
	
	// setup
	gtk_layer_init_for_window(c_window_);
	gtk_layer_set_layer(c_window_, GTK_LAYER_SHELL_LAYER_TOP);

	//margins
	set_margins(config_);

	//anchors
	set_anchors(config_);

	box_.pack_start(label_);
	window_.set_border_width(15);
	window_.add(box_);
	window_.show_all_children();
}

Settings::~Settings() {

}

int Settings::run() {
	return app_->run(window_);
}

Settings* Settings::create(const Json::Value& config) {
	return new Settings(config);
}

void Settings::close() {
	this->window_.close();
}

SettingsButton::SettingsButton(const Json::Value& config) : 
	Gtk::ToggleButton((Glib::ustring)(SETTINGS_STRING)),
	settings_(nullptr),
	config_(config) {

	this->signal_toggled().connect(
		sigc::mem_fun(*this, &SettingsButton::handle_toggled)
	);
}

SettingsButton::~SettingsButton() {

}

void SettingsButton::handle_toggled() {
	if (settings_ != nullptr) {
		spdlog::info("Quitting...");
		settings_->close();
	} else {
		spdlog::info("Settings panel opened");
		settings_ = Settings::create(config_);
		settings_->run();
	}

	settings_ = nullptr;
}

Gammastep::Gammastep(
	const waybar::Bar &bar, const std::string& id, const Json::Value& config) :	
		ALabel(config, "gammastep", id, "", 60, false, true, false),
		box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0),
		gamma_button(config),
		settings_button(config) {
	
	box_.set_homogeneous(false);
	box_.pack_start(gamma_button);
	box_.pack_end(settings_button);
	event_box_.remove();
	event_box_.add(box_);
/* 
	gint wx, wy;
	event_box_.translate_coordinates(event_box_.get_parent(), 0, 0, wx, wy);

	spdlog::info("Coordinates:[{},{}]", wx, wy); */
} 

Gammastep::~Gammastep() {

}

void Gammastep::update() {

}