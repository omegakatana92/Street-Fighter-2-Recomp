// OKI MSM6295 ADPCM after MAME 0.264 src/devices/sound/okim6295.cpp and
// okiadpcm.cpp (BSD-3-Clause; Mirko Buffoni, Aaron Giles and the MAME team).
// The stream plumbing is replaced: the board calls stream_update() before a
// read/write exactly where MAME updates its stream, and generate() fills
// samples in MAME's float format (one output, clamped to +-1).
#ifndef SF2_OKIM6295_H
#define SF2_OKIM6295_H

#include <cstdint>
#include "../emu/state_io.h"

struct oki_adpcm_state {
	int32_t m_signal = 0, m_step = 0, m_loop_signal = 0, m_loop_step = 0;
	bool m_saved = false;
	void reset();
	int16_t clock(uint8_t nibble);
	static void compute_tables();
	static int s_diff_lookup[49 * 16];
	static bool s_tables_computed;
};

class okim6295_device {
public:
	enum { PIN7_LOW = 0, PIN7_HIGH = 1 };
	struct voice {
		oki_adpcm_state m_adpcm;
		bool m_playing = false;
		uint32_t m_base_offset = 0, m_sample = 0, m_count = 0;
		float m_volume = 0;
	};

	void device_start(uint32_t clock, int pin7);
	void device_reset();
	uint8_t read();
	void write(uint8_t command);
	void set_pin7(int pin7);
	uint32_t sample_rate() const { return m_clock / (m_pin7_state ? 132 : 165); }
	void generate(float *out, uint32_t samples);

	const uint8_t *m_rom = nullptr;    // the "oki" region (0x40000 bytes)
	uint32_t m_rom_size = 0;
	void *ctx = nullptr;
	void (*stream_update)(void *ctx) = nullptr;            // m_stream->update()
	void (*rate_changed)(void *ctx, uint32_t rate) = nullptr;   // m_stream->set_sample_rate()

	void save(state_writer &w) const;
	void load(state_reader &r);

private:
	uint32_t m_clock = 0;
	voice m_voice[4];
	int32_t m_command = -1;
	uint8_t m_pin7_state = 0;
	uint8_t read_byte(uint32_t a) const { return a < m_rom_size ? m_rom[a] : 0; }
	void update() { if (stream_update) stream_update(ctx); }
};

#endif
