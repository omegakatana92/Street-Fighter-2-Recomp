// ROM sets of the three Street Fighter II versions: validation and region
// assembly, from MAME 0.264's ROM_START(sf2), ROM_START(sf2ce) and
// ROM_START(sf2hf) in src/mame/capcom/cps1.cpp.  The player supplies a zip
// archive or a folder; files are matched by name, or by size and CRC32 when
// the name differs (as MAME does).  Nothing is written anywhere.
#include "sf2/machine.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <strings.h>
#include "miniz.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

namespace sf2 {

struct rom_entry { const char *name; uint32_t size, crc; const char *region; };

static const rom_entry SF2_FILES[] = {
	{ "sf2e_30g.11e", 0x20000, 0xFE39EE33, "maincpu" }, { "sf2e_37g.11f", 0x20000, 0xFB92CD74, "maincpu" },
	{ "sf2e_31g.12e", 0x20000, 0x69A0A301, "maincpu" }, { "sf2e_38g.12f", 0x20000, 0x5E22DB70, "maincpu" },
	{ "sf2e_28g.9e", 0x20000, 0x8BF9F1E5, "maincpu" },  { "sf2e_35g.9f", 0x20000, 0x626EF934, "maincpu" },
	{ "sf2_29b.10e", 0x20000, 0xBB4AF315, "maincpu" },  { "sf2_36b.10f", 0x20000, 0xC02A13EB, "maincpu" },
	{ "sf2-5m.4a", 0x80000, 0x22C9CC8E, "gfx" },  { "sf2-7m.6a", 0x80000, 0x57213BE8, "gfx" },
	{ "sf2-1m.3a", 0x80000, 0xBA529B4F, "gfx" },  { "sf2-3m.5a", 0x80000, 0x4B1B33A8, "gfx" },
	{ "sf2-6m.4c", 0x80000, 0x2C7E2229, "gfx" },  { "sf2-8m.6c", 0x80000, 0xB5548F17, "gfx" },
	{ "sf2-2m.3c", 0x80000, 0x14B84312, "gfx" },  { "sf2-4m.5c", 0x80000, 0x5E9CD89A, "gfx" },
	{ "sf2-13m.4d", 0x80000, 0x994BFA58, "gfx" }, { "sf2-15m.6d", 0x80000, 0x3E66AD9D, "gfx" },
	{ "sf2-9m.3d", 0x80000, 0xC1BEFAA8, "gfx" },  { "sf2-11m.5d", 0x80000, 0x0627C831, "gfx" },
	{ "sf2_9.12a", 0x10000, 0xA4823A1B, "audiocpu" },
	{ "sf2_18.11c", 0x20000, 0x7F162009, "oki" }, { "sf2_19.12c", 0x20000, 0xBEADE53F, "oki" },
};
static const rom_entry SF2CE_FILES[] = {
	{ "s92e_23b.8f", 0x80000, 0x0AAA1A3A, "maincpu" }, { "s92_22b.7f", 0x80000, 0x2BBE15ED, "maincpu" },
	{ "s92_21a.6f", 0x80000, 0x925A7877, "maincpu" },
	{ "s92-1m.3a", 0x80000, 0x03B0D852, "gfx" },  { "s92-3m.5a", 0x80000, 0x840289EC, "gfx" },
	{ "s92-2m.4a", 0x80000, 0xCDB5F027, "gfx" },  { "s92-4m.6a", 0x80000, 0xE2799472, "gfx" },
	{ "s92-5m.7a", 0x80000, 0xBA8A2761, "gfx" },  { "s92-7m.9a", 0x80000, 0xE584BFB5, "gfx" },
	{ "s92-6m.8a", 0x80000, 0x21E3F87D, "gfx" },  { "s92-8m.10a", 0x80000, 0xBEFC47DF, "gfx" },
	{ "s92-10m.3c", 0x80000, 0x960687D5, "gfx" }, { "s92-12m.5c", 0x80000, 0x978ECD18, "gfx" },
	{ "s92-11m.4c", 0x80000, 0xD6EC9A0A, "gfx" }, { "s92-13m.6c", 0x80000, 0xED2C67F6, "gfx" },
	{ "s92_09.11a", 0x10000, 0x08F6B60E, "audiocpu" },
	{ "s92_18.11c", 0x20000, 0x7F162009, "oki" }, { "s92_19.12c", 0x20000, 0xBEADE53F, "oki" },
};
static const rom_entry SF2HF_FILES[] = {
	{ "s2te_23.8f", 0x80000, 0x2DD72514, "maincpu" }, { "s2te_22.7f", 0x80000, 0xAEA6E035, "maincpu" },
	{ "s2te_21.6f", 0x80000, 0xFD200288, "maincpu" },
	{ "s92-1m.3a", 0x80000, 0x03B0D852, "gfx" },  { "s92-3m.5a", 0x80000, 0x840289EC, "gfx" },
	{ "s92-2m.4a", 0x80000, 0xCDB5F027, "gfx" },  { "s92-4m.6a", 0x80000, 0xE2799472, "gfx" },
	{ "s92-5m.7a", 0x80000, 0xBA8A2761, "gfx" },  { "s92-7m.9a", 0x80000, 0xE584BFB5, "gfx" },
	{ "s92-6m.8a", 0x80000, 0x21E3F87D, "gfx" },  { "s92-8m.10a", 0x80000, 0xBEFC47DF, "gfx" },
	{ "s92-10m.3c", 0x80000, 0x960687D5, "gfx" }, { "s92-12m.5c", 0x80000, 0x978ECD18, "gfx" },
	{ "s92-11m.4c", 0x80000, 0xD6EC9A0A, "gfx" }, { "s92-13m.6c", 0x80000, 0xED2C67F6, "gfx" },
	{ "s92_09.11a", 0x10000, 0x08F6B60E, "audiocpu" },
	{ "s92_18.11c", 0x20000, 0x7F162009, "oki" }, { "s92_19.12c", 0x20000, 0xBEADE53F, "oki" },
};

struct set_def { const rom_entry *files; size_t count; };
static set_def files_of(version v)
{
	switch (v) {
	case version::world_warrior: return { SF2_FILES, sizeof(SF2_FILES) / sizeof(SF2_FILES[0]) };
	case version::champion_edition: return { SF2CE_FILES, sizeof(SF2CE_FILES) / sizeof(SF2CE_FILES[0]) };
	default: return { SF2HF_FILES, sizeof(SF2HF_FILES) / sizeof(SF2HF_FILES[0]) };
	}
}

// ------------------------------------------------------------------ sources

static FILE *u8fopen(const char *path)
{
#ifdef _WIN32
	int n = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
	if (n <= 0) return nullptr;
	std::unique_ptr<wchar_t[]> w(new wchar_t[size_t(n)]);
	MultiByteToWideChar(CP_UTF8, 0, path, -1, w.get(), n);
	return _wfopen(w.get(), L"rb");
#else
	return std::fopen(path, "rb");
#endif
}

static bool read_file(const char *path, std::vector<uint8_t> &out, size_t limit = 256u << 20)
{
	FILE *f = u8fopen(path);
	if (!f) return false;
	out.clear();
	uint8_t buf[65536];
	size_t n;
	while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
		out.insert(out.end(), buf, buf + n);
		if (out.size() > limit) { std::fclose(f); return false; }
	}
	std::fclose(f);
	return true;
}

