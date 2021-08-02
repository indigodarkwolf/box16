#include "sdl_events.h"

#include <SDL.h>

#include "debugger.h"
#include "display.h"
#include "glue.h"
#include "imgui/imgui_impl_sdl.h"
#include "overlay/overlay.h"
#include "joystick.h"
#include "keyboard.h"
#include "ps2.h"
#include "vera/sdcard.h"
#include "options.h"

#ifdef __APPLE__
#	define LSHORTCUT_KEY SDL_SCANCODE_LGUI
#	define RSHORTCUT_KEY SDL_SCANCODE_RGUI
#else
#	define LSHORTCUT_KEY SDL_SCANCODE_LCTRL
#	define RSHORTCUT_KEY SDL_SCANCODE_RCTRL
#endif

bool sdl_events_update()
{
	static bool cmd_down = false;

	bool mouse_state_change = false;

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				return false;

			case SDL_JOYDEVICEADDED:
				joystick_add(event.jdevice.which);
				break;

			case SDL_JOYDEVICEREMOVED:
				joystick_remove(event.jdevice.which);
				break;

			default:
				break;
		}

		ImGui_ImplSDL2_ProcessEvent(&event);

		if (ImGui::GetIO().WantCaptureMouse) {
			return true;
		}
		if (ImGui::GetIO().WantCaptureKeyboard) {
			return true;
		}
		if (ImGui::GetIO().WantTextInput) {
			return true;
		}

		switch (event.type) {
			case SDL_CONTROLLERBUTTONDOWN:
				joystick_button_down(event.cbutton.which, event.cbutton.button);
				break;

			case SDL_CONTROLLERBUTTONUP:
				joystick_button_up(event.cbutton.which, event.cbutton.button);
				break;

			case SDL_KEYDOWN: {
				bool consumed = false;
				if (event.key.keysym.sym == SDLK_F12) {
					Show_cpu_monitor = true;
					debugger_pause_execution();
				}
				if (!Options.no_keybinds) {
					if (cmd_down) {
						switch (event.key.keysym.sym) {
							case SDLK_s:
								machine_dump();
								consumed = true;
								break;
							case SDLK_r:
								machine_reset();
								consumed = true;
								break;
							case SDLK_v:
								keyboard_add_text(SDL_GetClipboardText());
								consumed = true;
								break;
							case SDLK_f:
							case SDLK_RETURN:
								display_toggle_fullscreen();
								consumed = true;
								break;
							case SDLK_PLUS:
							case SDLK_EQUALS:
								machine_toggle_warp();
								consumed = true;
								break;
							case SDLK_a:
								sdcard_attach();
								consumed = true;
								break;
							case SDLK_d:
								sdcard_detach();
								consumed = true;
								break;
						}
					}
					if (event.key.keysym.scancode == LSHORTCUT_KEY || event.key.keysym.scancode == RSHORTCUT_KEY) {
						cmd_down = true;
					}
				}
				if (!consumed) {
					keyboard_add_event(true, event.key.keysym.scancode);
				}
				break;
			}

			case SDL_KEYUP:
				if (event.key.keysym.scancode == LSHORTCUT_KEY || event.key.keysym.scancode == RSHORTCUT_KEY) {
					cmd_down = false;
				}
				keyboard_add_event(false, event.key.keysym.scancode);
				break;

			case SDL_MOUSEBUTTONDOWN:
				mouse_state_change = true;
				switch (event.button.button) {
					case SDL_BUTTON_LEFT:
						mouse_button_down(0);
						break;
					case SDL_BUTTON_RIGHT:
						mouse_button_down(1);
						break;
					case SDL_BUTTON_MIDDLE:
						mouse_button_down(2);
						break;
				}
				break;

			case SDL_MOUSEBUTTONUP:
				mouse_state_change = true;
				switch (event.button.button) {
					case SDL_BUTTON_LEFT:
						mouse_button_up(0);
						break;
					case SDL_BUTTON_RIGHT:
						mouse_button_up(1);
						break;
					case SDL_BUTTON_MIDDLE:
						mouse_button_up(2);
						break;
				}
				break;
			
			// Stub for mouse wheel support on emu side.
			// does nothing just yet
			case SDL_MOUSEWHEEL:
				//mouse_state_change = true; // uncomment when code activated
				if(event.wheel.y != 0)
				{
					 // mouse Z axis change
				}
				if(event.wheel.x != 0)
				{
					 // mouse W axis change
				}
				break;

			case SDL_MOUSEMOTION: {
				mouse_state_change = true;
				static int mouse_x;
				static int mouse_y;
				mouse_move(event.motion.x - mouse_x, event.motion.y - mouse_y);
				mouse_x = event.motion.x;
				mouse_y = event.motion.y;
			} break;

			default:
				break;
		}
	}

	if (mouse_state_change) {
		mouse_send_state();
	}
	return true;
}
