#include "ym2151.h"

#include "ymfm_opm.h"

#include "ymfm_fm.ipp"

#include "audio.h"
#include "bitutils.h"

class ym2151_interface : public ymfm::ymfm_interface
{
public:
	ym2151_interface()
	    : m_chip(*this),
	      m_timing_error(0)
	{
		m_chip_sample_rate = m_chip.sample_rate(3579545);
	}

	void generate(int16_t *stream, uint32_t samples, uint32_t buffer_sample_rate)
	{
		ymfm::ym2151::output_data ym0 = m_last_output[0];
		ymfm::ym2151::output_data ym1 = m_last_output[1];

		if (m_timing_error == 0) {
			ym0 = ym1;
			m_chip.generate(&ym1, 1);
		}

		auto lerp = [](double v0, double v1, double x, double x1) -> double {
			return v0 + (v1 - v0) * (x / x1);
		};

		const int16_t *stream_end = stream + (samples << 1);
		if (buffer_sample_rate > m_chip_sample_rate) {
			const uint32_t incremental_error = m_chip_sample_rate;
			while (stream < stream_end) {
				*stream = (int16_t)lerp(ym0.data[0], ym1.data[1], m_timing_error, buffer_sample_rate);
				++stream;
				*stream = (int16_t)lerp(ym0.data[1], ym1.data[1], m_timing_error, buffer_sample_rate);
				++stream;

				m_timing_error += incremental_error;

				while (m_timing_error >= buffer_sample_rate) {
					ym0 = ym1;
					m_chip.generate(&ym1, 1);

					m_timing_error -= buffer_sample_rate;
				}
			}
			m_last_output[0] = ym0;
			m_last_output[1] = ym1;
		} else {
			const uint32_t incremental_error = m_chip_sample_rate - buffer_sample_rate;
			while (stream < stream_end) {
				while (m_timing_error >= m_chip_sample_rate) {
					ym0 = ym1;
					m_chip.generate(&ym1, 1);
					m_timing_error -= m_chip_sample_rate;
				}
				*stream = (int16_t)lerp(ym0.data[0], ym1.data[1], m_timing_error, m_chip_sample_rate);
				++stream;
				*stream = (int16_t)lerp(ym0.data[1], ym1.data[1], m_timing_error, m_chip_sample_rate);
				++stream;

				ym0 = ym1;
				m_chip.generate(&ym1, 1);

				m_timing_error += incremental_error;
			}
			m_last_output[0] = ym0;
			m_last_output[1] = ym1;
		}
	}

	void write(uint8_t addr, uint8_t value)
	{
		m_chip.write_address(addr);
		m_chip.write_data(value, false);
	}

	void debug_write(uint8_t addr, uint8_t value)
	{
		// do a direct write without triggering the busy timer
		m_chip.write_address(addr);
		m_chip.write_data(value, true);
	}

	uint8_t debug_read(uint8_t addr)
	{
		return m_chip.get_registers().get_register_data(addr);
	}

	uint8_t read_status()
	{
		return m_chip.read_status();
	}

	uint8_t get_AMD()
	{
		return m_chip.get_registers().lfo_am_depth();
	}

	uint8_t get_PMD()
	{
		return m_chip.get_registers().lfo_pm_depth();
	}

	uint32_t get_LFO_phase()
	{
		return m_chip.get_registers().lfo_phase();
	}

	uint32_t get_freq(uint8_t slnum)
	{
		return m_chip.get_debug_op(slnum)->phase_step();
	}

	uint16_t get_EG_output(uint8_t slnum)
	{
		return m_chip.get_debug_op(slnum)->debug_eg_attenuation();
	}

	uint16_t get_final_env(uint8_t slnum)
	{
		uint32_t am = m_chip.get_registers().lfo_am_offset(slnum & 7);
		return m_chip.get_debug_op(slnum)->envelope_attenuation(am);
	}

	uint8_t get_env_state(uint8_t slnum)
	{
		switch (m_chip.get_debug_op(slnum)->debug_eg_state()) {
			case ymfm::EG_ATTACK: return 1;
			case ymfm::EG_DECAY: return 2;
			case ymfm::EG_SUSTAIN: return 3;
			case ymfm::EG_RELEASE: return 4;
			default: return 0;
		}
	}

	uint16_t get_timer_counter(uint8_t tnum)
	{
		// TODO ymfm doesn't implement timer emulation,
		// it's the interface (aka this class)'s responsibility to emulate them
		return 0;
	}

private:
	ymfm::ym2151              m_chip;
	ymfm::ym2151::output_data m_last_output[2];

