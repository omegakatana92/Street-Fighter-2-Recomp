// SDL2 frontend adapter for the Street Fighter II port: three versions
// (The World Warrior, Champion Edition, Hyper Fighting) in one application
// through the shared frontend's suite API.  Every version is a complete
// fe_game; they share the board code and the input layout.
#include "sf2/machine.h"

extern "C" {
#include "fe.h"
}
#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#ifndef SF2_VERSION
#define SF2_VERSION "dev"
#endif

namespace {

std::unique_ptr<sf2::machine> board;
bool loaded = false;
sf2::version loaded_version = sf2::version::world_warrior;

#define K(x) SDL_SCANCODE_##x
#define P(x) SDL_CONTROLLER_BUTTON_##x
// FE_PAD_AXIS_BASE + axis*2 + 1: trigger pulled
#define TRIG_R (100 + SDL_CONTROLLER_AXIS_TRIGGERRIGHT * 2 + 1)
#define TRIG_L (100 + SDL_CONTROLLER_AXIS_TRIGGERLEFT * 2 + 1)

// MAME cps1.cpp INPUT_PORTS_START(sf2) / cps1_6b: two players, 8-way stick,
// three punches (top row) and three kicks (bottom row) each, start, coins,
// service coin and the service (test) switch.
const fe_input_def INPUTS[] = {
	{ "p1_up", "P1 Up", 1, sf2::IN_P1_UP, K(UP), -1, P(DPAD_UP), 0 },
	{ "p1_down", "P1 Down", 1, sf2::IN_P1_DOWN, K(DOWN), -1, P(DPAD_DOWN), 0 },
	{ "p1_left", "P1 Left", 1, sf2::IN_P1_LEFT, K(LEFT), -1, P(DPAD_LEFT), 0 },
	{ "p1_right", "P1 Right", 1, sf2::IN_P1_RIGHT, K(RIGHT), -1, P(DPAD_RIGHT), 0 },
	{ "p1_lp", "P1 Light Punch (Jab)", 1, sf2::IN_P1_LP, K(A), -1, P(X), 0 },
	{ "p1_mp", "P1 Medium Punch (Strong)", 1, sf2::IN_P1_MP, K(S), -1, P(Y), 0 },
	{ "p1_hp", "P1 Heavy Punch (Fierce)", 1, sf2::IN_P1_HP, K(D), -1, P(RIGHTSHOULDER), 0 },
	{ "p1_lk", "P1 Light Kick (Short)", 1, sf2::IN_P1_LK, K(Z), -1, P(A), 0 },
	{ "p1_mk", "P1 Medium Kick (Forward)", 1, sf2::IN_P1_MK, K(X), -1, P(B), 0 },
	{ "p1_hk", "P1 Heavy Kick (Roundhouse)", 1, sf2::IN_P1_HK, K(C), -1, TRIG_R, 0 },
	{ "p1_start", "P1 Start", 1, sf2::IN_P1_START, K(RETURN), K(1), P(START), 0 },
	{ "p2_up", "P2 Up", 2, sf2::IN_P2_UP, K(I), -1, P(DPAD_UP), 0 },
	{ "p2_down", "P2 Down", 2, sf2::IN_P2_DOWN, K(K), -1, P(DPAD_DOWN), 0 },
	{ "p2_left", "P2 Left", 2, sf2::IN_P2_LEFT, K(J), -1, P(DPAD_LEFT), 0 },
	{ "p2_right", "P2 Right", 2, sf2::IN_P2_RIGHT, K(L), -1, P(DPAD_RIGHT), 0 },
	{ "p2_lp", "P2 Light Punch (Jab)", 2, sf2::IN_P2_LP, K(R), -1, P(X), 0 },
	{ "p2_mp", "P2 Medium Punch (Strong)", 2, sf2::IN_P2_MP, K(T), -1, P(Y), 0 },
	{ "p2_hp", "P2 Heavy Punch (Fierce)", 2, sf2::IN_P2_HP, K(Y), -1, P(RIGHTSHOULDER), 0 },
	{ "p2_lk", "P2 Light Kick (Short)", 2, sf2::IN_P2_LK, K(F), -1, P(A), 0 },
	{ "p2_mk", "P2 Medium Kick (Forward)", 2, sf2::IN_P2_MK, K(G), -1, P(B), 0 },
	{ "p2_hk", "P2 Heavy Kick (Roundhouse)", 2, sf2::IN_P2_HK, K(H), -1, TRIG_R, 0 },
	{ "p2_start", "P2 Start", 2, sf2::IN_P2_START, K(2), -1, P(START), 0 },
	// coins are pulses, like a real coin switch
	{ "coin1", "Coin 1", 1, sf2::IN_COIN1, K(5), -1, P(BACK), 5 },
	{ "coin2", "Coin 2", 2, sf2::IN_COIN2, K(6), -1, P(BACK), 5 },
	{ "service1", "Service Coin", 0, sf2::IN_SERVICE1, K(9), -1, -1, 5 },
	{ "test", "Service Mode (Test)", 0, sf2::IN_TEST, K(F2), -1, -1, 0 },
};

// MAME cps1.cpp INPUT_PORTS_START(sf2): DIP switches A, B, C (words 0..2,
// low byte).  The three sets use the same definition.
#define COINS(o) { { "4 Coins/1 Credit", 0x00 << o }, { "3 Coins/1 Credit", 0x01 << o }, { "2 Coins/1 Credit", 0x02 << o }, \
	{ "1 Coin/1 Credit", 0x07 << o }, { "1 Coin/2 Credits", 0x06 << o }, { "1 Coin/3 Credits", 0x05 << o }, \
	{ "1 Coin/4 Credits", 0x04 << o }, { "1 Coin/6 Credits", 0x03 << o } }
