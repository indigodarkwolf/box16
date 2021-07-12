#include "ym2151.h"

#include "ymfm_opm.h"
#include "audio.h"

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

	void write(uint8_t offset, uint8_t value)
	{
		m_chip.write(offset, value);
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
			case ymfm::EG_ATTACK:  return 1;
			case ymfm::EG_DECAY:   return 2;
			case ymfm::EG_SUSTAIN: return 3;
			case ymfm::EG_RELEASE: return 4;
			default:               return 0;
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

ym2151_interface Ym_interface;
uint8_t          last_address;
uint8_t          last_data;

void YM_render(int16_t *stream, uint32_t samples, uint32_t buffer_sample_rate)
{
	Ym_interface.generate(stream, samples, buffer_sample_rate);
}

void YM_write(uint8_t offset, uint8_t value)
{
	audio_lock_scope lock;
	// save the hassle to add interface to dig into opm_registers by caching the writes here
	if (offset & 1) { // data port
		last_data = value;
	} else { // address port
		last_address = value;
	}
	Ym_interface.write(offset, value);
}

void YM_debug_write(uint8_t addr, uint8_t value)
{
	audio_lock_scope lock;
	Ym_interface.debug_write(addr, value);
}

uint8_t YM_debug_read(uint8_t addr)
{
	return Ym_interface.debug_read(addr);
}

uint8_t YM_read_status()
{
	audio_lock_scope lock;
	return Ym_interface.read_status();
}

uint8_t YM_last_address()
{
	return last_address;
}

uint8_t YM_last_data()
{
	return last_data;
}

uint8_t YM_get_AMD()
{
	return Ym_interface.get_AMD();
}

uint8_t YM_get_PMD()
{
	return Ym_interface.get_PMD();
}

float YM_get_LFO_phase()
{
	return (Ym_interface.get_LFO_phase() & ((1 << 30) - 1)) / (float) (1 << 30);
}

uint32_t YM_get_freq(uint8_t slnum)
{
	return Ym_interface.get_freq(slnum);
}

float YM_get_EG_output(uint8_t slnum)
{
	return (1024 - Ym_interface.get_EG_output(slnum)) / 1024.f;
}

float YM_get_final_env(uint8_t slnum)
{
	return (1024 - Ym_interface.get_final_env(slnum)) / 1024.f;
}

uint8_t YM_get_env_state(uint8_t slnum)
{
	return Ym_interface.get_env_state(slnum);
}

uint16_t YM_get_timer_counter(uint8_t tnum)
{
	return Ym_interface.get_timer_counter(tnum);
}
