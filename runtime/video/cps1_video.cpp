// See cps1_video.h.  Function bodies follow MAME 0.264 cps1_v.cpp; the
// MAME function each one comes from is named above it.
#include "cps1_video.h"

#include <cstring>

using namespace mv;

namespace sf2 {

#define GFXTYPE_SPRITES   (1<<0)
#define GFXTYPE_SCROLL1   (1<<1)
#define GFXTYPE_SCROLL2   (1<<2)
#define GFXTYPE_SCROLL3   (1<<3)
#define GFXTYPE_STARS     (1<<4)

#define __not_applicable__  -1,-1,-1,-1,-1,-1,-1

/*                     CPSB ID    multiply protection      unknown      ctrl     priority masks   palctrl    layer enable masks  */
#define CPS_B_11     0x32,0x0401,          __not_applicable__,          0x26,{0x28,0x2a,0x2c,0x2e},0x30, {0x08,0x10,0x20,0x00,0x00}
#define CPS_B_21_DEF 0x32,  -1,   0x00,0x02,0x04,0x06, 0x08, -1,  -1,   0x26,{0x28,0x2a,0x2c,0x2e},0x30, {0x02,0x04,0x08,0x30,0x30}

static const gfx_range mapper_STF29_table[] =
{
	// verified from PAL dump:
	// bank 0 = pin 19 (ROMs 5,6,7,8)
	// bank 1 = pin 14 (ROMs 14,15,16,17)
	// bank 2 = pin 12 (ROMS 24,25,26,27)

	/* type            start    end      bank */
	{ GFXTYPE_SPRITES, 0x00000, 0x07fff, 0 },

	{ GFXTYPE_SPRITES, 0x08000, 0x0ffff, 1 },

	{ GFXTYPE_SPRITES, 0x10000, 0x11fff, 2 },
	{ GFXTYPE_SCROLL3, 0x02000, 0x03fff, 2 },
	{ GFXTYPE_SCROLL1, 0x04000, 0x04fff, 2 },
	{ GFXTYPE_SCROLL2, 0x05000, 0x07fff, 2 },
	{ 0, 0, 0, 0 }
};

static const gfx_range mapper_S9263B_table[] =
{
	// verified from PAL dump:
	// bank0 = pin 19 (ROMs 1,3) & pin 18 (ROMs 2,4)
	// bank1 = pin 17 (ROMs 5,7) & pin 16 (ROMs 6,8)
	// bank2 = pin 15 (ROMs 10,12) & pin 14 (ROMs 11,13)

	/* type            start    end      bank */
	{ GFXTYPE_SPRITES, 0x00000, 0x07fff, 0 },

	{ GFXTYPE_SPRITES, 0x08000, 0x0ffff, 1 },

	{ GFXTYPE_SPRITES, 0x10000, 0x11fff, 2 },
	{ GFXTYPE_SCROLL3, 0x02000, 0x03fff, 2 },
	{ GFXTYPE_SCROLL1, 0x04000, 0x04fff, 2 },
	{ GFXTYPE_SCROLL2, 0x05000, 0x07fff, 2 },
	{ 0, 0, 0, 0 }
};

#define mapper_STF29    { 0x8000, 0x8000, 0x8000, 0 }, mapper_STF29_table
#define mapper_S9263B   { 0x8000, 0x8000, 0x8000, 0 }, mapper_S9263B_table

// cps1_config_table entries for the three sets (the 4th value is in2_addr,
// where the CPS-B returns the kick buttons)
static const cps1_config cps1_config_table[] =
{
	{"sf2",         CPS_B_11,     mapper_STF29,  0x36, 0, 0, 0 },
	{"sf2ce",       CPS_B_21_DEF, mapper_S9263B, 0x36, 0, 0, 0 },
	{"sf2hf",       CPS_B_21_DEF, mapper_S9263B, 0x36, 0, 0, 0 },
	{nullptr, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0}, 0, {0, 0, 0, 0, 0}, {0, 0, 0, 0}, nullptr, 0, 0, 0, 0}
};

