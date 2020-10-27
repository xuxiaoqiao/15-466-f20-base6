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
	TextSpanPtr help_msg_ = std::make_shared<TextSpan>();
	std::function<void(int, int)> listener_;
	int claim_replica_ = 1;
	int claim_digit_ = 1;
	int element_focus_position_ = 0;
};

class RespondClaimDialog {
public:
	// listener: 0: end and reveal, 1: continue
	RespondClaimDialog();
	void set_claim_content(int claim_replica, int claim_digit) {
		claim_replica_ = claim_replica;
		claim_digit_ = claim_digit;
		update_content();
	}
	void set_listener_on_submit(std::function<void(int)> listener);
	void draw();
	bool handle_keypress(SDL_Keycode key);

private:
	void update_content();
	TextSpanPtr prompt1_ = std::make_shared<TextSpan>();
	TextSpanPtr prompt2_ = std::make_shared<TextSpan>();
	TextSpanPtr reveal_ = std::make_shared<TextSpan>();
	TextSpanPtr continue_ = std::make_shared<TextSpan>();
	TextSpanPtr help_msg_ = std::make_shared<TextSpan>();
	std::function<void(bool)> listener_;

	/**
	 * 0: focus on "[end]"
	 * 1: focus on "[continue]"
	 */
	int element_focus_position_ = 0;
	int claim_replica_ = 0, claim_digit_ = 0;

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
	RevealBoardDialog();
	void draw();
	void set_reveal_result(const std::vector<std::pair<std::string, std::vector<uint8_t>>> &result, bool win);
	void set_listener_on_done_reveal(std::function<void()> listener);
	bool handle_keypress(SDL_Keycode key);

private:
	TextSpanPtr label_ = std::make_shared<TextSpan>();
	std::vector<std::pair<TextSpanPtr, TextSpanPtr>> result_;
	TextSpanPtr help_msg_ = std::make_shared<TextSpan>();
	std::function<void()> listener_;
};

class InGamePanel {
public:
	InGamePanel();
	void draw();
	bool handle_keypress(SDL_Keycode key);
	void set_players(std::vector<std::pair<std::string, int>> &players);
	void set_self_dices(std::vector<uint8_t> dices);

	/* 0: waiting for other players.
	 * 1: received a claim from the other player, decide to "reveal" or "continue"
	 * 2: make a new claim (if choose "continue" in 1)
	 * 3: reveal the board, and prepare to start again.
	 */
	int get_panel_state();

	void set_state_respond_claim(int claim_replica, int claim_digit);
	void set_listener_respond_claim(std::function<void(int)> listener);

	void set_state_make_claim();
	void set_listener_make_claim(std::function<void(int, int)> listener);

	void set_state_waiting_others();

	void set_state_reveal(const std::vector<std::pair<std::string, std::vector<uint8_t>>> &dices, bool win);
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
	WaitingRoomPanel() {
		heading_->set_text("AAAA").set_position(0, 0).set_font_size(48);
		help_msg_->set_text("[Press enter to begin]");
	}
	void draw() {
		heading_->draw();
		help_msg_->draw();
		for (auto &p : players_) {
			p->draw();
		}
	}
	bool handle_keypress(SDL_Keycode key) {
		switch (key) {
			case SDLK_RETURN: listener_();
				return true;
			default: return false;
		}
	}

	void set_listener_on_start(std::function<void()> listener) { listener_ = std::move(listener); }
	void set_players(std::vector<std::pair<std::string, bool>> players) {
		size_t current_size = players_.size();
		size_t target_size = players.size();
		if (current_size > target_size) {
			players_.resize(target_size);
		} else if (current_size < target_size) {
			for (size_t i = current_size; i < target_size; i++) {
				players_.emplace_back(std::make_shared<TextSpan>());
			}
		}

		for (size_t i = 0; i < players.size(); i++) {
			std::string name = players.at(i).first;
			if (players.at(i).second) {
				name += " (You)";
			}
			players_.at(i)->set_text(name);
			players_.at(i)->set_position(200, 32 * i + 200).set_font_size(32);
		}
	}
private:
	std::vector<std::shared_ptr<TextSpan>> players_;
	std::function<void()> listener_;
	TextSpanPtr heading_ = std::make_shared<TextSpan>();
	TextSpanPtr help_msg_ = std::make_shared<TextSpan>();

};

}
