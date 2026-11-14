// Byte-stream writer/reader for save states (little endian, raw fields).
#ifndef SF2_EMU_STATE_IO_H
#define SF2_EMU_STATE_IO_H

#include <cstdint>
#include <cstring>
#include <vector>

struct state_writer {
	std::vector<uint8_t> buf;
	void raw(const void *p, size_t n) { const uint8_t *b = (const uint8_t *)p; buf.insert(buf.end(), b, b + n); }
	// Values are stored field by field where a struct has padding (attotime:
	// int32 seconds + int64 attoseconds), so a state file has the same bytes
	// on every compiler and platform.
	template <typename T> void put(const T &v) { put_impl(v, 0); }
private:
	template <typename T> auto put_impl(const T &v, int) -> decltype((void)v.sec, (void)v.atto, void()) { put(v.sec); put(v.atto); }
	template <typename T> void put_impl(const T &v, long) { raw(&v, sizeof(T)); }
};

struct state_reader {
	const uint8_t *p = nullptr;
	size_t n = 0, at = 0;
	bool bad = false;
	state_reader(const uint8_t *data, size_t size) : p(data), n(size) {}
	void raw(void *dst, size_t len) {
		if (at + len > n) { bad = true; std::memset(dst, 0, len); return; }
		std::memcpy(dst, p + at, len);
		at += len;
	}
	template <typename T> void get(T &v) { get_impl(v, 0); }
private:
	template <typename T> auto get_impl(T &v, int) -> decltype((void)v.sec, (void)v.atto, void()) { get(v.sec); get(v.atto); }
	template <typename T> void get_impl(T &v, long) { raw(&v, sizeof(T)); }
};

#endif
