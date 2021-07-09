#pragma once
#if !defined(YM2151_H)
#	define YM2151_H

//=============================================
//
// YM2151 wrapper around ymfm's API
//
// Copyright (c) 2021, Stephen Horn
// All Rights Reserved. License: 2-clause BSD
//
//---------------------------------------------

void YM_render(int16_t *stream, uint32_t samples, uint32_t buffer_sample_rate);

void    YM_write(uint8_t offset, uint8_t value);
uint8_t YM_read_status();

#endif
