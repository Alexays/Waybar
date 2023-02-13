#pragma once

#ifndef GAMMASTEP_H
#define GAMMASTEP_H

#include "ALabel.hpp"
#include "bar.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <string>
#include <memory>

#include "gtk-layer-shell.h"
#include <gtkmm/application.h>
#include <gtkmm/button.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/box.h>
#include <glibmm/refptr.h>
#include <gdkmm/event.h>
#include <gtkmm.h>
#include <glib.h>


#define MOON_STRING "\xf0\x9f\x8c\x99"
#define SUN_STRING "\xf0\x9f\x8c\x9e"
#define SETTINGS_STRING "⚙"

#define TEMPERATURE_DEFAULT 3500

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
private:
	Glib::RefPtr<Gtk::Application> app_;
	Gtk::Window window_;
	Gtk::Box box_;

	Gtk::Box H1_box_;
	Gtk::Label label_temp_;
	Gtk::Label label_title_;
	Glib::RefPtr<Gtk::Adjustment> adj_temp_;
	Gtk::Scale scale_temp_;
	unsigned int last_temp = 0;

	Json::Value& config_;
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
	const std::string command_reset = "killall gammastep";
	std::string command_start = "gammastep -m wayland -O ";
	Json::Value& config_;
};
	
class Gammastep : public ALabel {
public:
	Gammastep(const waybar::Bar&, const std::string&, const Json::Value&);
	virtual ~Gammastep();
	auto update() -> void;
private:
	Json::Value config_;

	Gtk::Box box_;
	GammaButton gamma_button;
	SettingsButton settings_button;
};



} // namespace waybar::module 

#endif // ! GAMMASTEP_H