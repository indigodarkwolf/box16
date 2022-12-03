// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// Copyright (c) 2021-2022 Stephen Horn, et al.
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
#include "hypercalls.h"
#include "i2c.h"
#include "ieee.h"
#include "joystick.h"
#include "keyboard.h"
#include "memory.h"
#include "midi.h"
#include "options.h"
#include "overlay/cpu_visualization.h"
#include "overlay/overlay.h"
#include "ring_buffer.h"
#include "rtc.h"
#include "sdl_events.h"
#include "serial.h"
#include "symbols.h"
#include "timing.h"
#include "utf8.h"
#include "utf8_encode.h"
#include "vera/sdcard.h"
#include "vera/vera_spi.h"
#include "vera/vera_video.h"
#include "version.h"
#include "via.h"
#include "wav_recorder.h"
#include "ym2151/ym2151.h"
#include "cpu/mnemonics.h"
#include "overlay/disasm.h"


#ifdef __EMSCRIPTEN__
#	include <emscripten.h>
#	include <pthread.h>
#endif

void emulator_loop();

bool debugger_enabled = true;

bool save_on_exit = true;

bool       has_boot_tasks = false;
SDL_RWops *prg_file       = nullptr;

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
		SDL_RWwrite(f, &state6502.a, sizeof(uint8_t), 1);
		SDL_RWwrite(f, &state6502.x, sizeof(uint8_t), 1);
		SDL_RWwrite(f, &state6502.y, sizeof(uint8_t), 1);
		SDL_RWwrite(f, &state6502.sp, sizeof(uint8_t), 1);
		SDL_RWwrite(f, &state6502.status, sizeof(uint8_t), 1);
		SDL_RWwrite(f, &state6502.pc, sizeof(uint16_t), 1);
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
	if (Options.warp_factor == 0) {
		Options.warp_factor = 9;
		vera_video_set_cheat_mask(0x3f);
		timing_init();
	} else {
		Options.warp_factor = 0;
		vera_video_set_cheat_mask(0);
		timing_init();
	}
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

	options_init(base_path, private_path, argc, argv);

	if (Options.log_video) {
		vera_video_set_log_video(true);
	}

	if (Options.warp_factor > 0) {
		vera_video_set_cheat_mask((1 << (Options.warp_factor - 1)) - 1);
	}

	// Initialize memory
	{
		memory_init_params memory_params;
		memory_params.randomize                           = Options.memory_randomize;
		memory_params.enable_uninitialized_access_warning = Options.memory_uninit_warn;
		memory_params.num_banks                           = Options.num_ram_banks;

		memory_init(memory_params);
	}

	// Initialize debugger
	{
		debugger_init(Options.num_ram_banks);
	}

	auto open_file = [](std::filesystem::path &path, char const *cmdline_option, char const *mode) -> SDL_RWops * {
		SDL_RWops *f = nullptr;

		option_source optsrc  = option_get_source(cmdline_option);
		char const   *srcname = option_get_source_name(optsrc);

		std::filesystem::path real_path;
		if (options_find_file(real_path, path)) {
			const std::string &real_path_string = real_path.generic_string();

			f = SDL_RWFromFile(real_path_string.c_str(), mode);
			printf("Using %s at %s\n", cmdline_option, real_path_string.c_str());
		}
		printf("\t-%s sourced from: %s\n ", cmdline_option, srcname);
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
		SDL_RWops *f = open_file(Options.rom_path, "rom", "rb");
		if (f == nullptr) {
			error("ROM error", "Could not find ROM.");
		}

		memset(ROM, 0, ROM_SIZE);
		SDL_RWread(f, ROM, ROM_SIZE, 1);
		SDL_RWclose(f);
	}

	// Load NVRAM, if specified
	if (!Options.nvram_path.empty()) {
		SDL_RWops *f = open_file(Options.nvram_path, "nvram", "rb");
		if (f) {
			SDL_RWread(f, nvram, sizeof(nvram), 1);
			SDL_RWclose(f);
		}
	}

	// Open SDCard, if specified
	if (!Options.sdcard_path.empty()) {
		std::filesystem::path sdcard_path;
		if (options_find_file(sdcard_path, Options.sdcard_path)) {
			sdcard_set_file(sdcard_path.generic_string().c_str());
		}
	}

	if (!Options.no_hypercalls) {
		if (!hypercalls_init()) {
			error("Boot error", "Could not initialize hypercalls. Disable hypercalls to boot with this ROM.");
		}
	}

