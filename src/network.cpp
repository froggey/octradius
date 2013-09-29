#include <stdint.h>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <stdexcept>
#include <iostream>
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include "network.hpp"
#include "octradius.pb.h"
#include "powers.hpp"
#include "gamestate.hpp"
#include "fontstuff.hpp"

Server::Server(uint16_t port, const std::string &s) :
	game_state(0), acceptor(io_service), scenario(*this)
{
	scenario.load_file(s);

	idcounter = 0;

	turn = clients.end();
	state = LOBBY;

	pspawn_turns = 1;
	pspawn_num = 1;

	boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), port);

	acceptor.open(endpoint.protocol());
	acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
	acceptor.bind(endpoint);
	acceptor.listen();

	StartAccept();

	worker = boost::thread(boost::bind(&Server::worker_main, this));
}

Server::~Server() {
	io_service.stop();

	std::cout << "Waiting for server thread to exit..." << std::endl;
	worker.join();

	delete game_state;
}

void Server::worker_main() {
	io_service.run();
}

void Server::StartAccept(void) {
	Server::Client::ptr client(new Server::Client(io_service, *this));

	acceptor.async_accept(client->socket, boost::bind(&Server::HandleAccept, this, client, boost::asio::placeholders::error));
}

void Server::HandleAccept(Server::Client::ptr client, const boost::system::error_code& err) {
	if(err) {
		if(err.value() == boost::asio::error::operation_aborted) {
			return;
		}

		throw std::runtime_error("Accept error: " + err.message());
	}

	StartAccept();

	if(state == GAME) {
		protocol::message qmsg;
		qmsg.set_msg(protocol::QUIT);
		qmsg.set_quit_msg("Game already in progress");

		client->Write(qmsg, &Server::Client::FinishQuit);

		return;
	}

	std::pair<client_set::iterator,bool> ir;

	do {
		client->id = idcounter++;
		ir = clients.insert(client);
	} while(!ir.second);

	client->BeginRead();
}

void Server::Client::BeginRead() {
	async_read(socket, boost::asio::buffer(&msgsize, sizeof(uint32_t)), boost::bind(&Server::Client::BeginRead2, this, boost::asio::placeholders::error, shared_from_this()));
}

void Server::Client::BeginRead2(const boost::system::error_code& error, ptr /*cptr*/) {
	if(qcalled) {
		return;
	}

	if(error) {
		Quit("Read error: " + error.message());
		return;
	}

	msgsize = ntohl(msgsize);

	if(msgsize > MAX_MSGSIZE) {
		Quit("Oversized message");
		return;
	}

	msgbuf.resize(msgsize);
	async_read(socket, boost::asio::buffer(&(msgbuf[0]), msgsize), boost::bind(&Server::Client::FinishRead, this, boost::asio::placeholders::error, shared_from_this()));
}

void Server::Client::FinishRead(const boost::system::error_code& error, ptr /*cptr*/) {
	if(qcalled) {
		return;
	}

	if(error) {
		Quit("Read error: " + error.message());
		return;
	}

	std::string msgstring(msgbuf.begin(), msgbuf.end());

	protocol::message msg;
	if(!msg.ParseFromString(msgstring)) {
		Quit("Invalid message recieved");
		return;
	}

	if(server.HandleMessage(shared_from_this(), msg)) {
		BeginRead();
	}
}

bool Server::HandleMessage(Server::Client::ptr client, const protocol::message &msg) {
	if(msg.msg() == protocol::CHAT) {
		protocol::message chat;
		chat.set_msgtext(msg.msgtext());
		chat.set_player_id(client->id);

		WriteAll(chat);

		return true;
	}

	if(state == LOBBY) {
		return handle_msg_lobby(client, msg);
	}else{
		return handle_msg_game(client, msg);
	}
}

void Server::Client::Write(const protocol::message &msg, write_cb callback) {
	send_queue.push(server_send_buf(msg));
	send_queue.back().callback = callback;

	if(send_queue.size() == 1) {
		async_write(socket, boost::asio::buffer(send_queue.front().buf.get(), send_queue.front().size), boost::bind(send_queue.front().callback, this, boost::asio::placeholders::error, shared_from_this()));
	}
}

void Server::Client::WriteBasic(protocol::msgtype type) {
	protocol::message msg;
	msg.set_msg(type);

	Write(msg);
}

