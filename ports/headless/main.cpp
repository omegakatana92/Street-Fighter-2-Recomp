// Deterministic headless runner for the Street Fighter II port: runs the
// CPS-1 board model for one version and writes frame hashes, PNG frame grabs,
// WAV audio and state files.  It is also the comparison tool against MAME
// 0.264 reference dumps (see tools/mame_reference.lua).
#include "sf2/machine.h"
#include "miniz.h"

#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace sf2 { extern FILE *g_evlog; }
namespace {

struct options {
	const char *rom = nullptr;
	const char *version = "sf2";
	int frames = 600;
	const char *inputs = nullptr;
	bool mame_latency = false;
	int hash_every = 0;
	std::map<int, std::string> png;
	const char *wav = nullptr;
	const char *compare = nullptr;
	int state_test = -1;
	int save_at = -1;
	std::string save_file;
	const char *load_file = nullptr;
	int dsw[3] = {-1, -1, -1};
	bool quiet = false;
	const char *trace_z80 = nullptr;
	int trace_frames = 0;
};
FILE *g_trace = nullptr;
void z80_trace(z80_device *z)
{
	if (!g_trace) return;
	std::fprintf(g_trace, "%04X\n", z->pc());
	if (z->pc() == 0x38) {
		const kemu::exec_device *e = z->m_exec;
		kemu::attotime t = e->local_time();
		std::fprintf(g_trace, "IRQ %d %lld cyc %llu\n", t.sec, (long long)t.atto, (unsigned long long)(e->totalcycles + uint64_t(e->cycles_running - *e->icountptr)));
	}
}


uint64_t fnv1a(uint64_t h, const void *data, size_t n)
{
	const uint8_t *p = static_cast<const uint8_t *>(data);
	for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 0x100000001B3ull; }
	return h;
}

// main work RAM, graphics RAM (big-endian bytes) and the Z80 RAM: the layout of the MAME dumps
constexpr size_t RAM_IMAGE = 0x10000 + 0x30000 + 0x800;
std::vector<uint8_t> ram_image(const sf2::machine &m)
{
	std::vector<uint8_t> out(RAM_IMAGE);
	for (int i = 0; i < 0x8000; i++) {
		out[2 * i] = uint8_t(m.m_ram[i] >> 8); out[2 * i + 1] = uint8_t(m.m_ram[i]);
	}
	for (int i = 0; i < 0x18000; i++) {
		out[0x10000 + 2 * i] = uint8_t(m.m_gfxram[i] >> 8); out[0x10000 + 2 * i + 1] = uint8_t(m.m_gfxram[i]);
	}
	std::memcpy(&out[0x40000], m.m_zram, 0x800);
	return out;
}

bool write_png(const char *path, sf2::machine &m)
{
	const int w = sf2::VIS_X1 - sf2::VIS_X0 + 1, h = sf2::VIS_Y1 - sf2::VIS_Y0 + 1;
	std::vector<uint8_t> rgb(size_t(w) * h * 3);
	const uint32_t *pens = m.m_video.palette();
	for (int y = 0; y < h; y++)
		for (int x = 0; x < w; x++) {
			uint32_t c = pens[m.m_video.bitmap().pix(sf2::VIS_Y0 + y, sf2::VIS_X0 + x) % 0xc00];
			uint8_t *p = &rgb[(size_t(y) * w + x) * 3];
			p[0] = uint8_t(c >> 16); p[1] = uint8_t(c >> 8); p[2] = uint8_t(c);
		}
	size_t zl = 0;
	void *z = tdefl_write_image_to_png_file_in_memory(rgb.data(), w, h, 3, &zl);
	if (!z) return false;
	FILE *f = std::fopen(path, "wb");
	if (!f) { mz_free(z); return false; }
	std::fwrite(z, 1, zl, f);
	std::fclose(f);
	mz_free(z);
	return true;
}

struct wav_writer {
	FILE *f = nullptr;
	uint32_t frames = 0, rate = 0;
	bool open(const char *path, uint32_t r)
	{
		rate = r;
		f = std::fopen(path, "wb");
		if (!f) return false;
		uint8_t header[44] = {};
		std::fwrite(header, 1, 44, f);
		return true;
	}
	void add(const std::vector<float> &mono)
	{
		for (float v : mono) {
			float s = v * 32768.0f;
			int16_t i = int16_t(s > 32767.0f ? 32767 : s < -32768.0f ? -32768 : int(s));
			std::fwrite(&i, 2, 1, f);
		}
		frames += uint32_t(mono.size());
	}
	void close()
	{
		if (!f) return;
		uint32_t data = frames * 2;
		auto put32 = [&](uint8_t *p, uint32_t v) { p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); p[2] = uint8_t(v >> 16); p[3] = uint8_t(v >> 24); };
		uint8_t h[44];
		std::memcpy(h, "RIFF", 4); put32(h + 4, 36 + data); std::memcpy(h + 8, "WAVEfmt ", 8);
		put32(h + 16, 16); h[20] = 1; h[21] = 0; h[22] = 1; h[23] = 0; put32(h + 24, rate); put32(h + 28, rate * 2);
		h[32] = 2; h[33] = 0; h[34] = 16; h[35] = 0; std::memcpy(h + 36, "data", 4); put32(h + 40, data);
		std::fseek(f, 0, SEEK_SET);
		std::fwrite(h, 1, 44, f);
		std::fclose(f);
		f = nullptr;
	}
};

