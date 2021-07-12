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

// debug stuff
void    YM_debug_write(uint8_t addr, uint8_t value);
uint8_t YM_debug_read(uint8_t addr);
uint8_t YM_last_address();
uint8_t YM_last_data();

uint8_t  YM_get_AMD();
uint8_t  YM_get_PMD();
float    YM_get_LFO_phase();
uint32_t YM_get_freq(uint8_t slnum);
float    YM_get_EG_output(uint8_t slnum);
float    YM_get_final_env(uint8_t slnum);
uint8_t  YM_get_env_state(uint8_t slnum);
uint16_t YM_get_timer_counter(uint8_t tnum);

#endif
