// Capcom CPS-1 board model for the three Street Fighter II sets.
//
// Reference: MAME 0.264 src/mame/capcom/cps1.cpp (machine configs cps1_10MHz /
// cps1_12MHz, main_map, sub_map, cpu_space_map, input ports "sf2"),
// cps1_v.cpp and the devices they use.  The machine owns the scheduler and
// every device; run_frame() advances to the next VBLANK the way MAME's
// timeslice loop does.  One machine runs one version at a time; the version
// is chosen when the ROMs are loaded.
#ifndef SF2_MACHINE_H
#define SF2_MACHINE_H

#include <cstdint>
#include <string>
#include <vector>
#include "../../emu/kemu.h"
#include "../../emu/state_io.h"
#include "../../m68000/m68000.h"
#include "../../z80/z80.h"
#include "../../devices/ym2151.h"
#include "../../devices/okim6295.h"
#include "../../video/cps1_video.h"

namespace sf2 {

// the three supported sets (MAME names in comments)
enum class version : int { world_warrior = 0, champion_edition = 1, hyper_fighting = 2 };
constexpr int VERSION_COUNT = 3;

struct version_info {
	version v;
	const char *id;            // profile / data id
	const char *mame_set;      // MAME 0.264 set name
	const char *title;         // short title
	const char *full_title;    // MAME description
	const char *rom_hint;      // usual archive name
	uint32_t main_clock;       // 68000 clock
};
const version_info &info(version v);
const version_info *find_version(const char *id_or_set);

constexpr uint32_t Z80_CLOCK = 3579545;             // XTAL(3'579'545)
constexpr uint32_t YM_CLOCK = 3579545;
constexpr uint32_t OKI_CLOCK = 16000000 / 4 / 4;    // 1 MHz, pin 7 high
constexpr int SCREEN_W = 512, SCREEN_H = 262;
constexpr int VIS_X0 = 64, VIS_X1 = 447, VIS_Y0 = 16, VIS_Y1 = 239;

// inputs, active high (the board model inverts the active-low bits).
// Button names follow MAME's cps1_6b: jab/strong/fierce punch, short/forward/roundhouse kick.
enum : uint32_t {
	IN_P1_UP = 1u << 0, IN_P1_DOWN = 1u << 1, IN_P1_LEFT = 1u << 2, IN_P1_RIGHT = 1u << 3,
	IN_P1_LP = 1u << 4, IN_P1_MP = 1u << 5, IN_P1_HP = 1u << 6,
	IN_P1_LK = 1u << 7, IN_P1_MK = 1u << 8, IN_P1_HK = 1u << 9, IN_P1_START = 1u << 10,
	IN_P2_UP = 1u << 11, IN_P2_DOWN = 1u << 12, IN_P2_LEFT = 1u << 13, IN_P2_RIGHT = 1u << 14,
	IN_P2_LP = 1u << 15, IN_P2_MP = 1u << 16, IN_P2_HP = 1u << 17,
	IN_P2_LK = 1u << 18, IN_P2_MK = 1u << 19, IN_P2_HK = 1u << 20, IN_P2_START = 1u << 21,
	IN_COIN1 = 1u << 22, IN_COIN2 = 1u << 23, IN_SERVICE1 = 1u << 24, IN_TEST = 1u << 25,
};

// DIP switch defaults (MAME INPUT_PORTS_START(sf2))
constexpr uint8_t DSWA_DEFAULT = 0xff, DSWB_DEFAULT = 0xfc, DSWC_DEFAULT = 0x9f;

struct rom_set {
	version v = version::world_warrior;
	std::vector<uint16_t> prog;     // "maincpu", 0x400000 bytes as 68000 words
	std::vector<uint8_t> gfx;       // "gfx", 0x600000
	std::vector<uint8_t> audio;     // "audiocpu", 0x18000
	std::vector<uint8_t> oki;       // "oki", 0x40000
	uint32_t fingerprint = 0;       // CRC32 over the program region
};

// Validation of a zip archive or a folder of files for one version.
// `found_other` is set when the source holds a different supported version.
bool check_roms(version v, const char *path, std::string &err, const version_info **found_other = nullptr);
bool load_roms(version v, const char *path, rom_set &out, std::string &err);
// which supported version a source holds (nullptr: none)
const version_info *identify_roms(const char *path);

// stream_buffer time bookkeeping of one MAME sound_stream (sound.cpp)
struct stream_clock {
	uint32_t rate = 0;
	kemu::attoseconds_t sample_attos = 0;
	int32_t end_second = 0;
	uint32_t end_sample = 0;
	uint64_t total = 0;
	void set_rate(uint32_t r);
	kemu::attotime end_time() const { return kemu::attotime(end_second, kemu::attoseconds_t(end_sample) * sample_attos); }
	uint32_t advance_to(const kemu::attotime &t);
};

class machine {
public:
	machine();
	~machine();
	bool load(version v, const char *path, std::string &err);
	version current_version() const { return m_rom.v; }
	void power_on();                 // device start + machine reset
	void run_frame();                // until the next VBLANK has been handled
	// Latches the inputs for the next frame, as MAME's ioport frame update
	// does: a coin input is ignored while the game holds that coin's lockout
	// (MAME's -coin_lockout, on by default).  Inputs are taken as given; the
	// front end applies MAME's opposing-direction rule (filter_opposing()).
	void set_inputs(uint32_t in)
	{
		if (m_coin_lockout[0]) in &= ~uint32_t(IN_COIN1);
		if (m_coin_lockout[1]) in &= ~uint32_t(IN_COIN2);
		m_inputs = in;
	}
	// MAME's digital joystick (ioport.cpp, without -joystick_contradictory):
	// left + right or up + down held together cancel each other, as on a real
	// 8-way stick.  Input set from a MAME Lua script bypasses this, so the
	// headless reference runs do not use it.
	static uint32_t filter_opposing(uint32_t in)
	{
		for (int p = 0; p < 2; p++) {
			const uint32_t up = uint32_t(IN_P1_UP) << (11 * p), down = uint32_t(IN_P1_DOWN) << (11 * p);
			const uint32_t left = uint32_t(IN_P1_LEFT) << (11 * p), right = uint32_t(IN_P1_RIGHT) << (11 * p);
			if ((in & (up | down)) == (up | down)) in &= ~(up | down);
			if ((in & (left | right)) == (left | right)) in &= ~(left | right);
		}
		return in;
	}
	void set_dips(uint8_t a, uint8_t b, uint8_t c) { m_dsw[0] = a; m_dsw[1] = b; m_dsw[2] = c; }

