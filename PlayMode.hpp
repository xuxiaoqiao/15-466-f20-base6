#include "Mode.hpp"

#include "Connection.hpp"
#include "GameView.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode(Client &client);
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	void switch_to_in_game();

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//last message from server:
	std::string server_message;

	//connection to server:
	Client &client;

	//
	std::shared_ptr<view::WaitingRoomPanel> waiting_room_panel = nullptr;
	std::shared_ptr<view::InGamePanel> in_game_panel = nullptr;
	int panel_state = 0;

	/* 0: in waiting room; 1: in game */
	int game_state = 0;
};
