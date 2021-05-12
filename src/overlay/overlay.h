#pragma once
#if !defined(OVERLAY_H)
#	define OVERLAY_H

#	define IMGUI_OVERLAY_MENU_BAR_HEIGHT (19)

extern bool Show_imgui_demo;
extern bool Show_memory_dump_1;
extern bool Show_memory_dump_2;
extern bool Show_monitor;
extern bool Show_VERA_monitor;

void overlay_draw();

#endif