const fe_dip_def DIPS[] = {
	{ "coin_a", "Coin A", 0, 0x07, 0x07, 8, COINS(0) },
	{ "coin_b", "Coin B", 0, 0x38, 0x38, 8, COINS(3) },
	{ "two_coins", "2 Coins to Start, 1 to Continue", 0, 0x40, 0x40, 2, { { "Off", 0x40 }, { "On", 0x00 } } },
	{ "difficulty", "Difficulty", 1, 0x07, 0x04, 8, { { "0 (Easiest)", 0x07 }, { "1", 0x06 }, { "2", 0x05 }, { "3 (Normal)", 0x04 },
	                                                 { "4", 0x03 }, { "5", 0x02 }, { "6", 0x01 }, { "7 (Hardest)", 0x00 } } },
	{ "free_play", "Free Play", 2, 0x04, 0x04, 2, { { "Off", 0x04 }, { "On", 0x00 } } },
	{ "freeze", "Freeze", 2, 0x08, 0x08, 2, { { "Off", 0x08 }, { "On", 0x00 } } },
	{ "flip", "Flip Screen", 2, 0x10, 0x10, 2, { { "Off", 0x10 }, { "On", 0x00 } } },
	{ "demo_sounds", "Demo Sounds", 2, 0x20, 0x00, 2, { { "Off", 0x20 }, { "On", 0x00 } } },
	{ "continue", "Allow Continue", 2, 0x40, 0x00, 2, { { "No", 0x40 }, { "Yes", 0x00 } } },
	{ "game_mode", "Game Mode", 2, 0x80, 0x80, 2, { { "Game", 0x80 }, { "Test", 0x00 } } },
};

// Verified per-version work-RAM values (see docs/FRONTEND.md, "Gadgets"):
// each address was matched against the in-game HUD in MAME 0.264 reference
// runs of one- and two-player games (timer at 9 frames, P1 score at 7, P2 score at 3
// per version).  Values are BCD, as the HUD shows them (the last score digit is
// the game's own continue counter).
struct hud_addrs { uint32_t timer, score_p1, score_p2; };
const hud_addrs HUD[sf2::VERSION_COUNT] = {
	{ 0xFF8ACE, 0xFF8660, 0xFF8960 },   // The World Warrior
	{ 0xFF8ABE, 0xFF8658, 0xFF8958 },   // Champion Edition
	{ 0xFF8ABE, 0xFF8658, 0xFF8958 },   // Hyper Fighting
};
uint8_t ram8(uint32_t a)
{
	uint16_t w = board->m_ram[(a & 0xffff) >> 1];
	return uint8_t((a & 1) ? w : w >> 8);
}
int bcd_text(uint32_t a, int bytes, char *t, size_t n, const char *prefix)
{
	if (!loaded) return 0;
	char digits[16];
	int k = 0;
	for (int i = 0; i < bytes; i++) {
		uint8_t b = ram8(a + uint32_t(i));
		if ((b >> 4) > 9 || (b & 15) > 9) return 0;   // not a BCD value now: show nothing
		digits[k++] = char('0' + (b >> 4));
		digits[k++] = char('0' + (b & 15));
	}
	digits[k] = 0;
	const char *d = digits;
	while (*d == '0' && d[1]) d++;
	std::snprintf(t, n, "%s%s", prefix, d);
	return 1;
}
int gadget_version(char *t, size_t n) { if (!loaded) return 0; std::snprintf(t, n, "%s", sf2::info(loaded_version).title); return 1; }
int gadget_timer(char *t, size_t n) { return bcd_text(HUD[int(loaded_version)].timer, 1, t, n, "Time "); }
int gadget_p1(char *t, size_t n) { return bcd_text(HUD[int(loaded_version)].score_p1, 4, t, n, "1P "); }
int gadget_p2(char *t, size_t n) { return bcd_text(HUD[int(loaded_version)].score_p2, 4, t, n, "2P "); }
const fe_gadget_def GADGETS[] = {
	{ "version", "Current Version", gadget_version },
	{ "timer", "Round Timer", gadget_timer },
	{ "score_p1", "Score (Player 1)", gadget_p1 },
	{ "score_p2", "Score (Player 2)", gadget_p2 },
};
const int GADGET_COUNT = int(sizeof(GADGETS) / sizeof(GADGETS[0]));

