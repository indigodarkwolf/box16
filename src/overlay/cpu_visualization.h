#pragma once

enum cpu_visualization_highlight {
	NONE = 0,
	IRQ,
	VISIBLE,
	INVISIBLE,
};

enum cpu_visualization_coloring {
	ADDRESS,
	INSTRUCTION,
	TEST
};

void            cpu_visualization_enable(bool enable);

void            cpu_visualization_step();
const uint32_t *cpu_visualization_get_framebuffer();

void                       cpu_visualization_set_coloring(cpu_visualization_coloring coloring);
cpu_visualization_coloring cpu_visualization_get_coloring();

void                        cpu_visualization_set_highlight(cpu_visualization_highlight highlight);
cpu_visualization_highlight cpu_visualization_get_highlight();
