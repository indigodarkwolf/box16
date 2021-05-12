#include "sdl_events.h"

#include <SDL.h>

#include "display.h"
#include "glue.h"
#include "imgui/imgui_impl_sdl.h"
#include "joystick.h"
#include "keyboard.h"
#include "ps2.h"
#include "vera/sdcard.h"

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
				if (cmd_down) {
					if (event.key.keysym.sym == SDLK_s) {
						machine_dump();
						consumed = true;
					} else if (event.key.keysym.sym == SDLK_r) {
						machine_reset();
						consumed = true;
					} else if (event.key.keysym.sym == SDLK_v) {
						keyboard_add_text(SDL_GetClipboardText());
						consumed = true;
					} else if (event.key.keysym.sym == SDLK_f || event.key.keysym.sym == SDLK_RETURN) {
						display_toggle_fullscreen();
						consumed = true;
					} else if (event.key.keysym.sym == SDLK_PLUS || event.key.keysym.sym == SDLK_EQUALS) {
						machine_toggle_warp();
						consumed = true;
					} else if (event.key.keysym.sym == SDLK_a) {
						sdcard_attach();
						consumed = true;
					} else if (event.key.keysym.sym == SDLK_d) {
						sdcard_detach();
						consumed = true;
					}
				}
				if (!consumed) {
					if (event.key.keysym.scancode == LSHORTCUT_KEY || event.key.keysym.scancode == RSHORTCUT_KEY) {
						cmd_down = true;
					}
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