	uint32_t m_timing_error;
	uint32_t m_chip_sample_rate;
};

static ym2151_interface Ym_interface;
static uint8_t          Last_address = 0;
static uint8_t          Last_data    = 0;
static uint8_t          Ym_registers[256];

void YM_render(int16_t *stream, uint32_t samples, uint32_t buffer_sample_rate)
{
	Ym_interface.generate(stream, samples, buffer_sample_rate);
}

void YM_write(uint8_t offset, uint8_t value)
{
	// save the hassle to add interface to dig into opm_registers by caching the writes here
	if (offset & 1) { // data port
		Last_data                  = value;
		Ym_registers[Last_address] = Last_data;

		audio_lock_scope lock;
		Ym_interface.write(Last_address, Last_data);
	} else { // address port
		Last_address = value;
	}
}

uint8_t YM_read_status()
{
	audio_lock_scope lock;
	return Ym_interface.read_status();
}

void YM_debug_write(uint8_t addr, uint8_t value)
{
	Ym_registers[addr] = value;

	audio_lock_scope lock;
	Ym_interface.debug_write(addr, value);
}

uint8_t YM_debug_read(uint8_t addr)
{
	return Ym_registers[addr];
}

uint8_t YM_last_address()
{
	return Last_address;
}

uint8_t YM_last_data()
{
	return Last_data;
}

void YM_get_modulation_regs(uint8_t *regs)
{
	regs[0x01] = Ym_registers[0x01];
	regs[0x0F] = Ym_registers[0x0F];
	regs[0x18] = Ym_registers[0x18];
	regs[0x19] = Ym_registers[0x19];
	regs[0x1B] = Ym_registers[0x1B];
}

void YM_get_voice_regs(uint8_t voice, uint8_t *regs)
{
	regs[YM_R_L_FB_CONN_OFFSET + voice] = Ym_registers[YM_R_L_FB_CONN_OFFSET + voice];
	regs[YM_KC_OFFSET + voice]          = Ym_registers[YM_KC_OFFSET + voice];
	regs[YM_KF_OFFSET + voice]          = Ym_registers[YM_KF_OFFSET + voice];
	regs[YM_PMS_AMS_OFFSET + voice]     = Ym_registers[YM_PMS_AMS_OFFSET + voice];
}

void YM_get_slot_regs(uint8_t voice, uint8_t slot, uint8_t *regs)
{
	regs[YM_DT1_MUL_OFFSET + (slot * 8) + voice] = Ym_registers[YM_DT1_MUL_OFFSET + (slot * 8) + voice];
	regs[YM_TL_OFFSET + (slot * 8) + voice]      = Ym_registers[YM_TL_OFFSET + (slot * 8) + voice];
	regs[YM_KS_AR_OFFSET + (slot * 8) + voice]   = Ym_registers[YM_KS_AR_OFFSET + (slot * 8) + voice];
	regs[YM_A_D1R_OFFSET + (slot * 8) + voice]   = Ym_registers[YM_A_D1R_OFFSET + (slot * 8) + voice];
	regs[YM_DT2_D2R_OFFSET + (slot * 8) + voice] = Ym_registers[YM_DT2_D2R_OFFSET + (slot * 8) + voice];
	regs[YM_D1L_RR_OFFSET + (slot * 8) + voice]  = Ym_registers[YM_D1L_RR_OFFSET + (slot * 8) + voice];
}

void YM_get_modulation_state(ym_modulation_state &data)
{
	audio_lock_scope lock;
	data.amplitude_modulation = Ym_interface.get_AMD();
	data.phase_modulation     = Ym_interface.get_PMD();
	data.LFO_phase            = (Ym_interface.get_LFO_phase() & ((1 << 30) - 1)) / (float)(1 << 30);
}

void YM_get_slot_state(uint8_t slnum, ym_slot_state &data)
{
	audio_lock_scope lock;
	data.frequency = Ym_interface.get_freq(slnum);
	data.eg_output = (1024 - Ym_interface.get_EG_output(slnum)) / 1024.f;
	data.final_env = (1024 - Ym_interface.get_final_env(slnum)) / 1024.f;
	data.env_state = Ym_interface.get_env_state(slnum);
}

uint16_t YM_get_timer_counter(uint8_t tnum)
{
	audio_lock_scope lock;
	return Ym_interface.get_timer_counter(tnum);
}

//
// Field Accessors
//

uint8_t YM_get_last_key_on()
{
	return Ym_registers[0x08];
}

uint8_t YM_get_lfo_frequency()
{
	return Ym_registers[0x18];
}

