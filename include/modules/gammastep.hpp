#pragma once

#ifndef GAMMASTEP_H
#define GAMMASTEP_H

#include "ALabel.hpp"
#include "bar.hpp"

#include <fmt/core.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <thread>

#include "gtk-layer-shell.h"
#include <gtkmm/application.h>
#include <gtkmm/button.h>
#include <gtkmm/box.h>
#include <gtkmm.h>
#include <glibmm/refptr.h>
#include <glib.h>


#define MOON_STRING "\xf0\x9f\x8c\x99"
#define SUN_STRING "\xf0\x9f\x8c\x9e"
#define SETTINGS_STRING "⚙"

namespace waybar::modules {

class Settings {
public:
	Settings();
	virtual ~Settings();
	int run();
private:
	Glib::RefPtr<Gtk::Application> app_;
	Gtk::Window window_;
	Gtk::Box box_;
	Gtk::Label label_;
};

class GammaButton : public Gtk::Button {
public:
	GammaButton();
	virtual ~GammaButton();
	void handle_clicked();
protected:
	const std::string command_reset = "killall gammastep";
	const std::string command_start = "gammastep -m wayland -O 3000 &";
};

class SettingsButton : public Gtk::Button {
public:
	SettingsButton();
	virtual ~SettingsButton();
	void handle_clicked();
private:
	Settings settings_;
};
	
class Gammastep : public ALabel {
public:
	Gammastep(const waybar::Bar&, const std::string&, const Json::Value&);
	virtual ~Gammastep();
	auto update() -> void;
	static void show_settings();
private:
	Gtk::Box box_;
	GammaButton gamma_button;
	SettingsButton settings_button;
};

} // namespace waybar::module 

#endif // ! GAMMASTEP_H