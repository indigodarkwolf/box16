#include "ym2151.h"

#include <queue>

#include "ymfm_opm.h"

#include "ymfm_fm.ipp"

#include "audio.h"
#include "bitutils.h"

//#define YM2151_USE_PICK 1
//#define YM2151_USE_LINEAR_INTERPOLATION 1
//#define YM2151_USE_R8BRAIN_RESAMPLING 1
#define YM2151_USE_LOWPASS_FILTER_RESAMPLING 1

#if !defined(YM2151_USE_PICK) && !defined(YM2151_USE_LINEAR_INTERPOLATION) && !defined(YM2151_USE_R8BRAIN_RESAMPLING) && !defined(YM2151_USE_LOWPASS_FILTER_RESAMPLING)
#	define YM2151_USE_LINEAR_INTERPOLATION 1
#endif

#if defined(YM2151_USE_R8BRAIN_RESAMPLING)
#	include "CDSPResampler.h"
#endif

class ym2151_interface : public ymfm::ymfm_interface
{
public:
	ym2151_interface()
	    : m_chip(*this),
	      m_chip_sample_rate(m_chip.sample_rate(YM_CLOCK_RATE)),
	      m_generation_time(0),
	      m_backbuffer_size(m_chip.sample_rate(YM_CLOCK_RATE)),
	      m_backbuffer_used(0),
	      m_previous_samples{ { 0, 0 }, { 0, 0 } },
	      m_timers{0, 0},
	      m_busy_timer{ 0 },
	      m_irq_status{ false }
	{
	}

	~ym2151_interface()
	{
	}

	//
	// timing and synchronizaton
	//

	// the chip implementation calls this when a write happens to the mode
	// register, which could affect timers and interrupts; our responsibility
	// is to ensure the system is up to date before calling the engine's
	// engine_mode_write() method
	virtual void ymfm_sync_mode_write(uint8_t data) override
	{
		m_engine->engine_mode_write(data);
	}

	// the chip implementation calls this when the chip's status has changed,
	// which may affect the interrupt state; our responsibility is to ensure
	// the system is up to date before calling the engine's
	// engine_check_interrupts() method
	virtual void ymfm_sync_check_interrupts() override
	{
		m_engine->engine_check_interrupts();
	}

