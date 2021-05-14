#pragma once
#if !defined(OPTIONS_H)
#	define OPTIONS_H

enum echo_mode_t {
	ECHO_MODE_NONE = 0,
	ECHO_MODE_RAW,
	ECHO_MODE_COOKED,
	ECHO_MODE_ISO,
};

enum class scale_quality_t {
	NEAREST,
	LINEAR,
	BEST
};

struct options {
	char hyper_path[PATH_MAX]  = ".";
	char rom_path[PATH_MAX]    = "rom.bin";
	char prg_path[PATH_MAX]    = "";
	char bas_path[PATH_MAX]    = "";
	char sdcard_path[PATH_MAX] = "";
	char nvram_path[PATH_MAX]  = "";
	char gif_path[PATH_MAX]    = "";

	bool run_after_load = false;
	bool run_geos       = false;
	bool run_test       = false;

	bool load_standard_symbols = false;

	bool        log_keyboard = false;
	bool        log_speed    = false;
	bool        log_video    = false;
	bool        dump_cpu     = false;
	bool        dump_ram     = false;
	bool        dump_bank    = false;
	bool        dump_vram    = false;
	echo_mode_t echo_mode    = ECHO_MODE_NONE;

	int             num_ram_banks = 64; // 512 KB default
	uint8_t         keymap        = 0;  // KERNAL's default
	int             test_number   = -1;
	int             warp_factor   = 0;
	int             window_scale  = 2;
	scale_quality_t scale_quality = scale_quality_t::NEAREST;

	char audio_dev_name[PATH_MAX] = "";
	bool no_sound                 = false;
	int  audio_buffers            = 8;

	bool set_system_time = false;
};

extern options Options;

void load_options(int argc, char **argv);
void save_options(bool all);

#endif