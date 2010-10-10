#include <utility>
#include <assert.h>
#include <SDL/SDL.h>
#include <stdexcept>
#include <iostream>
#include <boost/shared_ptr.hpp>

#include "gui.hpp"
#include "fontstuff.hpp"
#include "loadimage.hpp"

GUI::GUI(int ax, int ay, int aw, int ah) : x(ax), y(ay), w(aw), h(ah), v_focus(false), quit_callback(NULL) {
	set_bg_colour(0, 0, 0);
}

void GUI::set_bg_colour(int r, int g, int b) {
	bgcolour = SDL_MapRGB(screen->format, r, g, b);
	bgimg = NULL;
}

void GUI::set_bg_image(SDL_Surface *img) {
	bgimg = img;
}

void GUI::set_quit_callback(void_callback callback, void *arg) {
	quit_callback = callback;
	quit_callback_arg = arg;
}

void GUI::poll(bool read_events) {
	SDL_Event event;
	
	while(read_events && SDL_PollEvent(&event)) {
		handle_event(event);
	}
	
	if(bgimg) {
		SDL_Rect rect = {0,0,0,0};
		SDL_Rect srect = rect;
		
		for(rect.x = x; rect.x < x+w; rect.x += bgimg->w) {
			for(rect.y = y; rect.y < y+h; rect.y += bgimg->h) {
				srect.w = (x+w - rect.x >= bgimg->w ? bgimg->w : x+w - rect.x);
				srect.h = (y+h - rect.y >= bgimg->h ? bgimg->h : y+h - rect.y);
				
				assert(SDL_BlitSurface(bgimg, &srect, screen, &rect) == 0);
			}
		}
	}else{
		SDL_Rect rect = {x,y,w,h};
		assert(SDL_FillRect(screen, &rect, bgcolour) == 0);
	}
	
	for(thing_set::iterator t = things.begin(); t != things.end(); t++) {
		if(!(v_focus && t == focus)) {
			(*t)->Draw();
		}
	}
	
	if(v_focus) {
		(*focus)->Draw();
	}
	
	SDL_UpdateRect(screen, 0, 0, 0, 0);
}

void GUI::handle_event(const SDL_Event &event) {
	switch(event.type) {
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			for(thing_set::iterator t = things.begin(); t != things.end(); t++) {
				if(event.button.x >= (*t)->x && event.button.x < (*t)->x+(*t)->w && event.button.y >= (*t)->y && event.button.y < (*t)->y+(*t)->h && (*t)->enabled) {
					if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
						focus = things.find(*t);
						v_focus = true;
					}
					
					(*t)->HandleEvent(event);
					break;
				}
			}
			
			break;
			
		case SDL_KEYDOWN:
			if(event.key.keysym.sym == SDLK_TAB) {
				focus_next();
				break;
			}
			
		case SDL_MOUSEMOTION:
		case SDL_KEYUP:
			if(v_focus) {
				(*focus)->HandleEvent(event);
			}
			
			break;
			
		case SDL_QUIT:
			if(quit_callback) {
				quit_callback(*this, event, quit_callback_arg);
			}
			
			break;
			
		default:
			break;
	}
}

void GUI::add_thing(Thing *thing) {
	assert(things.find(thing) == things.end());
	things.insert(thing);
	
	if(!v_focus && thing->enabled) {
		focus_next();
	}
}

void GUI::del_thing(Thing *thing) {
	thing_set::iterator i = things.find(thing);
	
	if(focus == i) {
		focus_next();
	}
	
	things.erase(i);
}

void GUI::focus_next() {
	thing_set::iterator fv = things.end(), of = focus;
	
	for(thing_set::iterator i = things.begin(); i != things.end(); i++) {
		if((*i)->enabled) {
			fv = i;
			break;
		}
	}
	
	if(fv == things.end()) {
		v_focus = false;
		return;
	}
	
	if(!v_focus) {
		focus = fv;
		v_focus = true;
		return;
	}
	
	do {
		focus++;
	} while(focus != things.end() && !(*focus)->enabled);
	
	if(focus == things.end()) {
		if(fv == of) {
			v_focus = false;
		}else{
			focus = fv;
		}
	}
}

GUI::ImgButton::ImgButton(GUI &_gui, SDL_Surface *img, int ax, int ay, int to, callback_f cb, void *arg) : Thing(_gui), callback(cb), callback_arg(arg), image(img) {
	x = ax;
	y = ay;
	w = image->w;
	h = image->h;
	tab_order = to;
	
	gui.add_thing(this);
}

GUI::ImgButton::~ImgButton() {
	gui.del_thing(this);
}

