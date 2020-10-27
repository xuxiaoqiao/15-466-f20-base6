#include <string>
#include <iostream>
#include <utility>

#include "GL.hpp"
#include "gl_errors.hpp"

#include "View.hpp"
#include "Load.hpp"
#include "data_path.hpp"
#include "ColorTextureProgram.hpp"

namespace view {

struct RenderTextureProgram {
	// constructor and destructor: these are heavy weight functions
	// creating and freeing OpenGL resources
	RenderTextureProgram() {
		GLuint vs{0}, fs{0};

		const char *VERTEX_SHADER = ""
		                            "#version 410 core\n"
		                            "in vec4 in_Position;\n"
		                            "out vec2 texCoords;\n"
		                            "void main(void) {\n"
		                            "    gl_Position = vec4(in_Position.xy, 0, 1);\n"
		                            "    texCoords = in_Position.zw;\n"
		                            "}\n";

		const char *FRAGMENT_SHADER = ""
		                              "#version 410 core\n"
		                              "precision highp float;\n"
		                              "uniform sampler2D tex;\n"
		                              "uniform vec4 color;\n"
		                              "in vec2 texCoords;\n"
		                              "out vec4 fragColor;\n"
		                              "void main(void) {\n"
		                              "    fragColor = vec4(1, 1, 1, texture(tex, texCoords).r) * color;\n"
		                              "}\n";


		// Initialize shader
		vs = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vs, 1, &VERTEX_SHADER, 0);
		glCompileShader(vs);

		fs = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fs, 1, &FRAGMENT_SHADER, 0);
		glCompileShader(fs);

		program_ = glCreateProgram();
		glAttachShader(program_, vs);
		glAttachShader(program_, fs);
		glLinkProgram(program_);


		// Get shader uniforms
		glUseProgram(program_);
		glBindAttribLocation(program_, 0, "in_Position");
		tex_uniform_ = glGetUniformLocation(program_, "tex");
		color_uniform_ = glGetUniformLocation(program_, "color");
	}
	~RenderTextureProgram() {}
	GLuint program_ = 0;
	GLuint tex_uniform_ = 0;
	GLuint color_uniform_ = 0;
};

static Load<RenderTextureProgram> program(LoadTagEarly);

ViewContext ViewContext::singleton_{};

const ViewContext &ViewContext::get() {
	if (!singleton_.is_initialized) {
		std::cerr << "Accessing ViewContext singleton before initialization" << std::endl;
		std::abort();
	}
	return singleton_;
}
void ViewContext::set(const glm::uvec2 &logicalSize, const glm::uvec2 &drawableSize) {
	singleton_.logical_size_ = logicalSize;
	singleton_.drawable_size_ = drawableSize;
	singleton_.scale_factor_ = static_cast<float>(drawableSize.x) / logicalSize.x;
	singleton_.is_initialized = true;
}

GlyphTextureCache *GlyphTextureCache::singleton_ = nullptr;

GlyphTextureCache *GlyphTextureCache::get_instance() {
	if (!singleton_) {
		singleton_ = new GlyphTextureCache;
	}
	return singleton_;
}

GlyphTextureCache::GlyphTextureCache() {
	FT_Error error = FT_Init_FreeType(&ft_library_);
	if (error != 0) { throw std::runtime_error("Error in initializing FreeType library"); }
	for (FontFace f : {FontFace::IBMPlexMono, FontFace::ComputerModernRegular, FontFace::IBMPlexSans}) {
		const std::string font_path = data_path(get_font_filename(f));
		FT_Face face = nullptr;
		error = FT_New_Face(ft_library_, font_path.c_str(), 0, &face);
		if (error != 0) { throw std::runtime_error("Error initializing font face"); }
		assert(face != nullptr);
		font_faces_.emplace(f, face);
	}
}

GlyphTextureCache::~GlyphTextureCache() {
	for (const auto &p : font_faces_) {
		FT_Done_Face(p.second);
	}
	font_faces_.clear();
	FT_Done_FreeType(ft_library_);
	ft_library_ = nullptr;
}

