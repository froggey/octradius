#include "powers.hpp"
#include "octradius.hpp"
#include "tile_anims.hpp"
#include "animator.hpp"
#include "gamestate.hpp"
#include <boost/bind.hpp>

#undef ABSOLUTE
#undef RELATIVE

using namespace Powers;

int Powers::RandomPower(void) {
	int total = 0;
	for (size_t i = 0; i < powers.size(); i++) {
		total += Powers::powers[i].spawn_rate;
	}

	int n = rand() % total;
	for (size_t i = 0; i < powers.size(); i++) {
		if(n < Powers::powers[i].spawn_rate) {
			return i;
		}

		n -= Powers::powers[i].spawn_rate;
	}

	abort();
}

/// Functions for getting a list of tiles from a pawn.
typedef boost::function<Tile::List(pawn_ptr)> tile_area_function;

// Return the pawn's current tile, a single point.
static Tile::List point_tile(pawn_ptr pawn)
{
	return Tile::List(1, pawn->cur_tile);
}

// Return the surrounding radial tiles.
static tile_area_function radial_tiles = boost::bind(&Pawn::RadialTiles, _1);
// Functions that deal with the 3 linear directions.
static tile_area_function row_tiles = boost::bind(&Pawn::RowTiles, _1);
static tile_area_function fs_tiles = boost::bind(&Pawn::fs_tiles, _1);
static tile_area_function bs_tiles = boost::bind(&Pawn::bs_tiles, _1);

static void destroy_enemies(Tile::List area, pawn_ptr pawn, ServerGameState *state, Pawn::destroy_type dt, bool enemies_only, bool smash_tile) {
	for(Tile::List::iterator i = area.begin(); i != area.end(); ++i) {
		if((*i)->pawn && (!enemies_only || (*i)->pawn->colour != pawn->colour)) {
			state->destroy_pawn((*i)->pawn, dt, pawn);
			state->add_animator(new Animators::PawnPow((*i)->screen_x, (*i)->screen_y));
			if(smash_tile) {
				(*i)->smashed = true;
				(*i)->has_mine = false;
				(*i)->has_power = false;
				(*i)->has_landing_pad = false;
				state->update_tile(*i);
			}
		}
	}
}

static bool can_destroy_enemies(Tile::List area, pawn_ptr pawn, ServerGameState *, bool enemies_only) {
	for(Tile::List::iterator i = area.begin(); i != area.end(); ++i) {
		if((*i)->pawn && (!enemies_only || (*i)->pawn->colour != pawn->colour)) {
			return true;
		}
	}
	return false;
}

/// Destroy: Nice & simple, just destroy enemy pawns in the target area.
static bool test_destroy_power(tile_area_function area_fn, pawn_ptr pawn, ServerGameState *state)
{
	return can_destroy_enemies(area_fn(pawn), pawn, state, true);
}

static void use_destroy_power(tile_area_function area_fn, pawn_ptr pawn, ServerGameState *state)
{
	destroy_enemies(area_fn(pawn), pawn, state, Pawn::PWR_DESTROY, true, false);
}

/// Annihilate: Destroy *all* pawns in the target area.
static bool test_annihilate_power(tile_area_function area_fn, pawn_ptr pawn, ServerGameState *state) {
	return can_destroy_enemies(area_fn(pawn), pawn, state, false);
}

static void use_annihilate_power(tile_area_function area_fn, pawn_ptr pawn, ServerGameState *state) {
	destroy_enemies(area_fn(pawn), pawn, state, Pawn::PWR_ANNIHILATE, false, false);
}

/// Smash: Destroy enemy pawns in the target area and smash the tiles they're on.
static bool test_smash_power(tile_area_function area_fn, pawn_ptr pawn, ServerGameState *state) {
	return can_destroy_enemies(area_fn(pawn), pawn, state, true);
}

static void use_smash_power(tile_area_function area_fn, pawn_ptr pawn, ServerGameState *state) {
	destroy_enemies(area_fn(pawn), pawn, state, Pawn::PWR_SMASH, true, true);
}