std::vector<std::pair<int, uint32_t>> g_events;
size_t g_event_at;
options g_opt;
int g_compare_first_bad = -1;
int g_compared = 0;

void on_vblank(sf2::machine &m, void *)
{
	const int frame = int(m.frame_number());
	// MAME applies Lua set_value() inputs at the following frame's input update
	const int shift = g_opt.mame_latency ? 1 : 0;
	while (g_event_at < g_events.size() && g_events[g_event_at].first + shift <= frame) {
		m.set_inputs(g_events[g_event_at].second);
		g_event_at++;
	}
	if (g_opt.compare) {
		char path[1024];
		std::snprintf(path, sizeof path, "%s/r%05d.bin", g_opt.compare, frame);
		if (FILE *f = std::fopen(path, "rb")) {
			std::vector<uint8_t> ref(RAM_IMAGE);
			size_t got = std::fread(ref.data(), 1, ref.size(), f);
			std::fclose(f);
			std::vector<uint8_t> ours = ram_image(m);
			g_compared++;
			if (got != ref.size() || ref != ours) {
				if (g_compare_first_bad < 0) g_compare_first_bad = frame;
				size_t first = 0;
				while (first < ours.size() && ours[first] == ref[first]) first++;
				if (!g_opt.quiet) {
					size_t nm = 0, ng = 0, nz = 0;
					for (size_t i = 0; i < ours.size(); i++)
						if (ours[i] != ref[i]) (i < 0x10000 ? nm : i < 0x40000 ? ng : nz)++;
					std::printf("frame %d: RAM differs from the reference (first byte %s+%05zX; bytes main %zu gfx %zu z80 %zu)", frame,
					            first < 0x10000 ? "main" : first < 0x40000 ? "gfx" : "z80", first < 0x10000 ? first : first < 0x40000 ? first - 0x10000 : first - 0x40000,
					            nm, ng, nz);
					int shown = 0;
					for (size_t i = first; i < ours.size() && shown < 6; i++)
						if (ours[i] != ref[i]) { std::printf(" [%05zX %02X/%02X]", i, ours[i], ref[i]); shown++; }
					std::printf("\n");
				}
			}
		}
	}
	auto it = g_opt.png.find(frame);
	if (it != g_opt.png.end())
		write_png(it->second.c_str(), m);
}

void usage()
{
	std::puts("sf2_headless --rom PATH [--version sf2|sf2ce|sf2hf] [--frames N] [--inputs FILE] [--mame-latency]\n"
	          "             [--dsw A,B,C] [--hash-every N] [--png FRAME:FILE] [--wav FILE] [--compare DIR]\n"
	          "             [--state-test FRAME] [--save-state FRAME:FILE] [--load-state FILE] [--quiet]\n"
	          "PATH: zip archive or folder.  inputs FILE: lines \"FRAME MASK\" (MASK = sf2::IN_* bits), applied from FRAME on.\n"
	          "--load-state continues from the state's frame; --frames counts the frames run after it.\n"
	          "development: --trace-z80 FILE (sound CPU program counters), --event-log FILE (YM2151 writes,\n"
	          "             sound latch writes and interrupt acknowledges with their emulated times).");
}

} // namespace

