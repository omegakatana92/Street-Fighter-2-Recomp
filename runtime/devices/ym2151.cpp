// See ym2151.h.
#include "ym2151.h"
#include <algorithm>

void ym2151_device::device_start(kemu::scheduler &s, uint32_t clock, kemu::timer *t0, kemu::timer *t1, int sync_kind_mode, int sync_kind_irq)
{
	m_sched = &s;
	m_clock = clock;
	m_timer[0] = t0;
	m_timer[1] = t1;
	m_busy_end = kemu::attotime();
	m_reset_state = 1;
	m_kind_mode = sync_kind_mode;
	m_kind_irq = sync_kind_irq;
	// ymfm_device_base::device_start saves the initial state (no effect on the chip)
}

void ym2151_device::ymfm_sync_mode_write(uint8_t data)
{
	m_sched->synchronize(s_sync_mode, this, data, m_kind_mode);
}

void ym2151_device::ymfm_sync_check_interrupts()
{
	if (m_sched->currently_executing())
		m_sched->synchronize(s_sync_irq, this, 0, m_kind_irq);
	else
		m_engine->engine_check_interrupts();
}

void ym2151_device::ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks)
{
	if (duration_in_clocks >= 0)
		m_timer[tnum]->adjust(kemu::attotime::from_ticks(uint64_t(duration_in_clocks), m_clock), int32_t(tnum));
	else
		m_timer[tnum]->enable(false);
}

void ym2151_device::generate(int32_t *left, int32_t *right, uint32_t samples)
{
	ymfm::ym2151::output_data out[256];
	for (uint32_t done = 0; done < samples; ) {
		uint32_t n = std::min<uint32_t>(samples - done, 256);
		m_chip.generate(out, n);
		for (uint32_t i = 0; i < n; i++) {
			left[done + i] = out[i].data[0];
			right[done + i] = out[i].data[1];
		}
		done += n;
	}
}

void ym2151_device::save(state_writer &w)
{
	std::vector<uint8_t> blob;
	ymfm::ymfm_saved_state st(blob, true);
	m_chip.save_restore(st);
	uint32_t n = uint32_t(blob.size());
	w.put(n);
	w.raw(blob.data(), n);
	w.put(m_busy_end);
	w.put(m_reset_state);
}

void ym2151_device::load(state_reader &r)
{
	uint32_t n = 0;
	r.get(n);
	if (n > (1u << 20)) { r.bad = true; return; }
	std::vector<uint8_t> blob(n);
	r.raw(blob.data(), n);
	if (r.bad) return;
	ymfm::ymfm_saved_state st(blob, false);
	m_chip.save_restore(st);
	r.get(m_busy_end);
	r.get(m_reset_state);
}
