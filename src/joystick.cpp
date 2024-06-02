#include "joystick.h"

#include <SDL.h>
#include <unordered_map>

#define LOG_JOYSTICK(...) // fmt::format(__VA_ARGS__)

struct joystick_info {
	SDL_GameController *controller;
	uint16_t            button_mask;
	uint16_t            shift_mask;
	int                 current_slot;
};

static const uint16_t button_map[SDL_CONTROLLER_BUTTON_MAX] = {
	1 << 0,  //SDL_CONTROLLER_BUTTON_A,
	1 << 8,  //SDL_CONTROLLER_BUTTON_B,
	1 << 1,  //SDL_CONTROLLER_BUTTON_X,
	1 << 9,  //SDL_CONTROLLER_BUTTON_Y,
	1 << 2,  //SDL_CONTROLLER_BUTTON_BACK,
	0,       //SDL_CONTROLLER_BUTTON_GUIDE,
	1 << 3,  //SDL_CONTROLLER_BUTTON_START,
	0,       //SDL_CONTROLLER_BUTTON_LEFTSTICK,
	0,       //SDL_CONTROLLER_BUTTON_RIGHTSTICK,
	1 << 10, //SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
	1 << 11, //SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
	1 << 4,  //SDL_CONTROLLER_BUTTON_DPAD_UP,
	1 << 5,  //SDL_CONTROLLER_BUTTON_DPAD_DOWN,
	1 << 6,  //SDL_CONTROLLER_BUTTON_DPAD_LEFT,
	1 << 7,  //SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
};

static std::unordered_map<int, joystick_info> Joystick_controllers;
static int                                    Joystick_slots[NUM_JOYSTICKS];

static bool Joystick_latch = false;
uint8_t     Joystick_data  = 0;

bool joystick_init()
{
	for (int i = 0; i < NUM_JOYSTICKS; ++i) {
		Joystick_slots[i] = -1;
	}

	const int num_joysticks = SDL_NumJoysticks();
	for (int i = 0; i < num_joysticks; ++i) {
		joystick_add(i);
	}

	return true;
}

void joystick_add(int index)
{
	LOG_JOYSTICK("joystick_add({:d})\n", index);

	if (!SDL_IsGameController(index)) {
		return;
	}

	SDL_GameController *controller = SDL_GameControllerOpen(index);
	if (controller == nullptr) {
		fmt::print(stderr, "Could not open controller {:d}: {}\n", index, SDL_GetError());
		return;
	}

	SDL_JoystickID instance_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
	bool           exists      = false;
	for (int i = 0; i < NUM_JOYSTICKS; ++i) {
		if (Joystick_slots[i] == instance_id) {
			exists = true;
			break;
		}
	}

	if (!exists) {
		int slot;
		for (slot = 0; slot < NUM_JOYSTICKS; ++slot) {
			if (Joystick_slots[slot] == -1) {
				Joystick_slots[slot] = instance_id;
				break;
			}
		}
		Joystick_controllers.try_emplace(instance_id, joystick_info{ controller, 0xffff, 0, slot });
	}
}

void joystick_remove(int instance_id)
{
	LOG_JOYSTICK("joystick_remove({:d})\n", instance_id);

	for (int i = 0; i < NUM_JOYSTICKS; ++i) {
		if (Joystick_slots[i] == instance_id) {
			Joystick_slots[i] = -1;
			break;
		}
	}

	SDL_GameController *controller = SDL_GameControllerFromInstanceID(instance_id);
	if (controller == nullptr) {
		fmt::print(stderr, "Could not find controller from instance_id {:d}: {}\n", instance_id, SDL_GetError());
	} else {
		SDL_GameControllerClose(controller);
		Joystick_controllers.erase(instance_id);
	}
}