const cps1_config *cps1_find_config(const char *name)
{
	for (const cps1_config *c = cps1_config_table; c->name; c++)
		if (std::strcmp(c->name, name) == 0)
			return c;
	return nullptr;
}

/* Offset of each palette entry */
#define cps1_palette_entries (32*6)  /* Number colour schemes in palette */

/* CPS-A registers */
#define CPS1_OBJ_BASE           (0x00/2)    /* Base address of objects */
#define CPS1_SCROLL1_BASE       (0x02/2)    /* Base address of scroll 1 */
#define CPS1_SCROLL2_BASE       (0x04/2)    /* Base address of scroll 2 */
#define CPS1_SCROLL3_BASE       (0x06/2)    /* Base address of scroll 3 */
#define CPS1_OTHER_BASE         (0x08/2)    /* Base address of other video */
#define CPS1_PALETTE_BASE       (0x0a/2)    /* Base address of palette */
#define CPS1_SCROLL1_SCROLLX    (0x0c/2)    /* Scroll 1 X */
#define CPS1_SCROLL1_SCROLLY    (0x0e/2)    /* Scroll 1 Y */
#define CPS1_SCROLL2_SCROLLX    (0x10/2)    /* Scroll 2 X */
#define CPS1_SCROLL2_SCROLLY    (0x12/2)    /* Scroll 2 Y */
#define CPS1_SCROLL3_SCROLLX    (0x14/2)    /* Scroll 3 X */
#define CPS1_SCROLL3_SCROLLY    (0x16/2)    /* Scroll 3 Y */
#define CPS1_STARS1_SCROLLX     (0x18/2)    /* Stars 1 X */
#define CPS1_STARS1_SCROLLY     (0x1a/2)    /* Stars 1 Y */
#define CPS1_STARS2_SCROLLX     (0x1c/2)    /* Stars 2 X */
#define CPS1_STARS2_SCROLLY     (0x1e/2)    /* Stars 2 Y */
#define CPS1_ROWSCROLL_OFFS     (0x20/2)    /* base of row scroll offsets in other RAM */
#define CPS1_VIDEOCONTROL       (0x22/2)    /* flip screen, rowscroll enable */

static inline uint16_t combine(uint16_t &dst, uint16_t data, uint16_t mem_mask)
{
	dst = uint16_t((dst & ~mem_mask) | (data & mem_mask));
	return dst;
}

// cps1.cpp gfx_cps1
static const gfx_layout cps1_layout8x8 =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 24, 16, 8, 0 },
	{ STEP8(0, 1) },
	{ STEP8(0, 4*16) },
	64*8
};

static const gfx_layout cps1_layout8x8_2 =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 24, 16, 8, 0 },
	{ STEP8(32, 1) },
	{ STEP8(0, 4*16) },
	64*8
};

static const gfx_layout cps1_layout16x16 =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ 24, 16, 8, 0 },
	{ STEP8(0, 1), STEP8(32, 1) },
	{ STEP16(0, 4*16) },
	4*16*16
};

static const gfx_layout cps1_layout32x32 =
{
	32,32,
	RGN_FRAC(1,1),
	4,
	{ 24, 16, 8, 0 },
	{ STEP8(0, 1), STEP8(32, 1), STEP8(64, 1), STEP8(96, 1) },
	{ STEP32(0, 4*32) },
	4*32*32
};

cps1_video::cps1_video()
{
	std::memset(m_pens, 0, sizeof(m_pens));
	std::memset(m_empty_tile, 0, sizeof(m_empty_tile));
}

cps1_video::~cps1_video() = default;

