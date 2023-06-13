// Commander X16 Emulator
// Copyright (c) 2021-2023 Stephen Horn, et al.
// All rights reserved. License: 2-clause BSD

#pragma once
#ifndef DEBUGGER_H
#	define DEBUGGER_H

#	include <string>
#	include <set>
#	include <tuple>

#	include "boxmon/parser.h"
#	include "cpu/fake6502.h"

#	define DEBUG6502_EXPRESSION 0x80
#	define DEBUG6502_CONDITION 0x08

//
// Breakpoints
//

using breakpoint_type = std::tuple<uint16_t, uint8_t>;
using breakpoint_list = std::set<breakpoint_type>;

void debugger_init(int max_ram_banks);
void debugger_shutdown();
bool debugger_is_paused();

void debugger_process_cpu();
void debugger_pause_execution();
void debugger_continue_execution();
void debugger_step_execution();
void debugger_step_over_execution();
void debugger_step_out_execution();

uint64_t debugger_step_clocks();
void     debugger_interrupt();
bool     debugger_step_interrupted();

uint8_t     debugger_get_flags(uint16_t address, uint8_t bank);
std::string debugger_get_condition(uint16_t address, uint8_t bank);
void        debugger_set_condition(uint16_t address, uint8_t bank, const std::string &condition);
bool        debugger_evaluate_condition(uint16_t address, uint8_t bank);

    // Bank parameter is only meaninful for addresses >= $A000.
// Addresses < $A000 will force bank to 0.
void debugger_add_breakpoint(uint16_t address, uint8_t bank = 0, uint8_t flags = DEBUG6502_EXEC);
void debugger_remove_breakpoint(uint16_t address, uint8_t bank = 0, uint8_t flags = DEBUG6502_EXEC);
void debugger_activate_breakpoint(uint16_t address, uint8_t bank = 0, uint8_t flags = DEBUG6502_EXEC);
void debugger_deactivate_breakpoint(uint16_t address, uint8_t bank = 0, uint8_t flags = DEBUG6502_EXEC);
bool debugger_has_breakpoint(uint16_t address, uint8_t bank = 0, uint8_t flags = DEBUG6502_EXEC);
bool debugger_breakpoint_is_active(uint16_t address, uint8_t bank = 0, uint8_t flags = DEBUG6502_EXEC);

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
extern char const      *Debugger_size_types[Num_debugger_size_types];

void debugger_add_watch(uint16_t address, uint8_t bank, uint8_t size);
void debugger_remove_watch(uint16_t address, uint8_t bank, uint8_t size);

const watch_address_list &debugger_get_watchlist();

#endif