std::string GlyphTextureCache::get_font_filename(FontFace font_face) {
	switch (font_face) {
		case FontFace::IBMPlexMono : return "IBMPlexMono-Regular.ttf";
		case FontFace::ComputerModernRegular : return "cmunorm.ttf";
		case FontFace::IBMPlexSans : return "IBMPlexSans-Regular.ttf";
		default: throw std::runtime_error("unreachable code");
	}
}
void GlyphTextureCache::incTexRef(FontFace font_face,
                                  int font_size,
                                  hb_codepoint_t codepoint) {
	// TODO(xiaoqiao): manage the calls to GL_UNPACK_ALIGNMENT more carefully.
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	GlyphTextureKey key{font_face, font_size, codepoint};
	auto it = map_.find(key);
	if (it != map_.end()) {
		it->second.ref_cnt++;
		return;
	} else {
		FT_Face face = font_faces_.at(font_face);

		if (FT_Set_Pixel_Sizes(face, 0, ViewContext::compute_physical_px(font_size)) != 0) {
			throw std::runtime_error("Error setting char size");
		}

		if (FT_Load_Glyph(face, codepoint, FT_LOAD_DEFAULT) != 0) {
			throw std::runtime_error("Error loading glyph");
			std::cout << "Error rendering glyph" << std::endl;
		}

		if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
			throw std::runtime_error("Error rendering glyph");
			std::cout << "Error rendering glyph" << std::endl;
		}
		auto glyph = face->glyph;
		size_t buffer_len = glyph->bitmap.width * glyph->bitmap.rows;

		// I haven't figured out a proper way to deal with GlyphTextureEntry
		// copy / move constructor, as a result, I have to use this std::piecewise_construct
		// trick to make sure no copy/move constructor is called.
		auto emplace_result_pair =
			map_.emplace(std::piecewise_construct,
			             std::forward_as_tuple(font_face, font_size, codepoint),
			             std::forward_as_tuple(
				             std::vector<uint8_t>(glyph->bitmap.buffer,
				                                  glyph->bitmap.buffer
					                                  + buffer_len),
				             glyph->bitmap_left,
				             glyph->bitmap_top,
				             glyph->bitmap.width,
				             glyph->bitmap.rows,
				             1));

		glBindTexture(GL_TEXTURE_2D, emplace_result_pair.first->second.gl_texture_id);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
		             glyph->bitmap.width, glyph->bitmap.rows,
		             0, GL_RED, GL_UNSIGNED_BYTE, glyph->bitmap.buffer);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}
const GlyphTextureCache::GlyphTextureEntry *GlyphTextureCache::getTextRef(FontFace font_face,
                                                                          int font_size,
                                                                          hb_codepoint_t codepoint) {
	GlyphTextureKey key{font_face, font_size, codepoint};
	return &map_.at(key);
}

void GlyphTextureCache::decTexRef(FontFace font_face, int font_size, hb_codepoint_t codepoint) {
	GlyphTextureKey key{font_face, font_size, codepoint};
	auto it = map_.find(key);
	assert(it != map_.end());
	assert(it->second.ref_cnt > 0);
	if (it->second.ref_cnt == 1) {
		map_.erase(it);
	} else {
		it->second.ref_cnt--;
	}
}
FT_Face GlyphTextureCache::get_free_type_face(FontFace font_face) {
	return font_faces_.at(font_face);
}

GlyphTextureCache::GlyphTextureEntry::GlyphTextureEntry(const std::vector<uint8_t> &bitmap,
                                                        int bitmapLeft,
                                                        int bitmapTop,
                                                        int width,
                                                        int height,
                                                        int refCnt)
	: bitmap(bitmap),
	  bitmap_left(bitmapLeft),
	  bitmap_top(bitmapTop),
	  width(width),
	  height(height),
	  ref_cnt(refCnt) {
	glGenTextures(1, &gl_texture_id);
}

GlyphTextureCache::GlyphTextureEntry::~GlyphTextureEntry() {
	glDeleteTextures(1, &gl_texture_id);
}

bool GlyphTextureCache::GlyphTextureKey::operator<(const GlyphTextureCache::GlyphTextureKey &rhs) const {
	return std::tie(font_face, font_size, codepoint) < std::tie(rhs.font_face, rhs.font_size, rhs.codepoint);
}
GlyphTextureCache::GlyphTextureKey::GlyphTextureKey(FontFace fontFace, int fontSize, hb_codepoint_t codepoint)
	: font_face(fontFace), font_size(fontSize), codepoint(codepoint) {}

