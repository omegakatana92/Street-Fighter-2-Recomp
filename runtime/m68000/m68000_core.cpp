// MAME 0.264 src/devices/cpu/m68000/m68000.cpp (BSD-3-Clause, Olivier
// Galibert): execution loop, reset, interrupt lines and decode table, with
// the address-space setup replaced by the board's bus.
#include "m68000.h"

void (*g_m68000_trace)(m68000_device *) = nullptr;

m68000_device::m68000_device()
{
	m_mmu = nullptr;
	m_handlers_f = s_handlers_df;
	m_handlers_p = s_handlers_dp;
	m_program.bus = &m_bus;
	m_opcodes.bus = &m_bus;
	m_cpu_space.bus = &m_bus;
	m_cpu_space.cpu_space = true;
}

void m68000_device::abort_access(u32 reason)
{
	m_post_run = reason;
	m_post_run_cycles = m_icount;
	m_icount = 0;
}

void m68000_device::do_post_run()
{
	m_icount = m_post_run_cycles;
	m_post_run_cycles = 0;
	switch(m_post_run) {
	case PR_BERR:
		m_inst_state = S_BUS_ERROR;
		m_inst_substate = 0;
		m_icount -= 10;
		break;
	}
	m_post_run = 0;
}

bool m68000_device::supervisor_mode() const noexcept
{
	return m_sr & SR_S;
}

u16 m68000_device::get_fc() const noexcept
{
	return (m_base_ssw | (m_sr & SR_S ? SSW_S : 0)) & 7;
}

void m68000_device::execute_run()
{
	m_icount -= m_count_before_instruction_step;
	if(m_icount < 0) {
		m_count_before_instruction_step = -m_icount;
		m_icount = 0;
	} else
		m_count_before_instruction_step = 0;

	for(;;) {
		if(m_icount > 0 && m_inst_substate)
			(this->*(m_handlers_p[m_inst_state]))();

		while(m_icount > 0) {
			if(m_inst_state >= S_first_instruction) {
				m_ipc = m_pc - 2;
				m_irdi = m_ird;
				if (g_m68000_trace)
					g_m68000_trace(this);
			}
			(this->*(m_handlers_f[m_inst_state]))();
		}

		if(m_post_run)
			do_post_run();
		else
			break;
	}

	if(m_icount < 0) {
		m_count_before_instruction_step = -m_icount;
		m_icount = 0;
	}
}

void m68000_device::start()
{
	init_decode_table();

	memset(m_da, 0, sizeof(m_da));
	m_ipc = 0;
	m_pc = 0;
	m_au = 0;
	m_at = 0;
	m_aob = 0;
	m_dt = 0;
	m_int_vector = 0;
	m_sp = 0;
	m_bcount = 0;
	m_count_before_instruction_step = 0;
	m_t = 0;
	m_movems = 0;
	m_isr = 0;
	m_sr = 0;
	m_new_sr = 0;
	m_dbin = 0;
	m_dbout = 0;
	m_edb = 0;
	m_irc = 0;
	m_ir = 0;
	m_ird = 0;
	m_ftu = 0;
	m_aluo = 0;
	m_alue = 0;
	m_alub = 0;
	m_movemr = 0;
	m_irdi = 0;
	m_base_ssw = 0;
	m_ssw = 0;
	m_dcr = 0;

	m_virq_state = 0;
	m_nmi_pending = 0;
	m_int_level = 0;
	m_int_next_state = 0;
	m_nmi_uses_generic = false;
	m_last_vpa_time = 0;

	m_inst_state = 0;
	m_inst_substate = 0;
	m_next_state = 0;
	m_post_run = 0;
	m_post_run_cycles = 0;
	m_icount = 0;
}

void m68000_device::device_reset()
{
	m_inst_state = S_RESET;
	m_inst_substate = 0;
	m_count_before_instruction_step = 0;
	m_post_run = 0;
	m_post_run_cycles = 0;
	m_last_vpa_time = 0;
	update_user_super();
}

void m68000_device::update_user_super()
{
	// program and user program are the same space on this board
	m_sp = (m_sr & SR_S) ? 16 : 15;
}

