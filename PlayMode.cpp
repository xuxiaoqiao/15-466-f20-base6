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
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (state == State::WAITING){
			if (evt.key.keysym.sym == SDLK_RETURN){
				action = 1;
				return true;
			}
		}
		if (evt.key.repeat) {
			//ignore repeats
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
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
				case 'n':{
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
				case 'd':{
					if (c->recv_buffer.size() < 7) break;
					state = State::PLAYING;
					dices.clear();
					dices.insert(dices.begin(), c->recv_buffer.begin() + 1, c->recv_buffer.begin() + 7);
					c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 7);
					break;
				}
				case 'c':{
					if (c->recv_buffer[1] == 'a'){
						state = State::CLAIM;
						dice_num = c->recv_buffer[2];
						dice_point = c->recv_buffer[3];
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
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		auto draw_text = [&](glm::vec2 const &at, std::string const &text, float H) {
			lines.draw_text(text,
				glm::vec3(at.x, at.y, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			float ofs = 2.0f / drawable_size.y;
			lines.draw_text(text,
				glm::vec3(at.x + ofs, at.y + ofs, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		};

		std::string name_str;
		for (auto p : players){
			name_str += p.first;
		}

		std::string dice_str;
		for (auto d:dices){
			dice_str += std::to_string(d);
		}

		draw_text(glm::vec2(-aspect + 0.1f, 0.0f), server_message, 0.09f);
		draw_text(glm::vec2(-aspect + 0.1f, 0.1f), name_str, 0.09f);
		draw_text(glm::vec2(-aspect + 0.1f, 0.2f), dice_str, 0.09f);
		draw_text(glm::vec2(-aspect + 0.1f,-0.9f), "(press WASD to change your total)", 0.09f);
	}
	GL_ERRORS();
}
