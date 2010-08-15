#include <iostream>
#include <stdlib.h>
#include <SDL/SDL.h>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <fstream>
#include <string.h>
#include <map>
#include <SDL/SDL_ttf.h>
#include <math.h>

#include "loadimage.hpp"
#include "fontstuff.hpp"
#include "powers.hpp"
#include "octradius.hpp"
#include "network.hpp"
#include "client.hpp"

struct pmenu_entry {
	SDL_Rect rect;
	int power;
};

struct uistate {
	Pawn *dpawn;
	Pawn *mpawn;
	
	std::vector<struct pmenu_entry> pmenu;
	SDL_Rect pmenu_area;
	
	uistate() : dpawn(NULL), mpawn(NULL), pmenu_area((SDL_Rect){0,0,0,0}) {}
};

static char *next_value(char *str) {
	char *r = str+strcspn(str, "\t ");
	
	if(r[0]) {
		r[0] = '\0';
		r += strspn(r+1, "\t ")+1;
	}
	
	return r;
}

void LoadScenario(std::string filename, Scenario &sc) {
	std::fstream file(filename.c_str(), std::fstream::in);
	assert(file.is_open());
	
	char buf[1024], *bp;
	
	while(file.good()) {
		file.getline(buf, sizeof(buf));
		buf[strcspn(buf, "\n")] = '\0';
		
		bp = next_value(buf);
		std::string name = buf;
		
		if(name == "GRID") {
			sc.cols = atoi(bp);
			sc.rows = atoi(next_value(bp));
			
			assert(sc.cols > 0 && sc.rows > 0);
			
			for(int c = 0; c < sc.cols; c++) {
				for(int r = 0; r < sc.rows; r++) {
					sc.tiles.push_back(new Tile(c, r, 0));
				}
			}
		}
		if(name == "SPAWN") {
			assert(sc.cols > 0 && sc.rows > 0);
			
			/* SPAWN x y c */
			
			int x = atoi(bp);
			int y = atoi((bp = next_value(bp)));
			int c = atoi((bp = next_value(bp)));
			
			Tile *tile = FindTile(sc.tiles, x, y);
			assert(tile);
			
			tile->pawn = new Pawn((PlayerColour)c, sc.tiles, tile);
		}
		if(name == "HOLE") {
			int x = atoi(bp);
			int y = atoi(next_value(bp));
			
			Tile::List::iterator i = sc.tiles.begin();
			
			while(i != sc.tiles.end()) {
				if((*i)->col == x && (*i)->row == y) {
					delete *i;
					sc.tiles.erase(i);
					
					break;
				}
				
				i++;
			}
		}
	}
}

int main(int argc, char **argv) {
	srand(time(NULL));
	
	Scenario scn;
	const char* scenario_name = "scenario/default.txt";
	const char* host;
	int port;
	bool is_server = false;
	bool is_client = false;
	
	for(int i = 1; i < argc; i++) {
		if(strcmp(argv[i], "-s") == 0) {
			host = "127.0.0.1";
			port = atoi(argv[++i]);
			scenario_name = argv[++i];
			is_server = true;
		}
		else if(strcmp(argv[i], "-c") == 0) {
			host = argv[++i];
			port = atoi(argv[++i]);
			is_client = true;
		}
		else {
			std::cerr << "Unrecognized option " << argv[i] << ", learn to type kthx" << std::endl;
			return 1;
		}
	}
	
	if (is_server) {
		LoadScenario(scenario_name, scn);
		Server server(port, scn, 2);
		
		Client client(host, port, "test");

		assert(SDL_Init(SDL_INIT_VIDEO) == 0);
		assert(TTF_Init() == 0);
		
		do {
			server.DoStuff();
		} while(client.DoStuff());
	}
	else if (is_client) {
		Client client(host, port, "test");

		assert(SDL_Init(SDL_INIT_VIDEO) == 0);
		assert(TTF_Init() == 0);
		
		while (client.DoStuff()) {}
	}
	else {
		std::cerr << "Usage: " << argv[0] << " -s port scenario_name" << std::endl;
		std::cerr << "       " << argv[0] << " -c host port" << std::endl;
		return 1;
	}
	
	OctRadius::FreeImages();
	FontStuff::FreeFonts();
	
	SDL_Quit();
	
	return 0;
}
