#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "hex_dump.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

PlayMode::PlayMode(Client &client_, std::string name_) : client(client_) {
	name = name_;
	client.connections.back().send('j');
	client.connections.back().send(uint8_t(name.size() >> 16));
	client.connections.back().send(uint8_t((name.size() >> 8) % 256));
	client.connections.back().send(uint8_t(name.size() % 256));
	std::vector<char>* send_buf = &client.connections.back().send_buffer;
	send_buf->insert(send_buf->end(), name.begin(), name.end());
	players.push_back(std::make_pair(name, true));
	game_state = 0;
	waiting_room_panel = std::make_shared<view::WaitingRoomPanel>();
	waiting_room_panel->set_listener_on_start([this]() {
		client.connections.back().send('s');
	});
}

PlayMode::~PlayMode() {
}

bool PlayMode::
handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_KEYDOWN) {
		if (state == State::WAITING){
			if (evt.key.keysym.sym == SDLK_RETURN){
				action = 1;
			}
		}
		if (panel_state == 0) {
			waiting_room_panel->handle_keypress(evt.key.keysym.sym);
		} else {
			in_game_panel->handle_keypress(evt.key.keysym.sym);
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//queue data for sending to server:
	//TODO: send something that makes sense for your game
	if (action == 1){
		//let the server know a new game starts
		action = 0;
		state = State::PLAYING;
	}
	else if (action == 3){
		//reveal
		action = 0;
	}

	//send/receive data:
	client.poll([this](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			std::cout << "[" << c->socket << "] opened" << std::endl;
		} else if (event == Connection::OnClose) {
			std::cout << "[" << c->socket << "] closed (!)" << std::endl;
			throw std::runtime_error("Lost connection to server!");
		} else { assert(event == Connection::OnRecv);
			std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush();
			//expecting message(s) like 'm' + 3-byte length + length bytes of text:
			while (c->recv_buffer.size() >= 1) {
				std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush();
				char type = c->recv_buffer[0];
				switch (type)
				{
				case 'n':{ //< in waiting room, tells name of other players an self id.
					id = c->recv_buffer[1];
					uint32_t size = (
						(uint32_t(c->recv_buffer[2]) << 16) | (uint32_t(c->recv_buffer[3]) << 8) | (uint32_t(c->recv_buffer[4]))
					);
					if (c->recv_buffer.size() < 5 + size) break; //if whole message isn't here, can't process
					//whole message *is* here, so set current server message:
					other_name = std::string(c->recv_buffer.begin() + 5, c->recv_buffer.begin() + 5 + size);
					if (!other_player_present){
						players.push_back(std::make_pair(other_name, false));
						other_player_present = true;
					}
					if (panel_state == 0) {
						waiting_room_panel->set_players(players);
					}
					//and consume this part of the buffer:
					c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 5 + size);
					break;
				}
				case 'd':{ /// server tells you the state of my dice
					if (c->recv_buffer.size() < 7) break;
					if(to_be_update){
						dices.clear();
						dices.insert(dices.begin(), c->recv_buffer.begin() + 1, c->recv_buffer.begin() + 7);
						if (panel_state == 0) {
							switch_to_in_game();
						}
						in_game_panel->set_self_dices(dices);
					}
					c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 7);
					break;
				}
				case 'c':{ ///
					if(to_be_update){
						if (c->recv_buffer[1] == 'a'){
							//about to make claim
							state = State::CLAIM;
							dice_num = c->recv_buffer[2];
							dice_point = c->recv_buffer[3];
							if (first_round){
								//go to makeclaim dialog directly
								if (panel_state == 1) {
									in_game_panel->set_state_make_claim();
								}
							}else{
								//go to respond dialog
								if (panel_state == 1) {
									in_game_panel->set_state_respond_claim(dice_num, dice_point);
								}
							}
							to_be_update = false;
						}else{
							//waiting others
							std::cout<<"wating response" << std::endl;
							in_game_panel->set_state_waiting_others();
						}
						first_round = false;
					}
					c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 4);
					break;
				}
				case 'r':{
					winner = c->recv_buffer[1];
					bool win = (winner == id) ? true:false;
					std::cout<<"winner "<<winner<<std::endl;
					std::vector<std::pair<std::string, std::vector<uint8_t>>> res;
					other_dices.clear();
					other_dices.insert(other_dices.begin(), c->recv_buffer.begin() + 2, c->recv_buffer.begin() + 8);
					res.push_back(std::make_pair(other_name, other_dices));
					res.push_back(std::make_pair(name, dices));
					in_game_panel->set_state_reveal(res,win);
					c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 8);
				}
				default:
					throw std::runtime_error("Server sent unknown message type '" + std::to_string(type) + "'");
				}
			}
		}
	}, 0.0);
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		if (panel_state==0) {
			waiting_room_panel->draw();
		} else {
			in_game_panel->draw();
		}
	}
	GL_ERRORS();
}

void PlayMode::switch_to_in_game() {
	panel_state = 1;
	waiting_room_panel.reset();
	in_game_panel = std::make_shared<view::InGamePanel>();
	in_game_panel->set_listener_make_claim([this](int claim_replica_, int claim_digit_) {
		client.connections.back().send('c');
		client.connections.back().send((uint8_t) claim_replica_);
		client.connections.back().send((uint8_t) claim_digit_);
		to_be_update = true;
	});
	in_game_panel->set_listener_respond_claim([this](int respond){
		if (respond == 0) {
			client.connections.back().send('r');
			to_be_update = true;
		} else {
			in_game_panel->set_state_make_claim();
		}
	});
	in_game_panel->set_listener_done_reveal([](){ std::exit(0); });
}
