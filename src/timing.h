#pragma once
#if !defined(TIMING_H)
#	define TIMING_H

extern uint32_t Timing_perf;

void timing_init();
void timing_update();
uint32_t timing_total_microseconds();

#endif
