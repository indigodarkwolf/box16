#pragma once

#include <SDL.h>
#include "imgui/imgui.h"

struct display_settings {
	SDL_Rect window_rect;
	SDL_Rect video_rect;
};

bool display_init(const display_settings &);
void display_shutdown();
void display_process();

const display_settings &display_get_settings();

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
};

namespace ImGui
{
	bool TileButton(display_icons icon, bool enabled = true, bool *hovered = nullptr);
	void Tile(display_icons icon, float alpha = 1.0f);
	void TileDisabled(display_icons icon);
}; // namespace ImGui