void Server::Client::FinishWrite(const boost::system::error_code& error, ptr /*cptr*/) {
	send_queue.pop();

	if(qcalled) {
		return;
	}

	if(error) {
		Quit("Write error: " + error.message(), false);
		return;
	}

	if(!send_queue.empty()) {
		async_write(socket, boost::asio::buffer(send_queue.front().buf.get(), send_queue.front().size), boost::bind(send_queue.front().callback, this, boost::asio::placeholders::error, shared_from_this()));
	}
}

void Server::StartGame(void) {
	std::set<PlayerColour> colours;

	BOOST_FOREACH(Client::ptr c, clients) {
		colours.insert(c->colour);
	}

	game_state = static_cast<ServerGameState *>(scenario.init_game(colours));

	TTF_Font *bfont = FontStuff::LoadFont("fonts/DejaVuSansMono-Bold.ttf", 14);
	int bskip = TTF_FontLineSkip(bfont);

	// Initialize tile screen positions, required for animations.
	for(Tile::List::iterator ti = game_state->tiles.begin(); ti != game_state->tiles.end(); ++ti) {
		(*ti)->screen_x = BOARD_OFFSET + TILE_WOFF * (*ti)->col + (((*ti)->row % 2) * TILE_ROFF);
		(*ti)->screen_y = bskip + BOARD_OFFSET + TILE_HOFF * (*ti)->row;
		(*ti)->screen_x += (-1 * (*ti)->height) * TILE_HEIGHT_FACTOR;
		(*ti)->screen_y += (-1 * (*ti)->height) * TILE_HEIGHT_FACTOR;
	}

	protocol::message begin;
	begin.set_msg(protocol::BEGIN);
	WriteAll(begin);

	state = GAME;

	NextTurn();
}

void Server::WriteAll(const protocol::message &msg, Server::Client *exempt) {
	for(client_set::iterator i = clients.begin(); i != clients.end(); i++) {
		if((*i).get() != exempt && (*i)->colour != NOINIT) {
			(*i)->Write(msg);
		}
	}
}

void Server::Client::Quit(const std::string &msg, bool send_to_client) {
	if(qcalled) {
		return;
	}

	qcalled = true;

	if(colour != NOINIT) {
		protocol::message qmsg;
		qmsg.set_msg(protocol::PQUIT);
		qmsg.add_players();
		qmsg.mutable_players(0)->set_name(playername);
		qmsg.mutable_players(0)->set_colour((protocol::colour)colour);
		qmsg.mutable_players(0)->set_id(id);
		qmsg.set_quit_msg(msg);

		server.WriteAll(qmsg, this);
	}

	if(server.game_state) {
		server.game_state->destroy_team_pawns(colour);
	}

	if(*(server.turn) == shared_from_this()) {
		server.NextTurn();
	}

	if(send_to_client) {
		protocol::message pmsg;
		pmsg.set_msg(protocol::QUIT);
		pmsg.set_quit_msg(msg);

		Write(pmsg, &Server::Client::FinishQuit);
	}

	server.clients.erase(shared_from_this());
	
	server.CheckForGameOver();
}

void Server::Client::FinishQuit(const boost::system::error_code &, ptr /*cptr*/) {}

void Server::NextTurn() {
	client_set::iterator last = turn;

	black_hole_suck();

	while(1) {
		if(turn == clients.end()) {
			turn = clients.begin();
		}else{
			if(++turn == clients.end()) {
				continue;
			}
		}

		if((*turn)->colour != SPECTATE &&
		   !game_state->player_pawns((*turn)->colour).empty()) {
				break;
		}
	}

	if(--pspawn_turns == 0) {
		SpawnPowers();
	}

	protocol::message tmsg;
	tmsg.set_msg(protocol::TURN);
	tmsg.set_player_id((*turn)->id);

	WriteAll(tmsg);
}

bool Server::CheckForGameOver() {
	if (getenv("HR_DONT_END_GAME"))
		return false;
	if (!game_state)
		return false;
	
	int alive = 0;
	uint16_t winner_id;
	for (client_set::iterator it = clients.begin(); it != clients.end(); it++) {
		if (game_state->player_pawns((*it)->colour).size()) {
			alive++;
			winner_id = (*it)->id;
		}
	}
	
	if (alive <= 1) {
		state = LOBBY;
		
		protocol::message gover;
		gover.set_msg(protocol::GOVER);
		gover.set_is_draw(alive == 0);
		if (alive > 0)
			gover.set_player_id(winner_id);
		
		turn = clients.end();
		
		WriteAll(gover);
		
		return true;
	}
	else {
		return false;
	}
}

