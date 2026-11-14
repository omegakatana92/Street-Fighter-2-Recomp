// CPS-1 video (CPS-A / CPS-B) after MAME 0.264 src/mame/capcom/cps1_v.cpp
// (BSD-3-Clause; Paul Leaman, Curt Coder, Nicola Salmoria and the MAME team).
//
// Only what the three Street Fighter II sets use is kept: the CPS-B-11 and
// CPS-B-21 configurations, the STF29 and S9263B graphics-ROM mappers, three
// scroll layers, sprites with the one-frame sprite buffer, the palette copy
// on a palette-base write, layer priority masks and flip screen.  These
// boards have no starfield ROM.
#ifndef SF2_VIDEO_CPS1_VIDEO_H
#define SF2_VIDEO_CPS1_VIDEO_H

#include <cstdint>
#include <memory>
#include "mvideo.h"
#include "../emu/state_io.h"

namespace sf2 {

struct gfx_range {
	int type;
	int start;
	int end;
	int bank;
};

// cps1.h CPS1config (fields in MAME's order)
struct cps1_config {
	const char *name;
	int cpsb_addr, cpsb_value;
	int mult_factor1, mult_factor2, mult_result_lo, mult_result_hi;
	int unknown1, unknown2, unknown3;
	int layer_control;
	int priority[4];
	int palette_control;
	int layer_enable_mask[5];
	int bank_sizes[4];
	const gfx_range *bank_mapper;
	int in2_addr, in3_addr, out2_addr;
	int bootleg_kludge;
};

const cps1_config *cps1_find_config(const char *name);

class cps1_video {
public:
	// screen raw parameters (cps1.h): 8 MHz pixel clock, 512x262, visible 64-447 x 16-239
	static constexpr int SCREEN_W = 512, SCREEN_H = 262;
	static constexpr int VIS_X0 = 64, VIS_X1 = 447, VIS_Y0 = 16, VIS_Y1 = 239;

	cps1_video();
	~cps1_video();
	// video_start (includes MACHINE_RESET_MEMBER(cps): picks the config)
	void start(const cps1_config *cfg, const uint8_t *gfxrom, uint32_t gfxrom_bytes, uint16_t *gfxram, uint16_t *cps_a_regs, uint16_t *cps_b_regs);

	// bus handlers
	void cps_a_w(uint32_t offset, uint16_t data, uint16_t mem_mask);
	uint16_t cps_b_r(uint32_t offset, uint16_t in2);
	void cps_b_w(uint32_t offset, uint16_t data, uint16_t mem_mask);
	void gfxram_w(uint32_t offset, uint16_t data, uint16_t mem_mask);

	// screen
	void screen_update();               // screen_update_cps1 into m_bitmap
	void screen_vblank();               // screen_vblank_cps1(1)
	const mv::bitmap_ind16 &bitmap() const { return m_bitmap; }
	const uint32_t *palette() const { return m_pens; }    // xRGB8888, 0xc00 entries
	uint64_t m_frame_number = 0;         // screen.frame_number() (stars only)

	// save states
	void save(state_writer &w) const;
	void load(state_reader &r);

private:
	const cps1_config *m_game_config = nullptr;
	int m_cps_version = 1;
	uint16_t *m_gfxram = nullptr;
	uint16_t *m_cps_a_regs = nullptr;
	uint16_t *m_cps_b_regs = nullptr;
	int m_scroll_size = 0, m_obj_size = 0, m_other_size = 0, m_palette_align = 0, m_palette_size = 0;
	uint8_t m_empty_tile[32 * 32];
	uint16_t *m_scroll1 = nullptr, *m_scroll2 = nullptr, *m_scroll3 = nullptr, *m_obj = nullptr, *m_other = nullptr;
	std::unique_ptr<uint16_t[]> m_buffered_obj;
	int m_last_sprite_offset = 0;
	int m_scroll1x = 0, m_scroll1y = 0, m_scroll2x = 0, m_scroll2y = 0, m_scroll3x = 0, m_scroll3y = 0;
	int m_stars1x = 0, m_stars1y = 0, m_stars2x = 0, m_stars2y = 0;
	int m_stars_enabled[2] = {0, 0};
	int m_scanline1 = 0, m_scanline2 = 0, m_scancalls = 0;
	int m_flip = 0;                      // driver_device flip_screen (0 or 0xff)

	mv::gfxdecode m_gfxdecode;
	std::unique_ptr<mv::gfx_element> m_gfx[4];
	std::unique_ptr<mv::tilemap_t> m_bg_tilemap[3];
	mv::bitmap_ind16 m_bitmap, m_dummy_bitmap;
	mv::bitmap_ind8 m_priority;
	uint32_t m_pens[0xc00];

	uint16_t *cps1_base(int offset, int boundary);
	void cps1_get_video_base();
	int gfxrom_bank_mapper(int type, int code);
	void cps1_update_transmasks();
	void cps1_build_palette(const uint16_t *palette_base);
	void find_last_sprite();
	void cps1_render_sprites(mv::bitmap_ind16 &bitmap, const mv::rectangle &cliprect);
	void cps1_render_layer(mv::bitmap_ind16 &bitmap, const mv::rectangle &cliprect, int layer, int primask);
	void cps1_render_high_layer(mv::bitmap_ind16 &bitmap, const mv::rectangle &cliprect, int layer);
	void render_layers(mv::bitmap_ind16 &bitmap, const mv::rectangle &cliprect);
	void flip_screen_set(int state);
	mv::rectangle visarea() const { return mv::rectangle(VIS_X0, VIS_X1, VIS_Y0, VIS_Y1); }
};

} // namespace sf2

#endif