static bool is_directory(const char *path)
{
#ifdef _WIN32
	int n = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
	if (n <= 0) return false;
	std::unique_ptr<wchar_t[]> w(new wchar_t[size_t(n)]);
	MultiByteToWideChar(CP_UTF8, 0, path, -1, w.get(), n);
	DWORD a = GetFileAttributesW(w.get());
	return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

static std::vector<std::string> list_dir(const char *path)
{
	std::vector<std::string> names;
#ifdef _WIN32
	std::string pat = std::string(path) + "\\*";
	int n = MultiByteToWideChar(CP_UTF8, 0, pat.c_str(), -1, nullptr, 0);
	std::unique_ptr<wchar_t[]> w(new wchar_t[size_t(n)]);
	MultiByteToWideChar(CP_UTF8, 0, pat.c_str(), -1, w.get(), n);
	WIN32_FIND_DATAW fd;
	HANDLE h = FindFirstFileW(w.get(), &fd);
	if (h == INVALID_HANDLE_VALUE) return names;
	do {
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
		char u[1024];
		if (WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, u, sizeof u, nullptr, nullptr) > 0)
			names.push_back(u);
	} while (FindNextFileW(h, &fd));
	FindClose(h);
#else
	DIR *d = opendir(path);
	if (!d) return names;
	while (struct dirent *e = readdir(d)) {
		std::string full = std::string(path) + "/" + e->d_name;
		struct stat st;
		if (stat(full.c_str(), &st) == 0 && S_ISREG(st.st_mode))
			names.push_back(e->d_name);
	}
	closedir(d);
#endif
	return names;
}

