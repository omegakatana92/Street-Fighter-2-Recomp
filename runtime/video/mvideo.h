// Minimal MAME 0.264 video framework pieces used by the CPS-1 renderer:
// rectangle and bitmap (src/emu/rendutil.h, src/lib/util/bitmap.h),
// gfx_layout/gfx_element decoding (src/emu/drawgfx.cpp, digfx.cpp) and the
// cached tilemap (src/emu/tilemap.cpp: dirty tracking, pixmap and flagsmap
// rendering).  BSD-3-Clause, Nicola Salmoria, Aaron Giles and the MAME team.
#ifndef SF2_VIDEO_MVIDEO_H
#define SF2_VIDEO_MVIDEO_H

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint32_t offs_t;
typedef uint32_t pen_t;

namespace mv {

struct rectangle {
	s32 min_x = 0, max_x = 0, min_y = 0, max_y = 0;
	rectangle() {}
	rectangle(s32 minx, s32 maxx, s32 miny, s32 maxy) : min_x(minx), max_x(maxx), min_y(miny), max_y(maxy) {}
	s32 left() const { return min_x; }
	s32 right() const { return max_x; }
	s32 top() const { return min_y; }
	s32 bottom() const { return max_y; }
	s32 width() const { return max_x + 1 - min_x; }
	s32 height() const { return max_y + 1 - min_y; }
	void set(s32 minx, s32 maxx, s32 miny, s32 maxy) { min_x = minx; max_x = maxx; min_y = miny; max_y = maxy; }
	void setx(s32 minx, s32 maxx) { min_x = minx; max_x = maxx; }
	void sety(s32 miny, s32 maxy) { min_y = miny; max_y = maxy; }
	bool empty() const { return min_x > max_x || min_y > max_y; }
	rectangle &operator&=(const rectangle &s)
	{
		if (min_x < s.min_x) min_x = s.min_x;
		if (min_y < s.min_y) min_y = s.min_y;
		if (max_x > s.max_x) max_x = s.max_x;
		if (max_y > s.max_y) max_y = s.max_y;
		if (min_y > max_y) min_y = max_y;
		if (min_x > max_x) min_x = max_x;
		return *this;
	}
};

template <typename T>
class bitmap_t {
public:
	void allocate(int w, int h)
	{
		m_width = w;
		m_height = h;
		m_rowpixels = (w + 7) & ~7;
		m_data.assign(size_t(m_rowpixels) * h, T(0));
		m_cliprect = rectangle(0, w - 1, 0, h - 1);
	}
	int width() const { return m_width; }
	int height() const { return m_height; }
	int rowpixels() const { return m_rowpixels; }
	bool valid() const { return !m_data.empty(); }
	const rectangle &cliprect() const { return m_cliprect; }
	T &pix(s32 y, s32 x = 0) { return m_data[size_t(y) * m_rowpixels + x]; }
	const T &pix(s32 y, s32 x = 0) const { return m_data[size_t(y) * m_rowpixels + x]; }
	void fill(T v) { std::fill(m_data.begin(), m_data.end(), v); }
	void fill(T v, const rectangle &r)
	{
		rectangle c = r;
		c &= m_cliprect;
		if (c.empty()) return;
		for (s32 y = c.min_y; y <= c.max_y; y++)
			std::fill(&pix(y, c.min_x), &pix(y, c.max_x) + 1, v);
	}
	T *data() { return m_data.data(); }
private:
	int m_width = 0, m_height = 0, m_rowpixels = 0;
	std::vector<T> m_data;
	rectangle m_cliprect;
};

typedef bitmap_t<u8> bitmap_ind8;
typedef bitmap_t<u16> bitmap_ind16;
typedef bitmap_t<u32> bitmap_rgb32;

// rgb_t helpers (src/lib/util/palette.h)
struct rgb_t {
	u32 d;
	constexpr rgb_t(u32 v = 0) : d(v) {}
	constexpr rgb_t(u8 r, u8 g, u8 b) : d((0xffu << 24) | (u32(r) << 16) | (u32(g) << 8) | b) {}
	constexpr operator u32() const { return d; }
	constexpr u8 r() const { return u8(d >> 16); }
	constexpr u8 g() const { return u8(d >> 8); }
	constexpr u8 b() const { return u8(d); }
};

constexpr u32 GFX_RAW = 0x12345678;
constexpr u32 MAX_GFX_PLANES = 8;
constexpr u32 MAX_GFX_SIZE = 32;
#define RGN_FRAC(num,den) (0x80000000 | (((num) & 0x0f) << 27) | (((den) & 0x0f) << 23))
#define IS_FRAC(offset) ((offset) & 0x80000000)
#define FRAC_NUM(offset) (((offset) >> 27) & 0x0f)
#define FRAC_DEN(offset) (((offset) >> 23) & 0x0f)
#define FRAC_OFFSET(offset) ((offset) & 0x007fffff)
#define STEP2(START,STEP) (START),(START)+(STEP)
#define STEP4(START,STEP) STEP2(START,STEP),STEP2((START)+2*(STEP),STEP)
#define STEP8(START,STEP) STEP4(START,STEP),STEP4((START)+4*(STEP),STEP)
#define STEP16(START,STEP) STEP8(START,STEP),STEP8((START)+8*(STEP),STEP)
#define STEP32(START,STEP) STEP16(START,STEP),STEP16((START)+16*(STEP),STEP)

struct gfx_layout {
	u32 xoffs(int x) const { return xoffset[x]; }
	u32 yoffs(int y) const { return yoffset[y]; }
	u16 width;
	u16 height;
	u32 total;
	u16 planes;
	u32 planeoffset[MAX_GFX_PLANES];
	u32 xoffset[MAX_GFX_SIZE];
	u32 yoffset[MAX_GFX_SIZE];
	u32 charincrement;
};

class gfx_element {
public:
	// gfxdecode: layout with region fractions resolved against region_bytes
	gfx_element(const gfx_layout &gl, const u8 *srcdata, u32 region_bytes, u32 total_colors, u32 color_base);

