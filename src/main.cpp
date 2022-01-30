// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// All rights reserved. License: 2-clause BSD

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __MINGW32__
#	include <ctype.h>
#endif
#include "SDL.h"
#include "audio.h"
#include "cpu/fake6502.h"
#include "debugger.h"
#include "display.h"
#include "gif_recorder.h"
#include "glue.h"
#include "i2c.h"
#include "joystick.h"
#include "keyboard.h"
#include "loadsave.h"
#include "memory.h"
#include "midi.h"
#include "options.h"
#include "overlay/cpu_visualization.h"
#include "overlay/overlay.h"
#include "ps2.h"
#include "ring_buffer.h"
#include "rom_patch.h"
#include "rom_symbols.h"
#include "rtc.h"
#include "sdl_events.h"
#include "symbols.h"
#include "timing.h"
#include "unicode.h"
#include "utf8.h"
#include "utf8_encode.h"
#include "vera/sdcard.h"
#include "vera/vera_spi.h"
#include "vera/vera_video.h"
#include "version.h"
#include "via.h"
#include "wav_recorder.h"
#include "ym2151/ym2151.h"

#ifdef __EMSCRIPTEN__
#	include <emscripten.h>
#	include <pthread.h>
#endif

void emulator_loop();

bool debugger_enabled = true;

bool save_on_exit = true;

SDL_RWops *prg_file;
int        prg_override_start = -1;

void machine_dump()
{
	int  index = 0;
	char filename[22];
	for (;;) {
		if (!index) {
			strcpy(filename, "dump.bin");
		} else {
			sprintf(filename, "dump-%i.bin", index);
		}
		if (access(filename, F_OK) == -1) {
			break;
		}
		index++;
	}
	SDL_RWops *f = SDL_RWFromFile(filename, "wb");
	if (!f) {
		printf("Cannot write to %s!\n", filename);
		return;
	}

	if (Options.dump_cpu) {
		SDL_RWwrite(f, &a, sizeof(uint8_t), 1);
		SDL_RWwrite(f, &x, sizeof(uint8_t), 1);
		SDL_RWwrite(f, &y, sizeof(uint8_t), 1);
		SDL_RWwrite(f, &sp, sizeof(uint8_t), 1);
		SDL_RWwrite(f, &status, sizeof(uint8_t), 1);
		SDL_RWwrite(f, &pc, sizeof(uint16_t), 1);
	}
	memory_save(f, Options.dump_ram, Options.dump_bank);

	if (Options.dump_vram) {
		vera_video_save(f);
	}

	SDL_RWclose(f);
	printf("Dumped system to %s.\n", filename);
}

void machine_reset()
{
	memory_reset();
	vera_spi_init();
	via1_init();
	via2_init();
	vera_video_reset();
	YM_reset();
	reset6502();
}

void machine_toggle_warp()
{
	Options.warp_factor = 1;
	vera_video_set_cheat_mask(0x3f);
	timing_init();
}

static bool is_kernal()
{
	return read6502(0xfff6) == 'M' && // only for KERNAL
	       read6502(0xfff7) == 'I' &&
	       read6502(0xfff8) == 'S' &&
	       read6502(0xfff9) == 'T';
}

