#include "powers.hpp"
#include "octradius.hpp"

const Powers::Power Powers::powers[] = {
	{"Destroy Row", &Powers::destroy_row, 100},
	{"Destroy Radial", &Powers::destroy_radial, 100},
	{"Raise Tile", &Powers::raise_tile, 100},
	{"Lower Tile", &Powers::lower_tile, 100},
	{"Moar Range", &Powers::moar_range, 20},
	{"Climb Tile", &Powers::climb_tile, 100},
	{"Wall Row", &Powers::wall_row, 100},
	{"Armour", &Powers::armour, 100},
	{"Purify Row", &Powers::purify_row, 100},
	{"Purify Radial", &Powers::purify_radial, 100}
};

const int Powers::num_powers = sizeof(powers) / sizeof(Power);

int Powers::RandomPower(void) {
	int total = 0;
	for (int i = 0; i < Powers::num_powers; i++) {
		total += Powers::powers[i].spawn_rate;
	}
	
	int n = rand() % total;
	for (int i = 0; i < Powers::num_powers; i++) {
		if(n < Powers::powers[i].spawn_rate) {
			return i;
		}
		
		n -= Powers::powers[i].spawn_rate;
	}
	
	abort();
}

namespace Powers {
	static int destroy_enemies(Tile::List &area, Pawn *pawn) {
		Tile::List::iterator i = area.begin();
		int ret = 0;
		
		while(i != area.end()) {
			if((*i)->pawn && (*i)->pawn->colour != pawn->colour) {
				delete (*i)->pawn;
				(*i)->pawn = NULL;
				
				ret = 1;
			}
			
			i++;
		}
		
		return ret;
	}
	
	int destroy_row(Pawn *pawn) {
		Tile::List tiles = pawn->RowTiles();
		return destroy_enemies(tiles, pawn);
	}
	
	int destroy_radial(Pawn *pawn) {
		Tile::List tiles = pawn->RadialTiles();
		return destroy_enemies(tiles, pawn);
	}
	
	int raise_tile(Pawn *pawn) {
		return pawn->GetTile()->SetHeight(pawn->GetTile()->height+1);
	}
	
	int lower_tile(Pawn *pawn) {
		return pawn->GetTile()->SetHeight(pawn->GetTile()->height-1);
	}
	
	int moar_range(Pawn *pawn) {
		if (pawn->range < 3) {
			pawn->range++;
			return 1;
		}
		else return 0;
	}
	
	int climb_tile(Pawn *pawn) {
		if(pawn->flags & PWR_CLIMB) {
			return 0;
		}else{
			pawn->flags |= PWR_CLIMB;
			return 1;
		}
	}
	
	static int wall_tiles(Tile::List tiles) {
		Tile::List::iterator i = tiles.begin();
		int ret = 0;
		
		for(; i != tiles.end(); i++) {
			ret |= (*i)->SetHeight(2);
		}
		
		return ret;
	}
	
	int wall_row(Pawn *pawn) {
		return wall_tiles(pawn->RowTiles());
	}
	
	int armour(Pawn *pawn) {
		if(pawn->flags & PWR_ARMOUR) {
			return 0;
		}
		
		pawn->flags |= PWR_ARMOUR;
		return 1;
	}
	
	static int purify(Tile::List tiles, Pawn *pawn) {
		Tile::List::iterator i = tiles.begin();
		int ret = 0;
		
		for(; i != tiles.end(); i++) {
			if((*i)->pawn && (*i)->pawn->colour != pawn->colour && ((*i)->pawn->flags & PWR_GOOD || (*i)->pawn->range > 0)) {
				(*i)->pawn->flags &= ~PWR_GOOD;
				(*i)->pawn->range = 0;
				ret = 1;
			}
		}
		
		return ret;
	}
	
	int purify_row(Pawn *pawn) {
		return purify(pawn->RowTiles(), pawn);
	}
	
	int purify_radial(Pawn *pawn) {
		return purify(pawn->RadialTiles(), pawn);
	}
}
