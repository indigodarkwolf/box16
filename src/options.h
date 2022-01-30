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

enum class option_source {
	DEFAULT,
	INIFILE,
	CMDLINE
};

enum vsync_mode_t {
	VSYNC_MODE_NONE = 0,
	VSYNC_MODE_GET_SYNC,
	VSYNC_MODE_WAIT_SYNC,
};

struct options {
	char hyper_path[PATH_MAX]   = ".";
	char rom_path[PATH_MAX]     = "rom.bin";
	char patch_path[PATH_MAX]   = "patch.bpf";
	char patch_target[PATH_MAX] = "";
	char prg_path[PATH_MAX]     = "";
	char bas_path[PATH_MAX]     = "";
	char sdcard_path[PATH_MAX]  = "";
	char nvram_path[PATH_MAX]   = "";
	char gif_path[PATH_MAX]     = "";
	char wav_path[PATH_MAX]     = "";

	bool ignore_patch = true;
	bool create_patch = false;

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
	vsync_mode_t    vsync_mode    = vsync_mode_t::VSYNC_MODE_GET_SYNC;

	char audio_dev_name[PATH_MAX] = "";
	bool no_sound                 = false;
	int  audio_buffers            = 8;

	bool set_system_time = false;
	bool no_keybinds     = false;
	bool ym_irq          = false;
	bool ym_strict       = false;
};

extern options Options;

void load_options(const char *base_dir, const char *prefs_dir, int argc, char **argv);
void load_options();
void save_options(bool all);

int options_get_base_path(char *real_path, const char *path);
int options_get_prefs_path(char *real_path, const char *path);
int options_get_relative_path(char *real_path, const char *path);
int options_get_hyper_path(char *hyper_path, const char *path);

bool option_cmdline_option_was_set(char const *cmdline_option);
bool option_inifile_option_was_set(char const *cmdline_option);

option_source option_get_source(char const *cmdline_option);
char const *  option_get_source_name(option_source source);

#endif