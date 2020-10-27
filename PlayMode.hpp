#include "Mode.hpp"

#include "Connection.hpp"
#include "GameView.hpp"

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

	void switch_to_in_game();

	//----- game state -----
	std::string name;
	std::vector<std::pair<std::string, bool>> players;
	uint8_t id = 0;
	bool other_player_present = false;
	std::string other_name;
	bool first_round = true;
	uint8_t dice_num = 1;
	uint8_t dice_point = 2;
	uint8_t winner = 2;
	enum class State{
		WAITING,
		PLAYING,
		CLAIM,
		HOLDING
	};
	State state = State::WAITING;
	//input tracking:

	int action = 0;
	std::vector<uint8_t> dices;
	std::vector<uint8_t> other_dices;

	//last message from server:
	std::string server_message;

	//connection to server:
	Client &client;
	bool to_be_update = true;
	//
	std::shared_ptr<view::WaitingRoomPanel> waiting_room_panel = nullptr;
	std::shared_ptr<view::InGamePanel> in_game_panel = nullptr;
	int panel_state = 0;

	/* 0: in waiting room; 1: in game */
	int game_state = 0;
};