#undef main
int main(int argc, char **argv)
{
	const char *base_path    = SDL_GetBasePath();
	const char *private_path = SDL_GetPrefPath("Box16", "Box16");

	load_options(base_path, private_path, argc, argv);

	if (Options.log_video) {
		vera_video_set_log_video(true);
	}

	if (Options.warp_factor > 0) {
		vera_video_set_cheat_mask(0x3f);
	}

	auto open_cmdline_file = [](char const *path, char const *cmdline_option, char const *mode) -> SDL_RWops * {
		SDL_RWops *f = nullptr;

		option_source optsrc  = option_get_source(cmdline_option);
		char const *  srcname = option_get_source_name(optsrc);

		char rel_path_buffer[PATH_MAX];
		int  rel_path_len = options_get_relative_path(rel_path_buffer, path);

		char base_path_buffer[PATH_MAX];
		int  base_path_len = options_get_base_path(base_path_buffer, path);

		char prefs_path_buffer[PATH_MAX];
		int  prefs_path_len = options_get_prefs_path(prefs_path_buffer, path);

		if (optsrc == option_source::DEFAULT) {
			f = SDL_RWFromFile(base_path_buffer, mode);
			if (f) {
				goto have_file;
			}

			f = SDL_RWFromFile(prefs_path_buffer, mode);
			if (f) {
				goto have_file;
			}

			f = SDL_RWFromFile(rel_path_buffer, mode);
			if (f) {
				goto have_file;
			}
		} else {
			f = SDL_RWFromFile(rel_path_buffer, mode);
			if (f) {
				goto have_file;
			}

			f = SDL_RWFromFile(prefs_path_buffer, mode);
			if (f) {
				goto have_file;
			}

			f = SDL_RWFromFile(base_path_buffer, mode);
			if (f) {
				goto have_file;
			}
		}

		printf("Could not find %s in the following locations:\n\t%s\n\t%s\n\n-%s sourced from: %s", cmdline_option, rel_path_buffer, base_path_buffer, cmdline_option, srcname);
		return nullptr;

	have_file:
		printf("Using %s at %s\n\t-%s sourced from: %s\n", cmdline_option, base_path_buffer, cmdline_option, srcname);
		return f;
	};

	auto error = [](const char *title, const char *format, ...) {
		char    message_buffer[1024];
		va_list list;
		va_start(list, format);
		vsprintf(message_buffer, format, list);
		va_end(list);

		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message_buffer, display_get_window());
		exit(1);
	};

	// Load ROM
	{
		SDL_RWops *f = open_cmdline_file(Options.rom_path, "rom", "rb");
		if (f == nullptr) {
			error("ROM error", "Could not find ROM from %s", Options.rom_path);
		}

		memset(ROM, 0, ROM_SIZE);
		SDL_RWread(f, ROM, ROM_SIZE, 1);
		SDL_RWclose(f);
	}

	if (Options.create_patch) {
		struct rom_struct {
			uint8_t bytes[ROM_SIZE];
		};

		auto *target = new rom_struct;
		memset(target->bytes, 0, ROM_SIZE);

		SDL_RWops *target_file = open_cmdline_file(Options.patch_target, "patch_target", "rb");
		if (target_file != nullptr) {
			SDL_RWread(target_file, target->bytes, ROM_SIZE, 1);
			SDL_RWclose(target_file);

			SDL_RWops *patch_file = open_cmdline_file(Options.patch_path, "patch", "wb");
			if (patch_file != nullptr) {
				rom_patch_create(ROM, target->bytes, patch_file);
				SDL_RWclose(patch_file);
			}
		}

		delete target;
	}

	// Load patch
	if (!Options.ignore_patch) {
		SDL_RWops *patch_file = open_cmdline_file(Options.patch_path, "patch", "rb");
		if (patch_file == nullptr) {
			error("Patch error", "Could not find patch file from %s", Options.patch_path);
		}

		int result = rom_patch_load(patch_file, ROM);
		if (result != ROM_PATCH_LOAD_OK) {
			error("Patch error", "Could not load patch file from %s:\nerror %d", Options.patch_path, result);
		}
	}

	if (strlen(Options.nvram_path) > 0) {
		SDL_RWops *f = SDL_RWFromFile(Options.nvram_path, "rb");
		if (f) {
			printf("%s %u\n", Options.nvram_path, (uint32_t)sizeof(nvram));
			size_t l = SDL_RWread(f, nvram, 1, sizeof(nvram));
			printf("%d %x %x\n", (uint32_t)l, nvram[0], nvram[1]);
			SDL_RWclose(f);
		}
	}

	if (strlen(Options.sdcard_path) > 0) {
		sdcard_set_file(Options.sdcard_path);
	}

	prg_override_start = -1;
	if (strlen(Options.prg_path) > 0) {
		char path_buffer[PATH_MAX];
		int  path_len = options_get_hyper_path(path_buffer, Options.prg_path);

		auto find_comma = [](char *path_buffer, int path_len) -> char * {
			char *c = path_buffer + path_len - 1;
			while (path_len) {
				if (!isalnum(*c)) {
					return (*c == ',') ? c : nullptr;
				}
				--c;
				--path_len;
			}
			return nullptr;
		};

		char *comma = find_comma(&path_buffer[0], path_len);
		if (comma) {
			prg_override_start = (uint16_t)strtol(comma + 1, NULL, 16);
			*comma             = 0;
		}

		prg_file = SDL_RWFromFile(path_buffer, "rb");
		if (!prg_file) {
			printf("Cannot open PRG file %s!\n", path_buffer);
			exit(1);
		}
	}

	if (strlen(Options.bas_path) > 0) {
		keyboard_add_file(Options.bas_path);
		if (Options.run_after_load) {
			keyboard_add_text("RUN\r");
		}
	}
	if (Options.run_geos) {
		keyboard_add_text("GEOS\r");
	}

	if (Options.run_test) {
		char test_text[256];
		sprintf(test_text, "TEST %d\r", Options.test_number);
		keyboard_add_text(test_text);
	}