namespace {
// a zip archive (read into memory, so UTF-8 paths work on Windows) or a folder
struct rom_source {
	struct item { std::string name; uint32_t size = 0, crc = 0; int zip_index = -1; bool crc_known = false; };
	bool is_zip = false, ok = false;
	std::string dir;
	std::vector<uint8_t> zipdata;
	mz_zip_archive z;
	std::vector<item> items;

	explicit rom_source(const char *path)
	{
		std::memset(&z, 0, sizeof z);
		if (!path || !*path) return;
		if (is_directory(path)) {
			dir = path;
			for (const std::string &n : list_dir(path)) {
				item it;
				it.name = n;
				std::vector<uint8_t> d;
				std::string full = dir + "/" + n;
				FILE *f = u8fopen(full.c_str());
				if (!f) continue;
				std::fseek(f, 0, SEEK_END);
				long sz = std::ftell(f);
				std::fclose(f);
				if (sz < 0 || sz > (16 << 20)) continue;   // not a ROM of these sets
				it.size = uint32_t(sz);
				items.push_back(it);
			}
			ok = true;
			return;
		}
		if (!read_file(path, zipdata)) return;
		if (!mz_zip_reader_init_mem(&z, zipdata.data(), zipdata.size(), 0)) return;
		is_zip = true;
		int n = int(mz_zip_reader_get_num_files(&z));
		for (int i = 0; i < n; i++) {
			mz_zip_archive_file_stat st;
			if (!mz_zip_reader_file_stat(&z, mz_uint(i), &st) || st.m_is_directory) continue;
			const char *base = std::strrchr(st.m_filename, '/');
			if (!base) base = std::strrchr(st.m_filename, '\\');
			base = base ? base + 1 : st.m_filename;
			item it;
			it.name = base;
			it.size = uint32_t(st.m_uncomp_size);
			it.crc = st.m_crc32;
			it.crc_known = true;
			it.zip_index = i;
			items.push_back(it);
		}
		ok = true;
	}
	~rom_source() { if (is_zip) mz_zip_reader_end(&z); }