/// Raise Tile: Raise the pawn's current tile up one level.
static void raise_tile(pawn_ptr pawn, ServerGameState *state) {
	state->add_animator(new TileAnimators::ElevationAnimator(
				    Tile::List(1, pawn->cur_tile), pawn->cur_tile, 3.0, TileAnimators::RELATIVE, +1));
	state->set_tile_height(pawn->cur_tile, pawn->cur_tile->height + 1);
}

static bool can_raise_tile(pawn_ptr pawn, ServerGameState *) {
	return pawn->cur_tile->height != 2;
}

/// Lower Tile: Lower the pawn's current tile down one level.
static void lower_tile(pawn_ptr pawn, ServerGameState *state) {
	state->add_animator(new TileAnimators::ElevationAnimator(
				    Tile::List(1, pawn->cur_tile), pawn->cur_tile, 3.0, TileAnimators::RELATIVE, -1));
	state->set_tile_height(pawn->cur_tile, pawn->cur_tile->height - 1);
}

static bool can_lower_tile(pawn_ptr pawn, ServerGameState *) {
	return pawn->cur_tile->height != -2;
}

// Common test function for dig & elevate.
static bool can_dig_elevate_tiles(tile_area_function area_fn, pawn_ptr pawn, ServerGameState *, int target_elevation) {
	Tile::List tiles = area_fn(pawn);

	for(Tile::List::const_iterator i = tiles.begin(); i != tiles.end(); i++) {
		if((*i)->height != target_elevation) {
			return true;
		}
	}

	return false;
}

// Common use function for dig & elevate.
static void dig_elevate_tiles(tile_area_function area_fn, pawn_ptr pawn, ServerGameState *state, int target_elevation) {
	Tile::List tiles = area_fn(pawn);

	state->add_animator(new TileAnimators::ElevationAnimator(tiles, pawn->cur_tile, 3.0, TileAnimators::ABSOLUTE, target_elevation));

	for(Tile::List::const_iterator i = tiles.begin(); i != tiles.end(); i++) {
		state->set_tile_height(*i, target_elevation);
	}
}

/// Elevate: Raise the tiles up to the maximum elevation.
static void use_elevate_power(tile_area_function area_fn, pawn_ptr pawn, ServerGameState *state) {
	dig_elevate_tiles(area_fn, pawn, state, +2);
}

static bool test_elevate_power(tile_area_function area_fn, pawn_ptr pawn, ServerGameState *state) {
	return can_dig_elevate_tiles(area_fn, pawn, state, +2);
}

/// Dig: Lower the tiles down to the minimum elevation.
static void use_dig_power(tile_area_function area_fn, pawn_ptr pawn, ServerGameState *state) {
	dig_elevate_tiles(area_fn, pawn, state, -2);
}

static bool test_dig_power(tile_area_function area_fn, pawn_ptr pawn, ServerGameState *state) {
	return can_dig_elevate_tiles(area_fn, pawn, state, -2);
}

/// Purify: Clear bad upgrades from friendly pawns, good upgrades from enemy pawns and remove enemy tile modifications.
static void use_purify_power(tile_area_function area_fn, pawn_ptr pawn, ServerGameState *state) {
	Tile::List tiles = area_fn(pawn);

	for(Tile::List::iterator i = tiles.begin(); i != tiles.end(); i++) {
		if((*i)->has_mine && (*i)->mine_colour != pawn->colour) {
			(*i)->has_mine = false;
		}
		if((*i)->has_landing_pad && (*i)->landing_pad_colour != pawn->colour) {
			(*i)->has_landing_pad = false;
		}
		if((*i)->pawn && (*i)->pawn->colour != pawn->colour && ((*i)->pawn->flags & PWR_GOOD || (*i)->pawn->range > 0)) {
			(*i)->pawn->flags &= ~PWR_GOOD;
			(*i)->pawn->range = 0;
			state->update_pawn((*i)->pawn);
			// This pawn got changed, rerun the tile effects.
			state->move_pawn_to((*i)->pawn, (*i)->pawn->cur_tile);
		}
		state->update_tile(*i);
	}
}

