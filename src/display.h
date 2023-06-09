#pragma once

#include "imgui/imgui.h"

#include <glad/gl.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <filesystem>
#include <tuple>

#if SDL_MAJOR_VERSION <= 2 && SDL_MINOR_VERSION <= 0 && SDL_PATCHLEVEL <= 9
struct SDL_FRect {
	float x;
	float y;
	float w;
	float h;
};
#endif

class icon_set
{
public:
	bool load_file(const char *filename, int width, int height);
	bool load_memory(const void *buffer, int texture_width, int texture_height, int icon_width, int icon_height);
	void update_memory(const void *buffer);
	void unload();

	ImVec2                     get_top_left(int id);
	ImVec2                     get_bottom_right(int id);
	std::tuple<ImVec2, ImVec2> get_imvec2_corners(int id);
	SDL_FRect                  get_sdl_rect(int id);

	uint32_t get_texture_id();
	void     draw(int id, int x, int y, int w, int h, SDL_Color color);

private:
	uint32_t texture        = 0;
	int      texture_width  = 0;
	int      texture_height = 0;
	float    tile_uv_width  = 0.0f;
	float    tile_uv_height = 0.0f;
	int      map_width      = 0;
	int      map_height     = 0;
};

bool  display_init();
void  display_shutdown();
void  display_process();
float display_get_fps();
void  display_refund_render_time(uint32_t time_us);
void  display_video();

const float    display_get_aspect_ratio();
SDL_Window *   display_get_window();
const ImVec4 & display_get_rect();

void display_toggle_fullscreen();

enum display_icons {
	ICON_STOP = 0,
	ICON_RUN,
	ICON_PAUSE,
	ICON_STEP_OVER,
	ICON_STEP_INTO,
	ICON_STEP_OUT,
	ICON_REMOVE,
	ICON_WATCH,
	ICON_UNCHECKED,
	ICON_CHECKED,
	ICON_CHECK_UNCERTAIN,
	ICON_RETURN_TO_PC,
	ICON_ACTIVITY_LED_ON,
	ICON_ADD_BREAKPOINT,
	ICON_POWER_LED_ON,

	ICON_STOP_DISABLED = 16,
	ICON_RUN_DISABLED,
	ICON_PAUSE_DISABLED,
	ICON_STEP_OVER_DISABLED,
	ICON_STEP_INTO_DISABLED,
	ICON_STEP_OUT_DISABLED,
	ICON_REMOVE_DISABLED,
	ICON_WATCH_DISABLED,
	ICON_UNCHECKED_DISABLED,
	ICON_CHECKED_DISABLED,
	ICON_CHECK_UNCERTAIN_DISABLED,
	ICON_RETURN_TO_PC_DISABLED,
	ICON_ACTIVITY_LED_OFF,
	ICON_ADD_BREAKPOINT_DISABLED,
	ICON_POWER_LED_OFF,

	ICON_FM_ALG = 32
};

namespace ImGui
{
	bool TileButton(display_icons icon, bool enabled = true, bool *hovered = nullptr);
	void Tile(display_icons icon, float alpha = 1.0f);
	void Tile(display_icons icon, ImVec2 size, float alpha = 1.0f);
	void TileDisabled(display_icons icon);
	bool InputLog2(char const *label, uint8_t *value, const char *format, ImGuiInputTextFlags flags = 0);
	bool InputPow2(char const *label, int *value, const char *format, ImGuiInputTextFlags flags = 0);
	bool InputText(const char *label, std::filesystem::path &path, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void *user_data = nullptr);
	bool InputText(const char *label, std::string &str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void *user_data = nullptr);
} // namespace ImGui
