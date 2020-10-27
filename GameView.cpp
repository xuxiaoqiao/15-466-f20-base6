#include "GameView.hpp"
#include <cassert>
#include <sstream>

namespace view {

void TextSpanCenter(TextSpan *s, int y_pos) {
	int width = s->get_width();
	s->set_position(((int)ViewContext::get().logical_size_.x - width) / 2, y_pos);
};

InGamePanel::InGamePanel() {
	set_state_waiting_others();
	dice_view_->set_font_size(32).set_font(FontFace::IBMPlexMono);
}

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
	dice_view_->draw();
}

int InGamePanel::get_panel_state() {
	return dialog_.index();
}

void InGamePanel::set_state_respond_claim(int claim_replica, int claim_digit) {
	dialog_ = std::make_shared<RespondClaimDialog>();
	auto respond_claim_dialog = std::get<std::shared_ptr<RespondClaimDialog>>(dialog_);
	respond_claim_dialog->set_claim_content(claim_replica, claim_digit);
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
void InGamePanel::set_state_reveal(const std::vector<std::pair<std::string, std::vector<uint8_t>>> &dices, bool win) {
	dialog_ = std::make_shared<RevealBoardDialog>();
	auto reveal_board_dialog = std::get<std::shared_ptr<RevealBoardDialog>>(dialog_);
	reveal_board_dialog->set_reveal_result(dices, win);
	reveal_board_dialog->set_listener_on_done_reveal([this]() { done_reveal_listener_(); });
}

void InGamePanel::set_listener_done_reveal(std::function<void()> listener) {
	done_reveal_listener_ = std::move(listener);
}

//void InGamePanel::set_players(std::vector<std::pair<std::string, int>> &players) {
//	players_.resize(players.size());
//	for (size_t i = 0; i < players.size(); i++) {
//		players_[i]->set_username(players.at(i).first).set_position(500 + 500 * i, 500).set_score(players.at(i).second);
//	}
//}

void InGamePanel::set_self_dices(std::vector<uint8_t> dices) {
	std::stringstream ss;
	for (int i: dices) {
		ss << '[' << i << ']';
	}
	dice_view_->set_text("Your dices: " + ss.str());
	int dice_view_width = dice_view_->get_width();
	dice_view_->set_position(((int)ViewContext::get().logical_size_.x - dice_view_width) / 2, 16);
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
	TextSpanCenter(label_.get(), 600);
}

void WaitingClaimDialog::draw() { label_->draw(); }

/************** MakeClaimDialog **********/
MakeClaimDialog::MakeClaimDialog() {

	title_->set_text("Make Your Claim")
		.set_font_size(48);
	int title_width = title_->get_width();
	title_->set_position(((int)ViewContext::get().logical_size_.x - title_width) / 2, 200);

	help_msg_->set_text("[Press arrow keys to change claim. Press enter to submit]")
		.set_font_size(24);
	int help_msg_width = help_msg_->get_width();
	help_msg_->set_position(((int)ViewContext::get().logical_size_.x - help_msg_width) / 2, 600);


	update_content();
	constexpr int CONTENT_FONTSIZE = 32;
	constexpr int Y_POS = 400;
	constexpr FontFace FONT_FACE = FontFace::IBMPlexMono;
	int x_cursor = 300;
	prompt1_->set_text("\"There's at least ")
		.set_position(x_cursor, Y_POS)
		.set_font_size(CONTENT_FONTSIZE)
		.set_font(FONT_FACE);
	x_cursor += prompt1_->get_width();
	replica_view_->set_position(x_cursor, Y_POS)
		.set_font_size(CONTENT_FONTSIZE)
		.set_font(FONT_FACE);
	x_cursor += replica_view_->get_width();
	prompt2_->set_text(" of ").set_position(x_cursor, Y_POS)
		.set_font_size(CONTENT_FONTSIZE)
		.set_font(FONT_FACE);
	x_cursor += prompt2_->get_width();
	digit_view_->set_position(x_cursor, Y_POS)
		.set_font_size(CONTENT_FONTSIZE)
		.set_font(FONT_FACE);
	x_cursor += digit_view_->get_width();
	prompt3_->set_text(" on board.\"").set_position(x_cursor, Y_POS)
		.set_font_size(CONTENT_FONTSIZE)
		.set_font(FONT_FACE);
	x_cursor += prompt3_->get_width();
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
	replica_view_->draw();
	digit_view_->draw();
	help_msg_->draw();
}

void MakeClaimDialog::update_content() {
	std::string replica_str = std::to_string(claim_replica_);
	if (replica_str.size() == 1) { replica_str = " " + replica_str; }
	replica_view_->set_text(replica_str);
	replica_view_->set_color(element_focus_position_ == 0 ? glm::u8vec4(255, 0, 0, 255) : glm::u8vec4(255));
	digit_view_->set_text(std::to_string(claim_digit_));
	digit_view_->set_color(element_focus_position_ == 1 ? glm::u8vec4(255, 0, 0, 255) : glm::u8vec4(255));
}

/************** RespondClaimDialog **********/

RespondClaimDialog::RespondClaimDialog() {
	prompt1_->set_text("The other player made an claim:")
		.set_font_size(48)
		.set_position(300, 200);
	prompt2_->set_font_size(48).set_position(300, 248);
	reveal_->set_font_size(32).set_font(FontFace::IBMPlexMono).set_position(400, 450);
	continue_->set_font_size(32).set_font(FontFace::IBMPlexMono).set_position(600, 450);
	help_msg_->set_text("[Use arrow keys to select. Press enter to submit]");
	TextSpanCenter(help_msg_.get(), 600);
	update_content();
}

void RespondClaimDialog::draw() {
	prompt1_->draw();
	prompt2_->draw();
	reveal_->draw();
	continue_->draw();
	help_msg_->draw();
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
	prompt2_->set_text(
		"There're at least " + std::to_string(claim_replica_) + " of " + std::to_string(claim_digit_) + " on board");
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
void RevealBoardDialog::set_reveal_result(const std::vector<std::pair<std::string, std::vector<uint8_t>>> &result,
                                          bool win) {
	if (win) {
		label_->set_text("You win.");
	} else {
		label_->set_text("You lose.");
	}

	int x_cursor = 200;
	int y_cursor = 500;
	int font_size = 32;
	if (result_.size() > result.size()) {
		result_.resize(result.size());
	} else if (result_.size() < result.size()) {
		for (size_t i = result_.size(); i < result.size(); i++) {
			result_.emplace_back(std::make_shared<TextSpan>(), std::make_shared<TextSpan>());
		}
	}

	for (size_t i = 0; i < result.size(); i++) {
		result_.at(i).first->set_text(result.at(i).first + ": ").set_position(x_cursor, y_cursor);
		std::stringstream ss;
		for (int dice : result.at(i).second) {
			ss << '[' << dice << ']';
		}
		int player_field_width = result_.at(i).first->get_width();
		result_.at(i).second->set_text(ss.str()).set_position(x_cursor + player_field_width, y_cursor);
		y_cursor += font_size;
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

RevealBoardDialog::RevealBoardDialog() {
	label_->set_position(200, 400).set_font_size(48);
	help_msg_->set_position(550, 600).set_text("[End of game, press enter to quit]");
}

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

WaitingRoomPanel::WaitingRoomPanel() {
	heading_->set_text("Bragging").set_font_size(48);
	TextSpanCenter(heading_.get(), 50);
	waiting_room_label_->set_text("Waiting room").set_font_size(16);
	TextSpanCenter(waiting_room_label_.get(), 120);
	help_msg_->set_text("[Press enter to begin]").set_font_size(16);
	TextSpanCenter(help_msg_.get(), 600);
}
}

