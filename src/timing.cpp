#include "timing.h"

#include <SDL.h>

#include "glue.h"
#include "options.h"
#include "ring_buffer.h"

struct tick_record {
	uint32_t us;
	uint32_t total_us;
	uint32_t total_frames;
};

#if defined(PROFILE)
static constexpr const int Tick_history_length = 10000;
#else
static constexpr const int Tick_history_length = 100;
#endif

static ring_buffer<tick_record, Tick_history_length> Tick_history;

uint32_t        Timing_perf = 0;
static uint32_t Total_frames;
static uint64_t Base_performance_time;
static uint64_t Last_performance_time;
static uint64_t Performance_frequency;

static constexpr uint32_t Expected_frametime_us = 1000000 / 60;

static uint32_t perf_to_us(const uint64_t perf)
{
	return (uint32_t)(1000000 * perf / Performance_frequency);
}

void timing_init()
{
	Total_frames          = 0;
	Base_performance_time = SDL_GetPerformanceCounter();
	Last_performance_time = Base_performance_time;
	Performance_frequency = SDL_GetPerformanceFrequency();

	tick_record tick = { 0, 0, 0 };
	Tick_history.add(tick);
}

void timing_update()
{
	Total_frames++;
	const uint64_t current_performance_time = SDL_GetPerformanceCounter();

	const tick_record &last_tick       = Tick_history.get_newest();
	const uint64_t     tick_perf_diff  = current_performance_time - Last_performance_time;
	const uint64_t     total_perf_diff = current_performance_time - Base_performance_time;
	tick_record        tick            = { perf_to_us(tick_perf_diff), perf_to_us(total_perf_diff), Total_frames };

	const uint32_t us_elapsed = tick.total_us - last_tick.total_us;
	if (Options.warp_factor == 0 && us_elapsed < Expected_frametime_us) { // 60 fps
		usleep(Expected_frametime_us - us_elapsed);

		const uint64_t current_performance_time = SDL_GetPerformanceCounter();
		const uint64_t tick_perf_diff           = current_performance_time - Last_performance_time;
		const uint64_t total_perf_diff          = current_performance_time - Base_performance_time;

		tick = { perf_to_us(tick_perf_diff), perf_to_us(total_perf_diff), Total_frames };
	}

	Tick_history.add(tick);

	const tick_record &first_tick   = Tick_history.get_oldest();
	const uint64_t     diff_time_us = tick.total_us - first_tick.total_us;
	const uint64_t     diff_frames  = tick.total_frames - first_tick.total_frames;
	Timing_perf                     = (uint32_t)((100ULL * (diff_frames * Expected_frametime_us) + (diff_time_us >> 1)) / (diff_time_us));

	if (Options.log_speed) {
		fmt::print("Speed: {:d}%\n", Timing_perf);
		uint32_t load = (uint32_t)(100 * tick.us / Expected_frametime_us);
		fmt::print("Load: {:d}%\n", load > 100 ? 100 : load);
	}

	Last_performance_time = current_performance_time;

#if defined(PROFILE)
	if (Tick_history.count() == Tick_history_length) {
		fmt::print("Runtime: {:d}us\n", (uint32_t)diff_time_us);
		fmt::print("Frames:  {:d}\n", (uint32_t)diff_frames);
		fmt::print("Speed:   {:d}%\n", Timing_perf);
		state6502.pc = 0xffff;
	}
#endif
}

uint32_t timing_total_microseconds()
{
	return Tick_history.get_newest().total_us;
}

uint32_t timing_total_microseconds_realtime()
{
	const uint64_t current_performance_time = SDL_GetPerformanceCounter();

	const uint64_t total_perf_diff = current_performance_time - Base_performance_time;
	return perf_to_us(total_perf_diff);
}