#ifdef SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR
	// Don't disable compositing (on KDE for example)
	// Available since SDL 2.0.8
	SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO);

	if (!Options.no_sound) {
		audio_init(Options.audio_dev_name.size() > 0 ? Options.audio_dev_name.c_str() : nullptr, Options.audio_buffers);
		audio_set_render_callback(wav_recorder_process);
		YM_set_irq_enabled(Options.ym_irq);
		YM_set_strict_busy(Options.ym_strict);
	}

	// Initialize display
	{
		display_settings init_settings;
		init_settings.aspect_ratio  = Options.widescreen ? (16.0f / 9.0f) : (4.0f / 3.0f);
		init_settings.video_rect.x  = 0;
		init_settings.video_rect.y  = 0;
		init_settings.video_rect.w  = 640;
		init_settings.video_rect.h  = 480;
		init_settings.window_rect.x = 0;
		init_settings.window_rect.y = 0;
		init_settings.window_rect.w = (int)(480 * Options.window_scale * init_settings.aspect_ratio);
		init_settings.window_rect.h = (480 * Options.window_scale) + IMGUI_OVERLAY_MENU_BAR_HEIGHT;
		display_init(init_settings);
	}

	vera_video_reset();

	if (!Options.gif_path.empty()) {
		gif_recorder_set_path(Options.gif_path.generic_string().c_str());
		switch (Options.gif_start) {
			case gif_recorder_start_t::GIF_RECORDER_START_WAIT:
				gif_recorder_set(RECORD_GIF_PAUSE);
				break;
			case gif_recorder_start_t::GIF_RECORDER_START_NOW:
				gif_recorder_set(RECORD_GIF_RECORD);
				break;
			default:
				break;
		}
	}

	if (!Options.wav_path.empty()) {
		wav_recorder_set_path(Options.wav_path.generic_string().c_str());
		switch (Options.wav_start) {
			case wav_recorder_start_t::WAV_RECORDER_START_WAIT:
				wav_recorder_set(RECORD_WAV_PAUSE);
				break;
			case wav_recorder_start_t::WAV_RECORDER_START_AUTO:
				wav_recorder_set(RECORD_WAV_AUTOSTART);
				break;
			case wav_recorder_start_t::WAV_RECORDER_START_NOW:
				wav_recorder_set(RECORD_WAV_RECORD);
				break;
			default:
				break;
		}
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

	save_options_on_close(false);

	if (nvram_dirty && !Options.nvram_path.empty()) {
		SDL_RWops *f = SDL_RWFromFile(Options.nvram_path.generic_string().c_str(), "wb");
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
	debugger_shutdown();
	display_shutdown();
	SDL_Quit();

	return 0;
}

static char const *disasm_get_label(uint16_t address)
{
	static char label[256];

	const symbol_list_type &symbols = symbols_find(address);
	if (symbols.size() > 0) {
		strncpy(label, symbols.front().c_str(), 256);
		label[255] = '\0';
		return label;
	}

	for (uint16_t i = 1; i < 3; ++i) {
		const symbol_list_type &symbols = symbols_find(address - i);
		if (symbols.size() > 0) {
			snprintf(label, 256, "%s+%d", symbols.front().c_str(), i);
			label[255] = '\0';
			return label;
		}
	}

	return nullptr;
}

static char const *disasm_label(uint16_t target, bool branch_target, const char *hex_format)
{
	const char *symbol = disasm_get_label(target);

	static char inner[256];
	if (symbol != nullptr) {
		snprintf(inner, 256, "%s", symbol);
	} else {
		snprintf(inner, 256, hex_format, target);
	}
	inner[255] = '\0';
	return inner;
}

static char const *disasm_label_wrap(uint16_t target, bool branch_target, const char *hex_format, const char *wrapper_format)
{
	const char *symbol = disasm_get_label(target);

	char inner[256];
	if (symbol != nullptr) {
		snprintf(inner, 256, "%s", symbol);
	} else {
		snprintf(inner, 256, hex_format, target);
	}
	inner[255] = '\0';

	static char wrapped[512];
	snprintf(wrapped, 512, wrapper_format, inner);
	wrapped[511] = '\0';
	return wrapped;
}

void sncatf(char *&buffer_start, size_t &size_remaining, char const *fmt, ...)
{
	va_list arglist;
	va_start(arglist, fmt);
	size_t printed = vsnprintf(buffer_start, size_remaining, fmt, arglist);
    va_end(arglist);
	buffer_start += printed;
	size_remaining -= printed;
}

size_t disasm_code(char* buffer, size_t buffer_size, uint16_t pc, /*char *line, unsigned int max_line,*/ uint8_t bank)
{

	uint8_t     opcode   = debug_read6502(pc, bank);
	char const *mnemonic = mnemonics[opcode];
	op_mode     mode     = mnemonics_mode[opcode];

	//		Test for branches. These are BRA ($80) and
	//		$10,$30,$50,$70,$90,$B0,$D0,$F0.
	//
	bool is_branch = (opcode == 0x80) || ((opcode & 0x1F) == 0x10) || (opcode == 0x20);

	//		Ditto bbr and bbs, the "zero-page, relative" ops.
	//		$0F,$1F,$2F,$3F,$4F,$5F,$6F,$7F,$8F,$9F,$AF,$BF,$CF,$DF,$EF,$FF
	//
	bool is_zprel = (opcode & 0x0F) == 0x0F;

	is_branch = is_branch || is_zprel;
	char* buffer_beg = buffer;


	switch (mode) {
		case op_mode::MODE_ZPREL: {
			uint8_t  zp     = debug_read6502(pc + 1, bank);
			uint16_t target = pc + 3 + (int8_t)debug_read6502(pc + 2, bank);

			sncatf(buffer, buffer_size, "%s", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label(zp, is_branch, "$%02X") );
			sncatf(buffer, buffer_size, ", ");
			sncatf(buffer, buffer_size, "%s", disasm_label(target, is_branch, "$%04X") );
		} break;

		case op_mode::MODE_IMP:
			sncatf(buffer, buffer_size, "%s", mnemonic);
			break;

		case op_mode::MODE_IMM: {
			uint16_t value = debug_read6502(pc + 1, bank);

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "#$%02X", value);
		} break;

		case op_mode::MODE_ZP: {
			uint8_t value = debug_read6502(pc + 1, bank);

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "$%02X", value);
		} break;

		case op_mode::MODE_REL: {
			uint16_t target = pc + 2 + (int8_t)debug_read6502(pc + 1, bank);

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label(target, is_branch, "$%04X"));
		} break;

		case op_mode::MODE_ZPX: {
			uint8_t value = debug_read6502(pc + 1, bank);

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(value, is_branch, "$%02X", "%s,x"));
		} break;

		case op_mode::MODE_ZPY: {
			uint8_t value = debug_read6502(pc + 1, bank);

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(value, is_branch, "$%02X", "%s,y"));
		} break;

		case op_mode::MODE_ABSO: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label(target, is_branch, "$%04X"));
		} break;

		case op_mode::MODE_ABSX: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(target, is_branch, "$%04X", "%s,x"));
		} break;

		case op_mode::MODE_ABSY: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(target, is_branch, "$%04X", "%s,y"));
		} break;

		case op_mode::MODE_AINX: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(target, is_branch, "$%04X", "(%s,x)"));
		} break;

		case op_mode::MODE_INDY: {
			uint8_t target = debug_read6502(pc + 1, bank);

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(target, is_branch, "$%02X", "(%s),y"));
		} break;

		case op_mode::MODE_INDX: {
			uint8_t target = debug_read6502(pc + 1, bank);

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(target, is_branch, "$%02X", "(%s,x)"));
		} break;

		case op_mode::MODE_IND: {
			uint16_t target = debug_read6502(pc + 1, bank) | debug_read6502(pc + 2, bank) << 8;

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(target, is_branch, "$%04X", "(%s)"));
		} break;

		case op_mode::MODE_IND0: {
			uint8_t target = debug_read6502(pc + 1, bank);

			sncatf(buffer, buffer_size, "%s ", mnemonic);
			sncatf(buffer, buffer_size, "%s", disasm_label_wrap(target, is_branch, "$%02X", "(%s)"));
		} break;

		case op_mode::MODE_A:
			sncatf(buffer, buffer_size, "%s a", mnemonic);
			break;
	}
	return (size_t)(buffer - buffer_beg);
}

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
			uint16_t pc = state6502.pc;
			uint8_t  x  = state6502.x;
			uint8_t  y  = state6502.y;
			uint8_t  a  = state6502.a;
			uint8_t  status  = state6502.status;
			uint8_t  sp      = state6502.sp;
			uint8_t  ram     = memory_get_ram_bank();	
			uint8_t  rom     = memory_get_rom_bank();	
			uint8_t  cur     = memory_get_current_bank(pc);	
			char     buffer[1024];
			uint8_t  pos = 0;

			if ( (Options.log_cpu_main && (pc >= 0x0800 && pc <= 0x9FFF ) ) ||
			     (Options.log_cpu_bram && (pc >= 0xA000 && pc <= 0xBFFF ) ) ||
			     (Options.log_cpu_low  && (pc >= 0x0000 && pc <= 0x07FF ) ) ||
 			     (Options.log_cpu_brom && (pc >= 0xC000 && pc <= 0xFFFF ) )
				) {

				 printf("a:$%02x x:$%02x y:$%02x s:$%02x p:", a, x, y, sp);
				 for (int i = 7; i >= 0; i--) {
					 printf("%c", (status & (1 << i)) ? "czidb.vn"[i] : '-');
				 }

				 printf(" ram=$%02x rom=$%02x ", ram, rom);
				 const char *label     = disasm_get_label(pc);
				 size_t      label_len = label ? strlen(label) : 0;
				 if (label) {
					 printf("%s", label);
				 }
				 label_len = (label_len <= 25) ? label_len : 25;
				 for (size_t i = 0; i < 25 - label_len; i++) {
					 printf(" ");
				 }
				 printf("$%02x:$%04x ", cur, pc);
				 disasm_code(buffer, sizeof(buffer), pc, cur);
				 printf("%s", buffer);


				 printf("\n");
			}
		}
