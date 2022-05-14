// Commander X16 Emulator
// Copyright (c) 2019 Michael Steil
// Copyright (c) 2021-2022 Stephen Horn, et al.
// All rights reserved. License: 2-clause BSD

#pragma once
#ifndef LOADSAVE_H
#define LOADSAVE_H

void LOAD();
void SAVE();

int create_directory_listing(uint8_t *data);

#endif