#ifdef SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR
	// Don't disable compositing (on KDE for example)
	// Available since SDL 2.0.8
	SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO);

	if (!Options.no_sound) {
		audio_init(strlen(Options.audio_dev_name) > 0 ? Options.audio_dev_name : nullptr, Options.audio_buffers);
		audio_set_render_callback(wav_recorder_process);
		YM_set_irq_enabled(Options.ym_irq);
		YM_set_strict_busy(Options.ym_strict);
	}

	memory_init();

	{
		display_settings init_settings;
		init_settings.video_rect.w  = 640;
		init_settings.video_rect.h  = 480;
		init_settings.window_rect.w = 640 * Options.window_scale;
		init_settings.window_rect.h = (480 * Options.window_scale) + IMGUI_OVERLAY_MENU_BAR_HEIGHT;
		display_init(init_settings);
	}

	vera_video_reset();

	if (strlen(Options.gif_path) > 0) {
		gif_recorder_set_path(Options.gif_path);
	}

	if (strlen(Options.wav_path) > 0) {
		wav_recorder_set_path(Options.wav_path);
	}

	gif_recorder_init(SCREEN_WIDTH, SCREEN_HEIGHT);
	wav_recorder_init();

	joystick_init();

	midi_init();

	rtc_init(Options.set_system_time);

	machine_reset();

	timing_init();

#ifdef __EMSCRIPTEN__
	emscripten_set_main_loop(emulator_loop, 0, 1);
#else
	emulator_loop();
#endif

	if (nvram_dirty && strlen(Options.nvram_path) > 0) {
		SDL_RWops *f = SDL_RWFromFile(Options.nvram_path, "wb");
		if (f) {
			SDL_RWwrite(f, nvram, 1, sizeof(nvram));
			SDL_RWclose(f);
		}
		nvram_dirty = false;
	}

	SDL_free(const_cast<char *>(private_path));
	SDL_free(const_cast<char *>(base_path));

	audio_close();
	wav_recorder_shutdown();
	gif_recorder_shutdown();
	display_shutdown();
	SDL_Quit();

	return 0;
}

//
// Trace functionality preserved for comparison with official emulator releases
//
#if defined(TRACE)
#	include "rom_labels.h"
char *label_for_address(uint16_t address)
{
	uint16_t *addresses;
	char **   labels;
	int       count;
	switch (memory_get_rom_bank()) {
		case 0:
			addresses = addresses_bank0;
			labels    = labels_bank0;
			count     = sizeof(addresses_bank0) / sizeof(uint16_t);
			break;
		case 1:
			addresses = addresses_bank1;
			labels    = labels_bank1;
			count     = sizeof(addresses_bank1) / sizeof(uint16_t);
			break;
		case 2:
			addresses = addresses_bank2;
			labels    = labels_bank2;
			count     = sizeof(addresses_bank2) / sizeof(uint16_t);
			break;
		case 3:
			addresses = addresses_bank3;
			labels    = labels_bank3;
			count     = sizeof(addresses_bank3) / sizeof(uint16_t);
			break;
		case 4:
			addresses = addresses_bank4;
			labels    = labels_bank4;
			count     = sizeof(addresses_bank4) / sizeof(uint16_t);
			break;
		case 5:
			addresses = addresses_bank5;
			labels    = labels_bank5;
			count     = sizeof(addresses_bank5) / sizeof(uint16_t);
			break;
#	if 0
		case 6:
			addresses = addresses_bank6;
			labels    = labels_bank6;
			count     = sizeof(addresses_bank6) / sizeof(uint16_t);
			break;
		case 7:
			addresses = addresses_bank7;
			labels = labels_bank7;
			count = sizeof(addresses_bank7) / sizeof(uint16_t);
			break;
#	endif
		default:
			addresses = NULL;
			labels    = NULL;
	}

	if (!addresses) {
		return NULL;
	}

	for (int i = 0; i < count; i++) {
		if (address == addresses[i]) {
			return labels[i];
		}
	}
	return NULL;
}
#endif

