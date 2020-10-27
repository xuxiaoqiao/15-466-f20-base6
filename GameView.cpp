#include "GameView.hpp"
#include <cassert>
#include <sstream>

namespace view {

void InGamePanel::draw() {
	switch (dialog_.index()) {
		case 0: std::get<0>(dialog_)->draw();
			break;
		case 1: std::get<1>(dialog_)->draw();
			break;
		case 2: std::get<2>(dialog_)->draw();
			break;
		case 3: std::get<3>(dialog_)->draw();
			break;
		default: assert(0 && "unreachable code");
	}
}

int InGamePanel::get_panel_state() {
	return dialog_.index();
}

void InGamePanel::set_state_respond_claim(const std::string &player, const std::string &claim) {
	dialog_ = std::make_shared<RespondClaimDialog>();
	auto respond_claim_dialog = std::get<std::shared_ptr<RespondClaimDialog>>(dialog_);
	respond_claim_dialog->set_listener_on_submit([this](int respond_type) {
		assert(respond_type == 0 || respond_type == 1);
		respond_claim_listener_(respond_type);
	});
}
void InGamePanel::set_listener_respond_claim(std::function<void(int)> listener) {
	respond_claim_listener_ = std::move(listener);
}

void InGamePanel::set_state_make_claim() {
	dialog_ = std::make_shared<MakeClaimDialog>();
	auto make_claim_dialog = std::get<std::shared_ptr<MakeClaimDialog>>(dialog_);
	make_claim_dialog->set_listener_on_submit([this](int claim_replica, int claim_digit) {
		make_claim_listener_(claim_replica, claim_digit);
	});
}

void InGamePanel::set_listener_make_claim(std::function<void(int, int)> listener) {
	make_claim_listener_ = std::move(listener);
}

void InGamePanel::set_state_waiting_others() {
	dialog_ = std::make_shared<WaitingClaimDialog>();
}
void InGamePanel::set_state_reveal(const std::vector<std::array<int, 6>> &dices) {
	dialog_ = std::make_shared<RevealBoardDialog>();
	auto reveal_board_dialog = std::get<std::shared_ptr<RevealBoardDialog>>(dialog_);
	reveal_board_dialog->set_listener_on_done_reveal([this]() { done_reveal_listener_(); });
}

void InGamePanel::set_listener_done_reveal(std::function<void()> listener) {
	done_reveal_listener_ = std::move(listener);
}

void InGamePanel::set_players(std::vector<std::pair<std::string, int>> &players) {
	players_.resize(players.size());
	for (size_t i = 0; i < players.size(); i++) {
		players_[i]->set_username(players.at(i).first).set_position(500 + 500 * i, 500).set_score(players.at(i).second);
	}
}

void InGamePanel::set_self_dices(std::array<int, 6> dices) {
	std::stringstream ss;
	for (int i: dices) {
		ss << '[' << i << ']';
	}
	dice_view_->set_text(ss.str());
}
bool InGamePanel::handle_keypress(SDL_Keycode key) {
	switch (dialog_.index()) {
		case 0: return false;
		case 1: return std::get<1>(dialog_)->handle_keypress(key);
			break;
		case 2: return std::get<2>(dialog_)->handle_keypress(key);
			break;
		case 3: return std::get<3>(dialog_)->handle_keypress(key);
			break;
		default: assert(0 && "unreachable code");
	}
}

WaitingClaimDialog::WaitingClaimDialog() {
	label_->set_text("Waiting responses from the other player");
}

void WaitingClaimDialog::draw() { label_->draw(); }

/************** MakeClaimDialog **********/
MakeClaimDialog::MakeClaimDialog() {
	title_->set_text("MakeClaimDialog title");
}

void MakeClaimDialog::set_listener_on_submit(std::function<void(int, int)> listener) {
	listener_ = std::move(listener);
}

std::pair<int, int> MakeClaimDialog::get_claim_number() {
	return std::make_pair(claim_replica_, claim_digit_);
}

bool MakeClaimDialog::handle_keypress(SDL_Keycode key) {
	switch (key) {
		case SDLK_LEFT:
			element_focus_position_ = std::max(0, element_focus_position_ - 1);
			update_content();
			return true;
		case SDLK_RIGHT:
			element_focus_position_ = std::min(1, element_focus_position_ + 1);
			update_content();
			return true;
		case SDLK_UP:
			if (element_focus_position_ == 0) {
				claim_replica_ = std::min(12, claim_replica_ + 1);
			} else {
				claim_digit_ = std::min(6, claim_digit_ + 1);
			}
			update_content();
			return true;
		case SDLK_DOWN:
			if (element_focus_position_ == 0) {
				claim_replica_ = std::max(1, claim_replica_ - 1);
			} else {
				claim_digit_ = std::max(1, claim_digit_ - 1);
			}
			update_content();
			return true;
		case SDLK_RETURN:
			listener_(claim_replica_, claim_digit_);
			return true;
		default:
			return false;
	}
}

void MakeClaimDialog::draw() {
	title_->draw();
	prompt1_->draw();
	prompt2_->draw();
	prompt3_->draw();
}

void MakeClaimDialog::update_content() {
	replica_view_->set_text(std::to_string(claim_replica_));
	replica_view_->set_color(element_focus_position_ == 0 ? glm::u8vec4(255, 0, 0, 255) : glm::u8vec4(255));
	digit_view_->set_text(std::to_string(claim_digit_));
	digit_view_->set_color(element_focus_position_ == 1 ? glm::u8vec4(255, 0, 0, 255) : glm::u8vec4(255));
}

/************** RespondClaimDialog **********/

RespondClaimDialog::RespondClaimDialog() {
	prompt_->set_text("RespondClaimDialog prompt");
}

void RespondClaimDialog::draw() {
	prompt_->draw();
}
void RespondClaimDialog::set_listener_on_submit(std::function<void(int)> listener) { listener_ = std::move(listener); }
bool RespondClaimDialog::handle_keypress(SDL_Keycode key) {
	switch (key) {
		case SDLK_LEFT:
			element_focus_position_ = std::max(0, element_focus_position_ - 1);
			update_content();
			return true;
		case SDLK_RIGHT:
			element_focus_position_ = std::min(1, element_focus_position_ + 1);
			update_content();
			return true;
		case SDLK_RETURN:
			listener_(element_focus_position_);
			return true;
		default:
			return false;
	}
}
void RespondClaimDialog::update_content() {
	reveal_->set_text(element_focus_position_ == 0 ? "[reveal]" : " reveal ");
	continue_->set_text(element_focus_position_ == 1 ? "[continue]" : " continue ");
}

void RevealBoardDialog::draw() {
	for (auto &p : result_) {
		p.first->draw();
		p.second->draw();
	}
	label_->draw();
}
void RevealBoardDialog::set_reveal_result(const std::vector<std::pair<std::string, std::array<int, 6>>> &result,
                                          bool win) {
	result_.resize(result.size());
	for (size_t i = 0; i < result.size(); i++) {
		result_.at(i).first->set_text(result.at(i).first);
		std::stringstream ss;
		for (int dice : result.at(i).second) {
			ss << '[' << dice << ']';
		}
		result_.at(i).second->set_text(ss.str());
	}
	if (win) {
		label_->set_text("You win");
	} else {
		label_->set_text("You lose");
	}
}
bool RevealBoardDialog::handle_keypress(SDL_Keycode key) {
	switch (key) {
		case SDLK_RETURN:
			listener_();
			return true;
		default:
			return false;
	}
}
void RevealBoardDialog::set_listener_on_done_reveal(std::function<void()> listener) { listener_ = std::move(listener); }


PlayerTile &PlayerTile::set_username(const std::string &username) {
	username_->set_text(username);
	return *this;
}
PlayerTile &PlayerTile::set_score(int value) {
	score_->set_text(std::to_string(value));
	return *this;
}
PlayerTile &PlayerTile::set_position(int x, int y) {
	position_ = glm::ivec2(x, y);
	return *this;
}
PlayerTile &PlayerTile::set_position(glm::ivec2 pos) {
	position_ = pos;
	return *this;
}
void PlayerTile::draw() {
	username_->draw();
	dices_->draw();
	score_->draw();
}
}