// MACHINE_RESET_MEMBER(cps_state,cps) + cps_state::video_start
void cps1_video::start(const cps1_config *cfg, const uint8_t *gfxrom, uint32_t gfxrom_bytes, uint16_t *gfxram, uint16_t *cps_a_regs, uint16_t *cps_b_regs)
{
	m_game_config = cfg;
	m_gfxram = gfxram;
	m_cps_a_regs = cps_a_regs;
	m_cps_b_regs = cps_b_regs;
	m_cps_version = 1;
	m_scanline1 = m_scanline2 = m_scancalls = 0;   // init_cps1
	m_last_sprite_offset = 0;

	// GFXDECODE(gfx_cps1): four views of the "gfx" region, 0x100 colours of 16 pens
	m_gfx[0].reset(new gfx_element(cps1_layout8x8, gfxrom, gfxrom_bytes, 0x100, 0));
	m_gfx[1].reset(new gfx_element(cps1_layout8x8_2, gfxrom, gfxrom_bytes, 0x100, 0));
	m_gfx[2].reset(new gfx_element(cps1_layout16x16, gfxrom, gfxrom_bytes, 0x100, 0));
	m_gfx[3].reset(new gfx_element(cps1_layout32x32, gfxrom, gfxrom_bytes, 0x100, 0));
	for (int i = 0; i < 4; i++)
		m_gfxdecode.set_gfx(i, m_gfx[i].get());

	/* Put in some const */
	m_scroll_size    = 0x4000;  /* scroll1, scroll2, scroll3 */
	m_obj_size       = 0x0800;
	m_other_size     = 0x0800;
	m_palette_align  = 0x0400;  /* minimum alignment is a single palette page (512 colors). Verified on pcb. */
	m_palette_size   = cps1_palette_entries * 32; /* Size of palette RAM */

	/* create tilemaps */
	m_bg_tilemap[0].reset(new tilemap_t(m_gfxdecode,
		[this](tilemap_t &, tile_data &tileinfo, u32 tile_index) {
			// TILE_GET_INFO_MEMBER(cps_state::get_tile0_info)
			int code = m_scroll1[2 * tile_index];
			int attr = m_scroll1[2 * tile_index + 1];
			int gfxset;
			code = gfxrom_bank_mapper(GFXTYPE_SCROLL1, code);
			gfxset = (tile_index & 0x20) >> 5;
			tileinfo.set(u8(gfxset), u32(code), u32((attr & 0x1f) + 0x20), TILE_FLIPYX((attr & 0x60) >> 5));
			tileinfo.group = u8((attr & 0x0180) >> 7);
			if (code == -1)
				tileinfo.pen_data = m_empty_tile;
		},
		[](u32 col, u32 row, u32, u32) -> u32 { return (row & 0x1f) + ((col & 0x3f) << 5) + ((row & 0x20) << 6); },   // tilemap0_scan
		8, 8, 64, 64));
	m_bg_tilemap[1].reset(new tilemap_t(m_gfxdecode,
		[this](tilemap_t &, tile_data &tileinfo, u32 tile_index) {
			// get_tile1_info
			int code = m_scroll2[2 * tile_index];
			int attr = m_scroll2[2 * tile_index + 1];
			code = gfxrom_bank_mapper(GFXTYPE_SCROLL2, code);
			tileinfo.set(2, u32(code), u32((attr & 0x1f) + 0x40), TILE_FLIPYX((attr & 0x60) >> 5));
			tileinfo.group = u8((attr & 0x0180) >> 7);
			if (code == -1)
				tileinfo.pen_data = m_empty_tile;
		},
		[](u32 col, u32 row, u32, u32) -> u32 { return (row & 0x0f) + ((col & 0x3f) << 4) + ((row & 0x30) << 6); },   // tilemap1_scan
		16, 16, 64, 64));
	m_bg_tilemap[2].reset(new tilemap_t(m_gfxdecode,
		[this](tilemap_t &, tile_data &tileinfo, u32 tile_index) {
			// get_tile2_info
			int code = m_scroll3[2 * tile_index] & 0x3fff;
			int attr = m_scroll3[2 * tile_index + 1];
			code = gfxrom_bank_mapper(GFXTYPE_SCROLL3, code);
			tileinfo.set(3, u32(code), u32((attr & 0x1f) + 0x60), TILE_FLIPYX((attr & 0x60) >> 5));
			tileinfo.group = u8((attr & 0x0180) >> 7);
			if (code == -1)
				tileinfo.pen_data = m_empty_tile;
		},
		[](u32 col, u32 row, u32, u32) -> u32 { return (row & 0x07) + ((col & 0x3f) << 3) + ((row & 0x38) << 6); },   // tilemap2_scan
		32, 32, 64, 64));

	/* create empty tiles */
	std::memset(m_empty_tile, 0x0f, sizeof(m_empty_tile));

	/* front masks will change at runtime to handle sprite occluding */
	cps1_update_transmasks();

	m_buffered_obj.reset(new uint16_t[size_t(m_obj_size / 2)]);
	std::memset(m_buffered_obj.get(), 0, size_t(m_obj_size));

	/* clear RAM regions */
	std::memset(m_gfxram, 0, 0x30000);   /* Clear GFX RAM */
	std::memset(m_cps_a_regs, 0, 0x40);  /* Clear CPS-A registers */
	std::memset(m_cps_b_regs, 0, 0x40);  /* Clear CPS-B registers */

	/* Put in some defaults */
	m_cps_a_regs[CPS1_OBJ_BASE]     = 0x9200;
	m_cps_a_regs[CPS1_SCROLL1_BASE] = 0x9000;
	m_cps_a_regs[CPS1_SCROLL2_BASE] = 0x9040;
	m_cps_a_regs[CPS1_SCROLL3_BASE] = 0x9080;
	m_cps_a_regs[CPS1_OTHER_BASE]   = 0x9100;

	/* Set up old base */
	m_scroll1 = nullptr;
	m_scroll2 = nullptr;
	m_scroll3 = nullptr;
	m_obj = nullptr;
	m_other = nullptr;
	cps1_get_video_base();   /* Calculate base pointers */
	cps1_get_video_base();   /* Calculate old base pointers */

	// screen bitmaps (register_screen_bitmap) and the screen's priority bitmap
	m_bitmap.allocate(SCREEN_W, SCREEN_H);
	m_dummy_bitmap.allocate(SCREEN_W, SCREEN_H);
	m_priority.allocate(SCREEN_W, SCREEN_H);
	m_flip = 0;
	m_frame_number = 0;
	// PALETTE(...BLACK): every pen starts black
	for (auto &p : m_pens) p = 0xff000000u;
}