void emulator_loop()
{
	for (;;) {
		if (debugger_is_paused()) {
			vera_video_force_redraw_screen();
			display_process();
			if (!sdl_events_update()) {
				break;
			}
			timing_update();
			continue;
		}

		//
		// Trace functionality preserved for comparison with official emulator releases
		//
#if defined(TRACE)
		{
			printf("[%6d] ", instruction_counter);

			char *label     = label_for_address(pc);
			int   label_len = label ? strlen(label) : 0;
			if (label) {
				printf("%s", label);
			}
			for (int i = 0; i < 20 - label_len; i++) {
				printf(" ");
			}
			printf(" %02x:.,%04x ", memory_get_rom_bank(), pc);
			char disasm_line[15];
			int  len = disasm(pc, disasm_line, sizeof(disasm_line), 0);
			for (int i = 0; i < len; i++) {
				printf("%02x ", debug_read6502(pc + i));
			}
			for (int i = 0; i < 9 - 3 * len; i++) {
				printf(" ");
			}
			printf("%s", disasm_line);
			for (int i = 0; i < 15 - strlen(disasm_line); i++) {
				printf(" ");
			}

			printf("a=$%02x x=$%02x y=$%02x s=$%02x p=", a, x, y, sp);
			for (int i = 7; i >= 0; i--) {
				printf("%c", (status & (1 << i)) ? "czidb.vn"[i] : '-');
			}

			printf("\n");
		}
#endif

		uint64_t old_clockticks6502 = clockticks6502;
		step6502();
		cpu_visualization_step();
		uint8_t clocks = (uint8_t)(clockticks6502 - old_clockticks6502);
		bool new_frame = vera_video_step(MHZ, clocks);
		bool via1_irq_old = via1_irq();
		via1_step(clocks);
		via2_step(clocks);
		audio_render(clocks);

		if (new_frame) {
			midi_process();
			gif_recorder_update(vera_video_get_framebuffer());
			static uint32_t last_display_us = timing_total_microseconds();
			if (timing_total_microseconds() - last_display_us > 16000) { // Close enough I'm willing to pay for OpenGL's sync.
				display_process();
				last_display_us = timing_total_microseconds();
			}
			if (!sdl_events_update()) {
				break;
			}

			timing_update();
#ifdef __EMSCRIPTEN__
			// After completing a frame we yield back control to the browser to stay responsive
			return 0;
#endif
		}

		if (!via1_irq_old && via1_irq()) {
			nmi6502();
			debugger_interrupt();
		}

		if (vera_video_get_irq_out() || YM_irq() || via2_irq()) {
			if (!(status & 4)) {
				irq6502();
				debugger_interrupt();
			}
		}

		switch (pc) {
#ifdef LOAD_HYPERCALLS
			case 0xffd5:
			case 0xffd8:
				if (is_kernal() && RAM[FA] == 8 && !sdcard_is_attached()) {
					if (pc == 0xffd5) {
						LOAD();
					} else {
						SAVE();
					}
					pc = (RAM[0x100 + sp + 1] | (RAM[0x100 + sp + 2] << 8)) + 1;
					sp += 2;
					continue;
				}
				break;
#endif
			case 0xffd2:
				if (Options.echo_mode != ECHO_MODE_NONE && is_kernal()) {
					uint8_t c = a;
					if (Options.echo_mode == ECHO_MODE_COOKED) {
						if (c == 0x0d) {
							printf("\n");
						} else if (c == 0x0a) {
							// skip
						} else if (c < 0x20 || c >= 0x80) {
							printf("\\X%02X", c);
						} else {
							printf("%c", c);
						}
					} else if (Options.echo_mode == ECHO_MODE_ISO) {
						if (c == 0x0d) {
							printf("\n");
						} else if (c == 0x0a) {
							// skip
						} else if (c < 0x20 || (c >= 0x80 && c < 0xa0)) {
							printf("\\X%02X", c);
						} else {
							print_iso8859_15_char(c);
						}
					} else {
						printf("%c", c);
					}
					fflush(stdout);
				}
				break;

			case 0xffcf:
				if (is_kernal()) {
					// as soon as BASIC starts reading a line...
					if (prg_file) {
						// ...inject the app into RAM
						uint8_t  start_lo = SDL_ReadU8(prg_file);
						uint8_t  start_hi = SDL_ReadU8(prg_file);
						uint16_t start;
						if (prg_override_start >= 0) {
							start = prg_override_start;
						} else {
							start = start_hi << 8 | start_lo;
						}
						uint16_t end = start + (uint16_t)SDL_RWread(prg_file, RAM + start, 1, 65536 - start);
						SDL_RWclose(prg_file);
						prg_file = NULL;
						if (start == 0x0801) {
							// set start of variables
							RAM[VARTAB]     = end & 0xff;
							RAM[VARTAB + 1] = end >> 8;
						}

						if (Options.run_after_load) {
							if (start == 0x0801) {
								keyboard_add_text("RUN\r");
							} else {
								char sys_text[10];
								sprintf(sys_text, "SYS$%04X\r", start);
								keyboard_add_text(sys_text);
							}
						}
					}
				}
				break;

			case 0xffff:
				if (save_on_exit) {
					machine_dump();
				}
				return;
		}

		keyboard_process();
	}
}