	bool read(const item &it, std::vector<uint8_t> &out)
	{
		if (is_zip) {
			size_t size = 0;
			void *p = mz_zip_reader_extract_to_heap(&z, mz_uint(it.zip_index), &size, 0);
			if (!p) return false;
			out.assign((uint8_t *)p, (uint8_t *)p + size);
			mz_free(p);
			return true;
		}
		std::string full = dir + "/" + it.name;
		return read_file(full.c_str(), out);
	}
	// folders: the CRC is computed on first use
	uint32_t crc_of(item &it)
	{
		if (!it.crc_known) {
			std::vector<uint8_t> d;
			if (read(it, d))
				it.crc = uint32_t(mz_crc32(MZ_CRC32_INIT, d.data(), d.size()));
			it.crc_known = true;
		}
		return it.crc;
	}
	// MAME's matching: by name (any case); otherwise any file with the same size and CRC
	item *find(const rom_entry &e)
	{
		for (auto &it : items)
			if (!strcasecmp(it.name.c_str(), e.name))
				return &it;
		for (auto &it : items)
			if (it.size == e.size && crc_of(it) == e.crc)
				return &it;
		return nullptr;
	}
};

struct check_result { size_t found = 0, good = 0; std::string missing, bad; bool program_ok = false; };

check_result check_set(rom_source &src, version v)
{
	check_result r;
	set_def s = files_of(v);
	bool prog = true;
	for (size_t i = 0; i < s.count; i++) {
		const rom_entry &e = s.files[i];
		rom_source::item *it = src.find(e);
		bool is_prog = !std::strcmp(e.region, "maincpu");
		if (!it) {
			r.missing += std::string(" ") + e.name;
			if (is_prog) prog = false;
			continue;
		}
		r.found++;
		if (it->size != e.size || src.crc_of(*it) != e.crc) {
			r.bad += std::string(" ") + e.name;
			if (is_prog) prog = false;
			continue;
		}
		r.good++;
	}
	r.program_ok = prog;
	return r;
}
} // namespace

const version_info *identify_roms(const char *path)
{
	rom_source src(path);
	if (!src.ok) return nullptr;
	for (int v = 0; v < VERSION_COUNT; v++) {
		check_result r = check_set(src, version(v));
		if (r.missing.empty() && r.bad.empty())
			return &info(version(v));
	}
	return nullptr;
}

bool check_roms(version v, const char *path, std::string &err, const version_info **found_other)
{
	if (found_other) *found_other = nullptr;
	rom_source src(path);
	if (!src.ok) {
		err = std::string("Cannot open \"") + (path ? path : "") + "\" as a zip archive or a folder.";
		return false;
	}
	check_result r = check_set(src, v);
	if (r.missing.empty() && r.bad.empty())
		return true;
	const version_info &me = info(v);
	// another supported version's program ROMs: never substitute it
	for (int o = 0; o < VERSION_COUNT; o++) {
		if (version(o) == v) continue;
		check_result ro = check_set(src, version(o));
		if (ro.program_ok) {
			if (found_other) *found_other = &info(version(o));
			err = std::string("This holds ") + info(version(o)).full_title + " (MAME set '" + info(version(o)).mame_set +
			      "'), not " + me.title + ". Choose it under " + info(version(o)).title + ", or pick the '" +
			      me.mame_set + "' set here.";
			return false;
		}
	}
	err = std::string("Not the complete MAME 0.264 '") + me.mame_set + "' set (" + me.full_title + ").";
	if (!r.missing.empty()) err += " Missing:" + r.missing + ".";
	if (!r.bad.empty()) err += " Wrong size/CRC:" + r.bad + ".";
	if (r.found == 0)
		err += " No file of this version was found.";
	return false;
}