// --- per-version entry points (the frontend's callbacks carry no context)

int validate_for(sf2::version v, const char *path, char *err, size_t n)
{
	std::string e;
	if (sf2::check_roms(v, path, e)) return 1;
	std::snprintf(err, n, "%s", e.c_str());
	return 0;
}

int load_for(sf2::version v, const char *path, char *err, size_t n)
{
	board.reset();
	loaded = false;
	board.reset(new sf2::machine());
	std::string e;
	if (!board->load(v, path, e)) {
		std::snprintf(err, n, "%s", e.c_str());
		board.reset();
		return 0;
	}
	board->power_on();
	loaded_version = v;
	loaded = true;
	return 1;
}

int v_ww(const char *p, char *e, size_t n) { return validate_for(sf2::version::world_warrior, p, e, n); }
int v_ce(const char *p, char *e, size_t n) { return validate_for(sf2::version::champion_edition, p, e, n); }
int v_hf(const char *p, char *e, size_t n) { return validate_for(sf2::version::hyper_fighting, p, e, n); }
int l_ww(const char *p, char *e, size_t n) { return load_for(sf2::version::world_warrior, p, e, n); }
int l_ce(const char *p, char *e, size_t n) { return load_for(sf2::version::champion_edition, p, e, n); }
int l_hf(const char *p, char *e, size_t n) { return load_for(sf2::version::hyper_fighting, p, e, n); }

void g_unload() { board.reset(); loaded = false; }

uint16_t dsw[3] = { sf2::DSWA_DEFAULT, sf2::DSWB_DEFAULT, sf2::DSWC_DEFAULT };

void g_reset()
{
	if (!loaded) return;
	board->power_on();
	board->set_dips(uint8_t(dsw[0]), uint8_t(dsw[1]), uint8_t(dsw[2]));
}

void g_set_dips(const uint16_t *w)
{
	for (int i = 0; i < 3; i++) dsw[i] = uint16_t(w[i] & 0xff);
	if (loaded) board->set_dips(uint8_t(dsw[0]), uint8_t(dsw[1]), uint8_t(dsw[2]));
}

void g_set_inputs(uint32_t m) { if (loaded) board->set_inputs(sf2::machine::filter_opposing(m)); }

int g_run(int16_t *audio, int *frames, char *, size_t)
{
	board->run_frame();
	const std::vector<float> &a = board->m_audio;
	int count = int(a.size());
	if (count > 2048) count = 2048;
	for (int i = 0; i < count; i++) {
		float s = a[size_t(i)] * 32768.0f;
		int16_t v = int16_t(s > 32767.0f ? 32767 : s < -32768.0f ? -32768 : int(s));
		audio[2 * i] = audio[2 * i + 1] = v;    // mono speaker on both channels
	}
	if (frames) *frames = count;
	return 0;
}

void g_render32(uint32_t *px)
{
	const auto &bm = board->m_video.bitmap();
	const uint32_t *pens = board->m_video.palette();
	for (int y = sf2::VIS_Y0; y <= sf2::VIS_Y1; y++)
		for (int x = sf2::VIS_X0; x <= sf2::VIS_X1; x++)
			*px++ = pens[bm.pix(y, x) % 0xc00] & 0x00ffffffu;
}

