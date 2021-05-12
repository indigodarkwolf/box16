#pragma once
#ifndef DEBUGGER_H
#	define DEBUGGER_H

#	include <set>
#	include <tuple>

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
void                   debugger_add_breakpoint(uint16_t address, uint8_t bank = 0);
void                   debugger_remove_breakpoint(uint16_t address, uint8_t bank = 0);
const breakpoint_list &debugger_get_breakpoints();

#endif