	u16 width() const { return m_width; }
	u16 height() const { return m_height; }
	u32 elements() const { return m_total_elements; }
	u32 colorbase() const { return m_color_base; }
	u16 depth() const { return m_color_depth; }
	u16 granularity() const { return m_color_granularity; }
	u32 colors() const { return m_total_colors; }
	u32 rowbytes() const { return m_line_modulo; }
	u32 dirtyseq() const { return m_dirtyseq; }

	void set_layout(const gfx_layout &gl, const u8 *srcdata);
	void set_raw_layout(const u8 *srcdata, u32 width, u32 height, u32 total, u32 linemod, u32 charmod);
	void set_source(const u8 *source);
	void set_granularity(u16 g) { m_color_granularity = g; }
	void mark_dirty(u32 code) { if (code < elements()) { m_dirty[code] = 1; m_dirtyseq++; } }
	void mark_all_dirty() { std::memset(&m_dirty[0], 1, elements()); }
	bool has_pen_usage() const { return !m_pen_usage.empty(); }
	u32 pen_usage(u32 code) { if (code < m_dirty.size() && m_dirty[code]) decode(code); return m_pen_usage[code]; }
	// drawgfx.cpp prio_transpen / prio_opaque (bitmap_ind16 destination)
	void prio_transpen(bitmap_t<u16> &dest, const rectangle &cliprect, u32 code, u32 color, int flipx, int flipy, s32 destx, s32 desty, bitmap_t<u8> &priority, u32 pmask, u32 trans_pen);
	void prio_opaque(bitmap_t<u16> &dest, const rectangle &cliprect, u32 code, u32 color, int flipx, int flipy, s32 destx, s32 desty, bitmap_t<u8> &priority, u32 pmask);
	const u8 *get_data(u32 code)
	{
		if (code < m_dirty.size() && m_dirty[code]) decode(code);
		return m_gfxdata + code * m_char_modulo + m_starty * m_line_modulo + m_startx;
	}

private:
	void decode(u32 code);
	u16 m_width = 0, m_height = 0, m_startx = 0, m_starty = 0, m_origwidth = 0, m_origheight = 0;
	u32 m_total_elements = 0;
	u32 m_color_base = 0;
	u16 m_color_depth = 0, m_color_granularity = 0;
	u32 m_total_colors = 0;
	u32 m_line_modulo = 0, m_char_modulo = 0;
	const u8 *m_srcdata = nullptr;
	u32 m_dirtyseq = 1;
	u8 *m_gfxdata = nullptr;
	std::vector<u8> m_gfxdata_allocated;
	std::vector<u8> m_dirty;
	std::vector<u32> m_pen_usage;
	bool m_layout_is_raw = false;
	u8 m_layout_planes = 0;
	u32 m_layout_xormask = 0;
	u32 m_layout_charincrement = 0;
	std::vector<u32> m_layout_planeoffset, m_layout_xoffset, m_layout_yoffset;
};

struct gfxdecode {
	gfx_element *g[8] = {};
	gfx_element *gfx(int n) const { return g[n]; }
	void set_gfx(int n, gfx_element *e) { g[n] = e; }
};

constexpr u8 TILEMAP_PIXEL_CATEGORY_MASK = 0x0f;
constexpr u8 TILEMAP_PIXEL_TRANSPARENT = 0x00;
constexpr u8 TILEMAP_PIXEL_LAYER0 = 0x10;
constexpr u8 TILEMAP_PIXEL_LAYER1 = 0x20;
constexpr u8 TILEMAP_PIXEL_LAYER2 = 0x40;
constexpr u8 TILE_FLIPX = 0x01;
constexpr u8 TILE_FLIPY = 0x02;
constexpr u8 TILE_FORCE_LAYER0 = TILEMAP_PIXEL_LAYER0;
constexpr u8 TILE_FORCE_LAYER1 = TILEMAP_PIXEL_LAYER1;
constexpr u8 TILE_FORCE_LAYER2 = TILEMAP_PIXEL_LAYER2;
constexpr u32 TILEMAP_FLIPX = TILE_FLIPX;
constexpr u32 TILEMAP_FLIPY = TILE_FLIPY;
constexpr size_t TILEMAP_NUM_GROUPS = 256;
template <typename T> constexpr u8 TILE_FLIPYX(T yx) { return u8(yx & 3); }

enum tilemap_standard_mapper { TILEMAP_SCAN_ROWS = 0, TILEMAP_SCAN_COLS };

class tilemap_t;

struct tile_data {
	gfxdecode *decoder;
	const u8 *pen_data;
	const u8 *mask_data;
	pen_t palette_base;
	u8 category;
	u8 group;
	u8 flags;
	u8 pen_mask;
	u8 gfxnum;
	u32 code;
	void set(u8 _gfxnum, u32 rawcode, u32 rawcolor, u8 _flags)
	{
		gfx_element *gfx = decoder->gfx(_gfxnum);
		code = rawcode % gfx->elements();
		pen_data = gfx->get_data(code);
		palette_base = gfx->colorbase() + gfx->granularity() * (rawcolor % gfx->colors());
		flags = _flags;
		gfxnum = _gfxnum;
	}
};

typedef std::function<void(tilemap_t &, tile_data &, u32)> tilemap_get_info_delegate;
typedef std::function<u32(u32 col, u32 row, u32 num_cols, u32 num_rows)> tilemap_mapper_delegate;

constexpr u32 TILEMAP_DRAW_CATEGORY_MASK = 0x0f;
constexpr u32 TILEMAP_DRAW_LAYER0 = 0x10;
constexpr u32 TILEMAP_DRAW_LAYER1 = 0x20;
constexpr u32 TILEMAP_DRAW_LAYER2 = 0x40;
constexpr u32 TILEMAP_DRAW_OPAQUE = 0x80;
constexpr u32 TILEMAP_DRAW_ALL_CATEGORIES = 0x200;
constexpr int TILE_LINE_DISABLED = 0x80000000;

class tilemap_t {
public:
	tilemap_t(gfxdecode &decoder, tilemap_get_info_delegate tile_get_info, tilemap_standard_mapper mapper, u16 tilewidth, u16 tileheight, u32 cols, u32 rows);
	tilemap_t(gfxdecode &decoder, tilemap_get_info_delegate tile_get_info, tilemap_mapper_delegate mapper, u16 tilewidth, u16 tileheight, u32 cols, u32 rows);
	// tilemap.h / tilemap.cpp: enable, scrolling, transmasks and draw()
	void enable(bool en) { m_enable = en; }
	bool enabled() const { return m_enable; }
	u32 flip() const { return m_attributes; }
	void set_scrollx(int which, int value) { if (which < int(m_scrollrows)) m_rowscroll[size_t(which)] = value; }
	void set_scrolly(int which, int value) { if (which < int(m_scrollcols)) m_colscroll[size_t(which)] = value; }
	void set_scroll_rows(u32 n) { m_scrollrows = n; }
	void set_scroll_cols(u32 n) { m_scrollcols = n; }
	void map_pen_to_layer(int group, pen_t pen, u8 layermask);
	void set_transmask(int group, u32 fgmask, u32 bgmask);
	// screen_visarea: the screen's visible area (used to flip around its centre)
	void draw(bitmap_t<u16> &dest, bitmap_t<u8> &priority, const rectangle &screen_visarea, const rectangle &cliprect, u32 flags, u8 priority_code = 0, u8 priority_mask = 0xff);
	void mark_tile_dirty(u32 memindex);
	void mark_all_dirty() { m_all_tiles_dirty = true; m_all_tiles_clean = false; }
	void set_transparent_pen(pen_t pen);
	void set_flip(u32 attributes) { if (m_attributes != attributes) { m_attributes = attributes; mappings_update(); } }
	bitmap_ind16 &pixmap() { pixmap_update(); return m_pixmap; }
	bitmap_ind8 &flagsmap() { pixmap_update(); return m_flagsmap; }

private:
	static const u8 TILE_FLAG_DIRTY = 0xff;
	static const u32 INVALID_LOGICAL_INDEX = ~0u;
	static const pen_t MAX_PEN_TO_FLAGS = 256;
	u32 memory_index(u32 col, u32 row) const { return m_mapper ? m_mapper(col, row, m_cols, m_rows) : m_scan_cols ? col * m_rows + row : row * m_cols + col; }
	void init(gfxdecode &decoder, tilemap_get_info_delegate tile_get_info, u16 tilewidth, u16 tileheight, u32 cols, u32 rows);
	struct blit_parameters {
		bitmap_t<u8> *priority;
		rectangle cliprect;
		u32 tilemap_priority_code;
		u8 mask, value;
	};
	s32 effective_rowscroll(int index, u32 screen_width);
	s32 effective_colscroll(int index, u32 screen_height);
	void draw_instance(bitmap_t<u16> &dest, const blit_parameters &blit, int xpos, int ypos);
	void mappings_create();
	void mappings_update();
	void map_pens_to_layer(int group, pen_t pen, pen_t mask, u8 layermask);
	bool gfx_elements_changed();
	void realize_all_dirty_tiles();
	void pixmap_update();
	void tile_update(u32 logindex, u32 col, u32 row);
	u8 tile_draw(const u8 *pendata, u32 x0, u32 y0, u32 palette_base, u8 category, u8 group, u8 flags, u8 pen_mask);

	bool m_scan_cols = false;
	tilemap_mapper_delegate m_mapper;
	bool m_enable = true;
	u32 m_scrollrows = 1, m_scrollcols = 1;
	std::vector<s32> m_rowscroll, m_colscroll;
	s32 m_dx = 0, m_dx_flipped = 0, m_dy = 0, m_dy_flipped = 0;
	u32 m_rows, m_cols;
	u16 m_tilewidth, m_tileheight;
	u32 m_width, m_height;
	tilemap_get_info_delegate m_tile_get_info;
	tile_data m_tileinfo;
	u32 m_attributes = 0;
	bool m_all_tiles_dirty = true, m_all_tiles_clean = false;
	u32 m_gfx_used = 0;
	u32 m_gfx_dirtyseq[8] = {};
	std::vector<u32> m_memory_to_logical, m_logical_to_memory;
	std::vector<u8> m_tileflags;
	bitmap_ind16 m_pixmap;
	bitmap_ind8 m_flagsmap;
	std::vector<u8> m_pen_to_flags;
};

} // namespace mv

#endif
