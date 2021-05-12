#include "timing.h"

#include <SDL.h>

#include "options.h"
#include "ring_buffer.h"

struct tick_record {
	uint32_t sdl_ticks;
	int      frames;
};

static ring_buffer<tick_record, 100> Tick_history;
int                                  Timing_perf = 0;
static int                           frames;
static int32_t                       sdlTicks_base;
static int32_t                       last_perf_update;
static int32_t                       perf_frame_count;

void timing_init()
{
	frames           = 0;
	sdlTicks_base    = SDL_GetTicks();
	last_perf_update = 0;
	perf_frame_count = 0;

	tick_record tick = { SDL_GetTicks(), 0 };
	Tick_history.add(tick);
}

void timing_update()
{
	frames++;

	tick_record tick = { SDL_GetTicks(), frames };
	Tick_history.add(tick);

	const tick_record &oldest_tick = Tick_history.get_oldest();

	int ticks_elapsed  = tick.sdl_ticks - oldest_tick.sdl_ticks;
	int frames_elapsed = 1 + tick.frames - oldest_tick.frames;

	int32_t diff_time = 1000 * frames_elapsed / 60 - ticks_elapsed;
	if (Options.warp_factor == 0 && diff_time > 0) {
		usleep(1000 * diff_time);
	}

	int nominal_frames_elapsed = 1 + ticks_elapsed * 60 / 1000;

	Timing_perf = 100 * frames_elapsed / nominal_frames_elapsed;

	if (Options.log_speed) {
		float frames_behind = -((float)diff_time / 16.666666f);
		int   load          = (int)((1 + frames_behind) * 100);
		printf("Load: %d%%\n", load > 100 ? 100 : load);

		if ((int)frames_behind > 0) {
			printf("Rendering is behind %d frames.\n", -(int)frames_behind);
		} else {
		}
	}
}
