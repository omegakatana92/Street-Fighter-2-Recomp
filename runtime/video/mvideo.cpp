// See mvideo.h.  Bodies follow MAME 0.264 drawgfx.cpp, digfx.cpp and
// tilemap.cpp.
#include "mvideo.h"

namespace mv {

static inline int readbit(const u8 *src, unsigned bitnum) { return src[bitnum / 8] & (0x80 >> (bitnum % 8)); }

// device_gfx_interface::decode_gfx (region fractions) + gfx_element(layout)
gfx_element::gfx_element(const gfx_layout &glin, const u8 *srcdata, u32 region_bytes, u32 total_colors, u32 color_base)
{
	gfx_layout gl = glin;
	u32 region_length = 8 * region_bytes;
	if (IS_FRAC(gl.total))
		gl.total = region_length / gl.charincrement * FRAC_NUM(gl.total) / FRAC_DEN(gl.total);
	if (gl.planeoffset[0] != GFX_RAW) {
		for (int j = 0; j < gl.planes; j++)
			if (IS_FRAC(gl.planeoffset[j]))
				gl.planeoffset[j] = FRAC_OFFSET(gl.planeoffset[j]) + region_length * FRAC_NUM(gl.planeoffset[j]) / FRAC_DEN(gl.planeoffset[j]);
		for (int j = 0; j < gl.width; j++)
			if (IS_FRAC(gl.xoffset[j]))
				gl.xoffset[j] = FRAC_OFFSET(gl.xoffset[j]) + region_length * FRAC_NUM(gl.xoffset[j]) / FRAC_DEN(gl.xoffset[j]);
		for (int j = 0; j < gl.height; j++)
			if (IS_FRAC(gl.yoffset[j]))
				gl.yoffset[j] = FRAC_OFFSET(gl.yoffset[j]) + region_length * FRAC_NUM(gl.yoffset[j]) / FRAC_DEN(gl.yoffset[j]);
	}
	m_color_base = color_base;
	m_total_colors = total_colors;
	m_layout_xormask = 0;
	set_layout(gl, srcdata);
}

void gfx_element::set_layout(const gfx_layout &gl, const u8 *srcdata)
{
	m_srcdata = srcdata;
	m_width = m_origwidth = gl.width;
	m_height = m_origheight = gl.height;
	m_startx = m_starty = 0;
	m_total_elements = gl.total;
	m_color_depth = m_color_granularity = u16(1 << gl.planes);
	m_layout_is_raw = (gl.planeoffset[0] == GFX_RAW);
	m_layout_planes = u8(gl.planes);
	m_layout_charincrement = gl.charincrement;
	if (m_layout_is_raw) {
		m_layout_planeoffset.clear();
		m_layout_xoffset.clear();
		m_layout_yoffset.clear();
		m_gfxdata_allocated.clear();
		m_line_modulo = gl.yoffs(0) / 8;
		m_char_modulo = gl.charincrement / 8;
		m_gfxdata = const_cast<u8 *>(srcdata);
	} else {
		m_layout_planeoffset.resize(m_layout_planes);
		m_layout_xoffset.resize(m_width);
		m_layout_yoffset.resize(m_height);
		for (int p = 0; p < m_layout_planes; p++)
			m_layout_planeoffset[p] = gl.planeoffset[p];
		for (int y = 0; y < m_height; y++)
			m_layout_yoffset[y] = gl.yoffs(y);
		for (int x = 0; x < m_width; x++)
			m_layout_xoffset[x] = gl.xoffs(x);
		m_line_modulo = m_origwidth;
		m_char_modulo = m_line_modulo * m_origheight;
		m_gfxdata_allocated.resize(size_t(m_total_elements) * m_char_modulo);
		m_gfxdata = &m_gfxdata_allocated[0];
	}
	m_dirty.resize(m_total_elements);
	std::memset(&m_dirty[0], 1, m_total_elements);
	if (m_color_depth <= 32)
		m_pen_usage.resize(m_total_elements);
	else
		m_pen_usage.clear();
}

void gfx_element::set_raw_layout(const u8 *srcdata, u32 width, u32 height, u32 total, u32 linemod, u32 charmod)
{
	gfx_layout layout;
	std::memset(&layout, 0, sizeof(layout));
	layout.width = u16(width);
	layout.height = u16(height);
	layout.total = total;
	layout.planes = 8;
	layout.planeoffset[0] = GFX_RAW;
	layout.yoffset[0] = linemod;
	layout.charincrement = charmod;
	set_layout(layout, srcdata);
}

void gfx_element::set_source(const u8 *source)
{
	m_srcdata = source;
	std::memset(&m_dirty[0], 1, elements());
	if (m_layout_is_raw) m_gfxdata = const_cast<u8 *>(source);
}

void gfx_element::decode(u32 code)
{
	if (!m_layout_is_raw) {
		u8 *decode_base = m_gfxdata + code * m_char_modulo;
		std::memset(decode_base, 0, m_char_modulo);
		int plane, planebit;
		for (plane = 0, planebit = 1 << (m_layout_planes - 1); plane < m_layout_planes; plane++, planebit >>= 1) {
			int planeoffs = code * m_layout_charincrement + m_layout_planeoffset[plane];
			for (int y = 0; y < m_origheight; y++) {
				int yoffs = planeoffs + m_layout_yoffset[y];
				u8 *dp = decode_base + y * m_line_modulo;
				for (int x = 0; x < m_origwidth; x++)
					if (readbit(m_srcdata, (yoffs + m_layout_xoffset[x]) ^ m_layout_xormask))
						dp[x] |= planebit;
			}
		}
	}
	if (code < m_pen_usage.size()) {
		const u8 *dp = m_gfxdata + code * m_char_modulo;
		u32 usage = 0;
		for (int y = 0; y < m_origheight; y++) {
			for (int x = 0; x < m_origwidth; x++)
				usage |= 1u << dp[x];
			dp += m_line_modulo;
		}
		m_pen_usage[code] = usage;
	}
	m_dirty[code] = 0;
}

// ---------------------------------------------------------------- tilemap

tilemap_t::tilemap_t(gfxdecode &decoder, tilemap_get_info_delegate tile_get_info, tilemap_standard_mapper mapper, u16 tilewidth, u16 tileheight, u32 cols, u32 rows)
{
	m_scan_cols = (mapper == TILEMAP_SCAN_COLS);
	init(decoder, tile_get_info, tilewidth, tileheight, cols, rows);
}

tilemap_t::tilemap_t(gfxdecode &decoder, tilemap_get_info_delegate tile_get_info, tilemap_mapper_delegate mapper, u16 tilewidth, u16 tileheight, u32 cols, u32 rows)
{
	m_mapper = mapper;
	init(decoder, tile_get_info, tilewidth, tileheight, cols, rows);
}

void tilemap_t::init(gfxdecode &decoder, tilemap_get_info_delegate tile_get_info, u16 tilewidth, u16 tileheight, u32 cols, u32 rows)
{
	m_rows = rows;
	m_cols = cols;
	m_tilewidth = tilewidth;
	m_tileheight = tileheight;
	m_width = cols * tilewidth;
	m_height = rows * tileheight;
	m_tile_get_info = tile_get_info;
	m_attributes = 0;
	m_all_tiles_dirty = true;
	m_all_tiles_clean = false;
	m_gfx_used = 0;
	std::memset(m_gfx_dirtyseq, 0, sizeof(m_gfx_dirtyseq));
	m_enable = true;
	m_scrollrows = m_scrollcols = 1;
	m_rowscroll.assign(m_height, 0);
	m_colscroll.assign(m_width, 0);
	m_pixmap.allocate(int(m_width), int(m_height));
	m_flagsmap.allocate(int(m_width), int(m_height));
	m_pen_to_flags.assign(MAX_PEN_TO_FLAGS * TILEMAP_NUM_GROUPS, 0);
	mappings_create();
	std::memset(&m_tileinfo, 0, sizeof(m_tileinfo));
	m_tileinfo.decoder = &decoder;
	m_tileinfo.pen_mask = 0xff;
	m_tileinfo.gfxnum = 0xff;
	for (int group = 0; group < int(TILEMAP_NUM_GROUPS); group++)
		map_pens_to_layer(group, 0, 0, TILEMAP_PIXEL_LAYER0);
}

void tilemap_t::mark_tile_dirty(u32 memindex)
{
	if (memindex < m_memory_to_logical.size()) {
		u32 logindex = m_memory_to_logical[memindex];
		if (logindex != INVALID_LOGICAL_INDEX) {
			m_tileflags[logindex] = TILE_FLAG_DIRTY;
			m_all_tiles_clean = false;
		}
	}
}

void tilemap_t::map_pens_to_layer(int group, pen_t pen, pen_t mask, u8 layermask)
{
	pen_t start = pen & mask;
	pen_t stop = start | ~mask;
	stop = std::min(stop, MAX_PEN_TO_FLAGS - 1);
	u8 *array = &m_pen_to_flags[size_t(group) * MAX_PEN_TO_FLAGS];
	bool changed = false;
	for (pen_t cur = start; cur <= stop; cur++)
		if ((cur & mask) == pen && array[cur] != layermask) {
			changed = true;
			array[cur] = layermask;
		}
	if (changed)
		mark_all_dirty();
}

void tilemap_t::set_transparent_pen(pen_t pen)
{
	map_pens_to_layer(0, 0, 0, TILEMAP_PIXEL_LAYER0);
	map_pens_to_layer(0, pen, ~0u, TILEMAP_PIXEL_TRANSPARENT);
}

void tilemap_t::mappings_create()
{
	const u32 max_logical_index = m_rows * m_cols;
	u32 max_memory_index = 0;
	for (u32 row = 0; row < m_rows; row++)
		for (u32 col = 0; col < m_cols; col++)
			max_memory_index = std::max(max_memory_index, memory_index(col, row));
	max_memory_index++;
	m_memory_to_logical.resize(max_memory_index);
	m_logical_to_memory.resize(max_logical_index);
	m_tileflags.resize(max_logical_index);
	mappings_update();
}

void tilemap_t::mappings_update()
{
	std::fill(m_memory_to_logical.begin(), m_memory_to_logical.end(), INVALID_LOGICAL_INDEX);
	for (u32 logindex = 0; logindex < m_logical_to_memory.size(); logindex++) {
		u32 logical_col = logindex % m_cols;
		u32 logical_row = logindex / m_cols;
		u32 memindex = memory_index(logical_col, logical_row);
		if (m_attributes & TILEMAP_FLIPX)
			logical_col = (m_cols - 1) - logical_col;
		if (m_attributes & TILEMAP_FLIPY)
			logical_row = (m_rows - 1) - logical_row;
		u32 flipped_logindex = logical_row * m_cols + logical_col;
		m_memory_to_logical[memindex] = flipped_logindex;
		m_logical_to_memory[flipped_logindex] = memindex;
	}
	mark_all_dirty();
}

bool tilemap_t::gfx_elements_changed()
{
	u32 usedmask = m_gfx_used;
	bool isdirty = false;
	for (int gfxnum = 0; usedmask != 0; usedmask >>= 1, gfxnum++)
		if ((usedmask & 1) != 0)
			if (m_gfx_dirtyseq[gfxnum] != m_tileinfo.decoder->gfx(gfxnum)->dirtyseq()) {
				m_gfx_dirtyseq[gfxnum] = m_tileinfo.decoder->gfx(gfxnum)->dirtyseq();
				isdirty = true;
			}
	return isdirty;
}

void tilemap_t::realize_all_dirty_tiles()
{
	if (m_all_tiles_dirty || gfx_elements_changed()) {
		std::memset(&m_tileflags[0], TILE_FLAG_DIRTY, m_tileflags.size());
		m_all_tiles_dirty = false;
		m_gfx_used = 0;
	}
}

void tilemap_t::pixmap_update()
{
	if (gfx_elements_changed())
		mark_all_dirty();
	if (m_all_tiles_clean)
		return;
	realize_all_dirty_tiles();
	u32 logindex = 0;
	for (u32 row = 0; row < m_rows; row++)
		for (u32 col = 0; col < m_cols; col++, logindex++)
			if (m_tileflags[logindex] == TILE_FLAG_DIRTY)
				tile_update(logindex, col, row);
	m_all_tiles_clean = true;
}

void tilemap_t::tile_update(u32 logindex, u32 col, u32 row)
{
	u32 memindex = m_logical_to_memory[logindex];
	m_tile_get_info(*this, m_tileinfo, memindex);
	u32 flags = m_tileinfo.flags ^ (m_attributes & 0x03);
	u32 x0 = m_tilewidth * col;
	u32 y0 = m_tileheight * row;
	m_tileflags[logindex] = tile_draw(m_tileinfo.pen_data, x0, y0, m_tileinfo.palette_base, m_tileinfo.category, m_tileinfo.group, u8(flags), m_tileinfo.pen_mask);
	if (m_tileinfo.gfxnum != 0xff && (m_gfx_used & (1u << m_tileinfo.gfxnum)) == 0) {
		m_gfx_used |= 1u << m_tileinfo.gfxnum;
		m_gfx_dirtyseq[m_tileinfo.gfxnum] = m_tileinfo.decoder->gfx(m_tileinfo.gfxnum)->dirtyseq();
	}
}

u8 tilemap_t::tile_draw(const u8 *pendata, u32 x0, u32 y0, u32 palette_base, u8 category, u8 group, u8 flags, u8 pen_mask)
{
	category |= flags & (TILE_FORCE_LAYER0 | TILE_FORCE_LAYER1 | TILE_FORCE_LAYER2);
	int dy0 = 1;
	if (flags & TILE_FLIPY) {
		y0 += m_tileheight - 1;
		dy0 = -1;
	}
	int dx0 = 1;
	if (flags & TILE_FLIPX) {
		x0 += m_tilewidth - 1;
		dx0 = -1;
	}
	const u8 *penmap = &m_pen_to_flags[size_t(group) * MAX_PEN_TO_FLAGS];
	u8 andmask = 0xff, ormask = 0;
	for (u16 ty = 0; ty < m_tileheight; ty++) {
		u16 *pixptr = &m_pixmap.pix(s32(y0), s32(x0));
		u8 *flagsptr = &m_flagsmap.pix(s32(y0), s32(x0));
		y0 += dy0;
		int xoffs = 0;
		for (u16 tx = 0; tx < m_tilewidth; tx++) {
			u8 pen = u8((*pendata++) & pen_mask);
			u8 map = penmap[pen];
			pixptr[xoffs] = u16(palette_base + pen);
			flagsptr[xoffs] = map | category;
			andmask &= map;
			ormask |= map;
			xoffs += dx0;
		}
	}
	return andmask ^ ormask;
}

// tilemap.cpp map_pen_to_layer / set_transmask
void tilemap_t::map_pen_to_layer(int group, pen_t pen, u8 layermask)
{
	if (pen >= MAX_PEN_TO_FLAGS) return;
	u8 &v = m_pen_to_flags[size_t(group) * MAX_PEN_TO_FLAGS + pen];
	if (v != layermask) {
		v = layermask;
		mark_all_dirty();
	}
}

void tilemap_t::set_transmask(int group, u32 fgmask, u32 bgmask)
{
	for (pen_t pen = 0; pen < 32; pen++) {
		u8 fgbits = ((fgmask >> pen) & 1) ? TILEMAP_PIXEL_TRANSPARENT : TILEMAP_PIXEL_LAYER0;
		u8 bgbits = ((bgmask >> pen) & 1) ? TILEMAP_PIXEL_TRANSPARENT : TILEMAP_PIXEL_LAYER1;
		map_pen_to_layer(group, pen, u8(fgbits | bgbits));
	}
}

s32 tilemap_t::effective_rowscroll(int index, u32 screen_width)
{
	if (m_attributes & TILEMAP_FLIPY)
		index = int(m_scrollrows) - 1 - index;
	s32 value;
	if (!(m_attributes & TILEMAP_FLIPX))
		value = m_dx - m_rowscroll[size_t(index)];
	else
		value = s32(screen_width) - s32(m_width) - (m_dx_flipped - m_rowscroll[size_t(index)]);
	if (value < 0)
		value = s32(m_width) - (-value) % s32(m_width);
	else
		value %= s32(m_width);
	return value;
}

s32 tilemap_t::effective_colscroll(int index, u32 screen_height)
{
	if (m_attributes & TILEMAP_FLIPX)
		index = int(m_scrollcols) - 1 - index;
	s32 value;
	if (!(m_attributes & TILEMAP_FLIPY))
		value = m_dy - m_colscroll[size_t(index)];
	else
		value = s32(screen_height) - s32(m_height) - (m_dy_flipped - m_colscroll[size_t(index)]);
	if (value < 0)
		value = s32(m_height) - (-value) % s32(m_height);
	else
		value %= s32(m_height);
	return value;
}

// tilemap_t::draw_common for a bitmap_ind16 destination (no palette offset)
void tilemap_t::draw(bitmap_t<u16> &dest, bitmap_t<u8> &priority_bitmap, const rectangle &visarea, const rectangle &cliprect, u32 flags, u8 priority, u8 priority_mask)
{
	if (!m_enable)
		return;
	blit_parameters blit;
	blit.priority = &priority_bitmap;
	blit.cliprect = cliprect;
	blit.tilemap_priority_code = u32(priority) | (u32(priority_mask) << 8);
	u32 mask = TILEMAP_PIXEL_CATEGORY_MASK;
	u32 value = flags & TILEMAP_PIXEL_CATEGORY_MASK;
	if ((flags & (TILEMAP_DRAW_LAYER0 | TILEMAP_DRAW_LAYER1 | TILEMAP_DRAW_LAYER2)) == 0)
		flags |= TILEMAP_DRAW_LAYER0;
	mask |= flags & (TILEMAP_DRAW_LAYER0 | TILEMAP_DRAW_LAYER1 | TILEMAP_DRAW_LAYER2);
	value |= flags & (TILEMAP_DRAW_LAYER0 | TILEMAP_DRAW_LAYER1 | TILEMAP_DRAW_LAYER2);
	if (flags & TILEMAP_DRAW_OPAQUE) {
		mask &= ~u32(TILEMAP_PIXEL_LAYER0 | TILEMAP_PIXEL_LAYER1 | TILEMAP_PIXEL_LAYER2);
		value &= ~u32(TILEMAP_PIXEL_LAYER0 | TILEMAP_PIXEL_LAYER1 | TILEMAP_PIXEL_LAYER2);
	}
	if (flags & TILEMAP_DRAW_ALL_CATEGORIES) {
		mask &= ~u32(TILEMAP_PIXEL_CATEGORY_MASK);
		value &= ~u32(TILEMAP_PIXEL_CATEGORY_MASK);
	}
	blit.mask = u8(mask);
	blit.value = u8(value);

	realize_all_dirty_tiles();

	u32 const xextent = u32(visarea.max_x + visarea.min_x + 1);
	u32 const yextent = u32(visarea.max_y + visarea.min_y + 1);
	if (m_scrollrows == 1 && m_scrollcols == 1) {
		int scrollx = effective_rowscroll(0, xextent);
		int scrolly = effective_colscroll(0, yextent);
		for (int ypos = scrolly - int(m_height); ypos <= blit.cliprect.max_y; ypos += int(m_height))
			for (int xpos = scrollx - int(m_width); xpos <= blit.cliprect.max_x; xpos += int(m_width))
				draw_instance(dest, blit, xpos, ypos);
	} else if (m_scrollcols == 1) {
		const rectangle original_cliprect = blit.cliprect;
		int rowheight = int(m_height / m_scrollrows);
		int scrolly = effective_colscroll(0, yextent);
		for (int ypos = scrolly - int(m_height); ypos <= original_cliprect.max_y; ypos += int(m_height)) {
			int const firstrow = std::max((original_cliprect.min_y - ypos) / rowheight, 0);
			int const lastrow = std::min((original_cliprect.max_y - ypos) / rowheight, s32(m_scrollrows) - 1);
			int nextrow;
			for (int currow = firstrow; currow <= lastrow; currow = nextrow) {
				int scrollx = effective_rowscroll(currow, xextent);
				for (nextrow = currow + 1; nextrow <= lastrow; nextrow++)
					if (effective_rowscroll(nextrow, xextent) != scrollx)
						break;
				if (scrollx == TILE_LINE_DISABLED)
					continue;
				blit.cliprect.sety(currow * rowheight + ypos, nextrow * rowheight - 1 + ypos);
				blit.cliprect &= original_cliprect;
				for (int xpos = scrollx - int(m_width); xpos <= original_cliprect.max_x; xpos += int(m_width))
					draw_instance(dest, blit, xpos, ypos);
			}
		}
	} else if (m_scrollrows == 1) {
		const rectangle original_cliprect = blit.cliprect;
		int scrollx = effective_rowscroll(0, xextent);
		int colwidth = int(m_width / m_scrollcols);
		int nextcol;
		for (int curcol = 0; curcol < int(m_scrollcols); curcol = nextcol) {
			int scrolly = effective_colscroll(curcol, yextent);
			for (nextcol = curcol + 1; nextcol < int(m_scrollcols); nextcol++)
				if (effective_colscroll(nextcol, yextent) != scrolly)
					break;
			if (scrolly == TILE_LINE_DISABLED)
				continue;
			for (int xpos = scrollx - int(m_width); xpos <= original_cliprect.max_x; xpos += int(m_width)) {
				blit.cliprect.setx(curcol * colwidth + xpos, nextcol * colwidth - 1 + xpos);
				blit.cliprect &= original_cliprect;
				for (int ypos = scrolly - int(m_height); ypos <= original_cliprect.max_y; ypos += int(m_height))
					draw_instance(dest, blit, xpos, ypos);
			}
		}
	}
}

void tilemap_t::draw_instance(bitmap_t<u16> &dest, const blit_parameters &blit, int xpos, int ypos)
{
	int x1 = std::max(xpos, blit.cliprect.min_x);
	int x2 = std::min(xpos + int(m_width), blit.cliprect.max_x + 1);
	int y1 = std::max(ypos, blit.cliprect.min_y);
	int y2 = std::min(ypos + int(m_height), blit.cliprect.max_y + 1);
	if (x1 >= x2 || y1 >= y2)
		return;

	bitmap_t<u8> &priority_bitmap = *blit.priority;
	const int prio_rowpixels = priority_bitmap.rowpixels();
	const int dest_rowpixels = dest.rowpixels();
	// base pointers at (y1, xpos); xpos may be negative, so index arithmetic
	// stays in ptrdiff_t and only in-range pixels are touched
	u8 *priority_baseaddr = priority_bitmap.data() + ptrdiff_t(y1) * prio_rowpixels + xpos;
	u16 *dest_baseaddr = dest.data() + ptrdiff_t(y1) * dest_rowpixels + xpos;

	x1 -= xpos;
	y1 -= ypos;
	x2 -= xpos;
	y2 -= ypos;

	const u16 *source_baseaddr = &m_pixmap.pix(y1);
	const u8 *mask_baseaddr = &m_flagsmap.pix(y1);

	int mincol = x1 / m_tilewidth;
	int maxcol = (x2 + m_tilewidth - 1) / m_tilewidth;

	int y = y1;
	int nexty = m_tileheight * (y1 / m_tileheight) + m_tileheight;
	nexty = std::min(nexty, y2);
	const u32 pcode = blit.tilemap_priority_code;

	enum trans_t { WHOLLY_TRANSPARENT, WHOLLY_OPAQUE, MASKED };
	for (;;) {
		int row = y / m_tileheight;
		int x_start = x1;
		trans_t prev_trans = WHOLLY_TRANSPARENT;
		trans_t cur_trans;
		for (int column = mincol; column <= maxcol; column++) {
			int x_end;
			if (column == maxcol)
				cur_trans = WHOLLY_TRANSPARENT;
			else {
				u32 logindex = u32(row) * m_cols + u32(column);
				if (m_tileflags[logindex] == TILE_FLAG_DIRTY)
					tile_update(logindex, u32(column), u32(row));
				if ((m_tileflags[logindex] & blit.mask) != 0)
					cur_trans = MASKED;
				else
					cur_trans = ((mask_baseaddr[column * m_tilewidth] & blit.mask) == blit.value) ? WHOLLY_OPAQUE : WHOLLY_TRANSPARENT;
			}
			if (cur_trans == prev_trans)
				continue;
			x_end = column * m_tilewidth;
			x_end = std::max(x_end, x1);
			x_end = std::min(x_end, x2);
			if (prev_trans != WHOLLY_TRANSPARENT) {
				const u16 *source0 = source_baseaddr + x_start;
				u16 *dest0 = dest_baseaddr + x_start;
				u8 *pmap0 = priority_baseaddr + x_start;
				const int count = x_end - x_start;
				if (prev_trans == WHOLLY_OPAQUE) {
					for (int cury = y; cury < nexty; cury++) {
						std::memcpy(dest0, source0, size_t(count) * 2);
						if (pcode != 0xff00)
							for (int i = 0; i < count; i++)
								pmap0[i] = u8((pmap0[i] & (pcode >> 8)) | pcode);
						dest0 += dest_rowpixels;
						source0 += m_pixmap.rowpixels();
						pmap0 += prio_rowpixels;
					}
				} else {
					const u8 *mask0 = mask_baseaddr + x_start;
					for (int cury = y; cury < nexty; cury++) {
						if ((pcode & 0xffff) != 0xff00) {
							for (int i = 0; i < count; i++)
								if ((mask0[i] & blit.mask) == blit.value) {
									dest0[i] = source0[i];
									pmap0[i] = u8((pmap0[i] & (pcode >> 8)) | pcode);
								}
						} else {
							for (int i = 0; i < count; i++)
								if ((mask0[i] & blit.mask) == blit.value)
									dest0[i] = source0[i];
						}
						dest0 += dest_rowpixels;
						source0 += m_pixmap.rowpixels();
						mask0 += m_flagsmap.rowpixels();
						pmap0 += prio_rowpixels;
					}
				}
			}
			x_start = x_end;
			prev_trans = cur_trans;
		}
		if (nexty == y2)
			break;
		priority_baseaddr += prio_rowpixels * (nexty - y);
		source_baseaddr += m_pixmap.rowpixels() * (nexty - y);
		mask_baseaddr += m_flagsmap.rowpixels() * (nexty - y);
		dest_baseaddr += dest_rowpixels * (nexty - y);
		y = nexty;
		nexty += m_tileheight;
		nexty = std::min(nexty, y2);
	}
}

// ---------------------------------------------------------------- drawgfx

// drawgfx_core with a priority bitmap: clip, flip and walk the source
template <typename Op>
static void drawgfx_prio_core(gfx_element &gfx, bitmap_t<u16> &dest, const rectangle &cliprect, u32 code, int flipx, int flipy, s32 destx, s32 desty, bitmap_t<u8> &priority, Op op)
{
	rectangle clip = cliprect;
	clip &= dest.cliprect();
	const s32 w = gfx.width(), h = gfx.height();
	s32 x0 = std::max(destx, clip.min_x), x1 = std::min(destx + w - 1, clip.max_x);
	s32 y0 = std::max(desty, clip.min_y), y1 = std::min(desty + h - 1, clip.max_y);
	if (x0 > x1 || y0 > y1)
		return;
	const u8 *src = gfx.get_data(code);
	const u32 rowbytes = gfx.rowbytes();
	for (s32 y = y0; y <= y1; y++) {
		s32 sy = y - desty;
		if (flipy) sy = h - 1 - sy;
		const u8 *srow = src + size_t(sy) * rowbytes;
		u16 *d = &dest.pix(y);
		u8 *p = &priority.pix(y);
		for (s32 x = x0; x <= x1; x++) {
			s32 sx = x - destx;
			if (flipx) sx = w - 1 - sx;
			op(d[x], p[x], srow[sx]);
		}
	}
}

void gfx_element::prio_transpen(bitmap_t<u16> &dest, const rectangle &cliprect, u32 code, u32 color, int flipx, int flipy, s32 destx, s32 desty, bitmap_t<u8> &priority, u32 pmask, u32 trans_pen)
{
	if (trans_pen > 0xff)
		return prio_opaque(dest, cliprect, code, color, flipx, flipy, destx, desty, priority, pmask);
	code %= elements();
	if (has_pen_usage()) {
		u32 usage = pen_usage(code);
		if ((usage & ~(1u << trans_pen)) == 0)
			return;
		if ((usage & (1u << trans_pen)) == 0)
			return prio_opaque(dest, cliprect, code, color, flipx, flipy, destx, desty, priority, pmask);
	}
	pmask |= 1u << 31;
	color = colorbase() + granularity() * (color % colors());
	drawgfx_prio_core(*this, dest, cliprect, code, flipx, flipy, destx, desty, priority,
		[pmask, trans_pen, color](u16 &destp, u8 &pri, const u8 &srcp) {
			u32 srcdata = srcp;
			if (srcdata != trans_pen) {
				if (((1u << (pri & 0x1f)) & pmask) == 0)
					destp = u16(color + srcdata);
				pri = 31;
			}
		});
}

void gfx_element::prio_opaque(bitmap_t<u16> &dest, const rectangle &cliprect, u32 code, u32 color, int flipx, int flipy, s32 destx, s32 desty, bitmap_t<u8> &priority, u32 pmask)
{
	code %= elements();
	pmask |= 1u << 31;
	color = colorbase() + granularity() * (color % colors());
	drawgfx_prio_core(*this, dest, cliprect, code, flipx, flipy, destx, desty, priority,
		[pmask, color](u16 &destp, u8 &pri, const u8 &srcp) {
			if (((1u << (pri & 0x1f)) & pmask) == 0)
				destp = u16(color + srcp);
			pri = 31;
		});
}

} // namespace mv