bool load_roms(version v, const char *path, rom_set &out, std::string &err)
{
	if (!check_roms(v, path, err))
		return false;
	rom_source src(path);
	set_def s = files_of(v);
	std::vector<uint8_t> d;
	auto need = [&](const char *name, std::vector<uint8_t> &buf) {
		for (size_t i = 0; i < s.count; i++)
			if (!std::strcmp(s.files[i].name, name)) {
				rom_source::item *it = src.find(s.files[i]);
				if (!it || !src.read(*it, buf) || buf.size() != s.files[i].size ||
				    uint32_t(mz_crc32(MZ_CRC32_INIT, buf.data(), buf.size())) != s.files[i].crc) {
					err = std::string(name) + ": read error or CRC mismatch.";
					return false;
				}
				return true;
			}
		err = std::string(name) + ": not part of this set.";
		return false;
	};

	rom_set r;
	r.v = v;
	// "maincpu": CODE_SIZE 0x400000, unused space 0
	std::vector<uint8_t> prog(0x400000, 0);
	if (v == version::world_warrior) {
		// ROM_LOAD16_BYTE pairs, 0x20000 each
		const struct { const char *even, *odd; uint32_t at; } p[] = {
			{ "sf2e_30g.11e", "sf2e_37g.11f", 0x00000 }, { "sf2e_31g.12e", "sf2e_38g.12f", 0x40000 },
			{ "sf2e_28g.9e", "sf2e_35g.9f", 0x80000 },   { "sf2_29b.10e", "sf2_36b.10f", 0xc0000 },
		};
		std::vector<uint8_t> o;
		for (const auto &e : p) {
			if (!need(e.even, d) || !need(e.odd, o)) return false;
			for (size_t i = 0; i < d.size(); i++) {
				prog[e.at + 2 * i] = d[i];
				prog[e.at + 2 * i + 1] = o[i];
			}
		}
	} else {
		// ROM_LOAD16_WORD_SWAP, 0x80000 each
		const char *ce[] = { "s92e_23b.8f", "s92_22b.7f", "s92_21a.6f" };
		const char *hf[] = { "s2te_23.8f", "s2te_22.7f", "s2te_21.6f" };
		const char **names = v == version::champion_edition ? ce : hf;
		for (int k = 0; k < 3; k++) {
			if (!need(names[k], d)) return false;
			for (size_t i = 0; i < d.size(); i += 2) {
				prog[0x80000 * k + i] = d[i + 1];
				prog[0x80000 * k + i + 1] = d[i];
			}
		}
	}
	r.prog.resize(0x200000);
	for (size_t i = 0; i < r.prog.size(); i++)
		r.prog[i] = uint16_t((prog[2 * i] << 8) | prog[2 * i + 1]);
	r.fingerprint = uint32_t(mz_crc32(MZ_CRC32_INIT, prog.data(), prog.size()));

	// "gfx": ROM_LOAD64_WORD (one 16-bit word, then skip 6 bytes)
	r.gfx.assign(0x600000, 0);
	const char *gw[] = { "sf2-5m.4a", "sf2-7m.6a", "sf2-1m.3a", "sf2-3m.5a", "sf2-6m.4c", "sf2-8m.6c",
	                     "sf2-2m.3c", "sf2-4m.5c", "sf2-13m.4d", "sf2-15m.6d", "sf2-9m.3d", "sf2-11m.5d" };
	const char *gc[] = { "s92-1m.3a", "s92-3m.5a", "s92-2m.4a", "s92-4m.6a", "s92-5m.7a", "s92-7m.9a",
	                     "s92-6m.8a", "s92-8m.10a", "s92-10m.3c", "s92-12m.5c", "s92-11m.4c", "s92-13m.6c" };
	const char **gn = v == version::world_warrior ? gw : gc;
	for (int k = 0; k < 12; k++) {
		if (!need(gn[k], d)) return false;
		uint32_t base = uint32_t(0x200000 * (k / 4) + 2 * (k % 4));
		for (size_t i = 0; i < d.size() / 2; i++) {
			r.gfx[base + 8 * i] = d[2 * i];
			r.gfx[base + 8 * i + 1] = d[2 * i + 1];
		}
	}

	// "audiocpu": 0x8000 at 0, ROM_CONTINUE 0x8000 at 0x10000
	r.audio.assign(0x18000, 0);
	if (!need(v == version::world_warrior ? "sf2_9.12a" : "s92_09.11a", d)) return false;
	std::memcpy(&r.audio[0], d.data(), 0x8000);
	std::memcpy(&r.audio[0x10000], d.data() + 0x8000, 0x8000);

	// "oki"
	r.oki.assign(0x40000, 0);
	if (!need(v == version::world_warrior ? "sf2_18.11c" : "s92_18.11c", d)) return false;
	std::memcpy(&r.oki[0], d.data(), 0x20000);
	if (!need(v == version::world_warrior ? "sf2_19.12c" : "s92_19.12c", d)) return false;
	std::memcpy(&r.oki[0x20000], d.data(), 0x20000);

	out = std::move(r);
	return true;
}

} // namespace sf2
