#include "include/modules/gamma_control.hpp"

using namespace waybar::modules;

// ! GAMMA_BUTTON

void GammaButton::set_command_start(unsigned temp) {
	command_start = "gdbus call -e -d net.zoidplex.wlr_gamma_service \
		-o /net/zoidplex/wlr_gamma_service -m net.zoidplex.wlr_gamma_service.temperature.set " + std::to_string(temp);

	// spdlog::info("command changed: <{}>", command_start);
}

const std::string& GammaButton::get_command_start() {
	return command_start;
}

GammaButton::GammaButton(Json::Value& config) : 
	Gtk::ToggleButton((Glib::ustring)(SUN_STRING)),
	config_(config) {

	this->signal_toggled().connect(
		sigc::mem_fun(*this, &GammaButton::handle_toggled)
	);

	if (config_["temperature"].isUInt()) {
		unsigned int t = config_["temperature"].asUInt();
		command_start += std::to_string(t);
	} else {
		config_["temperature"] = TEMPERATURE_DEFAULT;
		command_start += std::to_string(config_["temperature"].asUInt());
	}

	set_command_start(config_["temperature"].asUInt());
}

GammaButton::~GammaButton() {

}

void GammaButton::handle_toggled() {

	if (this->get_label() == (Glib::ustring)(SUN_STRING)) {
		this->set_label((Glib::ustring)(MOON_STRING));
		system(this->command_start.c_str());
		spdlog::info("gamma modified: [{} K]", config_["temperature"].asUInt());
	} 
	else {
		this->set_label((Glib::ustring)(SUN_STRING));
		system(this->command_reset.c_str());
	}
}

// ! SETTINGS_BUTTON

SettingsButton::SettingsButton(Json::Value& config, GammaButton& gamma_button) : 
	Gtk::ToggleButton((Glib::ustring)(SETTINGS_STRING)),
	settings_(nullptr),
	config_(config),
	gamma_button_(gamma_button) {

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
		settings_ = Settings::create(config_, gamma_button_);
		settings_->run();
	}

	settings_ = nullptr;
}

// ! SETTINGS

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

void Settings::on_value_changed() {
	unsigned int t = (unsigned int)scale_temp_.get_value();
	if (t == last_temp)
		return;

	config_["temperature"] = t;
	last_temp = t;
	gamma_button_.set_command_start(t);

	if (gamma_button_.get_active()) {
		system(gamma_button_.get_command_start().c_str());
		spdlog::info("gammastep activated: [{} K]", config_["temperature"].asUInt());
	}
}

bool Settings::on_button_released(GdkEventButton* event) {
	// unsigned int t = (unsigned int)scale_temp_.get_value();
	// if (t == last_temp)
	// 	return false;

	// config_["temperature"] = t;
	// last_temp = t;
	// gamma_button_.set_command_start(t);

	// if (gamma_button_.get_active()) {
	// 	std::string command =  gamma_button_.get_command_start();
	// 	system("killall gammastep");
	// 	system(command.c_str());
	// 	spdlog::info("gammastep activated: [{} K]", config_["temperature"].asUInt());
	// }

    return false;
}

Settings::Settings(Json::Value& config, GammaButton& gamma_button) :
	app_(Gtk::Application::create("gamma.setting", Gio::APPLICATION_FLAGS_NONE)),
	box_(Gtk::ORIENTATION_VERTICAL, 0),
	H1_box_(Gtk::ORIENTATION_HORIZONTAL, 0),
	label_temp_("Temperature"),
	label_title_("Gamma Settings"),
	adj_temp_(Gtk::Adjustment::create(config["temperature"].isUInt() ? config["temperature"].asUInt() : TEMPERATURE_DEFAULT, 1700, 15001, 50.0, 100.0, 1.0)),
	scale_temp_(adj_temp_, Gtk::ORIENTATION_HORIZONTAL),
	config_(config),
	gamma_button_(gamma_button) {

	last_temp = config_["temperature"].isUInt() ? config_["temperature"].asUInt() : TEMPERATURE_DEFAULT;

	GtkWindow *c_window_ = window_.gobj();
	
	
	// setup
	gtk_layer_init_for_window(c_window_);
	gtk_layer_set_layer(c_window_, GTK_LAYER_SHELL_LAYER_TOP);
	gtk_layer_set_keyboard_mode(c_window_, GTK_LAYER_SHELL_KEYBOARD_MODE_NONE);

	//margins
	set_margins(config_);

	//anchors
	set_anchors(config_);

	label_title_.set_justify(Gtk::JUSTIFY_CENTER);
	label_title_.set_margin_top(5);
	label_title_.set_margin_bottom(5);

	scale_temp_.set_digits(0);
	scale_temp_.set_value_pos(Gtk::POS_LEFT);
	scale_temp_.set_draw_value(true);
	scale_temp_.set_margin_right(15);

	// temp_scale_ events
	scale_temp_.signal_value_changed().connect(
		sigc::mem_fun(*this, &Settings::on_value_changed),
		false
	);

	scale_temp_.add_events(Gdk::BUTTON_RELEASE_MASK);
	scale_temp_.signal_button_release_event().connect(
		sigc::mem_fun(*this, &Settings::on_button_released),
		false
	);

	H1_box_.pack_start(label_temp_);
	H1_box_.pack_end(scale_temp_);

	box_.pack_start(label_title_);
	box_.pack_end(H1_box_);
	box_.set_homogeneous(false);
	box_.set_size_request(400, -1);
	// window_.set_border_width(15);
	window_.add(box_);
	window_.show_all_children();
}

Settings::~Settings() {

}

int Settings::run() {
	return app_->run(window_);
}

Settings* Settings::create(Json::Value& config, GammaButton& gamma_button) {
	return new Settings(config, gamma_button);
}

void Settings::close() {
	this->window_.close();
}

GammaControl::GammaControl(
	const waybar::Bar &bar, const std::string& id, const Json::Value& config) :	
		ALabel(config, "gammastep", id, "", 60, false, true, false),
		config_(config),
		box_(bar.vertical ? Gtk::ORIENTATION_VERTICAL : Gtk::ORIENTATION_HORIZONTAL, 0),
		gamma_button(config_),
		settings_button(config_, gamma_button) {
	
	system("killall wlr-gamma-service");
	system("wlr-gamma-service &");
	box_.set_homogeneous(false);
	box_.pack_start(gamma_button);
	box_.pack_end(settings_button);
	event_box_.remove();
	event_box_.add(box_);
} 

GammaControl::~GammaControl() {

}

void GammaControl::update() {

}