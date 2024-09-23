#pragma once
#if !defined(KEYBOARD_H)
#	define KEYBOARD_H

// Commander X16 Emulator
// Copyright (c) 2021-2023 Stephen Horn, et al.
// All rights reserved. License: 2-clause BSD

#	include <SDL_keycode.h>
#include<filesystem>

void keyboard_process();

void keyboard_add_event(const bool down, const SDL_Scancode scancode);
void keyboard_add_text(const std::string &text);
void keyboard_add_file(const std::filesystem::path &path);

uint8_t keyboard_get_next_byte();

// fake mouse
void    mouse_button_down(int num);
void    mouse_button_up(int num);
void    mouse_move(int x, int y);
uint8_t mouse_read(uint8_t reg);
void    mouse_send_state();

uint8_t mouse_get_next_byte();

#endif