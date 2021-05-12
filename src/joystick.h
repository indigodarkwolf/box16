#pragma once
#ifndef JOYSTICK_H
#	define JOYSTICK_H

#	define JOY_LATCH_MASK 0x04
#	define JOY_CLK_MASK 0x08

#	define NUM_JOYSTICKS 4

#include <functional>
#include <SDL.h>

extern uint8_t Joystick_data;

bool joystick_init();
void joystick_add(int index);
void joystick_remove(int index);
void joystick_slot_remap(int slot, int instance_id);

void joystick_button_down(int instance_id, uint8_t button);
void joystick_button_up(int instance_id, uint8_t button);

void joystick_set_latch(bool value);
void joystick_set_clock(bool value);

void joystick_for_each(std::function<void(int, SDL_GameController *, int current_slot)> fn);
void joystick_for_each_slot(std::function<void(int, int, SDL_GameController *)> fn);

#endif