// cps_state::cps1_base
uint16_t *cps1_video::cps1_base(int offset, int boundary)
{
	int base = m_cps_a_regs[offset] * 256;
	base &= ~(boundary - 1);
	return &m_gfxram[(base & 0x3ffff) / 2];
}

// cps_state::cps1_cps_a_w
void cps1_video::cps_a_w(uint32_t offset, uint16_t data, uint16_t mem_mask)
{
	data = combine(m_cps_a_regs[offset], data, mem_mask);
	if (offset == CPS1_PALETTE_BASE)
		cps1_build_palette(cps1_base(CPS1_PALETTE_BASE, m_palette_align));
	(void)data;
}

// cps_state::cps1_cps_b_r
uint16_t cps1_video::cps_b_r(uint32_t offset, uint16_t in2)
{
	const int off = int(offset);
	if (off == m_game_config->cpsb_addr / 2)
		return uint16_t(m_game_config->cpsb_value);

	if (off == m_game_config->mult_result_lo / 2)
		return uint16_t((m_cps_b_regs[m_game_config->mult_factor1 / 2] *
				m_cps_b_regs[m_game_config->mult_factor2 / 2]) & 0xffff);

	if (off == m_game_config->mult_result_hi / 2)
		return uint16_t((m_cps_b_regs[m_game_config->mult_factor1 / 2] *
				m_cps_b_regs[m_game_config->mult_factor2 / 2]) >> 16);

	/* Extra input ports (on C-board) */
	if (m_game_config->in2_addr != 0 && off == m_game_config->in2_addr / 2)
		return in2;

	return 0xffff;
}

// cps_state::cps1_cps_b_w (no C-board outputs on these boards)
void cps1_video::cps_b_w(uint32_t offset, uint16_t data, uint16_t mem_mask)
{
	combine(m_cps_b_regs[offset], data, mem_mask);
}