static bool test_purify_power(tile_area_function area_fn, pawn_ptr pawn, ServerGameState *) {
	Tile::List tiles = area_fn(pawn);

	for(Tile::List::iterator i = tiles.begin(); i != tiles.end(); i++) {
		if((*i)->has_mine && (*i)->mine_colour != pawn->colour) {
			return true;
		}
		if((*i)->has_landing_pad && (*i)->landing_pad_colour != pawn->colour) {
			return true;
		}
		if((*i)->pawn && (*i)->pawn->colour != pawn->colour && ((*i)->pawn->flags & PWR_GOOD || (*i)->pawn->range > 0)) {
			return true;
		}
	}

	return false;
}

/// Teleport: Move to a random location on the board, will not land on a mine, smashed/black hole tile or existing pawn.
static void teleport(pawn_ptr pawn, ServerGameState *state) {
	state->teleport_hack(pawn);
}

static bool can_teleport(pawn_ptr, ServerGameState *state) {
	Tile::List targets = RandomTiles(state->tiles, 1, false, false, false, false);
	return !targets.empty();
}

/// Mine: Add a mine modification to the targeted area.
static bool test_mine_power(tile_area_function area_fn, pawn_ptr pawn, ServerGameState *) {
	Tile::List tiles = area_fn(pawn);

	for(Tile::List::iterator i = tiles.begin(); i != tiles.end(); ++i) {
		Tile *tile = *i;
		if(tile->has_mine) continue;
		if(tile->smashed) continue;
		if(tile->has_black_hole) continue;
		return true;
	}

	return false;
}

static void use_mine_power(tile_area_function area_fn, pawn_ptr pawn, ServerGameState *state) {
	Tile::List tiles = area_fn(pawn);

	for(Tile::List::iterator i = tiles.begin(); i != tiles.end(); ++i) {
		Tile *tile = *i;
		assert(!(tile->has_mine || tile->smashed || tile->has_black_hole));
		tile->has_mine = true;
		tile->mine_colour = pawn->colour;
		state->update_tile(tile);
	}
}

/// Landing Pad: Add a landing pad modification to the current tile.
/// Pawns can move to landing pads from anywhere on the board.
static void landing_pad(pawn_ptr pawn, ServerGameState *state) {
	pawn->cur_tile->has_landing_pad = true;
	pawn->cur_tile->landing_pad_colour = pawn->colour;
	state->update_tile(pawn->cur_tile);
}

static bool can_landing_pad(pawn_ptr pawn, ServerGameState *) {
	if(pawn->cur_tile->smashed) return false;
	if(pawn->cur_tile->has_landing_pad && pawn->cur_tile->landing_pad_colour == pawn->colour) return false;
	if(pawn->cur_tile->has_black_hole) return false;
	return true;
}

/// Black Hole: Overload the pawn's warp core to create a dangerous gravitational anomaly.
/// Black holes will pull pawns in from far & wide, and gain power when
/// created by a pawn with increased range.
static void black_hole(pawn_ptr pawn, ServerGameState *state) {
	Tile *tile = pawn->cur_tile;
	state->destroy_pawn(pawn, Pawn::BLACKHOLE, pawn);
	state->add_animator(new Animators::PawnOhShitIFellDownAHole(tile->screen_x, tile->screen_y));
	tile->has_black_hole = true;
	tile->black_hole_power = pawn->range + 1;
	tile->has_mine = false;
	tile->has_power = false;
	tile->has_landing_pad = false;
	state->update_tile(tile);
}

static bool can_black_hole(pawn_ptr, ServerGameState *) {
	return true;
}

/// Increase Range: Power up a pawn and increase the range of various powers.
static void increase_range(pawn_ptr pawn, ServerGameState *state) {
	assert(pawn->range < 3);
	pawn->range++;
	state->update_pawn(pawn);
}

static bool can_increase_range(pawn_ptr pawn, ServerGameState *) {
	return (pawn->range < 3);
}

