// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// All rights reserved. License: 2-clause BSD
#ifndef SD_CARD_H
#define SD_CARD_H

void sdcard_shutdown();
void sdcard_set_file(char const *path);
bool sdcard_path_is_set();
void sdcard_attach();
void sdcard_detach();
bool sdcard_is_attached();

void    sdcard_select(bool select);
uint8_t sdcard_handle(uint8_t inbyte);

#endif