// cps_state::cps1_get_video_base (no bootleg kludges)
void cps1_video::cps1_get_video_base()
{
	int layercontrol = 0, videocontrol = 0;

	if (m_scroll1 != cps1_base(CPS1_SCROLL1_BASE, m_scroll_size)) {
		m_scroll1 = cps1_base(CPS1_SCROLL1_BASE, m_scroll_size);
		m_bg_tilemap[0]->mark_all_dirty();
	}
	if (m_scroll2 != cps1_base(CPS1_SCROLL2_BASE, m_scroll_size)) {
		m_scroll2 = cps1_base(CPS1_SCROLL2_BASE, m_scroll_size);
		m_bg_tilemap[1]->mark_all_dirty();
	}
	if (m_scroll3 != cps1_base(CPS1_SCROLL3_BASE, m_scroll_size)) {
		m_scroll3 = cps1_base(CPS1_SCROLL3_BASE, m_scroll_size);
		m_bg_tilemap[2]->mark_all_dirty();
	}

	m_obj = cps1_base(CPS1_OBJ_BASE, m_obj_size);
	m_other = cps1_base(CPS1_OTHER_BASE, m_other_size);

	/* Get scroll values */
	m_scroll1x = m_cps_a_regs[CPS1_SCROLL1_SCROLLX];
	m_scroll1y = m_cps_a_regs[CPS1_SCROLL1_SCROLLY];
	m_scroll2x = m_cps_a_regs[CPS1_SCROLL2_SCROLLX];
	m_scroll2y = m_cps_a_regs[CPS1_SCROLL2_SCROLLY];
	m_scroll3x = m_cps_a_regs[CPS1_SCROLL3_SCROLLX];
	m_scroll3y = m_cps_a_regs[CPS1_SCROLL3_SCROLLY];
	m_stars1x = m_cps_a_regs[CPS1_STARS1_SCROLLX];
	m_stars1y = m_cps_a_regs[CPS1_STARS1_SCROLLY];
	m_stars2x = m_cps_a_regs[CPS1_STARS2_SCROLLX];
	m_stars2y = m_cps_a_regs[CPS1_STARS2_SCROLLY];

	/* Get layer enable bits */
	layercontrol = m_cps_b_regs[m_game_config->layer_control / 2];
	videocontrol = m_cps_a_regs[CPS1_VIDEOCONTROL];
	m_bg_tilemap[0]->enable(layercontrol & m_game_config->layer_enable_mask[0]);
	m_bg_tilemap[1]->enable((layercontrol & m_game_config->layer_enable_mask[1]) && (videocontrol & 4));
	m_bg_tilemap[2]->enable((layercontrol & m_game_config->layer_enable_mask[2]) && (videocontrol & 8));
	m_stars_enabled[0] = layercontrol & m_game_config->layer_enable_mask[3];
	m_stars_enabled[1] = layercontrol & m_game_config->layer_enable_mask[4];
}

// cps_state::cps1_gfxram_w (the RAM write itself is done by the board)
void cps1_video::gfxram_w(uint32_t offset, uint16_t data, uint16_t mem_mask)
{
	int page = (offset >> 7) & 0x3c0;
	combine(m_gfxram[offset], data, mem_mask);

	if (page == (m_cps_a_regs[CPS1_SCROLL1_BASE] & 0x3c0))
		m_bg_tilemap[0]->mark_tile_dirty(offset / 2 & 0x0fff);

	if (page == (m_cps_a_regs[CPS1_SCROLL2_BASE] & 0x3c0))
		m_bg_tilemap[1]->mark_tile_dirty(offset / 2 & 0x0fff);

	if (page == (m_cps_a_regs[CPS1_SCROLL3_BASE] & 0x3c0))
		m_bg_tilemap[2]->mark_tile_dirty(offset / 2 & 0x0fff);
}

