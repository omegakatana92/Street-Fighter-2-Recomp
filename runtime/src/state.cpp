// Save states of the whole board: CPUs, scheduler (timers in list order,
// pending synchronize() callbacks), devices, memory and the renderer's frame
// state.  A state names its version and ROM fingerprint and is refused by any
// other version, ROM revision or build layout.
#include "sf2/machine.h"

#include <cstring>

namespace sf2 {

kemu::timer::callback_fn sync_callback_for_kind(int kind, void **ctx);

static const uint32_t STATE_MAGIC = 0x53324653u;   // "SF2S"
static const uint32_t STATE_VERSION = 1;

// layout signature so a state from a different build layout is refused
static uint32_t build_signature()
{
	uint32_t h = 2166136261u;
	auto mix = [&](uint32_t v) { h = (h ^ v) * 16777619u; };
	mix(uint32_t(sizeof(m68000_device)));
	mix(uint32_t(sizeof(z80_device)));
	mix(uint32_t(sizeof(okim6295_device)));
	mix(uint32_t(sizeof(machine)));
	mix(STATE_VERSION);
	return h;
}

static void save_exec(const kemu::exec_device &d, state_writer &w)
{
	w.put(d.suspend); w.put(d.nextsuspend); w.put(d.eatcycles); w.put(d.nexteatcycles);
	w.put(d.totalcycles); w.put(d.localtime); w.put(d.cycles_running); w.put(d.cycles_stolen);
	for (const auto &in : d.inputs) {
		w.put(in.stored_vector); w.put(in.curvector); w.put(in.curstate); w.put(in.qindex); w.put(in.queue);
	}
}

static void load_exec(kemu::exec_device &d, state_reader &r)
{
	r.get(d.suspend); r.get(d.nextsuspend); r.get(d.eatcycles); r.get(d.nexteatcycles);
	r.get(d.totalcycles); r.get(d.localtime); r.get(d.cycles_running); r.get(d.cycles_stolen);
	for (auto &in : d.inputs) {
		r.get(in.stored_vector); r.get(in.curvector); r.get(in.curstate); r.get(in.qindex); r.get(in.queue);
		if (in.qindex < 0 || in.qindex > 32) { r.bad = true; in.qindex = 0; }
	}
}

void machine::save_state(std::vector<uint8_t> &out) const
{
	machine &self = const_cast<machine &>(*this);
	state_writer w;
	w.put(STATE_MAGIC);
	w.put(STATE_VERSION);
	w.put(build_signature());
	w.put(int32_t(m_rom.v));
	w.put(m_rom.fingerprint);

	// scheduler
	w.put(m_sched.m_basetime);
	w.put(m_sched.m_suspend_changes_pending);
	w.put(m_sched.m_quantum);
	save_exec(m_main_exec, w);
	save_exec(m_snd_exec, w);
	uint32_t ntimers = uint32_t(m_timers.size());
	w.put(ntimers);
	for (const kemu::timer *t : m_timers) {
		w.put(t->enabled); w.put(t->param); w.put(t->period); w.put(t->start); w.put(t->expire);
	}
	std::vector<std::pair<int32_t, const kemu::timer *>> active;
	for (const kemu::timer *t = m_sched.m_timer_list; t; t = t->next) {
		int32_t id = -1;
		for (size_t i = 0; i < m_timers.size(); i++)
			if (m_timers[i] == t) id = int32_t(i);
		active.push_back({id, t});
	}
	uint32_t nactive = uint32_t(active.size());
	w.put(nactive);
	for (const auto &a : active) {
		w.put(a.first);
		if (a.first < 0) {
			// temporary: an input-line queue flush (kind 1) or a synchronize() callback
			const kemu::timer *t = a.second;
			int32_t dev = t->ctx == &m_main_exec ? 0 : t->ctx == &m_snd_exec ? 1 : -1;
			w.put(t->kind); w.put(dev); w.put(t->param); w.put(t->start); w.put(t->expire); w.put(t->enabled);
		}
	}

	// CPUs and devices
	m_main.save(w);
	m_snd.save(w);
	self.m_ym.save(w);
	m_oki.save(w);

	// memory and board latches
	w.put(m_ram); w.put(m_gfxram); w.put(m_cps_a_regs); w.put(m_cps_b_regs); w.put(m_zram);
	w.put(m_bank); w.put(m_soundlatch); w.put(m_soundlatch2); w.put(m_coin_lockout); w.put(m_dsw);

	// streams (generated-but-unmixed audio is not saved: it is output, not state)
	w.put(m_ym_clock); w.put(m_oki_clock); w.put(m_oki_pending_rate);
	w.put(m_vblank_start_time); w.put(m_vblank_end_time);
	w.put(m_frame);

	m_video.save(w);
	out.swap(w.buf);
}

bool machine::validate_state(const uint8_t *data, size_t size) const
{
	state_reader r(data, size);
	uint32_t magic = 0, ver = 0, sig = 0, fp = 0;
	int32_t v = -1;
	r.get(magic); r.get(ver); r.get(sig); r.get(v); r.get(fp);
	return !r.bad && magic == STATE_MAGIC && ver == STATE_VERSION && sig == build_signature() &&
	       v == int32_t(m_rom.v) && fp == m_rom.fingerprint;
}

bool machine::load_state(const uint8_t *data, size_t size)
{
	if (!validate_state(data, size))
		return false;
	state_reader r(data, size);
	uint32_t magic, ver, sig, fp;
	int32_t v;
	r.get(magic); r.get(ver); r.get(sig); r.get(v); r.get(fp);

	// keep a copy of the current state so a damaged file cannot leave a half-loaded machine
	std::vector<uint8_t> backup;
	if (!m_in_restore)
		save_state(backup);
	auto fail = [&]() {
		if (!m_in_restore) {
			m_in_restore = true;
			load_state(backup.data(), backup.size());
			m_in_restore = false;
		}
		return false;
	};

	r.get(m_sched.m_basetime);
	r.get(m_sched.m_suspend_changes_pending);
	r.get(m_sched.m_quantum);
	load_exec(m_main_exec, r);
	load_exec(m_snd_exec, r);
	uint32_t ntimers = 0;
	r.get(ntimers);
	if (r.bad || ntimers != m_timers.size()) return fail();
	for (kemu::timer *t : m_timers) {
		r.get(t->enabled); r.get(t->param); r.get(t->period); r.get(t->start); r.get(t->expire);
		t->next = t->prev = nullptr;
	}
	// drop any temporary timers of the running machine
	for (kemu::timer *t = m_sched.m_timer_list; t; ) {
		kemu::timer *n = t->next;
		if (t->temporary) delete t;
		t = n;
	}
	m_sched.m_timer_list = nullptr;
	m_sched.m_inactive_timers = nullptr;
	uint32_t nactive = 0;
	r.get(nactive);
	if (r.bad || nactive > 4096) return fail();
	std::vector<kemu::timer *> order;
	std::vector<bool> is_active(m_timers.size(), false);
	for (uint32_t i = 0; i < nactive && !r.bad; i++) {
		int32_t id = 0;
		r.get(id);
		if (id >= 0) {
			if (size_t(id) >= m_timers.size()) return fail();
			order.push_back(m_timers[size_t(id)]);
			is_active[size_t(id)] = true;
		} else {
			kemu::timer *t = new kemu::timer();
			int32_t dev = 0;
			r.get(t->kind); r.get(dev); r.get(t->param); r.get(t->start); r.get(t->expire); r.get(t->enabled);
			t->sched = &m_sched;
			t->temporary = true;
			t->period = kemu::attotime::never();
			if (t->kind == 1) {
				t->ctx = dev == 0 ? &m_main_exec : &m_snd_exec;
				t->cb = [](void *ctx, int32_t param) { static_cast<kemu::exec_device *>(ctx)->empty_event_queue(param); };
			} else {
				t->cb = sync_callback_for_kind(t->kind, &t->ctx);
				if (!t->cb) { delete t; return fail(); }
			}
			order.push_back(t);
		}
	}
	for (size_t i = 0; i < m_timers.size(); i++)
		if (!is_active[i]) {
			kemu::timer *t = m_timers[i];
			t->in_active_list = false;
			if (m_sched.m_inactive_timers) m_sched.m_inactive_timers->prev = t;
			t->next = m_sched.m_inactive_timers;
			t->prev = nullptr;
			m_sched.m_inactive_timers = t;
		}
	kemu::timer *prev = nullptr;
	for (kemu::timer *t : order) {
		t->in_active_list = true;
		t->prev = prev;
		t->next = nullptr;
		if (prev) prev->next = t; else m_sched.m_timer_list = t;
		prev = t;
	}

	m_main.load(r);
	m_snd.load(r);
	m_ym.load(r);
	m_oki.load(r);

	r.get(m_ram); r.get(m_gfxram); r.get(m_cps_a_regs); r.get(m_cps_b_regs); r.get(m_zram);
	r.get(m_bank); r.get(m_soundlatch); r.get(m_soundlatch2); r.get(m_coin_lockout); r.get(m_dsw);
	r.get(m_ym_clock); r.get(m_oki_clock); r.get(m_oki_pending_rate);
	r.get(m_vblank_start_time); r.get(m_vblank_end_time);
	r.get(m_frame);
	m_video.load(r);

	// queued, unmixed audio belongs to the abandoned timeline
	m_ym_buf.clear();
	m_oki_hist.clear();
	m_mixed_ym = m_ym_clock.total;

	m_sched.m_execute_list = nullptr;
	m_sched.rebuild_execute_list();
	m_sched.m_executing_device = nullptr;
	m_sched.m_callback_timer = nullptr;
	if (r.bad || r.at != r.n)
		return fail();   // truncated or oversized file: restore the previous machine
	return true;
}

} // namespace sf2
