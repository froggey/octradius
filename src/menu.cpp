#include <cassert>
#include <iostream>
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <boost/foreach.hpp>
#include "menu.hpp"
#include "octradius.hpp"
#include "client.hpp"
#include "fontstuff.hpp"
#include "loadimage.hpp"
#include "network.hpp"
#include "gui.hpp"

static bool running, submenu;

static void main_join_cb() {
	JoinMenu jmenu;
	jmenu.run();
}

static void main_host_cb() {
	
}

static void quit_cb() {
	running = false;
}

static void back_cb() {
	submenu = false;
}

static void join_cb() {
	
}

MainMenu::MainMenu() : gui(0, 0, MENU_WIDTH, MENU_HEIGHT) {
	gui.set_bg_image(ImgStuff::GetImage("graphics/menu_background.png"));
	
	join_btn = new GUI::ImgButton(gui, ImgStuff::GetImage("graphics/menus/join_game.png"), 350, 300, 2, &main_join_cb);
	host_btn = new GUI::ImgButton(gui, ImgStuff::GetImage("graphics/menus/host_game.png"), 350, 250, 1, &main_host_cb);
	quit_btn = new GUI::ImgButton(gui, ImgStuff::GetImage("graphics/menus/quit.png"), 700, 570, 3, &quit_cb);
}

MainMenu::~MainMenu() {
	delete join_btn;
	delete host_btn;
	delete quit_btn;
}

void MainMenu::run() {
	running = true;
	
	while(running) {
		gui.poll(true);
		SDL_Delay(MENU_DELAY);
	}
}

JoinMenu::JoinMenu() : gui(0, 0, MENU_WIDTH, MENU_HEIGHT) {
	gui.set_bg_image(ImgStuff::GetImage("graphics/menu_background.png"));
	
	host_label = new GUI::ImgButton(gui, ImgStuff::GetImage("graphics/menus/host.png"), 315, 250, 100, NULL);
	host_input = new GUI::TextBox(gui, 375, 255, 200, 20, 1);
	
	port_label = new GUI::ImgButton(gui, ImgStuff::GetImage("graphics/menus/port.png"), 325, 300, 101, NULL);
	port_input = new GUI::TextBox(gui, 375, 305, 200, 20, 2);
	
	join_btn = new GUI::ImgButton(gui, ImgStuff::GetImage("graphics/menus/join_game.png"), 350, 350, 3, &join_cb);
	back_btn = new GUI::ImgButton(gui, ImgStuff::GetImage("graphics/menus/back.png"), 0, 570, 4, &back_cb);
	quit_btn = new GUI::ImgButton(gui, ImgStuff::GetImage("graphics/menus/quit.png"), 700, 570, 5, &quit_cb);
}

JoinMenu::~JoinMenu() {
	delete host_label;
	delete host_input;
	
	delete port_label;
	delete port_input;
	
	delete join_btn;
	delete back_btn;
	delete quit_btn;
}

void JoinMenu::run() {
	submenu = true;
	
	while(running && submenu) {
		gui.poll(true);
		SDL_Delay(MENU_DELAY);
	}
}

#if 0
		host_menu.screen = screen;
		host_menu += new Button(325, 250, 40, 30, "graphics/menus/port.png", NULL);
		host_port_box = new TextInput(375, 255, 200, 20, "9001", 14, textbox_fg, textbox_bg);
		host_menu += host_port_box;
		host_menu += new Button(275, 300, 90, 30, "graphics/menus/scenario.png", NULL);
		host_scenario_box = new TextInput(375, 305, 200, 20, "scenario/hex_2p.txt", 14, textbox_fg, textbox_bg);
		host_menu += host_scenario_box;
		host_menu += new Button(350, 350, 100, 30, "graphics/menus/host_game.png", start_server_callback);
		host_menu += new Button(0, 570, 100, 30, "graphics/menus/back.png", return_callback);
		host_menu += new Button(700, 570, 100, 30, "graphics/menus/quit.png", quit_callback);
		host_menu.background = main_menu.background;
		
		wait_for_players_menu.screen = screen;
		wait_for_players_menu += new Button(300, 250, 40, 30, "graphics/menus/waiting_for_players.png", NULL);
		wait_for_players_menu.background = main_menu.background;
		wait_for_players_menu.do_stuff = host_do_stuff;
#endif
