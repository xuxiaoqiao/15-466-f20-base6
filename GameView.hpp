#pragma once

#include <string>
#include <memory>
#include <variant>

#include "View.hpp"

namespace view {

class PlayerTile {
public:
	PlayerTile &set_username(const std::string &username);
	PlayerTile &set_score(int value);
	PlayerTile &set_position(int x, int y);
	PlayerTile &set_position(glm::ivec2 pos);

	void draw();

private:
	std::shared_ptr<TextSpan> username_ = std::make_shared<TextSpan>();
	std::shared_ptr<TextSpan> dices_ = std::make_shared<TextSpan>();
	std::shared_ptr<TextSpan> score_ = std::make_shared<TextSpan>();
	glm::ivec2 position_ = glm::ivec2(0, 0);
};


class MakeClaimDialog {
public:
	MakeClaimDialog();
	void set_listener_on_submit(std::function<void(int, int)> listener);
	std::pair<int,int> get_claim_number();
	bool handle_keypress(SDL_Keycode key);
	void draw();

private:
	void update_content();
	TextSpanPtr title_ = std::make_shared<TextSpan>();
	TextSpanPtr prompt1_ = std::make_shared<TextSpan>();
	TextSpanPtr prompt2_ = std::make_shared<TextSpan>();
	TextSpanPtr prompt3_ = std::make_shared<TextSpan>();
	TextSpanPtr replica_view_ = std::make_shared<TextSpan>();
	TextSpanPtr digit_view_ = std::make_shared<TextSpan>();
	std::function<void(int, int)> listener_;
	int claim_replica_ = 1;
	int claim_digit_ = 1;
	int element_focus_position_ = 0;
};

class RespondClaimDialog {
public:
	// listener: 0: end and reveal, 1: continue
	RespondClaimDialog();
	void set_listener_on_submit(std::function<void(int)> listener);
	void draw();
	bool handle_keypress(SDL_Keycode key);

private:
	void update_content();
	TextSpanPtr prompt_ = std::make_shared<TextSpan>();
	TextSpanPtr reveal_ = std::make_shared<TextSpan>();
	TextSpanPtr continue_ = std::make_shared<TextSpan>();
	std::function<void(bool)> listener_;

	/**
	 * 0: focus on "[end]"
	 * 1: focus on "[continue]"
	 */
	int element_focus_position_ = 0;

};

class WaitingClaimDialog {
public:
	WaitingClaimDialog();
	void draw();

private:
	TextSpanPtr label_ = std::make_shared<TextSpan>();
};


class RevealBoardDialog {
public:
	void draw();
	void set_reveal_result(const std::vector<std::pair<std::string, std::array<int, 6>>> &result, bool win);
	void set_listener_on_done_reveal(std::function<void()> listener);
	bool handle_keypress(SDL_Keycode key);

private:
	std::vector<std::pair<TextSpanPtr, TextSpanPtr>> result_;
	TextSpanPtr label_ = std::make_shared<TextSpan>();
	std::function<void()> listener_;
};

class InGamePanel {
public:
	void draw();
	bool handle_keypress(SDL_Keycode key);
	void set_players(std::vector<std::pair<std::string, int>> &players);
	void set_self_dices(std::array<int, 6> dices);

	/* 0: waiting for other players.
	 * 1: received a claim from the other player, decide to "reveal" or "continue"
	 * 2: make a new claim (if choose "continue" in 1)
	 * 3: reveal the board, and prepare to start again.
	 */
	int get_panel_state();

	void set_state_respond_claim(const std::string &player, const std::string &claim);
	void set_listener_respond_claim(std::function<void(int)> listener);

	void set_state_make_claim();
	void set_listener_make_claim(std::function<void(int, int)> listener);

	void set_state_waiting_others();

	void set_state_reveal(const std::vector<std::array<int, 6>> &dices);
	void set_listener_done_reveal(std::function<void()> listener);

private:
	std::function<void(int)> respond_claim_listener_;
	std::function<void(int, int)> make_claim_listener_;
	std::function<void()> done_reveal_listener_;

	TextSpanPtr dice_view_ = std::make_shared<TextSpan>();

	std::vector<std::shared_ptr<PlayerTile>> players_;

	std::variant<std::shared_ptr<WaitingClaimDialog>,
	             std::shared_ptr<RespondClaimDialog>,
	             std::shared_ptr<MakeClaimDialog>,
	             std::shared_ptr<RevealBoardDialog>> dialog_ = std::make_shared<WaitingClaimDialog>();
};

class WaitingRoomPanel {
public:
	void draw() {}
	bool handle_event(/* TODO: signature */) { return false; }

	void set_listener_on_join(std::function<void()> listener) {}
	void set_players(std::vector<std::pair<std::string, bool>> players) {}
};

}