void Server::SpawnPowers() {
	Tile::List stiles = RandomTiles(game_state->tiles, pspawn_num, true, true, false, false);

	protocol::message msg;
	msg.set_msg(protocol::UPDATE);

	for(Tile::List::iterator t = stiles.begin(); t != stiles.end(); t++) {
		if((*t)->smashed) continue;
		(*t)->power = Powers::RandomPower();
		(*t)->has_power = true;

		msg.add_tiles();
		(*t)->CopyToProto(msg.mutable_tiles(msg.tiles_size()-1));
	}

	pspawn_turns = (rand() % 6)+1;
	pspawn_num = (rand() % 4)+1;

	WriteAll(msg);
}

void Server::black_hole_suck() {
	std::set<Tile *> black_holes;
	std::set<pawn_ptr> pawns;

	// Find all pawn and all black holes.
	for(Tile::List::iterator t = game_state->tiles.begin(); t != game_state->tiles.end(); ++t) {
		if((*t)->pawn) {
			pawns.insert((*t)->pawn);
		}
		if((*t)->has_black_hole) {
			black_holes.insert(*t);
		}
	}

	// Draw pawns towards each black hole.
	// Chance is inversely proportional to the square of the euclidean distance
	// and increased by the black hole's power.
	for(std::set<Tile *>::iterator bh = black_holes.begin(); bh != black_holes.end(); ++bh) {
		float bx = (*bh)->col + (((*bh)->row % 2) * 0.5f);
		float by = (*bh)->row * 0.5f;
		for(std::set<pawn_ptr>::iterator p = pawns.begin(); p != pawns.end(); ++p) {
			if((*p)->destroyed()) {
				continue;
			}
			float px = (*p)->cur_tile->col + (((*p)->cur_tile->row % 2) * 0.5f);
			float py = (*p)->cur_tile->row * 0.5f;
			float dx = bx - px, dy = by - py;
			float distance = sqrt(dx * dx + dy * dy);
			float chance = (*bh)->black_hole_power / (distance * distance);
			if(rand() % 100 < (chance * 100)) {
				// OM NOM NOM.
				black_hole_suck_pawn(*bh, *p);
			}
		}
	}
}

// Return true if pawn can be pulled onto the tile.
static bool can_pull_on_to(Tile *tile, pawn_ptr pawn) {
	if(tile->pawn) return false;
	// Tiles can be pulled off ledges, but not up cliffs.
	if(tile->height > pawn->cur_tile->height + 1) return false;
	return true;
}

void Server::black_hole_suck_pawn(Tile *tile, pawn_ptr pawn) {
	Tile *target;
	
	float bx = tile->col + ((tile->row % 2) * 0.5f);
	float by = tile->row * 0.5f;
	float px = pawn->cur_tile->col + ((pawn->cur_tile->row % 2) * 0.5f);
	float py = pawn->cur_tile->row * 0.5f;
	float angle = atan2(py - by, px - bx);
	
	if (angle > -M_PI / 6 && angle <= M_PI / 6) // approaching from right
		target = game_state->tile_at(pawn->cur_tile->col - 1, pawn->cur_tile->row);
	else if (angle > M_PI / 6 && angle <= M_PI / 2) // from bottom right
		target = game_state->tile_at(pawn->cur_tile->col - !(pawn->cur_tile->row % 2), pawn->cur_tile->row - 1);
	else if (angle > M_PI / 2 && angle <= 5 * M_PI / 6) // from bottom left
		target = game_state->tile_at(pawn->cur_tile->col + (pawn->cur_tile->row % 2), pawn->cur_tile->row - 1);
	else if (angle < -M_PI / 6 && angle >= -M_PI / 2) // from top right
		target = game_state->tile_at(pawn->cur_tile->col - !(pawn->cur_tile->row % 2), pawn->cur_tile->row + 1);
	else if (angle < -M_PI / 2 && angle >= -5 * M_PI / 6) // from top left
		target = game_state->tile_at(pawn->cur_tile->col + (pawn->cur_tile->row % 2), pawn->cur_tile->row + 1);
	else if (angle > 5 * M_PI / 6 || angle < -5 * M_PI / 6) // from left
		target = game_state->tile_at(pawn->cur_tile->col + 1, pawn->cur_tile->row);
	else
		target = NULL;
	
	if (target && can_pull_on_to(target, pawn))
		game_state->move_pawn_to(pawn, target);
}

