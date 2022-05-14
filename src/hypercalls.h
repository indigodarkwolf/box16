// Commander X16 Emulator
// Copyright (c) 2022 Stephen Horn, et al.
// All rights reserved. License: 2-clause BSD

#pragma once
#if !defined(HYPERCALLS_H)
#	define HYPERCALLS_H

bool hypercalls_init();
void hypercalls_process();

#endif