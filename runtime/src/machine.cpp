// CPS-1 board model for the Street Fighter II sets; see include/sf2/machine.h.
#include "sf2/machine.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <cstdio>

using kemu::attotime;
using kemu::attoseconds_t;

namespace sf2 {

static machine *g_m = nullptr;
FILE *g_evlog = nullptr;   // development event log (headless --event-log)

// timer kinds (save states identify callbacks by kind; 1 is the scheduler's
// input-line queue flush)
enum {
	K_SOUND_UPDATE = 10, K_VBLANK_BEGIN, K_VBLANK_END, K_SCANLINE0, K_YM_TIMER0, K_YM_TIMER1,
	K_SYNC_YM_MODE = 20, K_SYNC_YM_IRQ, K_SYNC_LATCH1, K_SYNC_LATCH2,
};

static const version_info VERSIONS[VERSION_COUNT] = {
	{ version::world_warrior, "sf2_world_warrior", "sf2", "The World Warrior",
	  "Street Fighter II: The World Warrior (World 910522)", "sf2.zip", 10000000 },
	{ version::champion_edition, "sf2_champion_edition", "sf2ce", "Champion Edition",
	  "Street Fighter II': Champion Edition (World 920513)", "sf2ce.zip", 12000000 },
	{ version::hyper_fighting, "sf2_hyper_fighting", "sf2hf", "Hyper Fighting",
	  "Street Fighter II': Hyper Fighting (World 921209)", "sf2hf.zip", 12000000 },
};

const version_info &info(version v) { return VERSIONS[int(v)]; }

const version_info *find_version(const char *s)
{
	if (!s) return nullptr;
	for (const auto &v : VERSIONS)
		if (!std::strcmp(s, v.id) || !std::strcmp(s, v.mame_set))
			return &v;
	return nullptr;
}

// ------------------------------------------------------------------ trampolines

static uint16_t main_read16_cb(void *, offs_t a, uint16_t mask) { return g_m->main_read16(a, mask); }
static void main_write16_cb(void *, offs_t a, uint16_t d, uint16_t mask) { g_m->main_write16(a, d, mask); }
static void main_run(void *) { g_m->m_main.execute_run(); }
static void main_set_input(void *, int line, int state) { g_m->m_main.execute_set_input(line, state); }
static void main_reset_device(void *) { g_m->m_main.device_reset(); }

static uint8_t snd_read_cb(void *, uint16_t a) { return g_m->snd_read(a); }
static void snd_write_cb(void *, uint16_t a, uint8_t d) { g_m->snd_write(a, d); }
static void snd_run(void *) { g_m->m_snd.execute_run(); }
static void snd_set_input(void *, int line, int state) { g_m->m_snd.execute_set_input(line, state); }
static void snd_reset_device(void *) { g_m->m_snd.device_reset(); }

static void ym_stream_update(void *) { g_m->ym_update(); }
static void ym_irq(void *, int state) { g_m->m_snd_exec.set_input_line(0, state); }   // set_inputline(m_audiocpu, 0)
static void oki_stream_update(void *) { g_m->oki_update(); }
static void oki_rate_changed(void *, uint32_t rate)
{
	// sound_stream::set_sample_rate: applied at the next sound manager update
	if (rate != g_m->m_oki_clock.rate) g_m->m_oki_pending_rate = rate;
	else g_m->m_oki_pending_rate = 0;
}

static void t_sound_update(void *, int32_t) { g_m->sound_manager_update(); }
static void t_vblank_begin(void *, int32_t) { g_m->vblank_begin(); }
static void t_vblank_end(void *, int32_t) { g_m->vblank_end(); }
static void t_scanline0(void *, int32_t) { g_m->scanline0(); }
static void t_ym_timer(void *, int32_t p) { g_m->m_ym.timer_expired(p); }
// generic_latch_8_device::sync_callback
static void t_latch1(void *, int32_t p) { g_m->m_soundlatch = uint8_t(p); }
static void t_latch2(void *, int32_t p) { g_m->m_soundlatch2 = uint8_t(p); }

// temporary timers recreated from a save state
kemu::timer::callback_fn sync_callback_for_kind(int kind, void **ctx)
{
	switch (kind) {
	case K_SYNC_YM_MODE: *ctx = &g_m->m_ym; return ym2151_device::s_sync_mode;
	case K_SYNC_YM_IRQ: *ctx = &g_m->m_ym; return ym2151_device::s_sync_irq;
	case K_SYNC_LATCH1: *ctx = g_m; return t_latch1;
	case K_SYNC_LATCH2: *ctx = g_m; return t_latch2;
	}
	return nullptr;
}

// ------------------------------------------------------------------ streams

void stream_clock::set_rate(uint32_t r)
{
	rate = r;
	sample_attos = r == 0 ? kemu::ATTOSECONDS_PER_SECOND : (kemu::ATTOSECONDS_PER_SECOND + r - 1) / r;
}

uint32_t stream_clock::advance_to(const attotime &t)
{
	// stream_buffer::time_to_buffer_index(time, round_up = true, allow_expansion = true)
	uint32_t sample = uint32_t((t.atto + (sample_attos - 1)) / sample_attos);
	if (t.sec > end_second || (t.sec == end_second && sample > end_sample)) {
		uint64_t before = uint64_t(end_second) * rate + end_sample;
		end_sample = sample;
		end_second = t.sec;
		if (sample >= rate) {
			end_sample -= rate;
			end_second++;
		}
		uint64_t after = uint64_t(end_second) * rate + end_sample;
		total += after - before;
		return uint32_t(after - before);
	}
	return 0;
}

// ------------------------------------------------------------------ machine

machine::machine()
{
	g_m = this;
}

machine::~machine()
{
	if (g_m == this)
		g_m = nullptr;
}

bool machine::load(version v, const char *path, std::string &err)
{
	return load_roms(v, path, m_rom, err);
}

void machine::start_devices()
{
	g_m = this;
	m_sched = kemu::scheduler();
	m_timers.clear();
	auto alloc = [&](kemu::timer::callback_fn cb, int kind) {
		kemu::timer *t = m_sched.timer_alloc(cb, this, kind);
		m_timers.push_back(t);
		return t;
	};
	const version_info &vi = info(m_rom.v);

	// sound_manager: periodic update timer, created with the machine
	m_sound_update_timer = alloc(t_sound_update, K_SOUND_UPDATE);
	m_sound_update_timer->adjust(attotime::from_hz_u(50), 0, attotime::from_hz_u(50));

	// maincpu (device order: maincpu, audiocpu, screen, ..., 2151, oki)
	m_main_exec = kemu::exec_device();
	m_main_exec.configure(m_sched, "maincpu", vi.main_clock);
	m_main_exec.icountptr = &m_main.icount();
	m_main_exec.run = main_run;
	m_main_exec.set_input = main_set_input;
	m_main_exec.reset_device = main_reset_device;
	m_main.m_exec = &m_main_exec;
	m_main.m_bus.read16 = main_read16_cb;
	m_main.m_bus.write16 = main_write16_cb;
	m_main.start();

	m_snd_exec = kemu::exec_device();
	m_snd_exec.configure(m_sched, "audiocpu", Z80_CLOCK);
	m_snd_exec.icountptr = &m_snd.icount();
	m_snd_exec.run = snd_run;
	m_snd_exec.set_input = snd_set_input;
	m_snd_exec.reset_device = snd_reset_device;
	m_snd.m_exec = &m_snd_exec;
	m_snd.m_bus.read = snd_read_cb;
	m_snd.m_bus.write = snd_write_cb;
	m_snd.device_start();

	// screen: set_raw(8 MHz, 512, 64, 448, 262, 16, 240)
	m_frame_period = attoseconds_t(kemu::ATTOSECONDS_PER_SECOND / 8000000) * 512 * 262;
	m_vblank_period = m_frame_period / 262 * (262 - (240 - 16));
	m_scantime = m_frame_period / SCREEN_H;
	m_pixeltime = m_frame_period / (SCREEN_H * SCREEN_W);
	m_vblank_begin_timer = alloc(t_vblank_begin, K_VBLANK_BEGIN);
	m_vblank_end_timer = alloc(t_vblank_end, K_VBLANK_END);
	m_scanline0_timer = alloc(t_scanline0, K_SCANLINE0);
	m_vblank_start_time = attotime();
	m_vblank_end_time = attotime(0, m_vblank_period);
	m_vblank_begin_timer->adjust(time_until_pos(VIS_Y1 + 1));
	m_scanline0_timer->adjust(time_until_pos(0));

	// YM2151 (ym_generic_device::device_start: two timers)
	m_ym_timer[0] = alloc(t_ym_timer, K_YM_TIMER0);
	m_ym_timer[1] = alloc(t_ym_timer, K_YM_TIMER1);
	m_ym.device_start(m_sched, YM_CLOCK, m_ym_timer[0], m_ym_timer[1], K_SYNC_YM_MODE, K_SYNC_YM_IRQ);
	m_ym.ctx = this;
	m_ym.stream_update = ym_stream_update;
	m_ym.irq_handler = ym_irq;
	m_ym_clock = stream_clock();
	m_ym_clock.set_rate(m_ym.sample_rate());

	// OKI6295, pin 7 high
	m_oki.device_start(OKI_CLOCK, okim6295_device::PIN7_HIGH);
	m_oki.m_rom = m_rom.oki.data();
	m_oki.m_rom_size = uint32_t(m_rom.oki.size());
	m_oki.ctx = this;
	m_oki.stream_update = oki_stream_update;
	m_oki.rate_changed = oki_rate_changed;
	m_oki_clock = stream_clock();
	m_oki_clock.set_rate(m_oki.sample_rate());
	m_oki_pending_rate = 0;
	m_ym_buf.clear();
	m_oki_hist.clear();
	m_mixed_ym = 0;

	m_sched.rebuild_execute_list();
	m_sched.set_quantum(attotime::from_hz_u(60).atto);
}

void machine::reset_machine()
{
	// device_t::reset over the device tree in creation order, then machine_reset
	m_main_exec.pre_reset();
	m_main.device_reset();
	m_snd_exec.pre_reset();
	m_snd.device_reset();
	m_ym.device_reset();
	m_oki.device_reset();
	m_bank = 0;
	m_main_exec.post_reset();
	m_snd_exec.post_reset();
}

void machine::power_on()
{
	std::memset(m_ram, 0, sizeof(m_ram));
	std::memset(m_zram, 0, sizeof(m_zram));
	m_soundlatch = m_soundlatch2 = 0;
	m_frame = 0;
	m_coin_lockout[0] = m_coin_lockout[1] = 0;
	m_video.~cps1_video();
	new (&m_video) cps1_video();
	// video_start clears gfx RAM and the CPS-A/B registers
	m_video.start(cps1_find_config(info(m_rom.v).mame_set), m_rom.gfx.data(), uint32_t(m_rom.gfx.size()),
	              m_gfxram, m_cps_a_regs, m_cps_b_regs);
	start_devices();
	reset_machine();
}

void machine::run_frame()
{
	g_m = this;
	m_frame_done = false;
	m_audio.clear();
	while (!m_frame_done)
		m_sched.timeslice();
	mix_up_to_now();
}

// ------------------------------------------------------------------ screen

attotime machine::time_until_pos(int vpos, int hpos) const
{
	vpos += SCREEN_H - (VIS_Y1 + 1);
	vpos %= SCREEN_H;
	attoseconds_t targetdelta = attoseconds_t(vpos) * m_scantime + attoseconds_t(hpos) * m_pixeltime;
	attoseconds_t curdelta = (m_sched.time() - m_vblank_start_time).as_attoseconds();
	if (targetdelta <= curdelta + m_pixeltime / 2)
		targetdelta += m_frame_period;
	while (targetdelta <= curdelta)
		targetdelta += m_frame_period;
	return attotime(0, targetdelta - curdelta);
}

void machine::vblank_begin()
{
	m_vblank_start_time = m_sched.time();
	m_vblank_end_time = m_vblank_start_time + attotime(0, m_vblank_period);

	// video_manager::frame_update (screen update, Lua frame notifiers)
	m_frame++;
	if (m_render)
		m_video.screen_update();
	if (on_vblank_hook)
		on_vblank_hook(*this, hook_ctx);

	// device_execute_interface::on_vblank -> cps_state::cps1_interrupt
	if (!m_main_exec.suspended(kemu::SUSPEND_REASON_HALT | kemu::SUSPEND_REASON_RESET | kemu::SUSPEND_REASON_DISABLE | kemu::SUSPEND_REASON_CLOCK))
		m_main_exec.set_input_line(2, kemu::ASSERT_LINE);

	// screen_vblank_cps1(1)
	m_video.screen_vblank();

	m_vblank_begin_timer->adjust(time_until_pos(VIS_Y1 + 1));
	m_vblank_end_timer->adjust(m_vblank_end_time - m_sched.time());
	m_frame_done = true;
}

void machine::vblank_end()
{
	// screen_vblank_cps1(0) does nothing
}

void machine::scanline0()
{
	// reset_partial_updates
	m_scanline0_timer->adjust(time_until_pos(0));
}

// ------------------------------------------------------------------ sound

void machine::ym_update()
{
	attotime start = m_ym_clock.end_time();
	attotime end = m_sched.time();
	if (start >= end)
		return;
	uint32_t n = m_ym_clock.advance_to(end);
	if (!n)
		return;
	std::vector<int32_t> l(n), r(n);
	m_ym.generate(l.data(), r.data(), n);
	// mono speaker: both outputs at 0.35 (put_int(v, 32768))
	for (uint32_t i = 0; i < n; i++)
		m_ym_buf.push_back(float(l[i]) * (1.0f / 32768.0f) * 0.35f + float(r[i]) * (1.0f / 32768.0f) * 0.35f);
}

void machine::oki_update()
{
	attotime start = m_oki_clock.end_time();
	attotime end = m_sched.time();
	if (start >= end)
		return;
	uint32_t n = m_oki_clock.advance_to(end);
	if (!n)
		return;
	size_t at = m_oki_hist.size();
	m_oki_hist.resize(at + n);
	m_oki.generate(&m_oki_hist[at], n);
	for (uint32_t i = 0; i < n; i++)
		m_oki_hist[at + i] *= 0.30f;
}

// Hand the frontend the YM2151 stream (55,930 Hz) with the OKI stream added,
// each sample taking the OKI value current at its time (both streams are
// generated by the board exactly where MAME generates them; only this final
// mix differs from MAME's resampling speaker).
void machine::mix_up_to_now()
{
	ym_update();
	oki_update();
	const double ym_rate = double(m_ym_clock.rate), oki_rate = double(m_oki_clock.rate);
	const uint64_t oki_total = m_oki_clock.total;
	const uint64_t oki_first = oki_total - m_oki_hist.size();
	size_t used_ym = 0;
	for (size_t i = 0; i < m_ym_buf.size(); i++) {
		uint64_t ys = m_mixed_ym + i;
		// MAME's default resampler delays each input by its latency: 1 sample for
		// the undersampled OKI stream, 2 samples for the oversampled YM2151 stream
		// (sound.cpp default_resampler_stream); keep that relative alignment.
		double opos = double(ys) * oki_rate / ym_rate - 1.0 + 2.0 * oki_rate / ym_rate;
		if (opos >= double(oki_total))
			break;
		float o = 0.0f;
		if (opos >= 0.0) {
			uint64_t os = uint64_t(opos);
			if (os >= oki_first && os < oki_total) o = m_oki_hist[size_t(os - oki_first)];
		}
		m_audio.push_back(m_ym_buf[i] + o);
		used_ym++;
	}
	m_ym_buf.erase(m_ym_buf.begin(), m_ym_buf.begin() + std::ptrdiff_t(used_ym));
	m_mixed_ym += used_ym;
	// keep only the OKI samples still needed
	double needd = double(m_mixed_ym) * oki_rate / ym_rate - 1.0;
	uint64_t need = needd > 0 ? uint64_t(needd) : 0;
	if (need > oki_first) {
		size_t drop = size_t(std::min<uint64_t>(need - oki_first, m_oki_hist.size()));
		m_oki_hist.erase(m_oki_hist.begin(), m_oki_hist.begin() + std::ptrdiff_t(drop));
	}
}

void machine::sound_manager_update()
{
	// sound_manager::update: bring every stream up to date, then apply rate changes
	ym_update();
	oki_update();
	if (m_oki_pending_rate) {
		// a rate change restarts the mixing timeline of that stream
		mix_up_to_now();
		m_oki_clock.set_rate(m_oki_pending_rate);
		m_oki_pending_rate = 0;
	}
}

// ------------------------------------------------------------------ inputs

uint8_t machine::in0() const
{
	const uint32_t in = m_inputs;
	auto low = [&](uint32_t flag, uint8_t bit) -> uint8_t { return (in & flag) ? 0 : bit; };
	return uint8_t(low(IN_COIN1, 0x01) | low(IN_COIN2, 0x02) | low(IN_SERVICE1, 0x04) | 0x08 |
	               low(IN_P1_START, 0x10) | low(IN_P2_START, 0x20) | low(IN_TEST, 0x40) | 0x80);
}

uint16_t machine::in1() const
{
	const uint32_t in = m_inputs;
	auto low = [&](uint32_t flag, uint16_t bit) -> uint16_t { return (in & flag) ? 0 : bit; };
	return uint16_t(low(IN_P1_RIGHT, 0x0001) | low(IN_P1_LEFT, 0x0002) | low(IN_P1_DOWN, 0x0004) | low(IN_P1_UP, 0x0008) |
	                low(IN_P1_LP, 0x0010) | low(IN_P1_MP, 0x0020) | low(IN_P1_HP, 0x0040) | 0x0080 |
	                low(IN_P2_RIGHT, 0x0100) | low(IN_P2_LEFT, 0x0200) | low(IN_P2_DOWN, 0x0400) | low(IN_P2_UP, 0x0800) |
	                low(IN_P2_LP, 0x1000) | low(IN_P2_MP, 0x2000) | low(IN_P2_HP, 0x4000) | 0x8000);
}

uint8_t machine::in2() const
{
	const uint32_t in = m_inputs;
	auto low = [&](uint32_t flag, uint8_t bit) -> uint8_t { return (in & flag) ? 0 : bit; };
	return uint8_t(low(IN_P1_LK, 0x01) | low(IN_P1_MK, 0x02) | low(IN_P1_HK, 0x04) | 0x08 |
	               low(IN_P2_LK, 0x10) | low(IN_P2_MK, 0x20) | low(IN_P2_HK, 0x40) | 0x80);
}

// ------------------------------------------------------------------ main bus (cps_state::main_map)

uint16_t machine::main_read16(uint32_t a, uint16_t mask)
{
	if (a & 0x1000000u) {
		// cpu_space_map: 0xfffff2-0xffffff autovectors; clear the IPL1/IPL2 flip-flops
		a &= 0xffffff;
		if (a >= 0xfffff2) {
			if (g_evlog) { kemu::attotime t = m_sched.time(); std::fprintf(g_evlog, "IACK %X %d %lld\n", a, t.sec, (long long)t.atto); }
			m_main_exec.set_input_line(2, kemu::CLEAR_LINE);
			m_main_exec.set_input_line(4, kemu::CLEAR_LINE);
			return uint16_t(0x18 + ((a - 0xfffff2) >> 1) + 1);
		}
		return 0;
	}
	a &= 0xffffff;
	if (a < 0x400000)
		return m_rom.prog[a >> 1];
	if (a >= 0xff0000)
		return m_ram[(a & 0xffff) >> 1];
	if (a >= 0x900000 && a < 0x930000)
		return m_gfxram[(a - 0x900000) >> 1];
	if (a >= 0x800000 && a < 0x800008)
		return in1();
	if (a >= 0x800018 && a < 0x800020) {
		// cps1_dsw_r: IN0, DSWA, DSWB, DSWC in the high byte
		int off = int((a - 0x800018) >> 1);
		uint8_t v = off == 0 ? in0() : m_dsw[off - 1];
		return uint16_t((v << 8) | 0xff);
	}
	if (a >= 0x800140 && a < 0x800180)
		return m_video.cps_b_r((a - 0x800140) >> 1, in2());
	return 0;   // unmapped (including 0x800020 nopr and the write-only CPS-A)
}

void machine::main_write16(uint32_t a, uint16_t d, uint16_t mask)
{
	a &= 0xffffff;
	if (a >= 0xff0000) {
		uint16_t &w = m_ram[(a & 0xffff) >> 1];
		w = uint16_t((w & ~mask) | (d & mask));
		return;
	}
	if (a >= 0x900000 && a < 0x930000) {
		uint32_t off = (a - 0x900000) >> 1;
		m_gfxram[off] = uint16_t((m_gfxram[off] & ~mask) | (d & mask));
		m_video.gfxram_w(off, d, mask);
		return;
	}
	if (a >= 0x800030 && a < 0x800038) {
		// cps1_coinctrl_w
		if (mask & 0xff00) {
			m_coin_lockout[0] = (~d & 0x0400) ? 1 : 0;
			m_coin_lockout[1] = (~d & 0x0800) ? 1 : 0;
		}
		return;
	}
	if (a >= 0x800100 && a < 0x800140) {
		m_video.cps_a_w((a - 0x800100) >> 1, d, mask);
		return;
	}
	if (a >= 0x800140 && a < 0x800180) {
		m_video.cps_b_w((a - 0x800140) >> 1, d, mask);
		return;
	}
	if (a >= 0x800180 && a < 0x800188) {
		// cps1_soundlatch_w -> generic_latch_8_device::write (synchronize)
		uint8_t v = (mask & 0x00ff) ? uint8_t(d & 0xff) : uint8_t(d >> 8);
		if (g_evlog) { kemu::attotime t = m_sched.time(); std::fprintf(g_evlog, "LATCH %X %04X %d %lld\n", a, d, t.sec, (long long)t.atto); }
		m_sched.synchronize(t_latch1, this, v, K_SYNC_LATCH1);
		return;
	}
	if (a >= 0x800188 && a < 0x800190) {
		if (mask & 0x00ff)
			m_sched.synchronize(t_latch2, this, d & 0xff, K_SYNC_LATCH2);
		return;
	}
}

// ------------------------------------------------------------------ sound bus (cps_state::sub_map)

uint8_t machine::snd_read(uint16_t a)
{
	if (a < 0x8000)
		return m_rom.audio[a];
	if (a < 0xc000)
		return m_rom.audio[0x10000 + m_bank * 0x4000 + (a - 0x8000)];
	if (a >= 0xd000 && a < 0xd800)
		return m_zram[a - 0xd000];
	if (a == 0xf000 || a == 0xf001)
		return m_ym.read(a & 1);
	if (a == 0xf002)
		return m_oki.read();
	if (a == 0xf008)
		return m_soundlatch;
	if (a == 0xf00a)
		return m_soundlatch2;
	return 0;
}

void machine::snd_write(uint16_t a, uint8_t d)
{
	if (a >= 0xd000 && a < 0xd800) {
		m_zram[a - 0xd000] = d;
		return;
	}
	if (a == 0xf000 || a == 0xf001) {
		if (g_evlog) { kemu::attotime t = m_sched.time(); std::fprintf(g_evlog, "YM %X %02X %d %lld\n", a, d, t.sec, (long long)t.atto); }
		m_ym.write(a & 1, d);
		return;
	}
	if (a == 0xf002) {
		m_oki.write(d);
		return;
	}
	if (a == 0xf004) {
		m_bank = d & 1;
		return;
	}
	if (a == 0xf006) {
		m_oki.set_pin7(d & 1);
		return;
	}
}

} // namespace sf2
