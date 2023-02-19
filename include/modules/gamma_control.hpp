#pragma once

#ifndef GAMMA_CONTROL_H
#define GAMMA_CONTROL_H

#include "ALabel.hpp"
#include "bar.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <filesystem>
#include <optional>

#include "gtk-layer-shell.h"
#include <gtkmm.h>
#include <glibmm.h>
#include <gdk/gdk.h>
#include <gdkmm.h>
#include <giomm/application.h>

#define MOON_STRING "\xf0\x9f\x8c\x99"
#define SUN_STRING "\xf0\x9f\x8c\x9e"
#define SETTINGS_STRING "⚙"

#define TEMPERATURE_DEFAULT 3500
#define TEMPERATURE_NATURAL 6500

namespace waybar::modules {

class GammaButton;

class Settings {
public:
	struct settings_margins {
		uint top = 0;
		uint right = 0;
		uint bottom = 0;
		uint left = 0;
	};
	struct settings_anchors {
		bool top = true;
		bool right = true;
		bool bottom = false;
		bool left = false;
	};

	Settings(Json::Value&, GammaButton&);
	virtual ~Settings();
	static Settings* create(Json::Value&, GammaButton&);

	int run();
	void close();
protected:
	void set_margins(const Json::Value&);
	void set_anchors(const Json::Value&);
	void on_value_changed();
	bool on_button_released(GdkEventButton*);

	void open_config(std::fstream&);
	void update_config();
private:
	Glib::RefPtr<Gtk::Application> app_;
	Gtk::Window window_;
	Gtk::Box box_;

	Gtk::Box H1_box_;
	Gtk::Label label_title_;
	Gtk::Label label_temp_;
	Gtk::Separator separator_title;
	Glib::RefPtr<Gtk::Adjustment> adj_temp_;
	Gtk::Scale scale_temp_;
	unsigned int last_temp_ = 0;

	Json::Value& config_;
	std::optional<std::string> config_file_path_;
	std::fstream config_file_;
	GammaButton& gamma_button_;
};

class SettingsButton : public Gtk::ToggleButton {
public:
	SettingsButton(Json::Value&, GammaButton&);
	virtual ~SettingsButton();
	void handle_toggled();
private:
	Settings* settings_;
	Json::Value& config_;
	GammaButton& gamma_button_;
};

class GammaButton : public Gtk::ToggleButton {
public:
	GammaButton(Json::Value&);
	virtual ~GammaButton();
	void handle_toggled();

	void set_command_start(unsigned);
	const std::string& get_command_start();
private:
	std::string command_reset_ = "gdbus call -e -d net.zoidplex.wlr_gamma_service \
		-o /net/zoidplex/wlr_gamma_service -m net.zoidplex.wlr_gamma_service.temperature.set " + std::to_string(TEMPERATURE_NATURAL);
	std::string command_start_ = "gdbus call -e -d net.zoidplex.wlr_gamma_service \
		-o /net/zoidplex/wlr_gamma_service -m net.zoidplex.wlr_gamma_service.temperature.set " + std::to_string(TEMPERATURE_DEFAULT);
	Json::Value& config_;
};
	
class GammaControl : public ALabel {
public:
	GammaControl(const waybar::Bar&, const std::string&, const Json::Value&);
	virtual ~GammaControl();
	auto update() -> void;
private:
	Json::Value config_;

	Gtk::Box box_;
	GammaButton gamma_button_;
	SettingsButton settings_button_;
};


} // namespace waybar::module 

#endif // ! GAMMA_CONTROL_H