#pragma once
#if !defined(OPTIONS_H)
#	define OPTIONS_H

#	include <filesystem>

enum class echo_mode_t {
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

enum class vsync_mode_t {
	VSYNC_MODE_NONE = 0,
	VSYNC_MODE_GET_SYNC,
	VSYNC_MODE_WAIT_SYNC,
	VSYNC_MODE_DEBUG,
};

enum class gif_recorder_start_t {
	GIF_RECORDER_START_WAIT = 0,
	GIF_RECORDER_START_NOW
};

enum class wav_recorder_start_t {
	WAV_RECORDER_START_WAIT = 0,
	WAV_RECORDER_START_AUTO,
	WAV_RECORDER_START_NOW
};

struct options {
	std::filesystem::path rom_path     = "rom.bin";
	std::filesystem::path patch_path   = "";
	std::filesystem::path patch_target = "";
	std::filesystem::path nvram_path   = "";
	std::filesystem::path hyper_path   = ".";
	std::filesystem::path prg_path     = "";
	std::filesystem::path bas_path     = "";
	std::filesystem::path sdcard_path  = "";
	std::filesystem::path gif_path     = "";
	std::filesystem::path wav_path     = "";

	bool create_patch = false;
	bool apply_patch  = false;

	uint16_t prg_override_start = 0;

	gif_recorder_start_t gif_start = gif_recorder_start_t::GIF_RECORDER_START_NOW;
	wav_recorder_start_t wav_start = wav_recorder_start_t::WAV_RECORDER_START_NOW;

	bool run_after_load = false;
	bool run_geos       = false;
	bool run_test       = false;

	bool load_standard_symbols = false;

	bool log_verbose  = false;
	bool log_keyboard = false;
	bool log_speed    = false;
	bool log_video    = false;
	bool dump_cpu     = true;
	bool dump_ram     = true;
	bool dump_bank    = true;
	bool dump_vram    = true;

	echo_mode_t echo_mode = echo_mode_t::ECHO_MODE_NONE;

	int             num_ram_banks = 64; // 512 KB default
	uint8_t         keymap        = 0;  // KERNAL's default
	int             test_number   = -1;
	int             warp_factor   = 0;
	int             window_scale  = 2;
	bool            widescreen    = false;
	scale_quality_t scale_quality = scale_quality_t::NEAREST;
	vsync_mode_t    vsync_mode    = vsync_mode_t::VSYNC_MODE_GET_SYNC;

	std::string audio_dev_name = "";
	bool        no_sound       = false;
	int         audio_buffers  = 8;

	bool set_system_time    = false;
	bool no_keybinds        = false;
	bool no_ieee_hypercalls = false;
	bool no_hypercalls      = false;
	bool enable_serial      = false;
	bool ym_irq             = false;
	bool ym_strict          = false;
	bool memory_randomize   = false;
	bool memory_uninit_warn = false;
};

extern options Options;

void options_init(const char *base_dir, const char *prefs_dir, int argc, char **argv);
void load_options();
void save_options(bool all);
void save_options_on_close(bool all);

size_t options_get_base_path(std::filesystem::path &real_path, const std::filesystem::path &path);
size_t options_get_prefs_path(std::filesystem::path &real_path, const std::filesystem::path &path);
size_t options_get_hyper_path(std::filesystem::path &real_path, const std::filesystem::path &path);

bool option_cmdline_option_was_set(char const *cmdline_option);
bool option_inifile_option_was_set(char const *cmdline_option);

option_source option_get_source(char const *cmdline_option);
char const   *option_get_source_name(option_source source);

bool options_find_file(std::filesystem::path &real_path, const std::filesystem::path &search_path);

int options_log_verbose(const char *format, ...);

#endif