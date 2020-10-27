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
	waiting_room_panel->set_listener_on_join([]() {
		// TODO
		assert(false && "not implemented");
	});
	switch_to_in_game();
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
		in_game_panel->handle_keypress(evt.key.keysym.sym);
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//queue data for sending to server:
	//TODO: send something that makes sense for your game
	if (action == 1){
		//let the server know a new game starts
		client.connections.back().send('s');
		action = 0;
		state = State::PLAYING;
	}
	else if (action == 2){
		client.connections.back().send('c');
		client.connections.back().send(dice_num);
		client.connections.back().send(dice_point);
		action = 0;
	}
	else if (action == 3){
		//reveal
		client.connections.back().send('r');
		action = 0;
	}

	if (left.downs || right.downs || down.downs || up.downs) {
		//send a five-byte message of type 'b':
		client.connections.back().send('b');
		client.connections.back().send(left.downs);
		client.connections.back().send(right.downs);
		client.connections.back().send(down.downs);
		client.connections.back().send(up.downs);
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;

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
					std::string other_name = std::string(c->recv_buffer.begin() + 5, c->recv_buffer.begin() + 5 + size);
					if (!other_player_present){
						players.push_back(std::make_pair(other_name, false));
						other_player_present = true;
					}
					//and consume this part of the buffer:
					c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 5 + size);
					break;
				}
				case 'd':{ /// server tells you the state of my dice
					if (c->recv_buffer.size() < 7) break;
					state = State::PLAYING;
					dices.clear();
					dices.insert(dices.begin(), c->recv_buffer.begin() + 1, c->recv_buffer.begin() + 7);
					c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 7);
					break;
				}
				case 'c':{ ///
					if (c->recv_buffer[1] == 'a'){
						state = State::CLAIM;
						dice_num = c->recv_buffer[2];
						dice_point = c->recv_buffer[3];
						// TODO(xiaoqiao) switch to "respond to claim"
					}else{
						state = State::HOLDING;
					}

					c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 4);
					break;
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
}
