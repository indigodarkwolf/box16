#include "gif_recorder.h"

#include "gif/gif.h"
#include "vera/vera_video.h"

// GIF recorder states
enum gif_recorder_state_t {
	RECORD_GIF_DISABLED = 0,
	RECORD_GIF_PAUSED,
	RECORD_GIF_SINGLE,
	RECORD_GIF_RECORDING
};

static gif_recorder_state_t Gif_record_state = RECORD_GIF_DISABLED;
static char *               Gif_path = nullptr;
static GifWriter            Gif_writer;

static int Gif_width;
static int Gif_height;

void gif_recorder_set_path(char const *path)
{
	Gif_path = new char[strlen(path) + 1];
	strcpy(Gif_path, path);

	Gif_record_state = RECORD_GIF_PAUSED;
}

void gif_recorder_init(int width, int height)
{
	Gif_width  = width;
	Gif_height = height;

	if (Gif_record_state != RECORD_GIF_DISABLED) {
		if (!strcmp(Gif_path + strlen(Gif_path) - 5, ",wait")) {
			// wait for POKE
			Gif_record_state = RECORD_GIF_PAUSED;
			// move the string terminator to remove the ",wait"
			Gif_path[strlen(Gif_path) - 5] = 0;
		} else {
			// start now
			Gif_record_state = RECORD_GIF_RECORDING;
		}
		if (!GifBegin(&Gif_writer, Gif_path, SCREEN_WIDTH, SCREEN_HEIGHT, 1, 8, false)) {
			Gif_record_state = RECORD_GIF_DISABLED;
		}
	}
}

void gif_recorder_shutdown()
{
	if (Gif_record_state != RECORD_GIF_DISABLED) {
		GifEnd(&Gif_writer);
		Gif_record_state = RECORD_GIF_DISABLED;
	}
}

void gif_recorder_update(const uint8_t *image_bytes)
{
	if (Gif_record_state > RECORD_GIF_PAUSED) {
		if (!GifWriteFrame(&Gif_writer, image_bytes, SCREEN_WIDTH, SCREEN_HEIGHT, 2, 8, false)) {
			// if that failed, stop recording
			GifEnd(&Gif_writer);
			Gif_record_state = RECORD_GIF_DISABLED;
			fmt::print("Unexpected end of recording.\n");
		}
		if (Gif_record_state == RECORD_GIF_SINGLE) { // if single-shot stop recording
			Gif_record_state = RECORD_GIF_PAUSED;    // need to close in video_end()
		}
	}
}

// Control the GIF recorder
void gif_recorder_set(gif_recorder_command_t command)
{
	if (Gif_record_state == RECORD_GIF_DISABLED) {
		return;
	}

	// turning off while recording is enabled
	if (command == RECORD_GIF_PAUSE) {
		Gif_record_state = RECORD_GIF_PAUSED; // need to save
	}
	// turning on continuous recording
	if (command == RECORD_GIF_RECORD) {
		Gif_record_state = RECORD_GIF_RECORDING; // activate recording
	}

	// capture one frame
	if (command == RECORD_GIF_SNAP) {
		Gif_record_state = RECORD_GIF_SINGLE; // single-shot
	}
}

uint8_t gif_recorder_get_state()
{
	return static_cast<uint8_t>(Gif_record_state);
}