uint8_t YM_get_modulation_depth()
{
	return get_bit_field<6, 0>(Ym_registers[0x19]);
}

uint8_t YM_get_modulation_type()
{
	return get_bit_field<7>(Ym_registers[0x19]);
}

uint8_t YM_get_waveform()
{
	return get_bit_field<1, 0>(Ym_registers[0x1b]);
}

uint8_t YM_get_control_output_1()
{
	return get_bit_field<6>(Ym_registers[0x1b]);
}

uint8_t YM_get_control_output_2()
{
	return get_bit_field<7>(Ym_registers[0x1b]);
}

uint8_t YM_get_voice_connection_type(uint8_t voice)
{
	if (voice < 8) {
		return get_bit_field<2, 0>(Ym_registers[YM_R_L_FB_CONN_OFFSET + voice]);
	}

	return 0;
}

uint8_t YM_get_voice_self_feedback_level(uint8_t voice)
{
	if (voice < 8) {
		return get_bit_field<5, 3>(Ym_registers[YM_R_L_FB_CONN_OFFSET + voice]);
	}

	return 0;
}

uint8_t YM_get_voice_left_enable(uint8_t voice)
{
	if (voice < 8) {
		return get_bit_field<6>(Ym_registers[YM_R_L_FB_CONN_OFFSET + voice]);
	}

	return 0;
}

uint8_t YM_get_voice_right_enable(uint8_t voice)
{
	if (voice < 8) {
		return get_bit_field<7>(Ym_registers[YM_R_L_FB_CONN_OFFSET + voice]);
	}

	return 0;
}

uint8_t YM_get_voice_note(uint8_t voice)
{
	if (voice < 8) {
		return get_bit_field<3, 0>(Ym_registers[YM_KC_OFFSET + voice]);
	}

	return 0;
}

uint8_t YM_get_voice_octave(uint8_t voice)
{
	if (voice < 8) {
		return get_bit_field<6, 4>(Ym_registers[YM_KC_OFFSET + voice]);
	}

	return 0;
}

uint8_t YM_get_voice_key_fraction(uint8_t voice)
{
	if (voice < 8) {
		return get_bit_field<7, 2>(Ym_registers[YM_KF_OFFSET + voice]);
	}

	return 0;
}

uint8_t YM_get_voice_amplitude_modulation_sensitivity(uint8_t voice)
{
	if (voice < 8) {
		return get_bit_field<2, 1>(Ym_registers[YM_PMS_AMS_OFFSET + voice]);
	}

	return 0;
}

uint8_t YM_get_voice_phase_modulation_sensitivity(uint8_t voice)
{
	if (voice < 8) {
		return get_bit_field<6, 4>(Ym_registers[YM_PMS_AMS_OFFSET + voice]);
	}

	return 0;
}

uint8_t YM_get_operator_phase_multiply(uint8_t voice, uint8_t op)
{
	if (voice < 8 && op < 4) {
		return get_bit_field<3, 0>(Ym_registers[YM_DT1_MUL_OFFSET + (op * 8) + voice]);
	}
	return 0;
}

uint8_t YM_get_operator_detune_1(uint8_t voice, uint8_t op)
{
	if (voice < 8 && op < 4) {
		return get_bit_field<6, 4>(Ym_registers[YM_DT1_MUL_OFFSET + (op * 8) + voice]);
	}
	return 0;
}

uint8_t YM_get_operator_total_level(uint8_t voice, uint8_t op)
{
	if (voice < 8 && op < 4) {
		return get_bit_field<6, 0>(Ym_registers[YM_TL_OFFSET + (op * 8) + voice]);
	}
	return 0;
}

uint8_t YM_get_operator_attack_rate(uint8_t voice, uint8_t op)
{
	if (voice < 8 && op < 4) {
		return get_bit_field<4, 0>(Ym_registers[YM_KS_AR_OFFSET + (op * 8) + voice]);
	}
	return 0;
}

uint8_t YM_get_operator_key_scaling(uint8_t voice, uint8_t op)
{
	if (voice < 8 && op < 4) {
		return get_bit_field<7, 6>(Ym_registers[YM_KS_AR_OFFSET + (op * 8) + voice]);
	}
	return 0;
}

uint8_t YM_get_operator_decay_rate_1(uint8_t voice, uint8_t op)
{
	if (voice < 8 && op < 4) {
		return get_bit_field<7, 4>(Ym_registers[YM_A_D1R_OFFSET + (op * 8) + voice]);
	}
	return 0;
}