#endif

		uint64_t old_clockticks6502 = clockticks6502;
		step6502();
		if (debug6502) {
			debugger_process_cpu();
			if (debugger_is_paused()) {
				continue;
			} else {
				force6502();
			}
		}
		cpu_visualization_step();
		uint8_t clocks       = (uint8_t)(clockticks6502 - old_clockticks6502);
		bool    new_frame    = vera_video_step(MHZ, clocks);
		bool    via1_irq_old = via1_irq();
		via1_step(clocks);
		via2_step(clocks);
		rtc_step(clocks);
		if (Options.enable_serial) {
			serial_step(clocks);
		}
		audio_render(clocks);

		if (new_frame) {
			midi_process();
			gif_recorder_update(vera_video_get_framebuffer());
			static uint32_t last_display_us = timing_total_microseconds_realtime();
			const uint32_t  display_us      = timing_total_microseconds_realtime();
			if ((Options.warp_factor == 0) || (display_us - last_display_us > 16000)) { // Close enough I'm willing to pay for OpenGL's sync.
				display_process();
				last_display_us = display_us;
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
			irq6502();
			debugger_interrupt();
		}

		hypercalls_process();

		if (state6502.pc == 0xffff) {
			if (save_on_exit) {
				machine_dump();
			}
			return;
		}

		keyboard_process();
	}
}