uint8_t *g_save(size_t *size)
{
	std::vector<uint8_t> st;
	board->save_state(st);
	uint8_t *p = static_cast<uint8_t *>(std::malloc(st.size()));
	if (!p) return nullptr;
	std::memcpy(p, st.data(), st.size());
	*size = st.size();
	return p;
}

int g_validate_state(const uint8_t *d, size_t s, char *e, size_t n)
{
	if (loaded && board->validate_state(d, s)) return 1;
	std::snprintf(e, n, "This state belongs to another version, ROM revision or build of the runtime.");
	return 0;
}

int g_load_state(const uint8_t *d, size_t s, char *e, size_t n)
{
	if (board->load_state(d, s)) return 1;
	std::snprintf(e, n, "The state file is damaged or does not match this version.");
	return 0;
}

uint32_t g_fingerprint() { return loaded ? board->m_rom.fingerprint : 0; }

const char *const ABOUT_SUITE =
	"Street Fighter II  (Capcom, CPS-1: MC68000, Z80, YM2151, OKI MSM6295)\n"
	"Native port " SF2_VERSION ". The board is rebuilt after MAME 0.264 (scheduler, cycle-exact 68000,\n"
	"Z80, YM2151 through ymfm, OKI6295, CPS-A/CPS-B video). Requires your own MAME ROM sets;\n"
	"no game data is distributed. Hardware behaviour and core code come from MAME (BSD-3-Clause),\n"
	"ymfm (BSD-3-Clause). Frontend font: Inter (SIL OFL 1.1). SDL2 (zlib). miniz (MIT).\n"
	"This project's own code is GPL-2.0. See the LICENSES folder.";

fe_game make_game(sf2::version v)
{
	const sf2::version_info &vi = sf2::info(v);
	fe_game g = {};
	g.id = vi.id;
	g.title = vi.title;
	g.subtitle = vi.full_title;
	g.about = vi.full_title;
	g.rom_hint = vi.rom_hint;
	g.width = sf2::VIS_X1 - sf2::VIS_X0 + 1;
	g.height = sf2::VIS_Y1 - sf2::VIS_Y0 + 1;
	g.display_aspect = 4.0 / 3.0;
	g.rotation = 0;
	g.refresh_hz = 8000000.0 / (512.0 * 262.0);   // 59.637 Hz
	g.audio_rate = double(sf2::YM_CLOCK / 64);       // the YM2151 stream, 55,930 Hz
	g.max_audio_frames = 2048;
	g.dip_words = 3;
	g.players = 2;
	g.inputs = INPUTS;
	g.input_count = int(sizeof(INPUTS) / sizeof(INPUTS[0]));
	g.dips = DIPS;
	g.dip_count = int(sizeof(DIPS) / sizeof(DIPS[0]));
	g.gadgets = GADGETS;
	g.gadget_count = GADGET_COUNT;
	g.validate_rom = v == sf2::version::world_warrior ? v_ww : v == sf2::version::champion_edition ? v_ce : v_hf;
	g.load = v == sf2::version::world_warrior ? l_ww : v == sf2::version::champion_edition ? l_ce : l_hf;
	g.unload = g_unload;
	g.reset = g_reset;
	g.set_dips = g_set_dips;
	g.set_inputs = g_set_inputs;
	g.run_frame = g_run;
	g.render = nullptr;
	g.render32 = g_render32;
	g.save_state = g_save;
	g.validate_state = g_validate_state;
	g.load_state = g_load_state;
	g.rom_fingerprint = g_fingerprint;
	g.runtime_version = SF2_VERSION;
	g.profile_name = vi.id;
	return g;
}

} // namespace

int main(int argc, char **argv)
{
	static fe_game variants[sf2::VERSION_COUNT] = {
		make_game(sf2::version::world_warrior),
		make_game(sf2::version::champion_edition),
		make_game(sf2::version::hyper_fighting),
	};
	fe_suite suite = {};
	suite.id = "sf2";
	suite.title = "Street Fighter II";
	suite.about = ABOUT_SUITE;
	suite.variants = variants;
	suite.variant_count = sf2::VERSION_COUNT;
	int rc = fe_run_suite(&suite, argc, argv);
	board.reset();
	return rc;
}
