// Commander X16 Emulator
// Copyright (c) 2021-2022 Stephen Horn, et al.
// All rights reserved. License: 2-clause BSD

#pragma once
#ifndef DEBUGGER_H
#	define DEBUGGER_H

#	include <set>
#	include <tuple>

//
// Breakpoints
//

using breakpoint_type = std::tuple<uint16_t, uint8_t>;
using breakpoint_list = std::set<breakpoint_type>;

bool debugger_is_paused();

void debugger_pause_execution();
void debugger_continue_execution();
void debugger_step_execution();
void debugger_step_over_execution();
void debugger_step_out_execution();

uint64_t debugger_step_clocks();
void     debugger_interrupt();
bool     debugger_step_interrupted();

// Bank parameter is only meaninful for addresses >= $A000.
// Addresses < $A000 will force bank to 0.
void debugger_add_breakpoint(uint16_t address, uint8_t bank = 0);
void debugger_remove_breakpoint(uint16_t address, uint8_t bank = 0);
void debugger_activate_breakpoint(uint16_t address, uint8_t bank = 0);
void debugger_deactivate_breakpoint(uint16_t address, uint8_t bank = 0);
bool debugger_has_breakpoint(uint16_t address, uint8_t bank = 0);
bool debugger_breakpoint_is_active(uint16_t address, uint8_t bank = 0);

const breakpoint_list &debugger_get_breakpoints();

//
// Memory watch
//

using watch_address_type = std::tuple<uint16_t, uint8_t, uint8_t>;
using watch_address_list = std::set<watch_address_type>;

#	define DEBUGGER_SIZE_TYPE_U8 0
#	define DEBUGGER_SIZE_TYPE_U16 1
#	define DEBUGGER_SIZE_TYPE_U24 2
#	define DEBUGGER_SIZE_TYPE_U32 3
#	define DEBUGGER_SIZE_TYPE_S8 4
#	define DEBUGGER_SIZE_TYPE_S16 5
#	define DEBUGGER_SIZE_TYPE_S24 6
#	define DEBUGGER_SIZE_TYPE_S32 7

constexpr const uint8_t Num_debugger_size_types = 8;
extern char const *     Debugger_size_types[Num_debugger_size_types];

void                      debugger_add_watch(uint16_t address, uint8_t bank, uint8_t size);
void                      debugger_remove_watch(uint16_t address, uint8_t bank, uint8_t size);

const watch_address_list &debugger_get_watchlist();

#endif