	// state
	void save_state(std::vector<uint8_t> &out) const;
	bool load_state(const uint8_t *data, size_t size);
	bool validate_state(const uint8_t *data, size_t size) const;
	bool m_in_restore = false;

	// audio produced since the last call: mono at audio_rate(), MAME mixer scaling
	std::vector<float> m_audio;
	uint32_t audio_rate() const { return m_ym_clock.rate; }

	// debug / comparison
	const uint16_t *main_ram() const { return m_ram; }
	const uint8_t *sound_ram() const { return m_zram; }
	uint64_t frame_number() const { return m_frame; }
	void (*on_vblank_hook)(machine &m, void *ctx) = nullptr;
	void *hook_ctx = nullptr;
	kemu::scheduler &sched() { return m_sched; }

	cps1_video m_video;
	bool m_render = true;                 // run screen_update at each VBLANK
	rom_set m_rom;

	// --- internals (public for the bus trampolines) ---
	kemu::scheduler m_sched;
	m68000_device m_main;
	z80_device m_snd;
	kemu::exec_device m_main_exec, m_snd_exec;
	ym2151_device m_ym;
	okim6295_device m_oki;
	uint16_t m_ram[0x8000];        // 0xff0000-0xffffff
	uint16_t m_gfxram[0x18000];    // 0x900000-0x92ffff
	uint16_t m_cps_a_regs[0x20];
	uint16_t m_cps_b_regs[0x20];
	uint8_t m_zram[0x800];         // 0xd000-0xd7ff
	uint8_t m_bank = 0;
	uint8_t m_soundlatch = 0, m_soundlatch2 = 0;
	uint32_t m_inputs = 0;
	uint8_t m_dsw[3] = {DSWA_DEFAULT, DSWB_DEFAULT, DSWC_DEFAULT};
	uint8_t m_coin_lockout[2] = {0, 0};   // bookkeeping state; survives a reset as in MAME

	// streams
	stream_clock m_ym_clock, m_oki_clock;
	uint32_t m_oki_pending_rate = 0;
	std::vector<float> m_ym_buf, m_oki_hist;   // generated, not yet mixed
	uint64_t m_mixed_ym = 0;                   // ym samples already handed out
	void ym_update();
	void oki_update();
	void mix_up_to_now();
	kemu::timer *m_sound_update_timer = nullptr;
	kemu::timer *m_ym_timer[2] = {nullptr, nullptr};
	void sound_manager_update();

	// screen
	kemu::attoseconds_t m_frame_period = 0, m_scantime = 0, m_pixeltime = 0, m_vblank_period = 0;
	kemu::attotime m_vblank_start_time, m_vblank_end_time;
	kemu::timer *m_vblank_begin_timer = nullptr, *m_vblank_end_timer = nullptr, *m_scanline0_timer = nullptr;
	uint64_t m_frame = 0;
	bool m_frame_done = false;
	kemu::attotime time_until_pos(int vpos, int hpos = 0) const;
	void vblank_begin();
	void vblank_end();
	void scanline0();

	// buses
	uint16_t main_read16(uint32_t a, uint16_t mask);
	void main_write16(uint32_t a, uint16_t d, uint16_t mask);
	uint8_t snd_read(uint16_t a);
	void snd_write(uint16_t a, uint8_t d);
	uint16_t in1() const;
	uint8_t in0() const;
	uint8_t in2() const;

	std::vector<kemu::timer *> m_timers;   // every non-temporary timer, creation order
private:
	void start_devices();
	void reset_machine();
};

} // namespace sf2

#endif
