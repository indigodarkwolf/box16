#pragma once
#if !defined(YM2151_H)
#	define YM2151_H

//=============================================
//
// YM2151 wrapper around ymfm's API
//
// Copyright (c) 2021-2023 Stephen Horn, et al.
// All Rights Reserved. License: 2-clause BSD
//
//---------------------------------------------

#	define MAX_YM2151_VOICES (8)
#	define MAX_YM2151_SLOTS (MAX_YM2151_VOICES * 4)

#	define YM_R_L_FB_CONN_OFFSET 0x20
#	define YM_KC_OFFSET 0x28
#	define YM_KF_OFFSET 0x30
#	define YM_PMS_AMS_OFFSET 0x38

#	define YM_DT1_MUL_OFFSET 0x40
#	define YM_TL_OFFSET 0x60
#	define YM_KS_AR_OFFSET 0x80
#	define YM_A_D1R_OFFSET 0xA0
#	define YM_DT2_D2R_OFFSET 0xC0
#	define YM_D1L_RR_OFFSET 0xE0

#	define YM_CLOCK_RATE (3579545)
#	define YM_SAMPLE_RATE (YM_CLOCK_RATE >> 6)

void     YM_prerender(uint32_t clocks);
void     YM_render(int16_t *buffers, uint32_t samples, uint32_t sample_rate);
void     YM_clear_backbuffer();
uint32_t YM_get_sample_rate();

bool YM_irq_is_enabled();
void YM_set_irq_enabled(bool enabled);

bool YM_is_strict();
void YM_set_strict_busy(bool enable);

void    YM_write(uint8_t offset, uint8_t value);
uint8_t YM_read_status();
bool    YM_irq();
void    YM_reset();

// debug stuff
void    YM_debug_write(uint8_t addr, uint8_t value);
uint8_t YM_debug_read(uint8_t addr);

uint8_t YM_last_address();
uint8_t YM_last_data();

struct ym_modulation_state {
	uint8_t amplitude_modulation;
	uint8_t phase_modulation;
	float   LFO_phase;
};

struct ym_slot_state {
	uint32_t frequency;
	float    eg_output;
	float    final_env;
	uint8_t  env_state;
};

void     YM_get_modulation_regs(uint8_t *regs);
void     YM_get_slot_regs(uint8_t slnum, uint8_t *regs);
void     YM_get_modulation_state(ym_modulation_state &data);
void     YM_get_slot_state(uint8_t slnum, ym_slot_state &data);
uint16_t YM_get_timer_counter(uint8_t tnum);

uint8_t YM_get_last_key_on();
uint8_t YM_get_lfo_frequency();
uint8_t YM_get_modulation_depth();
uint8_t YM_get_modulation_type();
uint8_t YM_get_waveform();
uint8_t YM_get_control_output_1();
uint8_t YM_get_control_output_2();
uint8_t YM_get_voice_connection_type(uint8_t voice);
uint8_t YM_get_voice_self_feedback_level(uint8_t voice);
uint8_t YM_get_voice_left_enable(uint8_t voice);
uint8_t YM_get_voice_right_enable(uint8_t voice);
uint8_t YM_get_voice_note(uint8_t voice);
uint8_t YM_get_voice_octave(uint8_t voice);
uint8_t YM_get_voice_key_fraction(uint8_t voice);
uint8_t YM_get_voice_amplitude_modulation_sensitivity(uint8_t voice);
uint8_t YM_get_voice_phase_modulation_sensitivity(uint8_t voice);
uint8_t YM_get_operator_phase_multiply(uint8_t voice, uint8_t op);
uint8_t YM_get_operator_detune_1(uint8_t voice, uint8_t op);
uint8_t YM_get_operator_total_level(uint8_t voice, uint8_t op);
uint8_t YM_get_operator_attack_rate(uint8_t voice, uint8_t op);
uint8_t YM_get_operator_key_scaling(uint8_t voice, uint8_t op);
uint8_t YM_get_operator_decay_rate_1(uint8_t voice, uint8_t op);
uint8_t YM_get_operator_ams_enabled(uint8_t voice, uint8_t op);
uint8_t YM_get_operator_decay_rate_2(uint8_t voice, uint8_t op);
uint8_t YM_get_operator_detune_2(uint8_t voice, uint8_t op);
uint8_t YM_get_operator_release_rate(uint8_t voice, uint8_t op);
uint8_t YM_get_operator_decay_1_level(uint8_t voice, uint8_t op);

void YM_key_on(uint8_t channel, bool m1 = true, bool c1 = true, bool m2 = true, bool c2 = true);
void YM_set_lfo_frequency(uint8_t freq);
void YM_set_modulation_depth(uint8_t depth);
void YM_set_modulation_type(uint8_t mtype);
void YM_set_waveform(uint8_t wf);
void YM_set_control_output_1(bool enabled);
void YM_set_control_output_2(bool enabled);
void YM_set_voice_connection_type(uint8_t voice, uint8_t ctype);
void YM_set_voice_self_feedback_level(uint8_t voice, uint8_t fl);
void YM_set_voice_left_enable(uint8_t voice, bool enable);
void YM_set_voice_right_enable(uint8_t voice, bool enable);
void YM_set_voice_note(uint8_t voice, uint8_t note);
void YM_set_voice_octave(uint8_t voice, uint8_t octave);
void YM_set_voice_key_fraction(uint8_t voice, uint8_t fraction);
void YM_set_voice_amplitude_modulation_sensitivity(uint8_t voice, uint8_t ams);
void YM_set_voice_phase_modulation_sensitivity(uint8_t voice, uint8_t pms);
void YM_set_operator_phase_multiply(uint8_t voice, uint8_t op, uint8_t mul);
void YM_set_operator_detune_1(uint8_t voice, uint8_t op, uint8_t dt1);
void YM_set_operator_total_level(uint8_t voice, uint8_t op, uint8_t tl);
void YM_set_operator_attack_rate(uint8_t voice, uint8_t op, uint8_t ar);
void YM_set_operator_key_scaling(uint8_t voice, uint8_t op, uint8_t ks);
void YM_set_operator_decay_rate_1(uint8_t voice, uint8_t op, uint8_t dr1);
void YM_set_operator_ams_enabled(uint8_t voice, uint8_t op, bool enable);
void YM_set_operator_decay_rate_2(uint8_t voice, uint8_t op, uint8_t dr2);
void YM_set_operator_detune_2(uint8_t voice, uint8_t op, uint8_t dt2);
void YM_set_operator_release_rate(uint8_t voice, uint8_t op, uint8_t rr);
void YM_set_operator_decay_1_level(uint8_t voice, uint8_t op, uint8_t d1l);

#endif
