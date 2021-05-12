#pragma once

#include <SDL_keycode.h>

void keyboard_process();

void keyboard_add_event(const bool down, const SDL_Scancode scancode);
void keyboard_add_text(char const *const text);
void keyboard_add_file(char const *const path);