void GUI::ImgButton::HandleEvent(const SDL_Event &event) {
	if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
		x_down = event.button.x;
		y_down = event.button.y;
	}
	
	if(event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
		if(event.button.x == x_down && event.button.y == y_down && callback) {
			callback(*this, event, callback_arg);
		}
	}
}

void GUI::ImgButton::Draw() {
	SDL_Rect srect = {0, 0, image->w/2, image->h/2};
	SDL_Rect rect = {x, y, 0, 0};
	
	if(has_focus()) {
		srect.x = srect.w;
	}
	
	assert(SDL_BlitSurface(image, NULL, screen, &rect) == 0);
}

GUI::TextBox::TextBox(GUI &g, int ax, int ay, int aw, int ah, int to) : Thing(g) {
	x = gui.x + ax;
	y = gui.y + ay;
	w = aw;
	h = ah;
	tab_order = to;
	
	insert_offset = 0;
	
	enter_callback = NULL;
	enter_callback_arg = NULL;
	
	input_callback = NULL;
	input_callback_arg = NULL;
	
	gui.add_thing(this);
}

GUI::TextBox::~TextBox() {
	gui.del_thing(this);
}

void GUI::TextBox::set_text(const std::string &new_text) {
	text = new_text;
	insert_offset = text.length();
}

void GUI::TextBox::HandleEvent(const SDL_Event &event) {
	if(event.type == SDL_KEYDOWN) {
		if(event.key.keysym.sym == SDLK_BACKSPACE) {
			if(insert_offset > 0) {
				text.erase(--insert_offset, 1);
			}
		}else if(event.key.keysym.sym == SDLK_RETURN) {
			if(enter_callback) {
				enter_callback(*this, event, enter_callback_arg);
			}
		}else if(event.key.keysym.sym == SDLK_LEFT) {
			if(insert_offset > 0) {
				insert_offset--;
			}
		}else if(event.key.keysym.sym == SDLK_RIGHT) {
			if(insert_offset < text.length()) {
				insert_offset++;
			}
		}else if(event.key.keysym.sym == SDLK_HOME) {
			insert_offset = 0;
		}else if(event.key.keysym.sym == SDLK_END) {
			insert_offset = text.length();
		}else if(event.key.keysym.sym == SDLK_DELETE) {
			if(insert_offset < text.length()) {
				text.erase(insert_offset, 1);
			}
		}else if(isprint(event.key.keysym.sym)) {
			if(input_callback) {
				if(input_callback(*this, event, input_callback_arg)) {
					text.insert(insert_offset++, 1, event.key.keysym.sym);
				}else{
					std::cerr << "Illegal character" << std::endl;
				}
			}else{
				text.insert(insert_offset++, 1, event.key.keysym.sym);
			}
		}
	}else if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
		TTF_Font *font = FontStuff::LoadFont("fonts/DejaVuSansMono.ttf", 14);
		int fw = FontStuff::TextWidth(font, "A");
		
		if(event.button.y > y && event.button.y < y+h-1) {
			for(insert_offset = 0; event.button.x > x + 2 + fw * (insert_offset+1) && insert_offset < text.length();) {
				insert_offset++;
			}
		}
	}
}

void GUI::TextBox::Draw() {
	SDL_Rect rect = {x, y, w, h};
	
	assert(SDL_FillRect(screen, &rect, SDL_MapRGB(screen->format, 0, 0, 0)) == 0);
	
	rect.x += 1;
	rect.y += 1;
	rect.w -= 1;
	rect.h -= 1;
	
	assert(SDL_FillRect(screen, &rect, SDL_MapRGB(screen->format, 255, 255, 255)) == 0);
	
	TTF_Font *font = FontStuff::LoadFont("fonts/DejaVuSansMono.ttf", 14);
	int fh = TTF_FontHeight(font);
	
	rect.x = x + (h - fh) / 2;
	rect.y = y + (h - fh) / 2;
	
	FontStuff::BlitText(screen, rect, font, ImgStuff::Colour(0,0,0), text);
	
	if(has_focus()) {
		rect.x += FontStuff::TextWidth(font, text.substr(0, insert_offset));
		rect.w = FontStuff::TextWidth(font, insert_offset < text.length() ? text.substr(insert_offset, 1) : "A");
		rect.h = fh;
		
		assert(SDL_FillRect(screen, &rect, SDL_MapRGB(screen->format, 0, 0, 0)) == 0);
		
		if(insert_offset < text.length()) {
			FontStuff::BlitText(screen, rect, font, ImgStuff::Colour(255,255,255), text.substr(insert_offset, 1));
		}
	}
}

GUI::TextDisplay::TextDisplay(GUI &g, int ax, int ay, std::string txt) : Thing(g) {
	x = gui.x + ax;
	y = gui.y + ay;
	w = h = 0;
	tab_order = 0;
	enabled = false;
	
	text = txt;
	font = FontStuff::LoadFont("fonts/DejaVuSansMono.ttf", 12);
	colour = ImgStuff::Colour(255, 255, 255);
	
	gui.add_thing(this);
}