// cps_state::gfxrom_bank_mapper
int cps1_video::gfxrom_bank_mapper(int type, int code)
{
	const gfx_range *range = m_game_config->bank_mapper;
	int shift = 0;

	switch (type) {
	case GFXTYPE_SPRITES: shift = 1; break;
	case GFXTYPE_SCROLL1: shift = 0; break;
	case GFXTYPE_SCROLL2: shift = 1; break;
	case GFXTYPE_SCROLL3: shift = 3; break;
	}

	code <<= shift;

	while (range->type) {
		if (code >= range->start && code <= range->end) {
			if (range->type & type) {
				int base = 0;
				for (int i = 0; i < range->bank; ++i)
					base += m_game_config->bank_sizes[i];
				return (base + (code & (m_game_config->bank_sizes[range->bank] - 1))) >> shift;
			}
		}
		++range;
	}
	return -1;
}

// cps_state::cps1_update_transmasks
void cps1_video::cps1_update_transmasks()
{
	for (int i = 0; i < 4; i++) {
		int mask;
		if (m_game_config->priority[i] != -1)
			mask = m_cps_b_regs[m_game_config->priority[i] / 2] ^ 0xffff;
		else
			mask = 0xffff;
		m_bg_tilemap[0]->set_transmask(i, u32(mask), 0x8000);
		m_bg_tilemap[1]->set_transmask(i, u32(mask), 0x8000);
		m_bg_tilemap[2]->set_transmask(i, u32(mask), 0x8000);
	}
}

// cps_state::cps1_build_palette
void cps1_video::cps1_build_palette(const uint16_t *palette_base)
{
	const uint16_t *palette_ram = palette_base;
	int ctrl = m_cps_b_regs[m_game_config->palette_control / 2];

	for (int page = 0; page < 6; ++page) {
		if ((ctrl >> page) & 1) {
			for (int offset = 0; offset < 0x200; ++offset) {
				int palette = *(palette_ram++);
				int bright = 0x0f + ((palette >> 12) << 1);
				int r = ((palette >> 8) & 0x0f) * 0x11 * bright / 0x2d;
				int g = ((palette >> 4) & 0x0f) * 0x11 * bright / 0x2d;
				int b = ((palette >> 0) & 0x0f) * 0x11 * bright / 0x2d;
				m_pens[0x200 * page + offset] = 0xff000000u | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
			}
		} else {
			// skip page in gfxram, but only if we have already copied at least one page
			if (palette_ram != palette_base)
				palette_ram += 0x200;
		}
	}
}

// cps_state::find_last_sprite
void cps1_video::find_last_sprite()
{
	int offset = 0;
	while (offset < m_obj_size / 2) {
		int colour = m_buffered_obj[size_t(offset + 3)];
		if ((colour & 0xff00) == 0xff00) {
			m_last_sprite_offset = offset - 4;
			return;
		}
		offset += 4;
	}
	m_last_sprite_offset = m_obj_size / 2 - 4;
}

