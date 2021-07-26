#pragma once
#if !defined(YM2151_OVERLAY_H)
#define YM2151_OVERLAY_H

#include <functional>

struct ym_slot_data {
	int  dt1, dt2, mul;
	int  ar, d1r, d1l, d2r, rr, ks;
	int  tl;
	bool ame;
};

struct ym_keyon_state {
	bool debug_kon[4] = { 1, 1, 1, 1 };
	int  dkob_state   = 0;
};

struct ym_channel_data {
	int   con, fb;
	bool  l, r;
	float kc;
	int   ams, pms;

	ym_slot_data slot[4];
};

void draw_debugger_ym2151();

void debugger_draw_ym_voice(int i, uint8_t *regs, ym_channel_data &channel, ym_keyon_state *keyon, std::function<void(uint8_t, uint8_t)> apply_byte);
void debugger_draw_ym_voices(uint8_t *regs, ym_channel_data *channels, ym_keyon_state *keyons, int num_channels, std::function<void(uint8_t, uint8_t)> apply_byte);
void debugger_draw_ym_lfo_and_noise(uint8_t *regs);



#endif
