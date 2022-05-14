#pragma once
#if !defined(KEYBOARD_H)
#	define KEYBOARD_H

// Commander X16 Emulator
// Copyright (c) 2021-2022 Stephen Horn, et al.
// All rights reserved. License: 2-clause BSD

#include <SDL_keycode.h>

void keyboard_process();

void keyboard_add_event(const bool down, const SDL_Scancode scancode);
void keyboard_add_text(char const *const text);
void keyboard_add_file(char const *const path);

#endif