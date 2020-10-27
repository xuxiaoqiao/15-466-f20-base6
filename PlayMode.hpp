#include "Mode.hpp"

#include "Connection.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <string>

struct PlayMode : Mode {
	PlayMode(Client &client, std::string name);
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----
	std::string name;
	std::vector<std::pair<std::string, bool>> players;
	uint8_t id = 0;
	bool other_player_present = false;
	uint8_t dice_num = 1;
	uint8_t dice_point = 2;
	enum class State{
		WAITING,
		PLAYING,
		CLAIM,
		HOLDING
	};
	State state = State::WAITING;
	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;
	int action = 0;
	std::vector<u_int8_t> dices;

	//last message from server:
	std::string server_message;

	//connection to server:
	Client &client;

};