int main(int argc, char **argv)
{
	options &o = g_opt;
	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		auto next = [&]() -> const char * { return i + 1 < argc ? argv[++i] : ""; };
		if (a == "--rom") o.rom = next();
		else if (a == "--version") o.version = next();
		else if (a == "--frames") o.frames = std::atoi(next());
		else if (a == "--inputs") o.inputs = next();
		else if (a == "--mame-latency") o.mame_latency = true;
		else if (a == "--hash-every") o.hash_every = std::atoi(next());
		else if (a == "--png") { std::string v = next(); size_t c = v.find(':'); if (c != std::string::npos) o.png[std::atoi(v.c_str())] = v.substr(c + 1); }
		else if (a == "--wav") o.wav = next();
		else if (a == "--compare") o.compare = next();
		else if (a == "--state-test") o.state_test = std::atoi(next());
		else if (a == "--save-state") { std::string v = next(); size_t c = v.find(':'); if (c != std::string::npos) { o.save_at = std::atoi(v.c_str()); o.save_file = v.substr(c + 1); } }
		else if (a == "--load-state") o.load_file = next();
		else if (a == "--dsw") { std::sscanf(next(), "%x,%x,%x", &o.dsw[0], &o.dsw[1], &o.dsw[2]); }
		else if (a == "--quiet") o.quiet = true;
		else if (a == "--trace-z80") { o.trace_z80 = next(); }
		else if (a == "--event-log") { sf2::g_evlog = std::fopen(next(), "w"); }
		else { usage(); return 2; }
	}
	const sf2::version_info *vi = sf2::find_version(o.version);
	if (!o.rom || !vi) { usage(); return 2; }

	static sf2::machine m;
	std::string err;
	if (!m.load(vi->v, o.rom, err)) {
		std::fprintf(stderr, "%s\n", err.c_str());
		return 1;
	}
	if (o.inputs) {
		FILE *f = std::fopen(o.inputs, "r");
		if (!f) { std::fprintf(stderr, "cannot open %s\n", o.inputs); return 1; }
		int fr; unsigned mask;
		while (std::fscanf(f, "%d %u", &fr, &mask) == 2) g_events.push_back({fr, mask});
		std::fclose(f);
	}
	m.power_on();
	if (o.dsw[0] >= 0)
		m.set_dips(uint8_t(o.dsw[0]), uint8_t(o.dsw[1]), uint8_t(o.dsw[2]));
	m.on_vblank_hook = on_vblank;
	m.m_render = !o.png.empty();
	std::printf("%s (%s), ROM fingerprint %08X\n", vi->full_title, vi->mame_set, m.m_rom.fingerprint);

	if (o.load_file) {
		std::vector<uint8_t> st;
		FILE *f = std::fopen(o.load_file, "rb");
		if (f) {
			uint8_t buf[65536];
			size_t n;
			while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) st.insert(st.end(), buf, buf + n);
			std::fclose(f);
		}
		if (st.empty() || !m.load_state(st.data(), st.size())) { std::fprintf(stderr, "cannot load state %s\n", o.load_file); return 1; }
		std::printf("state loaded from %s at frame %llu\n", o.load_file, (unsigned long long)m.frame_number());
	}

	if (o.trace_z80) { g_trace = std::fopen(o.trace_z80, "w"); z80_device::trace = z80_trace; }
	wav_writer wav;
	if (o.wav && !wav.open(o.wav, m.audio_rate())) { std::fprintf(stderr, "cannot write %s\n", o.wav); return 1; }

	for (int f = 0; f < o.frames; f++) {
		if (f == o.state_test) {
			// save, run elsewhere for a while, restore: the run must continue identically
			std::vector<uint8_t> st;
			m.save_state(st);
			auto hook = m.on_vblank_hook;
			m.on_vblank_hook = nullptr;
			uint32_t keep = m.m_inputs;
			for (int k = 0; k < 300; k++) { m.set_inputs(uint32_t(k) * 2654435761u); m.run_frame(); }
			m.set_inputs(keep);
			// a damaged copy must be refused and leave the machine untouched
			std::vector<uint8_t> bad(st.begin(), st.end() - 7);
			if (m.load_state(bad.data(), bad.size())) { std::fprintf(stderr, "truncated state was accepted\n"); return 1; }
			if (!m.load_state(st.data(), st.size())) { std::fprintf(stderr, "state reload failed\n"); return 1; }
			std::vector<uint8_t> again;
			m.save_state(again);
			m.on_vblank_hook = hook;
			std::printf("state test: %zu bytes saved at frame %d, reloaded after 300 other frames (re-save %s)\n",
			            st.size(), f, again == st ? "identical" : "DIFFERENT");
		}
		if (int(m.frame_number()) == o.save_at) {
			std::vector<uint8_t> st;
			m.save_state(st);
			FILE *sf = std::fopen(o.save_file.c_str(), "wb");
			if (!sf || std::fwrite(st.data(), 1, st.size(), sf) != st.size()) { std::fprintf(stderr, "cannot write %s\n", o.save_file.c_str()); return 1; }
			std::fclose(sf);
			std::printf("state saved to %s at frame %d\n", o.save_file.c_str(), o.save_at);
		}
		m.run_frame();
		if (wav.f) wav.add(m.m_audio);
		const int done = int(m.frame_number());
		if (o.hash_every && done % o.hash_every == 0) {
			std::vector<uint8_t> img = ram_image(m);
			std::printf("frame %d hash %016" PRIx64 "\n", done, fnv1a(0xcbf29ce484222325ull, img.data(), img.size()));
		}
	}
	wav.close();
	if (o.compare)
		std::printf("compared %d frames with %s: %s\n", g_compared, o.compare,
		            g_compare_first_bad < 0 ? "all identical" : ("first difference at frame " + std::to_string(g_compare_first_bad)).c_str());
	return g_compare_first_bad < 0 ? 0 : 3;
}