void joystick_slot_remap(int slot, int instance_id)
{
	LOG_JOYSTICK("joystick_slot_remap({:d}, {:d})\n", slot, instance_id);

	if (slot < 0 || slot >= NUM_JOYSTICKS) {
		fmt::print(stderr, "Error: joystick_slot_remap({:d}, {:d}) trying to remap invalid controller port {:d}.\n", slot, instance_id, slot);
		return;
	}

	int slot_old_instance_id = Joystick_slots[slot];
	int instance_old_slot    = NUM_JOYSTICKS;

	if (instance_id < 0) {
		Joystick_slots[slot] = -1;
	} else {
		const auto &joy = Joystick_controllers.find(instance_id);
		if (joy == Joystick_controllers.end()) {
			fmt::print(stderr, "Error: joystick_slot_remap({:d}, {:d}) could not find instance_id {:d}.\n", slot, instance_id, instance_id);
			return;
		}

		instance_old_slot = joy->second.current_slot;

		Joystick_slots[slot]     = instance_id;
		joy->second.current_slot = slot;
	}

	if (slot_old_instance_id >= 0) {
		const auto &old_joy = Joystick_controllers.find(slot_old_instance_id);
		if (old_joy == Joystick_controllers.end()) {
			fmt::print(stderr, "Error: joystick_slot_remap({:d}, {:d}) could not find slot_old_instance_id {:d}.\n", slot, instance_id, slot_old_instance_id);
			return;
		}

		old_joy->second.current_slot = instance_old_slot;
	}

	if (instance_old_slot != NUM_JOYSTICKS) {
		Joystick_slots[instance_old_slot] = slot_old_instance_id;
	}
}

void joystick_button_down(int instance_id, uint8_t button)
{
	LOG_JOYSTICK("joystick_button_down({:d}, {:d})\n", instance_id, button);

	const auto &joy = Joystick_controllers.find(instance_id);
	if (joy != Joystick_controllers.end()) {
		joy->second.button_mask &= ~(button_map[button]);
	}
}

void joystick_button_up(int instance_id, uint8_t button)
{
	LOG_JOYSTICK("joystick_button_up({:d}, {:d})\n", instance_id, button);

	const auto &joy = Joystick_controllers.find(instance_id);
	if (joy != Joystick_controllers.end()) {
		joy->second.button_mask |= button_map[button];
	}
}

static void do_shift()
{
	for (int i = 0; i < NUM_JOYSTICKS; ++i) {
		if (Joystick_slots[i] >= 0) {
			const auto &joy = Joystick_controllers.find(Joystick_slots[i]);
			Joystick_data |= ((joy->second.shift_mask & 1) ? (0x80 >> i) : 0);
			joy->second.shift_mask >>= 1;
		} else {
			Joystick_data |= 0x80 >> i;
		}
	}
}

void joystick_set_latch(bool value)
{
	Joystick_latch = value;
	if (value) {
		for (auto &joy : Joystick_controllers) {
			joy.second.shift_mask = joy.second.button_mask | 0xF000;
		}
		do_shift();
	}
}

void joystick_set_clock(bool value)
{
	if (!Joystick_latch && value) {
		Joystick_data = 0;
		do_shift();
	}
}

void joystick_for_each(std::function<void(int, SDL_GameController *, int current_slot)> fn)
{
	for (auto& joy : Joystick_controllers) {
		fn(joy.first, joy.second.controller, joy.second.current_slot);
	}
}

void joystick_for_each_slot(std::function<void(int, int, SDL_GameController *)> fn)
{
	for (int i = 0; i < NUM_JOYSTICKS; ++i) {
		if (Joystick_slots[i] == -1) {
			fn(i, -1, nullptr);
		} else {
			const auto &joy = Joystick_controllers.find(Joystick_slots[i]);
			if (joy == Joystick_controllers.end()) {
				fmt::print(stderr, "joystick_for_each_slot(...) could not find Joystick_slots[{:d}] {:d}", i, Joystick_slots[i]);
				fn(i, -1, nullptr);
			} else {
				fn(i, joy->first, joy->second.controller);				
			}
		}
	}
}