// cps_state::cps1_render_sprites
void cps1_video::cps1_render_sprites(bitmap_ind16 &bitmap, const rectangle &cliprect)
{
#define DRAWSPRITE(CODE,COLOR,FLIPX,FLIPY,SX,SY)                    \
{                                                                   \
	if (m_flip)                                                     \
		m_gfxdecode.gfx(2)->prio_transpen(bitmap,                   \
				cliprect,                                           \
				u32(CODE),                                          \
				u32(COLOR),                                         \
				!(FLIPX),!(FLIPY),                                  \
				512-16-(SX),256-16-(SY), m_priority,0x02,15);       \
	else                                                            \
		m_gfxdecode.gfx(2)->prio_transpen(bitmap,                   \
				cliprect,                                           \
				u32(CODE),                                          \
				u32(COLOR),                                         \
				FLIPX,FLIPY,                                        \
				SX,SY, m_priority,0x02,15);                         \
}

	uint16_t *base = m_buffered_obj.get();
	const int baseadd = 4;

	for (int i = m_last_sprite_offset; i >= 0; i -= 4) {
		int x = *(base + 0);
		int y = *(base + 1);
		int code = *(base + 2);
		int colour = *(base + 3);
		int col = colour & 0x1f;

		code = gfxrom_bank_mapper(GFXTYPE_SPRITES, code);

		if (code != -1) {
			if (colour & 0xff00) {
				/* handle blocked sprites */
				int nx = (colour & 0x0f00) >> 8;
				int ny = (colour & 0xf000) >> 12;
				int nxs, nys, sx, sy;
				nx++;
				ny++;

				if (colour & 0x40) {
					/* Y flip */
					if (colour & 0x20) {
						for (nys = 0; nys < ny; nys++)
							for (nxs = 0; nxs < nx; nxs++) {
								sx = (x + nxs * 16) & 0x1ff;
								sy = (y + nys * 16) & 0x1ff;
								DRAWSPRITE((code & ~0xf) + ((code + (nx - 1) - nxs) & 0xf) + 0x10 * (ny - 1 - nys), (col & 0x1f), 1, 1, sx, sy);
							}
					} else {
						for (nys = 0; nys < ny; nys++)
							for (nxs = 0; nxs < nx; nxs++) {
								sx = (x + nxs * 16) & 0x1ff;
								sy = (y + nys * 16) & 0x1ff;
								DRAWSPRITE((code & ~0xf) + ((code + nxs) & 0xf) + 0x10 * (ny - 1 - nys), (col & 0x1f), 0, 1, sx, sy);
							}
					}
				} else {
					if (colour & 0x20) {
						for (nys = 0; nys < ny; nys++)
							for (nxs = 0; nxs < nx; nxs++) {
								sx = (x + nxs * 16) & 0x1ff;
								sy = (y + nys * 16) & 0x1ff;
								DRAWSPRITE((code & ~0xf) + ((code + (nx - 1) - nxs) & 0xf) + 0x10 * nys, (col & 0x1f), 1, 0, sx, sy);
							}
					} else {
						for (nys = 0; nys < ny; nys++)
							for (nxs = 0; nxs < nx; nxs++) {
								sx = (x + nxs * 16) & 0x1ff;
								sy = (y + nys * 16) & 0x1ff;
								DRAWSPRITE((code & ~0xf) + ((code + nxs) & 0xf) + 0x10 * nys, (col & 0x1f), 0, 0, sx, sy);
							}
					}
				}
			} else {
				/* Simple case... 1 sprite */
				DRAWSPRITE(code, (col & 0x1f), colour & 0x20, colour & 0x40, x & 0x1ff, y & 0x1ff);
			}
		}
		base += baseadd;
	}
#undef DRAWSPRITE
}

// cps_state::cps1_render_layer
void cps1_video::cps1_render_layer(bitmap_ind16 &bitmap, const rectangle &cliprect, int layer, int primask)
{
	switch (layer) {
	case 0:
		cps1_render_sprites(bitmap, cliprect);
		break;
	case 1:
	case 2:
	case 3:
		m_bg_tilemap[layer - 1]->draw(bitmap, m_priority, visarea(), cliprect, TILEMAP_DRAW_LAYER1, u8(primask));
		break;
	}
}

// cps_state::cps1_render_high_layer
void cps1_video::cps1_render_high_layer(bitmap_ind16 &, const rectangle &cliprect, int layer)
{
	switch (layer) {
	case 0:
		/* there are no high priority sprites */
		break;
	case 1:
	case 2:
	case 3:
		m_bg_tilemap[layer - 1]->draw(m_dummy_bitmap, m_priority, visarea(), cliprect, TILEMAP_DRAW_LAYER0, 1);
		break;
	}
}

// cps_state::render_layers
void cps1_video::render_layers(bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	int layercontrol = m_cps_b_regs[m_game_config->layer_control / 2];
	int l0 = (layercontrol >> 0x06) & 0x03;
	int l1 = (layercontrol >> 0x08) & 0x03;
	int l2 = (layercontrol >> 0x0a) & 0x03;
	int l3 = (layercontrol >> 0x0c) & 0x03;
	m_priority.fill(0, cliprect);

	cps1_render_layer(bitmap, cliprect, l0, 0);
	if (l1 == 0)
		cps1_render_high_layer(bitmap, cliprect, l0); /* prepare mask for sprites */
	cps1_render_layer(bitmap, cliprect, l1, 0);
	if (l2 == 0)
		cps1_render_high_layer(bitmap, cliprect, l1); /* prepare mask for sprites */
	cps1_render_layer(bitmap, cliprect, l2, 0);
	if (l3 == 0)
		cps1_render_high_layer(bitmap, cliprect, l2); /* prepare mask for sprites */
	cps1_render_layer(bitmap, cliprect, l3, 0);
}

