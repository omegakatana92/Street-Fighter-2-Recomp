// See okim6295.h.  Function bodies follow MAME 0.264 okim6295.cpp / okiadpcm.cpp.
#include "okim6295.h"
#include <algorithm>
#include <cmath>

int oki_adpcm_state::s_diff_lookup[49 * 16];
bool oki_adpcm_state::s_tables_computed = false;
static const int8_t s_index_shift[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

void oki_adpcm_state::reset()
{
	m_signal = m_loop_signal = 0;
	m_step = m_loop_step = 0;
	m_saved = false;
}

int16_t oki_adpcm_state::clock(uint8_t nibble)
{
	m_signal += s_diff_lookup[m_step * 16 + (nibble & 15)];
	if (m_signal > 2047)
		m_signal = 2047;
	else if (m_signal < -2048)
		m_signal = -2048;
	m_step += s_index_shift[nibble & 7];
	if (m_step > 48)
		m_step = 48;
	else if (m_step < 0)
		m_step = 0;
	return int16_t(m_signal);
}

void oki_adpcm_state::compute_tables()
{
	if (s_tables_computed)
		return;
	s_tables_computed = true;
	static const int8_t nbl2bit[16][4] = {
		{ 1, 0, 0, 0}, { 1, 0, 0, 1}, { 1, 0, 1, 0}, { 1, 0, 1, 1},
		{ 1, 1, 0, 0}, { 1, 1, 0, 1}, { 1, 1, 1, 0}, { 1, 1, 1, 1},
		{-1, 0, 0, 0}, {-1, 0, 0, 1}, {-1, 0, 1, 0}, {-1, 0, 1, 1},
		{-1, 1, 0, 0}, {-1, 1, 0, 1}, {-1, 1, 1, 0}, {-1, 1, 1, 1}
	};
	for (int step = 0; step <= 48; step++) {
		int stepval = int(std::floor(16.0 * std::pow(11.0 / 10.0, double(step))));
		for (int nib = 0; nib < 16; nib++)
			s_diff_lookup[step * 16 + nib] = nbl2bit[nib][0] *
					(stepval * nbl2bit[nib][1] + stepval / 2 * nbl2bit[nib][2] +
					 stepval / 4 * nbl2bit[nib][3] + stepval / 8);
	}
}

static const float s_volume_table[16] = {
	float(0x20) / float(0x20), float(0x16) / float(0x20), float(0x10) / float(0x20), float(0x0b) / float(0x20),
	float(0x08) / float(0x20), float(0x06) / float(0x20), float(0x04) / float(0x20), float(0x03) / float(0x20),
	float(0x02) / float(0x20), 0, 0, 0, 0, 0, 0, 0
};

void okim6295_device::device_start(uint32_t clock, int pin7)
{
	oki_adpcm_state::compute_tables();
	m_clock = clock;
	m_pin7_state = uint8_t(pin7 ? 1 : 0);
	m_command = -1;
	for (auto &v : m_voice) v = voice();
}

void okim6295_device::device_reset()
{
	update();
	for (auto &v : m_voice)
		v.m_playing = false;
}

void okim6295_device::set_pin7(int pin7)
{
	m_pin7_state = uint8_t(pin7 ? 1 : 0);
	if (rate_changed) rate_changed(ctx, sample_rate());   // device_clock_changed
}

uint8_t okim6295_device::read()
{
	uint8_t result = 0xf0;
	update();
	for (int v = 0; v < 4; v++)
		if (m_voice[v].m_playing)
			result |= uint8_t(1 << v);
	return result;
}

void okim6295_device::write(uint8_t command)
{
	if (m_command != -1) {
		int voicemask = command >> 4;
		update();
		for (int v = 0; v < 4; v++, voicemask >>= 1)
			if (voicemask & 1) {
				voice &vo = m_voice[v];
				if (!vo.m_playing) {
					uint32_t base = uint32_t(m_command) * 8;
					uint32_t start = (read_byte(base + 0) << 16) | (read_byte(base + 1) << 8) | read_byte(base + 2);
					start &= 0x3ffff;
					uint32_t stop = (read_byte(base + 3) << 16) | (read_byte(base + 4) << 8) | read_byte(base + 5);
					stop &= 0x3ffff;
					if (start < stop) {
						vo.m_playing = true;
						vo.m_base_offset = start;
						vo.m_sample = 0;
						vo.m_count = 2 * (stop - start + 1);
						vo.m_adpcm.reset();
						vo.m_volume = s_volume_table[command & 0x0f];
					}
				}
			}
		m_command = -1;
	} else if (command & 0x80)
		m_command = command & 0x7f;
	else {
		update();
		int voicemask = command >> 3;
		for (int v = 0; v < 4; v++, voicemask >>= 1)
			if (voicemask & 1)
				m_voice[v].m_playing = false;
	}
}

void okim6295_device::generate(float *out, uint32_t samples)
{
	std::fill(out, out + samples, 0.0f);
	for (auto &vo : m_voice) {
		if (!vo.m_playing)
			continue;
		for (uint32_t i = 0; i < samples; i++) {
			int nibble = read_byte(vo.m_base_offset + vo.m_sample / 2) >> (((vo.m_sample & 1) << 2) ^ 4);
			// write_stream_view::add_int(index, v, 2048)
			out[i] += float(vo.m_adpcm.clock(uint8_t(nibble)) * vo.m_volume) * (1.0f / 2048.0f);
			if (++vo.m_sample >= vo.m_count) {
				vo.m_playing = false;
				break;
			}
		}
	}
	for (uint32_t i = 0; i < samples; i++)
		out[i] = std::clamp(out[i], -1.0f, 1.0f);
}

void okim6295_device::save(state_writer &w) const
{
	w.put(m_command); w.put(m_pin7_state);
	for (const auto &v : m_voice) {
		w.put(v.m_playing); w.put(v.m_sample); w.put(v.m_count); w.put(v.m_adpcm.m_signal); w.put(v.m_adpcm.m_step);
		w.put(v.m_adpcm.m_loop_signal); w.put(v.m_adpcm.m_loop_step); w.put(v.m_adpcm.m_saved);
		w.put(v.m_volume); w.put(v.m_base_offset);
	}
}

void okim6295_device::load(state_reader &r)
{
	r.get(m_command); r.get(m_pin7_state);
	for (auto &v : m_voice) {
		r.get(v.m_playing); r.get(v.m_sample); r.get(v.m_count); r.get(v.m_adpcm.m_signal); r.get(v.m_adpcm.m_step);
		r.get(v.m_adpcm.m_loop_signal); r.get(v.m_adpcm.m_loop_step); r.get(v.m_adpcm.m_saved);
		r.get(v.m_volume); r.get(v.m_base_offset);
	}
}
