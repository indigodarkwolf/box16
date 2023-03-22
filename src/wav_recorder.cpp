#include "wav_recorder.h"

#include "SDL.h"
#include "audio.h"

// WAV recorder states
enum wav_recorder_state_t {
	RECORD_WAV_DISABLED = 0,
	RECORD_WAV_PAUSED,
	RECORD_WAV_AUTOSTARTING,
	RECORD_WAV_RECORDING
};

static wav_recorder_state_t Wav_record_state = RECORD_WAV_DISABLED;
static char *               Wav_path         = nullptr;

class wav_recorder
{
public:
	void begin(const char *path, int32_t sample_rate);
	void end();
	void add(const int16_t *samples, const int num_samples);

private:
#pragma pack(push, 1)
	struct riff_chunk {
		char     chunk_id[4] = { 'R', 'I', 'F', 'F' };
		uint32_t size        = 4;
		char     wave_id[4]  = { 'W', 'A', 'V', 'E' };
	};

	struct fmt_chunk {
		char     chunk_id[4]     = { 'f', 'm', 't', ' ' };
		uint32_t size            = 16;
		uint16_t format_tag      = 0x0001; // WAVE_FORMAT_PCM
		uint16_t channels        = 2;
		uint32_t samples_per_sec = 0;
		uint32_t bytes_per_sec   = 0;
		uint16_t block_align     = 0;
		uint16_t bits_per_sample = 16 * 2;
	};

	struct data_chunk {
		char     chunk_id[4] = { 'd', 'a', 't', 'a' };
		uint32_t size        = 0;
	};

	struct file_header {
		riff_chunk riff;
		fmt_chunk  fmt;
		data_chunk data;
	};
#pragma pack(pop)

	file_header header;
	uint32_t    samples_written = 0;

	SDL_RWops *wav_file = nullptr;

	void update_sizes()
	{
		header.data.size = sizeof(int16_t) * header.fmt.channels * samples_written;
		header.riff.size = 4 + sizeof(fmt_chunk) + sizeof(data_chunk) + (header.data.size);
	}
};

void wav_recorder::begin(const char *path, int32_t sample_rate)
{
	if (wav_file != nullptr) {
		if (header.fmt.samples_per_sec != sample_rate) {
			end();
		}
	}

	if (wav_file == nullptr) {
		wav_file = SDL_RWFromFile(path, "wb");

		if (wav_file != nullptr) {
			header.fmt.samples_per_sec = sample_rate;
			header.fmt.bytes_per_sec   = sample_rate * sizeof(int16_t) * header.fmt.channels;
			header.fmt.block_align     = sizeof(int16_t) * header.fmt.channels;
			header.fmt.bits_per_sample = (sizeof(int16_t)) << 3;

			const size_t written = SDL_RWwrite(wav_file, &header, sizeof(file_header), 1);
			if (written == 0) {
				SDL_RWclose(wav_file);
				wav_file = nullptr;
			}
		}
	}
}

void wav_recorder::end()
{
	if (wav_file != nullptr) {
		update_sizes();
		SDL_RWseek(wav_file, 0, RW_SEEK_SET);
		SDL_RWwrite(wav_file, &header, sizeof(file_header), 1);
		SDL_RWclose(wav_file);
		wav_file = nullptr;
	}
}

void wav_recorder::add(const int16_t *samples, const int num_samples)
{
	if (wav_file) {
		const size_t bytes   = sizeof(int16_t) * 2 * num_samples;
		const size_t written = SDL_RWwrite(wav_file, samples, bytes, 1);
		if (written == 0) {
			SDL_RWclose(wav_file);
			wav_file = nullptr;
		} else {
			samples_written += num_samples;
		}
	}
}

wav_recorder Wav_recorder;

void wav_recorder_init()
{
}

void wav_recorder_shutdown()
{
	Wav_recorder.end();
}

void wav_recorder_process(const int16_t *samples, const int num_samples)
{
	if (Wav_record_state == RECORD_WAV_AUTOSTARTING) {
		for (int i = 0; i < num_samples; ++i) {
			if (samples[i] != 0) {
				Wav_record_state = RECORD_WAV_RECORDING;
				Wav_recorder.begin(Wav_path, audio_get_sample_rate());
				break;
			}
		}
	}

	if (Wav_record_state == RECORD_WAV_RECORDING) {
		Wav_recorder.add(samples, num_samples);
	}
}

void wav_recorder_set(wav_recorder_command_t command)
{
	if (Wav_record_state != RECORD_WAV_DISABLED) {
		switch (command) {
			case RECORD_WAV_PAUSE:
				Wav_record_state = RECORD_WAV_PAUSED;
				break;
			case RECORD_WAV_RECORD:
				Wav_record_state = RECORD_WAV_RECORDING;
				Wav_recorder.begin(Wav_path, audio_get_sample_rate());
				break;
			case RECORD_WAV_AUTOSTART:
				if (Wav_record_state == RECORD_WAV_RECORDING) {
					Wav_recorder.end();
				}
				Wav_record_state = RECORD_WAV_AUTOSTARTING;
				break;
			default:
				printf("Unknown command %d passed to wav_recorder_set.\n", (int)command);
				break;
		}
	}
}

uint8_t wav_recorder_get_state()
{
	return (uint8_t)Wav_record_state;
}

void wav_recorder_set_path(const char *path)
{
	if (Wav_record_state == RECORD_WAV_RECORDING) {
		Wav_recorder.end();
	}

	if (Wav_path != nullptr) {
		delete[] Wav_path;
		Wav_path = nullptr;
	}

	if (path != nullptr) {
		Wav_path = new char[strlen(path) + 1];
		strcpy(Wav_path, path);

		if (!strcmp(Wav_path + strlen(Wav_path) - 5, ",wait")) {
            Wav_path[strlen(Wav_path) - 5] = 0;
			Wav_record_state = RECORD_WAV_PAUSED;
		} else if (!strcmp(Wav_path + strlen(Wav_path) - 5, ",auto")) {
			Wav_path[strlen(Wav_path) - 5] = 0;
			Wav_record_state               = RECORD_WAV_AUTOSTARTING;
		} else {
			Wav_record_state = RECORD_WAV_RECORDING;
			Wav_recorder.begin(Wav_path, audio_get_sample_rate());
		}
	} else {
		Wav_record_state = RECORD_WAV_DISABLED;
	}
}