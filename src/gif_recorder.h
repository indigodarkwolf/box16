#pragma once

// GIF recorder commands
enum gif_recorder_command_t {
	RECORD_GIF_PAUSE = 0,
	RECORD_GIF_SNAP,
	RECORD_GIF_RECORD
};

void gif_recorder_set_path(char const *path);
void gif_recorder_init(int width, int height);
void gif_recorder_shutdown();
void gif_recorder_update(const uint8_t *framebuffer_bytes);

void    gif_recorder_set(gif_recorder_command_t command);
uint8_t gif_recorder_get_state();