static bool can_use_upgrade(pawn_ptr pawn, ServerGameState *, uint32_t upgrade)
{
	return !(pawn->flags & upgrade);
}

static void use_upgrade_power(pawn_ptr pawn, ServerGameState *state, uint32_t upgrade)
{
	state->grant_upgrade(pawn, upgrade);
}

static void def_power(const char *name,
		      boost::function<void(pawn_ptr, ServerGameState *)> use_fn,
		      boost::function<bool(pawn_ptr, ServerGameState *)> test_fn,
		      int probability, Power::Directionality direction)
{
	powers.push_back((Power){name, use_fn, test_fn, probability, direction});
}


// Define powers for each direction.
static void def_directional_power(const char *name,
				  boost::function<void(tile_area_function, pawn_ptr, ServerGameState *)> use_fn,
				  boost::function<bool(tile_area_function, pawn_ptr, ServerGameState *)> test_fn,
				  int radial_probability,
				  int linear_probability)
{
	def_power(name,
		  boost::bind(use_fn, radial_tiles, _1, _2),
		  boost::bind(test_fn, radial_tiles, _1, _2),
		  radial_probability,
		  Powers::Power::radial);
	def_power(name,
		  boost::bind(use_fn, row_tiles, _1, _2),
		  boost::bind(test_fn, row_tiles, _1, _2),
		  linear_probability,
		  Powers::Power::row);
	def_power(name,
		  boost::bind(use_fn, bs_tiles, _1, _2),
		  boost::bind(test_fn, bs_tiles, _1, _2),
		  linear_probability,
		  Powers::Power::nw_se);
	def_power(name,
		  boost::bind(use_fn, fs_tiles, _1, _2),
		  boost::bind(test_fn, fs_tiles, _1, _2),
		  linear_probability,
		  Powers::Power::ne_sw);
}

static void def_upgrade_power(const char *name, uint32_t upgrade, int probabiliy)
{
	def_power(name,
		  boost::bind(use_upgrade_power, _1, _2, upgrade),
		  boost::bind(can_use_upgrade, _1, _2, upgrade),
		  probabiliy,
		  Powers::Power::undirected);
}

std::vector<Powers::Power> Powers::powers;
void Powers::init_powers()
{
	def_directional_power("Destroy", use_destroy_power, test_destroy_power, 50, 50);
	def_directional_power("Annihilate", use_annihilate_power, test_annihilate_power, 50, 50);
	def_directional_power("Smash", use_smash_power, test_smash_power, 50, 50);
	def_directional_power("Elevate", use_elevate_power, test_elevate_power, 35, 35);
	def_directional_power("Dig", use_dig_power, test_dig_power, 35, 35);
	def_directional_power("Purify", use_purify_power, test_purify_power, 50, 50);
	def_directional_power("Mine", use_mine_power, test_mine_power, 40, 20);
	def_power("Mine",
		  boost::bind(use_mine_power, point_tile, _1, _2),
		  boost::bind(test_mine_power, point_tile, _1, _2),
		  60,
		  Powers::Power::undirected);

	def_upgrade_power("Hover", PWR_CLIMB, 30);
	def_upgrade_power("Shield", PWR_SHIELD, 30);
	def_upgrade_power("Invisibility", PWR_INVISIBLE, 40);
	def_upgrade_power("Infravision", PWR_INFRAVISION, 40);

	def_power("Raise Tile", &raise_tile, can_raise_tile, 50, Powers::Power::undirected);
	def_power("Lower Tile", &lower_tile, can_lower_tile, 50, Powers::Power::undirected);
	def_power("Increase Range", &increase_range, can_increase_range, 20, Powers::Power::undirected);
	def_power("Teleport", &teleport, can_teleport, 60, Powers::Power::undirected);
	def_power("Landing Pad", &landing_pad, can_landing_pad, 60, Powers::Power::undirected);
	def_power("Black Hole", &black_hole, can_black_hole, 15, Powers::Power::undirected);
}