uint8_t YM_get_operator_ams_enabled(uint8_t voice, uint8_t op)
{
	if (voice < 8 && op < 4) {
		return get_bit_field<7>(Ym_registers[YM_A_D1R_OFFSET + (op * 8) + voice]);
	}
	return 0;
}

uint8_t YM_get_operator_decay_rate_2(uint8_t voice, uint8_t op)
{
	if (voice < 8 && op < 4) {
		return get_bit_field<4, 0>(Ym_registers[YM_DT2_D2R_OFFSET + (op * 8) + voice]);
	}
	return 0;
}

uint8_t YM_get_operator_detune_2(uint8_t voice, uint8_t op)
{
	if (voice < 8 && op < 4) {
		return get_bit_field<7, 6>(Ym_registers[YM_DT2_D2R_OFFSET + (op * 8) + voice]);
	}
	return 0;
}

uint8_t YM_get_operator_release_rate(uint8_t voice, uint8_t op)
{
	if (voice < 8 && op < 4) {
		return get_bit_field<3, 0>(Ym_registers[YM_D1L_RR_OFFSET + (op * 8) + voice]);
	}
	return 0;
}

uint8_t YM_get_operator_decay_1_level(uint8_t voice, uint8_t op)
{
	if (voice < 8 && op < 4) {
		return get_bit_field<7, 4>(Ym_registers[YM_D1L_RR_OFFSET + (op * 8) + voice]);
	}
	return 0;
}

//
// Field Mutators
//

void YM_key_on(uint8_t channel, bool m1, bool c1, bool m2, bool c2)
{
	YM_debug_write(0x08, channel | (m1 ? 0x8 : 0) | (c1 ? 0x10 : 0) | (m2 ? 0x20 : 0) | (c2 ? 0x40 : 0));
}

void YM_set_lfo_frequency(uint8_t freq)
{
	YM_debug_write(0x18, freq);
}

void YM_set_modulation_depth(uint8_t depth)
{
	YM_debug_write(0x19, set_bit_field<6, 0>(Ym_registers[0x19], depth));
}

void YM_set_modulation_type(uint8_t mtype)
{
	YM_debug_write(0x19, set_bit_field<7>(Ym_registers[0x19], mtype));
}

void YM_set_waveform(uint8_t wf)
{
	YM_debug_write(0x1b, set_bit_field<1, 0>(Ym_registers[0x1b], wf));
}

void YM_set_control_output_1(bool enabled)
{
	YM_debug_write(0x1b, set_bit_field<6>(Ym_registers[0x1b], enabled));
}

void YM_set_control_output_2(bool enabled)
{
	YM_debug_write(0x1b, set_bit_field<7>(Ym_registers[0x1b], enabled));
}

void YM_set_voice_connection_type(uint8_t voice, uint8_t ctype)
{
	if (voice < 8) {
		const uint8_t addr = YM_R_L_FB_CONN_OFFSET + voice;
		YM_debug_write(addr, set_bit_field<2, 0>(Ym_registers[addr], ctype & 0x7));
	}
}

void YM_set_voice_self_feedback_level(uint8_t voice, uint8_t fl)
{
	if (voice < 8) {
		const uint8_t addr = YM_R_L_FB_CONN_OFFSET + voice;
		YM_debug_write(addr, set_bit_field<5, 3>(Ym_registers[addr], fl));
	}
}

void YM_set_voice_left_enable(uint8_t voice, bool enable)
{
	if (voice < 8) {
		const uint8_t addr = YM_R_L_FB_CONN_OFFSET + voice;
		YM_debug_write(addr, set_bit_field<6>(Ym_registers[addr], enable));
	}
}

void YM_set_voice_right_enable(uint8_t voice, bool enable)
{
	if (voice < 8) {
		const uint8_t addr = YM_R_L_FB_CONN_OFFSET + voice;
		YM_debug_write(addr, set_bit_field<7>(Ym_registers[addr], enable));
	}
}

void YM_set_voice_note(uint8_t voice, uint8_t note)
{
	if (voice < 8) {
		const uint8_t addr = YM_KC_OFFSET + voice;
		YM_debug_write(addr, set_bit_field<3, 0>(Ym_registers[addr], note));
	}
}

void YM_set_voice_octave(uint8_t voice, uint8_t octave)
{
	if (voice < 8) {
		const uint8_t addr = YM_KC_OFFSET + voice;
		YM_debug_write(addr, set_bit_field<6, 4>(Ym_registers[addr], octave));
	}
}

void YM_set_voice_key_fraction(uint8_t voice, uint8_t fraction)
{
	if (voice < 8) {
		const uint8_t addr = YM_KF_OFFSET + voice;
		YM_debug_write(addr, set_bit_field<7, 2>(Ym_registers[addr], fraction));
	}
}

