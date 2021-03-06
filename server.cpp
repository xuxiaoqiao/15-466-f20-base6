
#include "Connection.hpp"

#include "hex_dump.hpp"

#include <chrono>
#include <stdexcept>
#include <iostream>
#include <cassert>
#include <unordered_map>
#include <time.h>

void game_start(std::vector<uint8_t>& dices){
	std::srand(time(NULL));
	for(unsigned int i = 0; i< dices.size();i++){
		dices[i] = 1+(std::rand()%6);
	}
}

bool check_result(std::vector<uint8_t>& dices, uint8_t dice_num, uint8_t dice_point){
	for(auto d: dices){
		if (d == dice_point){
			dice_num -= 1;
			if (dice_num == 0){
				return true;
			}
		}
	}
	return false;
}

int main(int argc, char **argv) {
#ifdef _WIN32
	//when compiled on windows, unhandled exceptions don't have their message printed, which can make debugging simple issues difficult.
	try {
#endif

	//------------ argument parsing ------------

	if (argc != 2) {
		std::cerr << "Usage:\n\t./server <port>" << std::endl;
		return 1;
	}

	//------------ initialization ------------

	Server server(argv[1]);


	//------------ main loop ------------
	constexpr float ServerTick = 1.0f; //TODO: set a server tick that makes sense for your game

	//server state:
	std::vector<uint8_t> dices(12,1);
	
	//per-client state:
	struct PlayerInfo {
		PlayerInfo() {
			static uint8_t next_player_id = 0;
			player_id = next_player_id;
			next_player_id += 1;
		}
		std::string name;
		uint8_t player_id;

	};

	uint32_t cur_player = 0;
	uint8_t dice_num = 1;
	uint8_t dice_point = 1;
	std::unordered_map< Connection *, PlayerInfo > players;
	bool waiting = false;
	u_int8_t winner = 0;
	u_int8_t state = 0; // inital state of waiting room
	//0: waiting romm
	//1: rolling dices
	//2: playing
	std::vector<std::string> player_name;

	while (true) {
		static auto next_tick = std::chrono::steady_clock::now() + std::chrono::duration< double >(ServerTick);
		//process incoming data from clients until a tick has elapsed:
		while (true) {
			auto now = std::chrono::steady_clock::now();
			double remain = std::chrono::duration< double >(next_tick - now).count();
			if (remain < 0.0) {
				next_tick += std::chrono::duration< double >(ServerTick);
				break;
			}
			server.poll([&](Connection *c, Connection::Event evt){
				if (evt == Connection::OnOpen) {
					//client connected:

					//create some player info for them:
					players.emplace(c, PlayerInfo());


				} else if (evt == Connection::OnClose) {
					//client disconnected:

					//remove them from the players list:
					auto f = players.find(c);
					assert(f != players.end());
					players.erase(f);


				} else { assert(evt == Connection::OnRecv);
					//got data from client:
					std::cout << "got bytes:\n" << hex_dump(c->recv_buffer); std::cout.flush();

					//look up in players list:
					auto f = players.find(c);
					assert(f != players.end());
					PlayerInfo &player = f->second;

					//handle messages from client:
					//TODO: update for the sorts of messages your clients send
					while (c->recv_buffer.size() >= 1) {
						//expecting five-byte messages 'b' (left count) (right count) (down count) (up count)
						char type = c->recv_buffer[0];
						if (type == 'j'){
							//player first join game
							int32_t size = (
								(uint32_t(c->recv_buffer[1]) << 16) | (uint32_t(c->recv_buffer[2]) << 8) | (uint32_t(c->recv_buffer[3]))
							);
							if (c->recv_buffer.size() < 4 + size) break;
							player.name = std::string(c->recv_buffer.begin() + 4, c->recv_buffer.begin() + 4 + size);
							player_name.push_back(player.name);
							c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 4 + size);
						}
						else if (type == 's'){
							state = 1;
							game_start(dices);
							c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin()+1);
							std::cout<<" Game Start " <<std::endl;
						}
						else if (type == 'c'){
							dice_num = c->recv_buffer[1];
							dice_point = c->recv_buffer[2];
							cur_player = (cur_player + 1) % 2;
							c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin()+3);
						}
						else if (type == 'r'){
							bool res = check_result(dices, dice_num, dice_point);
							if (res) {
								winner = (player.player_id+1)%2;
							}else{
								winner = player.player_id;
							}
							state = 3;
							c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin()+1);
						}
						else{
							std::cout << " message of unknown type received from client!" << std::endl;
							//shut down client connection:
							c->close();
							return;
						}
						


					}
				}
			}, remain);
		}

		//send updated game state to all clients
		//TODO: update for your game state
		int cur = 0;
		for (auto &[c, player] : players) {
			// (void)player; //work around "unused variable" warning on whatever g++ github actions uses
			//send an update starting with 'm', a 24-bit size, and a blob of text:
			if (state == 0) {
				for (std::string name : player_name){
					if (name != player.name){
						c->send('n');
						c->send(player.player_id);
						c->send(uint8_t(name.size() >> 16));
						c->send(uint8_t((name.size() >> 8) % 256));
						c->send(uint8_t(name.size() % 256));
						c->send_buffer.insert(c->send_buffer.end(), name.begin(), name.end());
					}
				}
			}else if (state == 1){
				//send inital dice states
				c->send('d');
				c->send_buffer.insert(c->send_buffer.end(), dices.begin()+cur, dices.begin()+cur+6);
				cur += 6;
			}
			else if (state == 3){
				//send reveal states
				std::cout<<"send reveal state"<<std::endl;
				c->send('r');
				c->send(winner);
				cur = player.player_id == 0 ? 0 : 6;
				c->send_buffer.insert(c->send_buffer.end(), dices.begin()+cur, dices.begin()+cur+6);
			}
			else if(state == 2){
				//send action requirements
				c->send('c');
				if (player.player_id == cur_player){
					c->send('a');
				}else{
					c->send('w');
				}
				c->send(dice_num);
				c->send(dice_point);
				waiting = true;
			}
		}
		if (state == 1){
			state = 2;
		}

	}


	return 0;

#ifdef _WIN32
	} catch (std::exception const &e) {
		std::cerr << "Unhandled exception:\n" << e.what() << std::endl;
		return 1;
	} catch (...) {
		std::cerr << "Unhandled exception (unknown type)." << std::endl;
		throw;
	}
#endif
}
