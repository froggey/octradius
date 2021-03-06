#ifndef MENU_HPP
#define MENU_HPP

#include <SDL/SDL.h>
#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>

#include "gui.hpp"

const int MENU_WIDTH = 800;
const int MENU_HEIGHT = 600;
const int MENU_DELAY = 50;

struct MainMenu {
	GUI gui;
	GUI::TextButton join_btn;
	GUI::TextButton host_btn;
	GUI::TextButton edit_btn;
	GUI::TextButton options_btn;
	GUI::TextButton quit_btn;

	MainMenu();

	void run();
};

struct JoinMenu {
	GUI gui;

	GUI::TextButton host_label;
	GUI::TextBox host_input;

	GUI::TextButton port_label;
	GUI::TextBox port_input;

	GUI::TextButton join_btn;
	GUI::TextButton back_btn;

	JoinMenu();

	void run();
};

struct HostMenu {
	GUI gui;

	GUI::TextButton port_label;
	GUI::TextBox port_input;

	GUI::TextButton scenario_label;
	GUI::DropDown<std::string> scenario_input;

	GUI::TextButton host_btn;
	GUI::TextButton back_btn;

	HostMenu();

	void run();
};

extern bool running, submenu;

#endif