bool Server::handle_msg_lobby(Server::Client::ptr client, const protocol::message &msg) {
	if(msg.msg() == protocol::INIT) {
		int c;
		bool match = true;

		for(c = 0; c < SPECTATE && match;) {
			match = false;

			for(client_set::iterator i = clients.begin(); i != clients.end(); i++) {
				if((*i)->colour != SPECTATE && (*i)->colour == (PlayerColour)c) {
					match = true;
					c++;

					break;
				}
			}
		}

		if(msg.player_name().empty()) {
			client->Quit("No player name supplied");
			return false;
		}

		client->playername = msg.player_name();
		client->colour = (PlayerColour)c;

		protocol::message ginfo;

		ginfo.set_msg(protocol::GINFO);

		ginfo.set_player_id(client->id);

		for(client_set::iterator c = clients.begin(); c != clients.end(); c++) {
			if((*c)->colour == NOINIT) {
				continue;
			}

			int i = ginfo.players_size();

			ginfo.add_players();
			ginfo.mutable_players(i)->set_name((*c)->playername);
			ginfo.mutable_players(i)->set_colour((protocol::colour)(*c)->colour);
			ginfo.mutable_players(i)->set_id((*c)->id);
		}

		scenario.store_proto(ginfo);

		client->Write(ginfo);

		protocol::message pjoin;

		pjoin.set_msg(protocol::PJOIN);

		pjoin.add_players();
		pjoin.mutable_players(0)->set_name(client->playername);
		pjoin.mutable_players(0)->set_colour((protocol::colour)client->colour);
		pjoin.mutable_players(0)->set_id(client->id);

		WriteAll(pjoin, client.get());
	}else if(msg.msg() == protocol::BEGIN && client->id == ADMIN_ID) {
		StartGame();
	}else if(msg.msg() == protocol::CCOLOUR && msg.players_size() == 1) {
		if(client->id == ADMIN_ID || client->id == msg.players(0).id()) {
			Client *c = get_client(msg.players(0).id());

			if(c) {
				c->colour = (PlayerColour)msg.players(0).colour();
				WriteAll(msg);
			}else{
				std::cout << "Invalid player ID in CCOLOUR message" << std::endl;
			}
		}
	}

	return true;
}

bool Server::handle_msg_game(Server::Client::ptr client, const protocol::message &msg) {
	if(msg.msg() == protocol::MOVE) {
		if(msg.pawns_size() != 1) {
			return true;
		}

		const protocol::pawn &p_pawn = msg.pawns(0);

		pawn_ptr pawn = game_state->pawn_at(p_pawn.col(), p_pawn.row());
		Tile *tile = game_state->tile_at(p_pawn.new_col(), p_pawn.new_row());

		if(!pawn || !tile || pawn->colour != client->colour || *turn != client) {
			return true;
		}


		if(pawn->can_move(tile, game_state)) {
			game_state->move_pawn_to(pawn, tile);
			if((pawn->flags & PWR_JUMP) && !pawn->destroyed()) {
				pawn->flags &= ~PWR_JUMP;
				game_state->update_pawn(pawn);
			}
			if (!CheckForGameOver())
				NextTurn();
		}else{
			client->WriteBasic(protocol::BADMOVE);
		}
	}else if(msg.msg() == protocol::USE && msg.pawns_size() == 1) {
		Tile *tile = game_state->tile_at(msg.pawns(0).col(), msg.pawns(0).row());
		pawn_ptr pawn = tile ? tile->pawn : pawn_ptr();

		int power = msg.pawns(0).use_power();

		if(!pawn || !pawn->UsePower(power, game_state)) {
			client->WriteBasic(protocol::BADMOVE);
		}else{
			if(!pawn->destroyed()) {
				update_one_pawn(pawn);
			}

			client->WriteBasic(protocol::OK);
			
			CheckForGameOver();
		}
	}

	return true;
}

Server::Client *Server::get_client(uint16_t id) {
	client_iterator i = clients.begin();

	for(; i != clients.end(); i++) {
		if((*i)->id == id) {
			return i->get();
		}
	}

	return NULL;
}

void Server::update_one_pawn(pawn_ptr pawn)
{
	protocol::message update;
	update.set_msg(protocol::UPDATE);

	update.add_pawns();
	pawn->CopyToProto(update.mutable_pawns(0), true);

	WriteAll(update);
}

void Server::update_one_tile(Tile *tile)
{
	protocol::message update;
	update.set_msg(protocol::UPDATE);

	tile->CopyToProto(update.add_tiles());

	WriteAll(update);
}