// driver_device::flip_screen_set + updateflip (tilemap set_flip_all)
void cps1_video::flip_screen_set(int state)
{
	if (state)
		state = 0xff;
	if (m_flip != state) {
		m_flip = state;
		for (auto &t : m_bg_tilemap)
			t->set_flip(u32((TILEMAP_FLIPX & m_flip) | (TILEMAP_FLIPY & m_flip)));
	}
}

// cps_state::screen_update_cps1 (the whole visible area in one update)
void cps1_video::screen_update()
{
	const rectangle cliprect = visarea();
	bitmap_ind16 &bitmap = m_bitmap;
	int videocontrol = m_cps_a_regs[CPS1_VIDEOCONTROL];

	flip_screen_set(videocontrol & 0x8000);

	/* Get video memory base registers */
	cps1_get_video_base();

	/* Find the offset of the last sprite in the sprite table */
	find_last_sprite();

	cps1_update_transmasks();

	m_bg_tilemap[0]->set_scrollx(0, m_scroll1x);
	m_bg_tilemap[0]->set_scrolly(0, m_scroll1y);

	if (videocontrol & 0x01) {   /* linescroll enable */
		int scrly = -m_scroll2y;
		m_bg_tilemap[1]->set_scroll_rows(1024);
		int otheroffs = m_cps_a_regs[CPS1_ROWSCROLL_OFFS];
		for (int i = 0; i < 256; i++)
			m_bg_tilemap[1]->set_scrollx((i - scrly) & 0x3ff, m_scroll2x + m_other[(i + otheroffs) & 0x3ff]);
	} else {
		m_bg_tilemap[1]->set_scroll_rows(1);
		m_bg_tilemap[1]->set_scrollx(0, m_scroll2x);
	}
	m_bg_tilemap[1]->set_scrolly(0, m_scroll2y);
	m_bg_tilemap[2]->set_scrollx(0, m_scroll3x);
	m_bg_tilemap[2]->set_scrolly(0, m_scroll3y);

	/* Blank screen: CPS1 games use pen 0xbff as background color */
	bitmap.fill(0xbff, cliprect);

	// no stars region on these boards
	render_layers(bitmap, cliprect);
}

// cps_state::screen_vblank_cps1, rising edge
void cps1_video::screen_vblank()
{
	cps1_get_video_base();
	/* CPS1 sprites have to be delayed one frame */
	std::memcpy(m_buffered_obj.get(), m_obj, size_t(m_obj_size));
}

// save states: what MAME registers (scanline counters, last sprite offset,
// the buffered sprite list) plus the palette and flip state this port keeps
// outside gfxram; derived pointers and tilemaps are rebuilt after loading
// (MAME's register_postload(cps1_get_video_base)).
void cps1_video::save(state_writer &w) const
{
	w.put(m_scanline1); w.put(m_scanline2); w.put(m_scancalls);
	w.put(m_last_sprite_offset);
	w.raw(m_buffered_obj.get(), size_t(m_obj_size));
	w.put(m_pens);
	w.put(m_flip);
	w.put(m_frame_number);
}

void cps1_video::load(state_reader &r)
{
	r.get(m_scanline1); r.get(m_scanline2); r.get(m_scancalls);
	r.get(m_last_sprite_offset);
	r.raw(m_buffered_obj.get(), size_t(m_obj_size));
	r.get(m_pens);
	int flip = 0;
	r.get(flip);
	r.get(m_frame_number);
	// set the tilemap flip to the loaded state, then mark everything dirty
	m_flip = flip ? 0 : 0xff;
	flip_screen_set(flip);
	for (auto &t : m_bg_tilemap)
		t->mark_all_dirty();
	cps1_get_video_base();
}

} // namespace sf2
