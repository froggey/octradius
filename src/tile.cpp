#include <boost/foreach.hpp>

#include "octradius.hpp"
#include "loadimage.hpp"

Tile::Tile(int c, int r, int h) :
	col(c), row(r), height(h),
	power(-1), has_power(false), smashed(false),
	pawn(pawn_ptr()),
	animating(false), screen_x(0), screen_y(0),
	render_pawn(pawn_ptr()),
	has_mine(false) {
}

bool Tile::SetHeight(int h) {
	if(h != height && h <= 2 && h >= -2) {
		height = h;
		return true;
	}
	else {
		return false;
	}
}

void Tile::CopyToProto(protocol::tile *t) {
	t->set_col(col);
	t->set_row(row);
	t->set_height(height);
	t->set_power(power >= 0 ? true : false);
}

Tile::List RandomTiles(Tile::List tiles, int num, bool uniq) {
	Tile::List ret;

	while(tiles.size() && num) {
		Tile::List::iterator i = tiles.begin();
		i += rand() % tiles.size();

		ret.push_back(*i);

		if(uniq) {
			tiles.erase(i);
		}

		num--;
	}

	return ret;
}
