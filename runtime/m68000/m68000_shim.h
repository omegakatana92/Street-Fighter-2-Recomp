// Types and stand-ins needed to compile MAME's generated 68000 handlers.
#ifndef SF2_M68000_SHIM_H
#define SF2_M68000_SHIM_H

#include <cstdint>
#include <cstring>
#include <vector>
#include "../emu/kemu.h"
#include "../emu/state_io.h"

#include "mame_types.h"   // u8..s64, offs_t, logerror, BIT

struct null_callback {
	bool isnull() const { return true; }
	template <typename... T> void operator()(T &&...) const {}
};

// 16-bit big-endian bus (memory_access<24,1,0,BIG>::specific)
struct m68000_bus {
	u16 (*read16)(void *ctx, offs_t address, u16 mem_mask) = nullptr;
	void (*write16)(void *ctx, offs_t address, u16 data, u16 mem_mask) = nullptr;
	void *ctx = nullptr;
};

struct m68000_space {
	m68000_bus *bus = nullptr;
	offs_t addr_mask = 0xffffff;
	bool cpu_space = false;
	u16 read_interruptible(offs_t a) { return bus->read16(bus->ctx, (a & addr_mask & ~1u) | (cpu_space ? 0x1000000u : 0), 0xffff); }
	u16 read_interruptible(offs_t a, u16 mask) { return bus->read16(bus->ctx, (a & addr_mask & ~1u) | (cpu_space ? 0x1000000u : 0), mask); }
	void write_interruptible(offs_t a, u16 d) { bus->write16(bus->ctx, a & addr_mask & ~1u, d, 0xffff); }
	void write_interruptible(offs_t a, u16 d, u16 mask) { bus->write16(bus->ctx, a & addr_mask & ~1u, d, mask); }
	u16 read_word(offs_t a) { return read_interruptible(a); }
};

#endif