GUI::TextDisplay::~TextDisplay() {
	gui.del_thing(this);
}

void GUI::TextDisplay::Draw() {
	SDL_Rect rect = {x, y, 0, 0};
	FontStuff::BlitText(screen, rect, font, colour, text);
}

GUI::TextButton::TextButton(GUI &g, int ax, int ay, int aw, int ah, int to, std::string text, void_callback callback, void *callback_arg) : Thing(g) {
	x = gui.x + ax;
	y = gui.y + ay;
	w = aw;
	h = ah;
	tab_order = to;
	enabled = tab_order ? true : false;
	
	m_align = CENTER;
	m_fgc = ImgStuff::Colour(255, 255, 255);
	m_bgc = ImgStuff::Colour(0, 0, 0);
	m_borders = true;
	m_opacity = 178;
	
	assert((m_bgs = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, screen->format->BitsPerPixel, 0, 0, 0, 0)));
	assert(SDL_SetAlpha(m_bgs, SDL_SRCALPHA, m_opacity) == 0);
	assert(SDL_FillRect(m_bgs, NULL, ImgStuff::MapColour(m_bgc)) == 0);
	
	m_text = text;
	m_font = FontStuff::LoadFont("fonts/DejaVuSans.ttf", 16);
	m_callback = callback;
	m_arg = callback_arg;
	
	gui.add_thing(this);
}

GUI::TextButton::~TextButton() {
	gui.del_thing(this);
	SDL_FreeSurface(m_bgs);
}

void GUI::TextButton::Draw() {
	SDL_Rect rect = {x, y, w, h};
	
	assert(SDL_BlitSurface(m_bgs, NULL, screen, &rect) == 0);
	
	if(m_borders) {
		Uint32 bcolour = has_focus() ? SDL_MapRGB(screen->format, 255, 255, 0) : SDL_MapRGB(screen->format, 255, 255, 255);
		SDL_Rect ra = {x,y,w,1}, rb = {x,y,1,h}, rc = {x,y+h,w,1}, rd = {x+w,y,1,h+1};
		
		assert(SDL_FillRect(screen, &ra, bcolour) == 0);
		assert(SDL_FillRect(screen, &rb, bcolour) == 0);
		assert(SDL_FillRect(screen, &rc, bcolour) == 0);
		assert(SDL_FillRect(screen, &rd, bcolour) == 0);
	}
	
	rect.w = 0;
	rect.h = 0;
	
	int hoff = (h - TTF_FontLineSkip(m_font)) / 2;
	int width = FontStuff::TextWidth(m_font, m_text);
	
	switch(m_align) {
		case LEFT:
			rect.x = x + hoff;
			rect.y = y + hoff;
			break;
			
		case CENTER:
			rect.x = x + (w - width) / 2;
			rect.y = y + hoff;
			break;
			
		case RIGHT:
			rect.x = (x + w) - (width + hoff);
			rect.y = y + hoff;
			break;
	}
	
	FontStuff::BlitText(screen, rect, m_font, m_fgc, m_text);
}

void GUI::TextButton::HandleEvent(const SDL_Event &event) {
	if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
		x_down = event.button.x;
		y_down = event.button.y;
	}else if(event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
		if(event.button.x == x_down && event.button.y == y_down && m_callback) {
			m_callback(*this, event, m_arg);
		}
	}else if(event.type == SDL_KEYDOWN) {
		if((event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_SPACE) && m_callback) {
			m_callback(*this, event, m_arg);
		}
	}
}

GUI::DropDown::DropDown(GUI &g, int ax, int ay, int aw, int ah, int to) : Thing(g), button(g, ax, ay, aw-ah, ah, 0, "UNSET") {
	x = gui.x + ax;
	y = gui.y + ay;
	w = aw;
	h = ah;
	tab_order = to;
	
	button.align(LEFT);
	button.set_fg_colour(255, 0, 0);
	
	selected = items.end();
	
	callback = NULL;
	callback_arg = NULL;
	
	gui.add_thing(this);
}

GUI::DropDown::~DropDown() {
	gui.del_thing(this);
}

void GUI::DropDown::Draw() {
	SDL_Rect rect = {x+w-h, y, h, h};
	
	assert(SDL_FillRect(screen, &rect, SDL_MapRGB(screen->format, 0, 0, 0)) == 0);
	
	Uint32 bcolour = has_focus() ? SDL_MapRGB(screen->format, 255, 255, 0) : SDL_MapRGB(screen->format, 255, 255, 255);
	SDL_Rect ra = {x,y,w,1}, rb = {x,y,1,h}, rc = {x,y+h,w,1}, rd = {x+w,y,1,h+1}, re = {x+w-h,y,1,h};
	
	assert(SDL_FillRect(screen, &ra, bcolour) == 0);
	assert(SDL_FillRect(screen, &rb, bcolour) == 0);
	assert(SDL_FillRect(screen, &rc, bcolour) == 0);
	assert(SDL_FillRect(screen, &rd, bcolour) == 0);
	assert(SDL_FillRect(screen, &re, bcolour) == 0);
}