void YM_set_voice_amplitude_modulation_sensitivity(uint8_t voice, uint8_t ams)
{
	if (voice < 8) {
		const uint8_t addr = YM_PMS_AMS_OFFSET + voice;
		YM_debug_write(addr, set_bit_field<1, 0>(Ym_registers[addr], ams));
	}
}

void YM_set_voice_phase_modulation_sensitivity(uint8_t voice, uint8_t pms)
{
	if (voice < 8) {
		const uint8_t addr = YM_PMS_AMS_OFFSET + voice;
		YM_debug_write(addr, set_bit_field<6, 4>(Ym_registers[addr], pms));
	}
}

void YM_set_operator_phase_multiply(uint8_t voice, uint8_t op, uint8_t mul)
{
	if (voice < 8 && op < 4) {
		const uint8_t addr = YM_DT1_MUL_OFFSET + (op * 8) + voice;
		YM_debug_write(addr, set_bit_field<3, 0>(Ym_registers[addr], mul));
	}
}

void YM_set_operator_detune_1(uint8_t voice, uint8_t op, uint8_t dt1)
{
	if (voice < 8 && op < 4) {
		const uint8_t addr = YM_DT1_MUL_OFFSET + (op * 8) + voice;
		YM_debug_write(addr, set_bit_field<6, 4>(Ym_registers[addr], dt1));
	}
}

void YM_set_operator_total_level(uint8_t voice, uint8_t op, uint8_t tl)
{
	if (voice < 8 && op < 4) {
		const uint8_t addr = YM_TL_OFFSET + (op * 8) + voice;
		YM_debug_write(addr, set_bit_field<6, 0>(Ym_registers[addr], tl));
	}
}

void YM_set_operator_attack_rate(uint8_t voice, uint8_t op, uint8_t ar)
{
	if (voice < 8 && op < 4) {
		const uint8_t addr = YM_KS_AR_OFFSET + (op * 8) + voice;
		YM_debug_write(addr, set_bit_field<4, 0>(Ym_registers[addr], ar));
	}
}

void YM_set_operator_key_scaling(uint8_t voice, uint8_t op, uint8_t ks)
{
	if (voice < 8 && op < 4) {
		const uint8_t addr = YM_KS_AR_OFFSET + (op * 8) + voice;
		YM_debug_write(addr, set_bit_field<7, 6>(Ym_registers[addr], ks));
	}
}

void YM_set_operator_decay_rate_1(uint8_t voice, uint8_t op, uint8_t dr1)
{
	if (voice < 8 && op < 4) {
		const uint8_t addr = YM_A_D1R_OFFSET + (op * 8) + voice;
		YM_debug_write(addr, set_bit_field<4, 0>(Ym_registers[addr], dr1));
	}
}

void YM_set_operator_ams_enabled(uint8_t voice, uint8_t op, bool enable)
{
	if (voice < 8 && op < 4) {
		const uint8_t addr = YM_A_D1R_OFFSET + (op * 8) + voice;
		YM_debug_write(addr, set_bit_field<7>(Ym_registers[addr], enable));
	}
}

void YM_set_operator_decay_rate_2(uint8_t voice, uint8_t op, uint8_t dr2)
{
	if (voice < 8 && op < 4) {
		const uint8_t addr = YM_DT2_D2R_OFFSET + (op * 8) + voice;
		YM_debug_write(addr, set_bit_field<4, 0>(Ym_registers[addr], dr2));
	}
}

void YM_set_operator_detune_2(uint8_t voice, uint8_t op, uint8_t dt2)
{
	if (voice < 8 && op < 4) {
		const uint8_t addr = YM_DT2_D2R_OFFSET + (op * 8) + voice;
		YM_debug_write(addr, set_bit_field<7, 6>(Ym_registers[addr], dt2));
	}
}

void YM_set_operator_release_rate(uint8_t voice, uint8_t op, uint8_t rr)
{
	if (voice < 8 && op < 4) {
		const uint8_t addr = YM_D1L_RR_OFFSET + (op * 8) + voice;
		YM_debug_write(addr, set_bit_field<3, 0>(Ym_registers[addr], rr));
	}
}

void YM_set_operator_decay_1_level(uint8_t voice, uint8_t op, uint8_t d1l)
{
	if (voice < 8 && op < 4) {
		const uint8_t addr = YM_D1L_RR_OFFSET + (op * 8) + voice;
		YM_debug_write(addr, set_bit_field<7, 4>(Ym_registers[addr], d1l));
	}
}
