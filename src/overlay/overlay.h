#pragma once
#if !defined(OVERLAY_H)
#	define OVERLAY_H

#	define IMGUI_OVERLAY_MENU_BAR_HEIGHT (19)

extern bool Show_monitor_console;
extern bool Show_memory_dump_1;
extern bool Show_memory_dump_2;
extern bool Show_cpu_monitor;
extern bool Show_disassembler;
extern bool Show_breakpoints;
extern bool Show_watch_list;
extern bool Show_symbols_list;
extern bool Show_symbols_files;
extern bool Show_cpu_visualizer;
extern bool Show_VRAM_visualizer;
extern bool Show_VERA_vram_dump;
extern bool Show_VERA_monitor;
extern bool Show_VERA_palette;
extern bool Show_VERA_layers;
extern bool Show_VERA_sprites;
extern bool Show_VERA_PSG_monitor;
extern bool Show_YM2151_monitor;
extern bool Show_midi_overlay;
extern bool Show_display;

extern bool display_focused;
extern bool mouse_captured;

void overlay_draw();

#endif
