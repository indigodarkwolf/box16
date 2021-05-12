// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// All rights reserved. License: 2-clause BSD

#include "audio.h"
#include "glue.h"
#include "keyboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char javascript_text_data[65536];

void j2c_reset()
{
	machine_reset();
}

void j2c_paste(char *buffer)
{
	if (buffer != nullptr && *buffer != 0) {
		keyboard_add_text(buffer);
	}
}

void j2c_start_audio(bool start)
{
	if (start)
		audio_init(NULL, 8);
	else
		audio_close();
}
