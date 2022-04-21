/*

MIT License

Copyright (c) 2021 Stephen Horn


Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include <GL/glew.h>
#include <atomic>
#include <thread>

#include "display.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl2.h"
#include "imgui/imgui_impl_sdl.h"
#include "lodepng.h"
#include "memory.h"
#include "options.h"
#include "overlay/overlay.h"
#include "timing.h"
#include "vera/vera_video.h"
#include "version.h"

static display_settings Display;

static SDL_Window *  Display_window = nullptr;
static SDL_GLContext Display_context;

static bool Fullscreen = false;

static GLuint Display_framebuffer_handle;
static GLuint Display_framebuffer_texture_handle;

static GLuint Video_framebuffer_texture_handle;
static GLuint Icon_tilemap;

static GLsync Render_complete = 0;
static uint32_t Last_render_time = 0;

static bool Initd_sdl_image           = false;
static bool Initd_sdl_gl              = false;
static bool Initd_display_context     = false;
static bool Initd_glew                = false;
static bool Initd_display_framebuffer = false;
static bool Initd_video_framebuffer   = false;
static bool Initd_imgui               = false;
static bool Initd_imgui_sdl2          = false;
static bool Initd_imgui_opengl        = false;
static bool Initd_appicon             = false;
static bool Initd_icons               = false;

#if defined(GL_EXT_texture_filter_anisotropic)
static float Max_anisotropy = 1.0f;
#endif

bool icon_set::load_file(const char *filename, int icon_width, int icon_height)
{
	if (texture != 0) {
		unload();
	}

	std::vector<unsigned char> icons_buf;
	unsigned                   icons_w;
	unsigned                   icons_h;
	if (lodepng::decode(icons_buf, icons_w, icons_h, filename, LCT_RGBA) != 0) {
		printf("Unable to load file %s\n", filename);
		return false;
	}
	SDL_Surface *icons = SDL_CreateRGBSurfaceWithFormatFrom(icons_buf.data(), icons_w, icons_h, 32, icons_w * 4, SDL_PIXELFORMAT_RGBA8888);

	int mode = GL_RGB;
	if (icons->format->BytesPerPixel == 4) {
		mode = GL_RGBA;
	}
	texture_width            = icons->w;
	texture_height           = icons->h;
	map_width                = icons->w / icon_width;
	map_height               = icons->h / icon_height;
	const float map_width_f  = (float)map_width;
	const float map_height_f = (float)map_height;
	tile_uv_width            = 1.0f / map_width_f;
	tile_uv_height           = 1.0f / map_height_f;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, icons->w, icons->h, 0, mode, GL_UNSIGNED_BYTE, icons->pixels);

	// free the surface since we now have a texture
	SDL_FreeSurface(icons);

	return true;
}

bool icon_set::load_memory(const void *buffer, int texture_width, int texture_height, int icon_width, int icon_height)
{
	if (texture != 0) {
		unload();
	}

	this->texture_width      = texture_width;
	this->texture_height     = texture_height;
	map_width                = texture_width / icon_width;
	map_height               = texture_height / icon_height;
	const float map_width_f  = (float)texture_width / (float)icon_width;
	const float map_height_f = (float)texture_height / (float)icon_height;
	tile_uv_width            = 1.0f / map_width_f;
	tile_uv_height           = 1.0f / map_height_f;

	int mode = GL_RGBA;

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_width, texture_height, 0, mode, GL_UNSIGNED_INT_8_8_8_8, buffer);

	return true;
}

void icon_set::update_memory(const void *buffer)
{
	glBindTexture(GL_TEXTURE_2D, texture);
	glTextureSubImage2D(texture, 0, 0, 0, texture_width, texture_height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, buffer);
}

void icon_set::unload()
{
	glDeleteTextures(1, &texture);
	texture = 0;
}

ImVec2 icon_set::get_top_left(int id)
{
	return { (float)(id % map_width) * tile_uv_width, (float)(id / map_width) * tile_uv_height };
}

ImVec2 icon_set::get_bottom_right(int id)
{
	const ImVec2 topleft = get_top_left(id);
	return { topleft.x + tile_uv_width, topleft.y + tile_uv_height };
}

std::tuple<ImVec2, ImVec2> icon_set::get_imvec2_corners(int id)
{
	const ImVec2 topleft = get_top_left(id);
	return { { topleft }, { topleft.x + tile_uv_width, topleft.y + tile_uv_height } };
}

SDL_FRect icon_set::get_sdl_rect(int id)
{
	return { (float)(id % map_width) * tile_uv_width, (float)(id / map_width) * tile_uv_height, tile_uv_width, tile_uv_height };
}

uint32_t icon_set::get_texture_id()
{
	return texture;
}

void icon_set::draw(int id, int x, int y, int w, int h, SDL_Color color)
{
	ImVec2 topleft{ (float)(id % map_width) * tile_uv_width, (float)(id / tile_uv_height) * tile_uv_height };
	ImVec2 botright{ topleft.x + tile_uv_width, topleft.y + tile_uv_height };

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glBindTexture(GL_TEXTURE_2D, Icon_tilemap);
	glColor4f(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
	glBegin(GL_QUADS);
	glTexCoord2f(topleft.x, topleft.y);
	glVertex2i(x, y + h);
	glTexCoord2f(botright.x, topleft.y);
	glVertex2i(x + w, y + h);
	glTexCoord2f(botright.x, botright.y);
	glVertex2i(x + w, y);
	glTexCoord2f(topleft.x, botright.y);
	glVertex2i(x, y);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_BLEND);
}

static void display_video()
{
	if (!vera_video_is_cheat_frame()) {
		const uint8_t *video_buffer = vera_video_get_framebuffer();
		glTextureSubImage2D(Video_framebuffer_texture_handle, 0, 0, 0, Display.video_rect.w, Display.video_rect.h, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, video_buffer);
		if (Options.scale_quality == scale_quality_t::BEST) {
			glGenerateTextureMipmap(Video_framebuffer_texture_handle);
		}
		GLenum result = glGetError();
		if (result != GL_NO_ERROR) {
			printf("GL error %s\n", glewGetErrorString(result));
		}
	}

	SDL_GetWindowSize(Display_window, &Display.window_rect.w, &Display.window_rect.h);
	SDL_Rect client_rect = Display.window_rect;
	client_rect.h -= IMGUI_OVERLAY_MENU_BAR_HEIGHT;
	client_rect.x = 0;
	client_rect.y = 0;

	SDL_Rect video_rect = client_rect;
	float    ratio      = ((float)client_rect.w / (float)client_rect.h) / (640.0f / 480.0f);
	if (ratio > 1.0f) {
		video_rect.w = (int)(video_rect.w / ratio);
		video_rect.x = (client_rect.w - video_rect.w) / 2;
	} else {
		video_rect.h = (int)(video_rect.h * ratio);
		video_rect.y = (client_rect.h - video_rect.h) / 2;
	}

	GLint filter = []() {
		switch (Options.scale_quality) {
			case scale_quality_t::NEAREST: return GL_NEAREST;
			case scale_quality_t::LINEAR: return GL_LINEAR;
			case scale_quality_t::BEST: return GL_LINEAR_MIPMAP_LINEAR;
			default: return GL_NEAREST;
		}
	}();
	glBindTexture(GL_TEXTURE_2D, Video_framebuffer_texture_handle);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (Options.scale_quality == scale_quality_t::NEAREST) ? GL_NEAREST : GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if defined(GL_EXT_texture_filter_anisotropic)
	if(Max_anisotropy > 0) {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, (Options.scale_quality == scale_quality_t::BEST) ? Max_anisotropy : 1.0f);
	}
#endif
	glColor3f(1.0f, 1.0f, 1.0f);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f, 0.0f);
	glVertex2i(video_rect.x, video_rect.y + video_rect.h);
	glTexCoord2f(1.0f, 0.0f);
	glVertex2i(video_rect.x + video_rect.w, video_rect.y + video_rect.h);
	glTexCoord2f(1.0f, 1.0f);
	glVertex2i(video_rect.x + video_rect.w, video_rect.y);
	glTexCoord2f(0.0f, 1.0f);
	glVertex2i(video_rect.x, video_rect.y);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);
}

bool display_init(const display_settings &settings)
{
	Display = settings;

	if (Display.window_rect.w == 0) {
		Display.window_rect.w = Display.video_rect.w;
	}

	if (Display.window_rect.h == 0) {
		Display.window_rect.h = Display.video_rect.h + 10; // Account for menu
	}

	// Initialize SDL_GL
	{
		uint32_t sdl_window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;

#ifdef __EMSCRIPTEN__
		// Setting this flag would render the web canvas outside of its bounds on high dpi screens
		sdl_window_flags &= ~SDL_WINDOW_ALLOW_HIGHDPI;
#endif

		if (SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) < 0) {
			printf("Unable to set SDL GL attribute SDL_GL_DOUBLEBUFFER: %s\n", SDL_GetError());
			return false;
		}
		if (SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32) < 0) {
			printf("Unable to set SDL GL attribute SDL_GL_BUFFER_SIZE: %s\n", SDL_GetError());
			return false;
		}
		if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2) < 0) {
			printf("Unable to set SDL GL attribute SDL_GL_CONTEXT_MAJOR_VERSION: %s\n", SDL_GetError());
			return false;
		}
		if (SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2) < 0) {
			printf("Unable to set SDL GL attribute SDL_GL_CONTEXT_MINOR_VERSION: %s\n", SDL_GetError());
			return false;
		}

		char title[128];
#if defined(WIN32)
		sprintf_s(title, "%s %s (%s)", VER_TITLE, VER_NUM, VER_NAME);
#else
		sprintf(title, "%s %s (%s)", VER_TITLE, VER_NUM, VER_NAME);
#endif

		Display_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, Display.window_rect.w, Display.window_rect.h, sdl_window_flags);
		if (Display_window == nullptr) {
			printf("Unable to create SDL window: %s\n", SDL_GetError());
			return false;
		}

		// SDL_SetWindowTitle(Display_window, "Box16 Emulator for Commander X16");
	}
	Initd_sdl_gl = true;

	// Initialize context
	{
		Display_context = SDL_GL_CreateContext(Display_window);

		if (SDL_GL_MakeCurrent(Display_window, Display_context) < 0) {
			printf("Create display context (SDL_GL_MakeCurrent): %s\n", SDL_GetError());
			return false;
		}

		if (SDL_GL_SetSwapInterval(1) < 0) {
			printf("Create display context (SDL_GL_SetSwapInterval): %s\n", SDL_GetError());
			return false;
		}
	}
	Initd_display_context = true;

	// Initialize GLEW
	{
		glewExperimental = GL_TRUE;

		GLenum result = glewInit();
		if (result != GLEW_OK) {
			printf("Unable to initialize GL: %s\n", glewGetErrorString(result));
			return false;
		}
	}
	Initd_glew = true;

#if defined(GL_EXT_texture_filter_anisotropic)
	if(glewIsSupported("EXT_texture_filter_anisotropic")) {
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &Max_anisotropy);
	} else {
		Max_anisotropy = 0;
	}
#endif
	// Initialize display framebuffer
	{
		glGenFramebuffers(1, &Display_framebuffer_handle);
		glBindFramebuffer(GL_FRAMEBUFFER, Display_framebuffer_handle);
		glGenTextures(1, &Display_framebuffer_texture_handle);
		glBindTexture(GL_TEXTURE_2D, Display_framebuffer_texture_handle);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Display.video_rect.w, Display.video_rect.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// attach texture to framebuffer
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Display_framebuffer_texture_handle, 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			printf("Unable to create framebuffer for render to texture: %d.\n", glCheckFramebufferStatus(GL_FRAMEBUFFER));
			return false;
		}
	}
	Initd_display_framebuffer = true;

	// Initialize video framebuffer
	{
		glGenTextures(1, &Video_framebuffer_texture_handle);
		glBindTexture(GL_TEXTURE_2D, Video_framebuffer_texture_handle);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Display.video_rect.w, Display.video_rect.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// attach texture to framebuffer
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Video_framebuffer_texture_handle, 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			printf("Unable to create framebuffer for render to texture.\n");
			return false;
		}
	}
	Initd_video_framebuffer = true;

	// Initialize ImGUI
	{
		IMGUI_CHECKVERSION();
		if (ImGui::CreateContext() == nullptr) {
			printf("Unable to create ImGui context\n");
			return false;
		}

		ImGuiIO &io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();
	}
	Initd_imgui = true;

	if (!ImGui_ImplSDL2_InitForOpenGL(Display_window, Display_context)) {
		printf("Unable to init ImGui SDL2\n");
		return false;
	}
	Initd_imgui_sdl2 = true;

	if (!ImGui_ImplOpenGL2_Init()) {
		printf("Unable to init ImGui OpenGL\n");
		return false;
	}
	Initd_imgui_opengl = true;

	// Load app icon
	{
		std::vector<unsigned char> icons_buf;
		unsigned                   icons_w;
		unsigned                   icons_h;

		std::filesystem::path icons_path;
		options_get_base_path(icons_path, "box16-icon56-24.png");

		if (lodepng::decode(icons_buf, icons_w, icons_h, icons_path.generic_string(), LCT_RGB) != 0) {
			printf("Unable to load icon resources from %s\n", icons_path.generic_string().c_str());
			return false;
		}

		SDL_Surface *icon = SDL_CreateRGBSurfaceWithFormatFrom(icons_buf.data(), icons_w, icons_h, 24, icons_w * 3, SDL_PIXELFORMAT_RGB24);
		SDL_SetWindowIcon(Display_window, icon);
	}
	Initd_appicon = true;

	// Load icons
	{
		std::vector<unsigned char> icons_buf;
		unsigned                   icons_w;
		unsigned                   icons_h;

		std::filesystem::path icons_path;
		options_get_base_path(icons_path, "icons.png");

		if (lodepng::decode(icons_buf, icons_w, icons_h, icons_path.generic_string(), LCT_RGBA) != 0) {
			printf("Unable to load icon resources from %s\n", icons_path.generic_string().c_str());
			return false;
		}
		SDL_Surface *icons = SDL_CreateRGBSurfaceWithFormatFrom(icons_buf.data(), icons_w, icons_h, 32, icons_w * 4, SDL_PIXELFORMAT_RGBA8888);

		int mode = GL_RGB;
		if (icons->format->BytesPerPixel == 4) {
			mode = GL_RGBA;
		}

		glGenTextures(1, &Icon_tilemap);
		glBindTexture(GL_TEXTURE_2D, Icon_tilemap);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, icons->w, icons->h, 0, mode, GL_UNSIGNED_BYTE, icons->pixels);

		// free the surface since we now have a texture
		SDL_FreeSurface(icons);
	}
	Initd_icons = true;

	SDL_ShowCursor(SDL_DISABLE);

	if (Options.vsync_mode == vsync_mode_t::VSYNC_MODE_GET_SYNC || Options.vsync_mode == vsync_mode_t::VSYNC_MODE_WAIT_SYNC)
		Render_complete = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

	return true;
}

void display_shutdown()
{
	if (Initd_imgui_opengl)
		ImGui_ImplOpenGL2_Shutdown();

	if (Initd_imgui_sdl2)
		ImGui_ImplSDL2_Shutdown();

	if (Initd_imgui)
		ImGui::DestroyContext();

	if (Initd_display_context)
		SDL_GL_DeleteContext(Display_context);

	if (Initd_sdl_gl)
		SDL_DestroyWindow(Display_window);

	Initd_sdl_image           = false;
	Initd_sdl_gl              = false;
	Initd_display_context     = false;
	Initd_glew                = false;
	Initd_display_framebuffer = false;
	Initd_imgui               = false;
	Initd_imgui_sdl2          = false;
	Initd_imgui_opengl        = false;
}

void display_process()
{
	uint32_t current_render_time = timing_total_microseconds();
	if (Options.vsync_mode != vsync_mode_t::VSYNC_MODE_NONE && current_render_time - Last_render_time > 5000000) {
		// Seems like vsync isn't working, let's disable it.
		SDL_ShowSimpleMessageBox(SDL_MessageBoxFlags::SDL_MESSAGEBOX_WARNING, "V-Sync was automatically disabled", "Box16 has detected a problem with the current V-Sync settings.\nV-Sync has been disabled.", display_get_window());
		Options.vsync_mode = vsync_mode_t::VSYNC_MODE_NONE;
	}

	if (Render_complete != 0) {
		switch (Options.vsync_mode) {
			case vsync_mode_t::VSYNC_MODE_NONE:
				// Handle asynchronous vsync disable
				glDeleteSync(Render_complete);
				Render_complete = 0;
				break;

			case vsync_mode_t::VSYNC_MODE_GET_SYNC: {
				GLsizei num_sync_values;
				GLint   sync_status;
				glGetSynciv(Render_complete, GL_SYNC_STATUS, sizeof(sync_status), &num_sync_values, &sync_status);

				if (num_sync_values != 0 && sync_status == GL_UNSIGNALED) {
					return;
				}

				glDeleteSync(Render_complete);
				Render_complete = 0;
				break;
			}
			case vsync_mode_t::VSYNC_MODE_WAIT_SYNC:
				if (glClientWaitSync(Render_complete, 0, 0) == GL_TIMEOUT_EXPIRED) {
					return;
				}

				glDeleteSync(Render_complete);
				Render_complete = 0;
				break;
			case vsync_mode_t::VSYNC_MODE_DEBUG:
				return;
		}
	}

	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplSDL2_NewFrame(Display_window);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, Display.window_rect.w, Display.window_rect.h);
	glClearColor(0.5f, 0.5f, 0.5f, 0);
	glClear(GL_COLOR_BUFFER_BIT);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(false);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0f, (float)Display.window_rect.w, 0.0f, (float)Display.window_rect.h, 0.0f, 1.0f);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glEnable(GL_TEXTURE_2D);

	display_video();

	// back to main framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, Display.window_rect.w, Display.window_rect.h);
	glOrtho(0.0f, (float)Display.window_rect.w, (float)Display.window_rect.h, 0.0f, 0.0f, 1.0f);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	ImGui::NewFrame();

	overlay_draw();

	ImGui::Render();

	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
	SDL_GL_SwapWindow(Display_window);

	if (Options.vsync_mode != vsync_mode_t::VSYNC_MODE_NONE) {
		Render_complete = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		if (Render_complete == 0) {
			printf("Error: glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0) returned 0, V-Sync is probably not supported by this system's drivers.\n");
		}
	}

	Last_render_time = current_render_time;
}

const display_settings &display_get_settings()
{
	return Display;
}

SDL_Window *display_get_window()
{
	return Display_window;
}

void display_toggle_fullscreen()
{
	Fullscreen = !Fullscreen;
	SDL_SetWindowFullscreen(Display_window, Fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
}

namespace ImGui
{
	bool TileButton(display_icons icon, bool enabled, bool *hovered)
	{
		ImVec2 topleft{ (float)((int)icon % 16) / 16.0f, (float)((int)icon >> 4) / 16.0f };
		ImVec2 botright{ topleft.x + 1.0f / 16.0f, topleft.y + 1.0f / 16.0f };

		ImVec4 tint = [&]() {
			if (!enabled) {
				return GetStyleColorVec4(ImGuiCol_TextDisabled);
			}
			if (hovered != nullptr && !(*hovered)) {
				return ImVec4(0.9f, 0.9f, 0.9f, 0.9f);
			}
			return ImVec4(1, 1, 1, 1);
		}();

		PushID(icon);
		bool result = [&]() {
			if (enabled) {
				return ImageButton((void *)(intptr_t)Icon_tilemap, ImVec2(16.0f, 16.0f), topleft, botright, 0, ImVec4(0, 0, 0, 0), tint);
			} else {
				Image((void *)(intptr_t)Icon_tilemap, ImVec2(16.0f, 16.0f), topleft, botright, tint);
				return false;
			}
		}();
		if (hovered != nullptr) {
			*hovered = IsItemHovered();
		}
		PopID();
		return enabled && result;
	}

	void Tile(display_icons icon, float alpha /* = 1.0f */)
	{
		ImVec2 topleft{ (float)((int)icon % 16) / 16.0f, (float)((int)icon >> 4) / 16.0f };
		ImVec2 botright{ topleft.x + 1.0f / 16.0f, topleft.y + 1.0f / 16.0f };
		Image((void *)(intptr_t)Icon_tilemap, ImVec2(16.0f, 16.0f), topleft, botright, ImVec4(1, 1, 1, alpha));
	}

	void Tile(display_icons icon, ImVec2 size, float alpha /* = 1.0f */)
	{
		ImVec2 topleft{ (float)((int)icon % 16) / 16.0f, (float)((int)icon >> 4) / 16.0f };
		ImVec2 botright{ topleft.x + size.x / 256.0f, topleft.y + size.y / 256.0f };
		Image((void *)(intptr_t)Icon_tilemap, size, topleft, botright, ImVec4(1, 1, 1, alpha));
	}

	void TileDisabled(display_icons icon)
	{
		ImVec2 topleft{ (float)((int)icon % 16) / 16.0f, (float)((int)icon >> 4) / 16.0f };
		ImVec2 botright{ topleft.x + 1.0f / 16.0f, topleft.y + 1.0f / 16.0f };
		ImVec4 tint = GetStyleColorVec4(ImGuiCol_TextDisabled);
		Image((void *)(intptr_t)Icon_tilemap, ImVec2(16.0f, 16.0f), topleft, botright, tint);
	}

	bool InputLog2(char const *label, uint8_t *value, const char *format, ImGuiInputTextFlags flags)
	{
		static const uint32_t incr_one = 1;
		const uint32_t        original = 1 << *value;
		uint32_t              input    = 1 << *value;

		bool result = ImGui::InputScalar(label, ImGuiDataType_U32, &input, &incr_one, nullptr, format, flags);
		if (result) {
			if (input > original) {
				*value += 1;
			} else if (input < original) {
				*value -= 1;
			}
		}
		return result;
	}

	bool InputPow2(char const *label, int *value, const char *format, ImGuiInputTextFlags flags)
	{
		static const uint32_t incr_one = 1;
		const uint32_t        original = *value;
		uint32_t              input    = *value;

		bool result = ImGui::InputScalar(label, ImGuiDataType_U32, &input, &incr_one, nullptr, format, flags);
		if (result) {
			if (input > original) {
				*value <<= 1;
			} else if (input < original) {
				*value >>= 1;
			}
		}
		return result;
	}

	bool InputText(const char *label, std::filesystem::path &path, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void *user_data)
	{
		char input_buffer[PATH_MAX];
		strncpy(input_buffer, path.generic_string().c_str(), PATH_MAX);
		input_buffer[PATH_MAX - 1] = '\0';
		if (InputText(label, input_buffer, PATH_MAX, flags, callback, user_data)) {
			path = input_buffer;
			return true;
		}
		return false;
	}

	bool InputText(const char *label, std::string &str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void *user_data)
	{
		char input_buffer[PATH_MAX];
		strncpy(input_buffer, str.c_str(), PATH_MAX);
		input_buffer[PATH_MAX - 1] = '\0';
		if (InputText(label, input_buffer, PATH_MAX, flags, callback, user_data)) {
			str = input_buffer;
			return true;
		}
		return false;
	}

} // namespace ImGui