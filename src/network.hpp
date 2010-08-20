#ifndef OR_NETWORK_HPP
#define OR_NETWORK_HPP

#include <queue>
#include <stdint.h>
#include <string>
#include <set>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "octradius.pb.h"
#include "octradius.hpp"

class Server {
	struct Client : public boost::enable_shared_from_this<Server::Client> {
		typedef boost::shared_ptr<Server::Client> ptr;
		typedef boost::shared_array<char> wbuf_ptr;
		typedef void (Server::Client::*write_cb)(const boost::system::error_code&, wbuf_ptr);
		
		Client(boost::asio::io_service &io_service, Server &s) : socket(io_service), server(s), colour(SPECTATE) {}
		
		boost::asio::ip::tcp::socket socket;
		Server &server;
		
		uint32_t msgsize;
		std::vector<char> msgbuf;
		
		std::string playername;
		PlayerColour colour;
		
		char shit[81920];
		
		void BeginRead();
		void BeginRead2(const boost::system::error_code& error);
		void FinishRead(const boost::system::error_code& error);
		
		void FinishWrite(const boost::system::error_code& error, wbuf_ptr wb);
		void Write(const protocol::message &msg, write_cb callback = &Server::Client::FinishWrite);
		void WriteBasic(protocol::msgtype type);
		
		void Quit(const std::string &msg);
		void FinishQuit(const boost::system::error_code& error, wbuf_ptr wb);
		void Close();
	};
	
	public:
		Server(uint16_t port, Scenario &s, uint players);
		void DoStuff(void);
		
		~Server() {
			FreeTiles(tiles);
		}
		
	private:
		boost::asio::io_service io_service;
		boost::asio::ip::tcp::acceptor acceptor;
		
		std::set<Server::Client::ptr> clients;
		Tile::List tiles;
		
		Scenario scenario;
		uint req_players;
		
		std::set<Server::Client::ptr>::iterator turn;
		
		int pspawn_turns;
		int pspawn_num;
		
		void StartAccept(void);
		void HandleAccept(Server::Client::ptr client, const boost::system::error_code& err);
		bool HandleMessage(Server::Client::ptr client, const protocol::message &msg);
		
		typedef boost::shared_array<char> wbuf_ptr;
		
		void WriteAll(const protocol::message &msg);
		
		void StartGame(void);
		
		void NextTurn(void);
		void SpawnPowers(void);
};

#endif /* !OR_NETWORK_HPP */