void m68000_device::execute_set_input(int inputnum, int state)
{
	if(inputnum == INPUT_LINE_NMI) {
		inputnum = 7;
		m_nmi_uses_generic = true;
	} else if(inputnum == 7)
		m_nmi_uses_generic = false;

	if(inputnum > (m_interrupt_mixer ? 7 : 3))
		return;

	u32 old_level = m_int_level;
	u32 vstate = m_virq_state;
	u32 blevel;

	if(state == kemu::ASSERT_LINE)
		vstate |= 1 << inputnum;
	else
		vstate &= ~(1 << inputnum);
	m_virq_state = vstate;

	if(m_interrupt_mixer) {
		for(blevel = 7; blevel > 0; blevel--)
			if(vstate & (1 << blevel))
				break;
	} else
		blevel = vstate;

	m_int_level = blevel;

	if(old_level != 7 && m_int_level == 7)
		m_nmi_pending = true;

	update_interrupt();
}

void m68000_device::update_interrupt()
{
	if(m_nmi_pending)
		m_int_next_state = (7 << 24) | S_INTERRUPT;
	else if(m_int_level > ((m_sr >> 8) & 7))
		m_int_next_state = (m_int_level << 24) | S_INTERRUPT;
	else
		m_int_next_state = 0;
}

void m68000_device::init_decode_table()
{
	m_decode_table.resize(0x10000, S_ILLEGAL);

	for(int i=0; s_packed_decode_table[i].mask; i++) {
		u16 value = s_packed_decode_table[i].value;
		u16 mask = s_packed_decode_table[i].mask;
		u16 state = s_packed_decode_table[i].state;

		u16 cvalue = 0;
		do {
			if(m_decode_table[value | cvalue] == S_ILLEGAL)
				m_decode_table[value | cvalue] = state;
			cvalue = ((cvalue | mask) + 1) & ~mask;
		} while(cvalue);
	}
}

void m68000_device::set_ftu_const()
{
	switch(m_ird >> 12) {
	case 0x4: m_ftu = 0x80; break;
	case 0x5: case 0xe: m_ftu = (m_ird >> 9) & 7; if(!m_ftu) m_ftu = 8; break;
	case 0x6: case 0x7: m_ftu = s8(m_ird); break;
	case 0x8: case 0xc: m_ftu = 0xf; break;
	default: m_ftu = 0; break;
	}
}

void m68000_device::start_interrupt_vector_lookup()
{
	int level = m_next_state >> 24;
	if(!m_disable_interrupt_callback) {
		if(m_interrupt_mixer)
			standard_irq_callback(level == 7 && m_nmi_uses_generic ? INPUT_LINE_NMI : level, m_pc);
		else {
			for(int i=0; i<3; i++)
				if(level & (1<<i))
					standard_irq_callback(i, m_pc);
		}
	}

	if(level == 7) {
		m_nmi_pending = false;
		update_interrupt();
	}
}

void m68000_device::end_interrupt_vector_lookup()
{
	m_int_vector = (m_edb & 0xff) << 2;
	m_int_next_state = 0;
}

#define M68K_STATE_FIELDS(X) \
	X(m_da) X(m_ipc) X(m_pc) X(m_au) X(m_at) X(m_aob) X(m_dt) X(m_int_vector) X(m_sp) \
	X(m_icount) X(m_bcount) X(m_count_before_instruction_step) X(m_t) X(m_movems) X(m_isr) X(m_sr) \
	X(m_new_sr) X(m_dbin) X(m_dbout) X(m_edb) X(m_irc) X(m_ir) X(m_ird) X(m_ftu) X(m_aluo) X(m_alue) \
	X(m_alub) X(m_movemr) X(m_irdi) X(m_base_ssw) X(m_ssw) X(m_dcr) X(m_virq_state) X(m_nmi_pending) \
	X(m_int_level) X(m_int_next_state) X(m_nmi_uses_generic) X(m_last_vpa_time) X(m_inst_state) \
	X(m_inst_substate) X(m_next_state) X(m_post_run) X(m_post_run_cycles)

void m68000_device::save(state_writer &w) const
{
#define X(f) w.put(f);
	M68K_STATE_FIELDS(X)
#undef X
}

void m68000_device::load(state_reader &r)
{
#define X(f) r.get(f);
	M68K_STATE_FIELDS(X)
#undef X
	update_user_super();
}