static void dropdown_set(const GUI::TextButton &button, const SDL_Event &event, void *arg) {
	GUI::DropDown *drop = (GUI::DropDown*)arg;
	
	for(GUI::DropDown::item_list::iterator i = drop->items.begin(); i != drop->items.end(); i++) {
		if(button.m_text == (*i).text) {
			drop->select(i);
			break;
		}
	}
	
	drop->item_buttons.clear();
}

void GUI::DropDown::HandleEvent(const SDL_Event &event) {
	if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
		if(item_buttons.empty()) {
			int ty = y+h;
			int to = 2000;
			
			for(std::vector<Item>::iterator i = items.begin(); i != items.end(); i++) {
				boost::shared_ptr<TextButton> btn(new TextButton(gui, x, ty, w, h, to, (*i).text, &dropdown_set, this));
				btn->set_fg_colour((*i).colour);
				btn->align(LEFT);
				btn->m_borders = false;
				
				item_buttons.push_back(btn);
				
				ty += h;
				to++;
			}
		}else{
			item_buttons.clear();
		}
	}else if(event.type == SDL_MOUSEMOTION) {
		std::vector<boost::shared_ptr<TextButton> >::iterator i= item_buttons.begin();
		
		for(; i != item_buttons.end(); i++) {
			int mx = event.motion.x;
			int my = event.motion.y;
			
			if(mx >= (*i)->x && mx < (*i)->x+w && my >= (*i)->y && my < (*i)->y+h) {
				(*i)->set_bg_colour(45, 45, 0);
			}else{
				(*i)->set_bg_colour(0, 0, 0);
			}
		}
	}else if(event.type == SDL_KEYDOWN) {
		if(event.key.keysym.sym == SDLK_ESCAPE) {
			item_buttons.clear();
		}
	}
}

void GUI::DropDown::select(item_list::iterator item) {
	if(!callback || callback(*this, *item, callback_arg)) {
		button.m_text = item->text;
		button.set_fg_colour(item->colour);
		selected = item;
	}
}

GUI::Checkbox::Checkbox(GUI &g, int ax, int ay, int aw, int ah, int to, bool default_state) : Thing(g) {
	x = gui.x + ax;
	y = gui.y + ay;
	w = aw;
	h = ah;
	tab_order = to;
	
	state = default_state;
	x_down = -1;
	y_down = -1;
	
	gui.add_thing(this);
}

GUI::Checkbox::~Checkbox() {
	gui.del_thing(this);
}

void GUI::Checkbox::set_callback(void_callback callback, void *arg) {
	toggle_callback = callback;
	toggle_callback_arg = arg;
}

void GUI::Checkbox::Draw() {
	SDL_Rect rect = {x+1,y+1,w-2,h-2};
	
	SDL_Colour black = ImgStuff::Colour(0,0,0);
	SDL_Colour white = ImgStuff::Colour(255,255,255);
	
	SDL_Colour bc = has_focus() ? ImgStuff::Colour(255,255,0) : white;
	SDL_Rect bt = {x,y,w,1}, bb = {x,y+h,w,1}, bl = {x,y,1,h}, br = {x+w,y,1,h};
	
	ImgStuff::draw_rect(bt, bc, 255);
	ImgStuff::draw_rect(bb, bc, 255);
	ImgStuff::draw_rect(bl, bc, 255);
	ImgStuff::draw_rect(br, bc, 255);
	
	ImgStuff::draw_rect(rect, black, 178);
	
	if(state) {
		TTF_Font *font = FontStuff::LoadFont("fonts/DejaVuSansMono.ttf", 18);
		int fh = TTF_FontHeight(font);
		int fw = FontStuff::TextWidth(font, "X");
		
		rect.x = x + (w - fw) / 2;
		rect.y = y + (h - fh) / 2;
		
		FontStuff::BlitText(screen, rect, font, white, "X");
	}
}

void GUI::Checkbox::HandleEvent(const SDL_Event &event) {
	if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
		x_down = event.button.x;
		y_down = event.button.y;
	}else if(event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
		if(event.button.x == x_down && event.button.y == y_down) {
			state = !state;
		}
	}else if(event.type == SDL_KEYDOWN) {
		if(event.key.keysym.sym == SDLK_SPACE) {
			state = !state;
		}
	}
}
