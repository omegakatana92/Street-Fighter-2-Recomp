// YM2151 (OPM) as MAME 0.264 runs it: the ymfm core (3rdparty/ymfm, BSD-3-Clause,
// Aaron Giles) behind ymfm_mame.h's ym_generic_device / ymfm_device_base and
// ymopm.cpp, with MAME's timers, synchronize() calls, busy flag and stream
// update points mapped onto the kemu scheduler.
#ifndef SF2_YM2151_H
#define SF2_YM2151_H

#include <cstdint>
#include <vector>
#include "../third_party/ymfm/ymfm_opm.h"
#include "../emu/kemu.h"
#include "../emu/state_io.h"

class ym2151_device : public ymfm::ymfm_interface {
public:
	ym2151_device() : m_chip(*this) {}
	// timer / sync kinds for save states are assigned by the board
	void device_start(kemu::scheduler &s, uint32_t clock, kemu::timer *t0, kemu::timer *t1, int sync_kind_mode, int sync_kind_irq);
	void device_reset() { m_chip.reset(); }
	uint32_t sample_rate() const { return m_chip.sample_rate(m_clock); }

	uint8_t read(uint32_t offset) { update(); return m_chip.read(offset); }
	void write(uint32_t offset, uint8_t data) { if (m_reset_state == 0) return; update(); m_chip.write(offset, data); }
	void generate(int32_t *left, int32_t *right, uint32_t samples);

	// callbacks from the board's timers / synchronize()
	void timer_expired(int32_t param) { m_engine->engine_timer_expired(uint32_t(param)); }
	void sync_mode_write(int32_t param) { m_engine->engine_mode_write(uint8_t(param)); }
	void sync_check_interrupts(int32_t) { m_engine->engine_check_interrupts(); }

	void *ctx = nullptr;
	void (*stream_update)(void *ctx) = nullptr;
	void (*irq_handler)(void *ctx, int state) = nullptr;
	static void s_sync_mode(void *self, int32_t p) { static_cast<ym2151_device *>(self)->sync_mode_write(p); }
	static void s_sync_irq(void *self, int32_t p) { static_cast<ym2151_device *>(self)->sync_check_interrupts(p); }

	void save(state_writer &w);
	void load(state_reader &r);

protected:
	void ymfm_sync_mode_write(uint8_t data) override;
	void ymfm_sync_check_interrupts() override;
	void ymfm_set_timer(uint32_t tnum, int32_t duration_in_clocks) override;
	void ymfm_update_irq(bool asserted) override { if (irq_handler) irq_handler(ctx, asserted ? kemu::ASSERT_LINE : kemu::CLEAR_LINE); }
	void ymfm_set_busy_end(uint32_t clocks) override { m_busy_end = m_sched->time() + kemu::attotime::from_ticks(clocks, m_clock); }
	bool ymfm_is_busy() override { return m_sched->time() < m_busy_end; }

private:
	void update() { if (stream_update) stream_update(ctx); }
	ymfm::ym2151 m_chip;
	kemu::scheduler *m_sched = nullptr;
	uint32_t m_clock = 0;
	kemu::timer *m_timer[2] = {nullptr, nullptr};
	kemu::attotime m_busy_end;
	uint8_t m_reset_state = 1;
	int m_kind_mode = 0, m_kind_irq = 0;
};

#endif