	// the chip implementation calls this when one of the two internal timers
	// has changed state; our responsibility is to arrange to call the engine's
	// engine_timer_expired() method after the provided number of clocks; if
	// duration_in_clocks is negative, we should cancel any outstanding timers
	virtual void ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) override
	{
		if (tnum >= 2) {
			printf("ERROR: Not enough timers implemented for ymfm_set_timer\n");
			return;
		}
		m_timers[tnum] = duration_in_clocks;
	}

	// the chip implementation calls this to indicate that the chip should be
	// considered in a busy state until the given number of clocks has passed;
	// our responsibility is to compute and remember the ending time based on
	// the chip's clock for later checking
	virtual void ymfm_set_busy_end(uint32_t clocks) override
	{
		m_busy_timer = clocks;
	}

	// the chip implementation calls this to see if the chip is still currently
	// is a busy state, as specified by a previous call to ymfm_set_busy_end();
	// our responsibility is to compare the current time against the previously
	// noted busy end time and return true if we haven't yet passed it
	virtual bool ymfm_is_busy() override
	{
		return m_busy_timer > 0;
	}

	//
	// I/O functions
	//

	// the chip implementation calls this when the state of the IRQ signal has
	// changed due to a status change; our responsibility is to respond as
	// needed to the change in IRQ state, signaling any consumers
	virtual void ymfm_update_irq(bool asserted) override
	{
		m_irq_status = asserted;
	}

	// the chip implementation calls this whenever data is read from outside
	// of the chip; our responsibility is to provide the data requested
	virtual uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override
	{
		return 0;
	}

	// the chip implementation calls this whenever data is written outside
	// of the chip; our responsibility is to pass the written data on to any consumers
	virtual void ymfm_external_write(ymfm::access_class type, uint32_t address, uint8_t data) override
	{
		// Nop.
	}

	void update_clocks()
	{
		m_busy_timer = std::max(0, m_busy_timer - 64);
		for (int i = 0; i < 2; ++i) {
			if (m_timers[i] > 0) {
				m_timers[i] = std::max(0, m_timers[i] - 64);
				if (m_timers[i] <= 0) {
					m_engine->engine_timer_expired(i);
				}
			}
		}
	}

	void update_clocks(int cycles)
	{
		m_busy_timer = std::max(0, m_busy_timer - (64 * cycles));
		for (int i = 0; i < 2; ++i) {
			if (m_timers[i] > 0) {
				m_timers[i] = std::max(0, m_timers[i] - (64 * cycles));
				if (m_timers[i] <= 0) {
					m_engine->engine_timer_expired(i);
				}
			}
		}	
	}

	void pregenerate()
	{
		if (m_backbuffer_used < m_backbuffer_size) {
			if (m_write_queue.size() > 0) {
				auto [addr, value] = m_write_queue.front();
				m_chip.write_address(addr);
				m_chip.write_data(value, false);
			}

			m_chip.generate(&m_backbuffer[m_backbuffer_used], 1);
			update_clocks();
			++m_backbuffer_used;
		}
	}
		

	void pregenerate(uint32_t samples)
	{
		if (m_backbuffer_used + samples > m_backbuffer_size) {
			samples = m_backbuffer_size - m_backbuffer_used;
		}

		while (samples > 0 && m_write_queue.size() > 0) {
			auto [addr, value] = m_write_queue.front();
			m_chip.write_address(addr);
			m_chip.write_data(value, false);

			m_chip.generate(&m_backbuffer[m_backbuffer_used], 1);
			update_clocks();
			++m_backbuffer_used;
			--samples;

			m_write_queue.pop();
		}

		if (samples > 0) {
			m_chip.generate(&m_backbuffer[m_backbuffer_used], samples);
			update_clocks(samples);

			m_backbuffer_used += samples;
		}
	}

	void generate(int16_t *buffers, uint32_t samples, uint32_t sample_rate)
	{
		uint32_t samples_needed = samples * m_chip_sample_rate / sample_rate;
		if (m_backbuffer_used < samples_needed) {
			pregenerate(samples_needed - m_backbuffer_used);
		}

		uint32_t samples_used = 0;

#if defined(YM2151_USE_PICK)
		auto     pick         = [&samples_used, this](ymfm::ym2151::output_data &ym) {
            if (samples_used >= m_backbuffer_used) {
                pregenerate(1);
            }

            ym = m_backbuffer[samples_used];
            ++samples_used;
		};

		const uint64_t generation_step = 0x100000000ULL / m_chip_sample_rate;
		const uint64_t sample_step     = 0x100000000ULL / sample_rate;

		ymfm::ym2151::output_data ym0 = m_previous_samples[0];
		ymfm::ym2151::output_data ym1 = m_previous_samples[1];

		for (uint32_t s = 0; s < samples; ++s) {
			while (m_generation_time < sample_step) {
				ym0 = ym1;
				pick(ym1);
				m_generation_time += generation_step;
			}
			m_generation_time -= sample_step;

			*buffers = ym1.data[0];
			++buffers;
			*buffers = ym1.data[1];
			++buffers;
		}

		m_previous_samples[0] = ym0;
		m_previous_samples[1] = ym1;
#elif defined(YM2151_USE_LINEAR_INTERPOLATION)
		auto     pick         = [&samples_used, this](ymfm::ym2151::output_data &ym) {
            if (samples_used >= m_backbuffer_used) {
                pregenerate(1);
            }

            ym = m_backbuffer[samples_used];
            ++samples_used;
		};

		auto lerp = [](double v0, double v1, double x, double x_min, double x_max) -> double {
			const double ratio = (x - x_min) / (x_max - x_min);
			return v0 + (v1 - v0) * ratio;
		};

		const uint64_t generation_step = 0x100000000ULL / m_chip_sample_rate;
		const uint64_t sample_step     = 0x100000000ULL / sample_rate;

		ymfm::ym2151::output_data ym0 = m_previous_samples[0];
		ymfm::ym2151::output_data ym1 = m_previous_samples[1];

		for (uint32_t s = 0; s < samples; ++s) {
			while (m_generation_time < sample_step) {
				ym0 = ym1;
				pick(ym1);
				m_generation_time += generation_step;
			}
			m_generation_time -= sample_step;

			*buffers = (int16_t)lerp(ym0.data[0], ym1.data[0], (double)m_generation_time, 0, (double)m_chip_sample_rate);
			++buffers;
			*buffers = (int16_t)lerp(ym0.data[1], ym1.data[1], (double)m_generation_time, 0, (double)m_chip_sample_rate);
			++buffers;
		}

		m_previous_samples[0] = ym0;
		m_previous_samples[1] = ym1;

#elif defined(YM2151_USE_R8BRAIN_RESAMPLING)
		r8b::CDSPResampler16 resampler[2]{
			r8b::CDSPResampler16(m_chip_sample_rate, sample_rate, m_chip_sample_rate),
			r8b::CDSPResampler16(m_chip_sample_rate, sample_rate, m_chip_sample_rate)
		};

		for (int i = 0; i < 2; ++i) {
			double *input = static_cast<double *>(alloca(sizeof(double) * samples_needed));
			for (uint32_t s = 0; s < samples_needed; ++s) {
				input[s] = m_backbuffer[s].data[i];
			}

			double * output;
			int16_t *out_stream = &buffers[i];
			int      out_needed = samples;

			int out_written = resampler[i].process(input, samples_needed, output);
			out_written     = std::min(out_written, out_needed);
			for (int o = 0; o < out_written; ++o) {
				*out_stream = (int16_t)output[o];
				out_stream += 2;
			}
			out_needed -= out_written;

			memset(input, 0, sizeof(double) * samples_needed);
			while (out_needed > 0) {
				out_written = resampler[i].process(input, samples_needed, output);
				out_written = std::min(out_written, out_needed);
				for (int o = 0; o < out_written; ++o) {
					*out_stream = (int16_t)output[o];
					out_stream += 2;
				}
				out_needed -= out_written;
			}
		}

		samples_used = samples_needed;
#elif defined(YM2151_USE_LOWPASS_FILTER_RESAMPLING)
		// The idea is to upsample the YM2151 signal (which comes in at 55.93 kHz), 
		// then use a simple FIR lowpass-filter to restrict the signal to a 44 kHz band.
		// This will then remove the high requency content responsible for aliasing.
		// Then, the signal is downsampled again (how??)

		const int32_t old_ringbuffer_end = m_ringbuffer_end;
		const int32_t old_ringbuffer_end_2 = m_ringbuffer_end_2;

		// Upsample the signal
		for (int32_t s = 0; s < samples_needed; s++) {
			// insert the original sample first
			upsampled_input_ring_buffers[0][m_ringbuffer_end] = (float) m_backbuffer[s].data[0];
			upsampled_input_ring_buffers[1][m_ringbuffer_end] = (float) m_backbuffer[s].data[1];
			ringbuffer_advance(m_ringbuffer_end);

			// then pad with zeros.
			for (int i = 1; i < upsampling_factor; i++) {
				upsampled_input_ring_buffers[0][m_ringbuffer_end] = 0.f;
				upsampled_input_ring_buffers[1][m_ringbuffer_end] = 0.f;
				ringbuffer_advance(m_ringbuffer_end);
			}
		}

		// Filter the signal
		for (int32_t s = 0; s < upsampling_factor * samples_needed; s++) {
			// find the starting index of that sample in the ring buffer
			int32_t start_sample = (old_ringbuffer_end + s) % ringbuffer_size;
			for (int i = 0; i < 2; i++) {
				float sum = 0.f;
				int32_t input_idx = start_sample;
				for (int32_t k = 0; k < filter_kernel_length; k++) {
					sum += filter_kernel[k] * upsampled_input_ring_buffers[i][input_idx];
					ringbuffer_revert(input_idx);
				}
				m_filtered_signal_buffer[i][m_ringbuffer_end_2] = sum;
				ringbuffer_advance(m_ringbuffer_end_2);
			}
		}

		// Downsample: "pick" strategy
		int16_t *out_streams[2] = {&buffers[0], &buffers[1]};
		for (uint32_t s = 0; s < samples; s++) {
			int32_t pick_index = (old_ringbuffer_end_2 + (int32_t)(s * (m_chip_sample_rate * upsampling_factor) / sample_rate)) % ringbuffer_size;
			for (int i = 0; i < 2; i++) {
				*out_streams[i] = (int16_t)(m_filtered_signal_buffer[i][pick_index]);
				out_streams[i] += 2;
			}
		}

		samples_used = samples_needed;
#endif

		if (samples_used < m_backbuffer_used) {
			memmove(&m_backbuffer[0], &m_backbuffer[samples_used], sizeof(ymfm::ym2151::output_data) * (m_backbuffer_used - samples_used));
			m_backbuffer_used -= samples;
		} else {
			m_backbuffer_used = 0;
		}
	}

	void clear_backbuffer()
	{
		m_backbuffer_used = 0;
	}

	void write(uint8_t addr, uint8_t value)
	{
		if (ymfm_is_busy()) {
			if (YM_is_strict()) {
				printf("WARN: Write to YM2151 ($%02X <- $%02X) while busy.\n", (int)addr, (int)value);
			} else {
				m_write_queue.push({ addr, value });
			}
		} else {
			m_chip.write_address(addr);
			m_chip.write_data(value, false);
		}
	}

	void reset()
	{
		m_chip.reset();
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

	bool get_irq_status()
	{
		return m_irq_status;
	}

	uint32_t get_sample_rate() const
	{
		return m_chip_sample_rate;
	}

private:
	ymfm::ym2151 m_chip;
	uint32_t     m_chip_sample_rate;
	uint64_t     m_generation_time;

	ymfm::ym2151::output_data m_backbuffer[YM_SAMPLE_RATE];
	uint32_t                  m_backbuffer_size;
	uint32_t                  m_backbuffer_used;

	std::queue<std::tuple<uint8_t, uint8_t>> m_write_queue;

	ymfm::ym2151::output_data m_previous_samples[2];

	int32_t m_timers[2];
	int32_t m_busy_timer;

	bool m_irq_status;

#if defined(YM2151_USE_LOWPASS_FILTER_RESAMPLING)
	static constexpr int upsampling_factor = 4;
	static constexpr int ringbuffer_size = YM_SAMPLE_RATE * upsampling_factor;
	float upsampled_input_ring_buffers[2][ringbuffer_size];
	int32_t m_ringbuffer_begin = 0; // corresponds to the oldest sample
	int32_t m_ringbuffer_end = 1; // corresponds to (one past) the newest sample

	void ringbuffer_advance(int32_t &x) {
		x = (x + 1) % ringbuffer_size;
	}

	void ringbuffer_revert(int32_t &x) {
		x = (x - 1) % ringbuffer_size;
	}

	static constexpr int filter_kernel_length = 32;
	static constexpr float filter_kernel[filter_kernel_length] = {
		1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f,
		0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f,
		0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
	
	float m_filtered_signal_buffer[2][ringbuffer_size];
	int32_t m_ringbuffer_begin_2 = 0;
	int32_t m_ringbuffer_end_2 = 1;
#endif
};

static ym2151_interface Ym_interface;
static uint8_t          Last_address = 0;
static uint8_t          Last_data    = 0;
static uint8_t          Ym_registers[256];
static bool             Ym_irq_enabled = false;
static bool             Ym_strict_busy = false;

void YM_prerender(uint32_t clocks)
{
	static uint32_t clocks_elapsed = 0;
	clocks_elapsed += clocks;

	const uint32_t clocks_per_sample = 8000000 / Ym_interface.get_sample_rate();
	const uint32_t samples_to_render = clocks_elapsed / clocks_per_sample;

	if (samples_to_render > 0) {
		Ym_interface.pregenerate(samples_to_render);
		clocks_elapsed -= samples_to_render * clocks_per_sample;
	}
}

void YM_render(int16_t *buffer, uint32_t samples, uint32_t sample_rate)
{
	Ym_interface.generate(buffer, samples, sample_rate);
}

void YM_clear_backbuffer()
{
	Ym_interface.clear_backbuffer();
}

uint32_t YM_get_sample_rate()
{
	return Ym_interface.get_sample_rate();
}

bool YM_irq_is_enabled()
{
	return Ym_irq_enabled;
}

void YM_set_irq_enabled(bool enabled)
{
	Ym_irq_enabled = enabled;
}

bool YM_is_strict()
{
	return Ym_strict_busy;
}

void YM_set_strict_busy(bool enable)
{
	Ym_strict_busy = enable;
}

void YM_write(uint8_t offset, uint8_t value)
{
	// save the hassle to add interface to dig into opm_registers by caching the writes here
	if (offset & 1) { // data port
		Last_data                  = value;
		Ym_registers[Last_address] = Last_data;

		Ym_interface.write(Last_address, Last_data);
	} else { // address port
		Last_address = value;
	}
}

uint8_t YM_read_status()
{
	return Ym_interface.read_status();
}

bool YM_irq()
{
	return Ym_irq_enabled && Ym_interface.get_irq_status();
}

void YM_reset()
{
	Ym_interface.reset();
	memset(Ym_registers, 0, 256);
	memset(&Ym_registers[0x20], 0xc0, 8);
}

void YM_debug_write(uint8_t addr, uint8_t value)
{
	Ym_registers[addr] = value;
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
	data.amplitude_modulation = Ym_interface.get_AMD();
	data.phase_modulation     = Ym_interface.get_PMD();
	data.LFO_phase            = (Ym_interface.get_LFO_phase() & ((1 << 30) - 1)) / (float)(1 << 30);
}

void YM_get_slot_state(uint8_t slnum, ym_slot_state &data)
{
	data.frequency = Ym_interface.get_freq(slnum);
	data.eg_output = (1024 - Ym_interface.get_EG_output(slnum)) / 1024.f;
	data.final_env = (1024 - Ym_interface.get_final_env(slnum)) / 1024.f;
	data.env_state = Ym_interface.get_env_state(slnum);
}

uint16_t YM_get_timer_counter(uint8_t tnum)
{
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