TextSpan::TextSpan() {
	scale_factor_ = get_scale_physical();

	// initialize opengl resources
	glGenBuffers(1, &vbo_);
	glGenVertexArrays(1, &vao_);

	glGenSamplers(1, &sampler_);
	glSamplerParameteri(sampler_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(sampler_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glSamplerParameteri(sampler_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glSamplerParameteri(sampler_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Set some initialize GL state
	glEnable(GL_BLEND);
	glDisable(GL_CULL_FACE);
//	glDisable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	hb_buffer_ = hb_buffer_create();
	if (hb_buffer_ == nullptr) { throw std::runtime_error("Error in creating harfbuzz buffer"); }
}

TextSpan::TextSpan(const TextSpan &that) {
	set_text(that.text_);
	set_font(that.font_);
	set_font_size(that.font_size_);
	set_position(that.position_);
	set_color(that.color_);
	animation_speed_ = that.animation_speed_;
	callback_ = that.callback_;
	is_visible_ = that.is_visible_;
	total_time_elapsed_ = that.total_time_elapsed_;
	visible_glyph_count_ = that.visible_glyph_count_;
}

TextSpan &TextSpan::operator=(const TextSpan &that) {
	if (this == &that) {
		// skip self assignment
		return *this;
	}
	undo_render();
	set_text(that.text_);
	set_font(that.font_);
	set_font_size(that.font_size_);
	set_position(that.position_);
	set_color(that.color_);
	animation_speed_ = that.animation_speed_;
	callback_ = that.callback_;
	is_visible_ = that.is_visible_;
	total_time_elapsed_ = that.total_time_elapsed_;
	visible_glyph_count_ = that.visible_glyph_count_;
	return *this;
}

TextSpan::~TextSpan() {
	undo_render();
	hb_buffer_destroy(hb_buffer_);
	hb_buffer_ = nullptr;

	glDeleteBuffers(1, &vbo_);
	glDeleteSamplers(1, &sampler_);
	glDeleteVertexArrays(1, &vao_);
}

void TextSpan::update(float elapsed) {
	if (is_visible_ && animation_speed_.has_value() && visible_glyph_count_ < glyph_count_) {
		// show "appear letters one by one" animation
		total_time_elapsed_ += elapsed;
		visible_glyph_count_ =
			std::min(static_cast<unsigned>(total_time_elapsed_ * animation_speed_.value()), glyph_count_);
		if (visible_glyph_count_ == glyph_count_ && callback_.has_value()) {
			(*callback_)();
		}
	}
}

void TextSpan::draw() {
	if (!is_visible_) { return; }
	do_render();
	// Bind Stuff
	GL_ERRORS();
	glBindSampler(0, sampler_);
	glBindVertexArray(vao_);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glUseProgram(program->program_);
	glm::vec4 color_fp = glm::vec4(color_) / 255.0f;
	glUniform4f(program->color_uniform_, color_fp.x, color_fp.y, color_fp.z, color_fp.w);
	glUniform1i(program->tex_uniform_, 0);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	GL_ERRORS();

	float cursor_x = cursor_.x, cursor_y = cursor_.y - float(font_size_) * 2.0f / ViewContext::get().logical_size_.y;

	GlyphTextureCache *cache = GlyphTextureCache::get_instance();

	size_t shown_glyph_count = animation_speed_.has_value() ? visible_glyph_count_ : glyph_count_;
	assert(shown_glyph_count <= glyph_count_);
	for (size_t i = 0; i < shown_glyph_count; ++i) {
		hb_codepoint_t glyphid = glyph_info_[i].codepoint;
		float x_offset = glyph_pos_[i].x_offset / 64.0f;
		float y_offset = glyph_pos_[i].y_offset / 64.0f;
		float x_advance = glyph_pos_[i].x_advance / 64.0f;
		float y_advance = glyph_pos_[i].y_advance / 64.0f;

		auto *glyph_texture = cache->getTextRef(font_, font_size_, glyphid);
		assert(glyph_texture != nullptr);

		const float vx = cursor_x + x_offset + glyph_texture->bitmap_left * scale_factor_.x;
		const float vy = cursor_y + y_offset + glyph_texture->bitmap_top * scale_factor_.y;
		const float w = glyph_texture->width * scale_factor_.x;
		const float h = glyph_texture->height * scale_factor_.y;

		struct {
			float x, y, s, t;
		} data[6] = {
			{vx, vy, 0, 0},
			{vx, vy - h, 0, 1},
			{vx + w, vy, 1, 0},
			{vx + w, vy, 1, 0},
			{vx, vy - h, 0, 1},
			{vx + w, vy - h, 1, 1}
		};

		glBindTexture(GL_TEXTURE_2D, glyph_texture->gl_texture_id);
		glActiveTexture(GL_TEXTURE0);
		glBufferData(GL_ARRAY_BUFFER, 24 * sizeof(float), data, GL_DYNAMIC_DRAW);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindTexture(GL_TEXTURE_2D, 0);

		cursor_x += x_advance * scale_factor_.x;
		cursor_y += y_advance * scale_factor_.y;
	}

//	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	GL_ERRORS();
}

void TextSpan::redo_shape() {
	undo_render();
	do_render();
}

TextSpan &TextSpan::set_text(std::string text) {
	if (this->text_ == text) { return *this; }
	undo_render();
	this->text_ = std::move(text);
	return *this;
}

TextSpan &TextSpan::set_font(FontFace font_face) {
	undo_render();
	font_ = font_face;
	return *this;
}

TextSpan &TextSpan::set_font_size(unsigned font_size) {
	undo_render();
	font_size_ = font_size;
	return *this;
}

TextSpan &TextSpan::set_position(int x, int y) {
	set_position(glm::ivec2(x, y));
	return *this;
}

TextSpan &TextSpan::set_position(glm::ivec2 pos) {
	position_ = pos;
	cursor_.x = 2.0f * float(position_.x) / float(ViewContext::get().logical_size_.x) - 1.0f;
	cursor_.y = -2.0f * float(position_.y) / float(ViewContext::get().logical_size_.y) + 1.0f;
	return *this;
}

TextSpan &TextSpan::set_color(glm::u8vec4 color) {
	color_ = color;
	return *this;
}

TextSpan &TextSpan::disable_animation() {
	animation_speed_ = std::nullopt;
	callback_ = std::nullopt;
	return *this;
}

TextSpan &TextSpan::set_animation(float speed, std::optional<std::function<void()>> callback) {
	if (speed <= 0.0f) {
		throw std::invalid_argument("appear_by_letter_speed must be either empty or a positive float");
	}
	animation_speed_ = speed;
	callback_ = callback;
	return *this;
}

TextSpan &TextSpan::set_visibility(bool value) {
	is_visible_ = value;
	return *this;
}

int TextSpan::get_width() {
	do_render();
	float width = 0.0f;
	for (size_t i = 0; i < glyph_count_; ++i) {
		float x_advance = glyph_pos_[i].x_advance / 64.0f;
		width += x_advance / ViewContext::get().scale_factor_;
	}
	return (int) lround(width);
}

void TextSpan::undo_render() {
	if (!text_is_rendered_) { return; }
	GlyphTextureCache *cache = GlyphTextureCache::get_instance();
	for (size_t i = 0; i < glyph_count_; ++i) {
		hb_codepoint_t glyphid = glyph_info_[i].codepoint;
		cache->decTexRef(font_, font_size_, glyphid);
	}
	hb_buffer_reset(hb_buffer_);
	glyph_count_ = 0;
	glyph_info_ = nullptr;
	glyph_pos_ = nullptr;
	text_is_rendered_ = false;
}

void TextSpan::do_render() {
	if (text_is_rendered_) { return; }
	GlyphTextureCache *cache = GlyphTextureCache::get_instance();

	// TODO(xiaoqiao): is there a better way to handle font size setting?
	FT_Face ft_face = cache->get_free_type_face(font_);
	if (FT_Set_Pixel_Sizes(ft_face, 0, ViewContext::compute_physical_px(font_size_)) != 0) {
		throw std::runtime_error("Error setting char size");
	}
	hb_font_t *hb_font = hb_ft_font_create_referenced(ft_face);
	assert(hb_font != nullptr);
	hb_buffer_reset(hb_buffer_);
	hb_buffer_add_utf8(hb_buffer_, text_.c_str(), -1, 0, (int)text_.size());
	hb_buffer_set_direction(hb_buffer_, HB_DIRECTION_LTR);
	hb_buffer_set_script(hb_buffer_, HB_SCRIPT_LATIN);
	hb_buffer_set_language(hb_buffer_, hb_language_from_string("en", -1));
	hb_shape(hb_font, hb_buffer_, nullptr, 0);
	glyph_info_ = hb_buffer_get_glyph_infos(hb_buffer_, &glyph_count_);
	glyph_pos_ = hb_buffer_get_glyph_positions(hb_buffer_, &glyph_count_);
	for (size_t i = 0; i < glyph_count_; ++i) {
		hb_codepoint_t glyphid = glyph_info_[i].codepoint;
		cache->incTexRef(font_, font_size_, glyphid);
	}
	hb_font_destroy(hb_font);
	text_is_rendered_ = true;
}

void TextBox::update(float elapsed) {
	for (auto &line : lines_) {
		line->update(elapsed);
	}
}

void TextBox::draw() {
	for (auto &line : lines_) {
		line->draw();
	}
}
TextBox &TextBox::set_contents(std::vector<std::pair<glm::u8vec4, std::string>> contents) {
	contents_ = std::move(contents);
	lines_.clear();
	for (size_t i = 0; i < contents_.size(); i++) {
		auto text_line_ptr = std::make_shared<TextSpan>();
		text_line_ptr->set_text(contents_.at(i).second)
			.set_color(contents_.at(i).first)
			.set_font(font_face_)
			.set_font_size(font_size_)
			.set_position(position_.x, position_.y + int(font_size_ + line_space_) * int(i));
		lines_.push_back(text_line_ptr);
	}
	return *this;
}

TextBox &TextBox::set_position(glm::ivec2 pos) {
	position_ = pos;
	for (size_t i = 0; i < lines_.size(); i++) {
		auto text_line_ptr = lines_.at(i);
		text_line_ptr->set_position(position_.x, position_.y + int(font_size_ * i + line_space_ * i));
	}
	return *this;
}

TextBox &TextBox::set_line_space(int line_space) {
	line_space_ = line_space;
	for (size_t i = 0; i < lines_.size(); i++) {
		auto text_line_ptr = lines_.at(i);
		text_line_ptr->set_position(position_.x, position_.y + int(font_size_ * i + line_space_ * i));
	}
	return *this;
}

TextBox &TextBox::set_font_size(unsigned font_size) {
	font_size_ = font_size;
	return *this;
}

TextBox &TextBox::set_font_face(FontFace font_face) {
	font_face_ = font_face;
	for (auto &text_line_ptr : lines_) {
		text_line_ptr->set_font(font_face);
	}
	return *this;
}

TextBox &TextBox::disable_animation() {
	animation_speed_ = std::nullopt;
	callback_ = std::nullopt;
	for (auto &text_line : lines_) {
		text_line->disable_animation();
	}
	return *this;
}

TextBox &TextBox::set_animation(float speed, std::optional<std::function<void()>> callback) {
	animation_speed_ = speed;
	callback_ = callback;
	for (size_t i = 0; i < lines_.size(); i++) {
		auto text_line_ptr = lines_.at(i);
		std::optional<std::function<void()>> line_callback;
		if (i + 1 < lines_.size()) {
			line_callback = [i, this]() { this->lines_.at(i + 1)->set_visibility(true); };
		} else {
			line_callback = callback;
		}
		text_line_ptr->set_animation(speed, line_callback);
	}
	return *this;
}

TextBox &TextBox::show() {
	if (animation_speed_.has_value()) {
		if (!lines_.empty()) {
			lines_.at(0)->set_visibility(true);
			for (size_t i = 1; i < lines_.size(); i++) {
				lines_.at(i)->set_visibility(false);
			}
		}
	} else {
		for (auto &line : lines_) {
			line->set_visibility(true);
		}
	}
	return *this;
}

Rectangle::Rectangle(glm::ivec2 position, glm::ivec2 size)
	: visibility_{true},
	position_{position},
	size_{size} {}

void Rectangle::set_visibility(bool value) { visibility_ = value; }
void Rectangle::set_position(glm::ivec2 position) { position_ = position; }
void Rectangle::set_size(glm::ivec2 size) { size_ = size; }
//void Rectangle::draw() {
//	if (!visibility_) { return; }
//	glEnable(GL_BLEND);
//	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//	//don't use the depth test:
//	glDisable(GL_DEPTH_TEST);
//
//	//upload vertices to vertex_buffer:
//	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer); //set vertex_buffer as current
//	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STREAM_DRAW); //upload vertices array
//	glBindBuffer(GL_ARRAY_BUFFER, 0);
//
//	//set color_texture_program as current program:
//	glUseProgram(color_texture_program->program);
//
//	//upload OBJECT_TO_CLIP to the proper uniform location:
//	glUniformMatrix4fv(color_texture_program->OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(court_to_clip));
//
//	//use the mapping vertex_buffer_for_color_texture_program to fetch vertex data:
//	glBindVertexArray(vertex_buffer_for_color_texture_program);
//
//	//bind the solid white texture to location zero so things will be drawn just with their colors:
//	glActiveTexture(GL_TEXTURE0);
//	glBindTexture(GL_TEXTURE_2D, white_tex);
//
//	//run the OpenGL pipeline:
//	glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));
//
//	//unbind the solid white texture:
//	glBindTexture(GL_TEXTURE_2D, 0);
//
//	//reset vertex array to none:
//	glBindVertexArray(0);
//
//	//reset current program to none:
//	glUseProgram(0);
//
//}

}