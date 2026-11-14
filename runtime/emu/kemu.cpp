// See kemu.h.  Each function mirrors the MAME 0.264 function named in its
// comment.
#include "kemu.h"

namespace kemu {

// --- attotime (src/emu/attotime.h/.cpp) ---

attotime operator+(const attotime &l, const attotime &r)
{
	if (l.sec >= ATTOTIME_MAX_SECONDS || r.sec >= ATTOTIME_MAX_SECONDS)
		return attotime::never();
	attotime res(l.sec + r.sec, l.atto + r.atto);
	if (res.atto >= ATTOSECONDS_PER_SECOND) {
		res.atto -= ATTOSECONDS_PER_SECOND;
		res.sec++;
	}
	if (res.sec >= ATTOTIME_MAX_SECONDS)
		return attotime::never();
	return res;
}

attotime operator-(const attotime &l, const attotime &r)
{
	if (l.sec >= ATTOTIME_MAX_SECONDS)
		return attotime::never();
	attotime res(l.sec - r.sec, l.atto - r.atto);
	if (res.atto < 0) {
		res.atto += ATTOSECONDS_PER_SECOND;
		res.sec--;
	}
	return res;
}

attotime &attotime::operator+=(const attotime &r) { return *this = *this + r; }
attotime &attotime::operator-=(const attotime &r) { return *this = *this - r; }

attotime attotime::from_ticks(uint64_t ticks, uint32_t frequency)
{
	if (frequency == 0)
		return never();
	attoseconds_t apt = HZ_TO_ATTOSECONDS_U(frequency);
	if (ticks < frequency)
		return attotime(0, attoseconds_t(ticks) * apt);
	uint32_t rem = uint32_t(ticks % frequency);
	return attotime(seconds_t(ticks / frequency), attoseconds_t(uint64_t(rem) * uint64_t(apt)));
}

uint64_t attotime::as_ticks(uint32_t frequency) const
{
	// (attotime(0, atto) * frequency).seconds + seconds * frequency
	unsigned __int128 t = (unsigned __int128)(uint64_t)atto * frequency;
	uint32_t fracticks = uint32_t(t / (unsigned __int128)ATTOSECONDS_PER_SECOND);
	return uint64_t(uint32_t(sec)) * frequency + fracticks;
}

attotime clocks_to_attotime(uint64_t clocks, uint32_t clock)
{
	if (clock == 0)
		return attotime::never();
	attoseconds_t apc = HZ_TO_ATTOSECONDS_U(clock);
	if (clocks < clock)
		return attotime(0, attoseconds_t(clocks) * apc);
	uint32_t rem = uint32_t(clocks % clock);
	return attotime(seconds_t(clocks / clock), attoseconds_t(uint64_t(rem) * uint64_t(apc)));
}

// --- emu_timer (src/emu/schedule.cpp) ---

void timer::adjust(attotime start_delay, int32_t prm, attotime per)
{
	if (sched->m_callback_timer == this)
		sched->m_callback_timer_modified = true;
	param = prm;
	enabled = true;
	if (start_delay.sec < 0)
		start_delay = attotime();
	start = sched->time();
	expire = start + start_delay;
	period = per;
	sched->timer_list_remove(*this);
	sched->timer_list_insert(*this);
	if (this == sched->first_timer())
		sched->abort_timeslice();
}

bool timer::enable(bool en)
{
	bool old = enabled;
	if (old != en) {
		enabled = en;
		sched->timer_list_remove(*this);
		sched->timer_list_insert(*this);
	}
	return old;
}

attotime timer::remaining() const
{
	attotime curtime = sched->time();
	if (curtime >= expire)
		return attotime();
	return expire - curtime;
}

// --- device_scheduler ---

scheduler::scheduler() {}

attotime scheduler::time() const
{
	if (m_executing_device != nullptr)
		return m_executing_device->local_time();
	return m_basetime;
}

timer *scheduler::timer_alloc(timer::callback_fn cb, void *ctx, int kind)
{
	timer *t = new timer();
	t->sched = this;
	t->cb = cb;
	t->ctx = ctx;
	t->kind = kind;
	t->param = 0;
	t->temporary = false;
	t->period = attotime::never();
	t->start = time();
	t->expire = attotime::never();
	t->enabled = false;
	timer_list_insert(*t);
	if (t == first_timer())
		abort_timeslice();
	return t;
}

void scheduler::synchronize(timer::callback_fn cb, void *ctx, int32_t param, int kind)
{
	timer *t;
	if (!m_temp_pool.empty()) {
		t = m_temp_pool.back();
		m_temp_pool.pop_back();
		*t = timer();
	} else {
		t = new timer();
	}
	t->sched = this;
	t->cb = cb;
	t->ctx = ctx;
	t->kind = kind;
	t->param = param;
	t->temporary = true;
	t->period = attotime::never();
	t->start = time();
	t->expire = t->start;
	t->enabled = !t->expire.is_never();
	timer_list_insert(*t);
	if (t == first_timer())
		abort_timeslice();
}

void scheduler::timer_list_insert(timer &t)
{
	if (!t.expire.is_never() && t.enabled) {
		t.in_active_list = true;
		timer *prevtimer = nullptr;
		for (timer *cur = m_timer_list; cur; prevtimer = cur, cur = cur->next) {
			if (cur->expire > t.expire) {
				t.prev = prevtimer;
				t.next = cur;
				if (prevtimer)
					prevtimer->next = &t;
				else
					m_timer_list = &t;
				cur->prev = &t;
				return;
			}
		}
		if (prevtimer)
			prevtimer->next = &t;
		else
			m_timer_list = &t;
		t.prev = prevtimer;
		t.next = nullptr;
	} else {
		t.in_active_list = false;
		if (m_inactive_timers)
			m_inactive_timers->prev = &t;
		t.next = m_inactive_timers;
		t.prev = nullptr;
		m_inactive_timers = &t;
	}
}

void scheduler::timer_list_remove(timer &t)
{
	if (t.prev)
		t.prev->next = t.next;
	else if (&t == m_timer_list)
		m_timer_list = t.next;
	else if (&t == m_inactive_timers)
		m_inactive_timers = t.next;
	if (t.next)
		t.next->prev = t.prev;
	t.next = t.prev = nullptr;
}

void scheduler::abort_timeslice()
{
	if (m_executing_device != nullptr)
		m_executing_device->abort_timeslice();
}

void scheduler::rebuild_execute_list()
{
	exec_device **active_tail = &m_execute_list;
	*active_tail = nullptr;
	exec_device *suspend_list = nullptr;
	exec_device **suspend_tail = &suspend_list;
	for (exec_device *d : m_devices) {
		d->nextexec = nullptr;
		if (d->suspend == 0) {
			*active_tail = d;
			active_tail = &d->nextexec;
		} else {
			*suspend_tail = d;
			suspend_tail = &d->nextexec;
		}
	}
	*active_tail = suspend_list;
}

void scheduler::apply_suspend_changes()
{
	uint32_t changed = 0;
	for (exec_device *d = m_execute_list; d != nullptr; d = d->nextexec) {
		changed |= d->suspend ^ d->nextsuspend;
		d->suspend = d->nextsuspend;
		d->nextsuspend &= ~SUSPEND_REASON_TIMESLICE;
		d->eatcycles = d->nexteatcycles;
	}
	if (changed != 0)
		rebuild_execute_list();
	else
		m_suspend_changes_pending = false;
}

void scheduler::timeslice()
{
	if (m_execute_list == nullptr)
		rebuild_execute_list();

	while (m_basetime < m_timer_list->expire) {
		attotime target(m_basetime + attotime(0, m_quantum));
		if (m_timer_list->expire < target)
			target = m_timer_list->expire;

		if (m_suspend_changes_pending)
			apply_suspend_changes();

		for (exec_device *exec = m_execute_list; exec != nullptr; exec = exec->nextexec) {
			if ((exec->suspend == 0 || exec->eatcycles) && target.sec >= exec->localtime.sec) {
				attoseconds_t delta = target.atto - exec->localtime.atto;
				if (delta < 0 && target.sec > exec->localtime.sec)
					delta += ATTOSECONDS_PER_SECOND;

				if (exec->attoseconds_per_cycle == 0) {
					exec->localtime = target;
				} else if (delta >= exec->attoseconds_per_cycle) {
					int32_t ran = exec->cycles_running = int32_t((uint64_t(delta) >> exec->divshift) / exec->divisor);
					if (exec->suspend == 0) {
						exec->cycles_stolen = 0;
						m_executing_device = exec;
						*exec->icountptr = exec->cycles_running;
						exec->run(exec->ctx);
						ran -= *exec->icountptr;
						ran -= exec->cycles_stolen;
					}
					exec->totalcycles += ran;
					attotime deltatime;
					if (uint32_t(ran) < exec->cycles_per_second)
						deltatime = attotime(0, exec->attoseconds_per_cycle * ran);
					else {
						uint32_t rem = uint32_t(uint32_t(ran) % exec->cycles_per_second);
						deltatime = attotime(seconds_t(uint32_t(ran) / exec->cycles_per_second), attoseconds_t(uint64_t(rem) * exec->attoseconds_per_cycle));
					}
					exec->localtime += deltatime;
					if (exec->localtime < target)
						target = exec->localtime < m_basetime ? m_basetime : exec->localtime;
				}
			}
		}
		m_executing_device = nullptr;
		m_basetime = target;
	}

	execute_timers();
}

void scheduler::execute_timers()
{
	while (m_timer_list->expire <= m_basetime) {
		timer &t = *m_timer_list;
		bool was_enabled = t.enabled;
		if (t.period.is_zero() || t.period.is_never())
			t.enabled = false;
		m_callback_timer_modified = false;
		m_callback_timer = &t;
		m_callback_timer_expire_time = t.expire;
		if (was_enabled && t.cb)
			t.cb(t.ctx, t.param);
		if (!m_callback_timer_modified) {
			if (!t.temporary) {
				t.start = t.expire;
				t.expire += t.period;
				timer_list_remove(t);
				timer_list_insert(t);
			} else {
				timer_list_remove(t);
				t.cb = nullptr;
				m_temp_pool.push_back(&t);
			}
		}
	}
	m_callback_timer = nullptr;
}

// --- device_execute_interface (src/emu/diexec.cpp) ---

void exec_device::configure(scheduler &s, const char *t, uint32_t clk)
{
	sched = &s;
	tag = t;
	clock = clk;
	cycles_per_second = clk;
	attoseconds_per_cycle = HZ_TO_ATTOSECONDS_U(clk);
	attoseconds_t attos = attoseconds_per_cycle;
	divshift = 0;
	while (attos >= (attoseconds_t(1) << 31)) {
		divshift++;
		attos >>= 1;
	}
	divisor = uint32_t(attos);
	suspend = SUSPEND_REASON_RESET;
	for (int i = 0; i < MAX_INPUT_LINES; i++)
		inputs[i].linenum = i;
	s.add_exec(this);
}

bool exec_device::executing() const { return sched->currently_executing() == this; }

attotime exec_device::local_time() const
{
	if (executing()) {
		int cycles = cycles_running - *icountptr;
		return localtime + cycles_to_attotime(uint64_t(int64_t(cycles)));
	}
	return localtime;
}

void exec_device::abort_timeslice()
{
	if (!executing())
		return;
	if (icountptr != nullptr) {
		int delta = *icountptr;
		cycles_stolen += delta;
		cycles_running -= delta;
		*icountptr -= delta;
	}
}

void exec_device::suspend_dev(uint32_t reason, bool eat)
{
	nextsuspend |= reason;
	nexteatcycles = eat;
	sched->suspend_resume_changed();
	abort_timeslice();
}

void exec_device::resume(uint32_t reason)
{
	nextsuspend &= ~reason;
	sched->suspend_resume_changed();
	abort_timeslice();
}

void exec_device::pre_reset()
{
	totalcycles = 0;
	if (disabled)
		suspend_dev(SUSPEND_REASON_DISABLE, true);
	else if (clock != 0)
		resume(SUSPEND_ANY_REASON);
}

void exec_device::post_reset()
{
	for (auto &in : inputs)
		in.curvector = in.stored_vector = 0;
}

static void empty_queue_cb(void *ctx, int32_t param)
{
	static_cast<exec_device *>(ctx)->empty_event_queue(param);
}

void exec_device::set_input_line(int line, int state)
{
	input &in = inputs[line];
	int event_index = in.qindex++;
	if (event_index >= 32) {
		in.qindex--;
		empty_event_queue(line);
		event_index = in.qindex++;
	}
	if (event_index < 32) {
		in.queue[event_index] = (state & 0xff) | (in.stored_vector << 8);
		if (event_index == 0)
			sched->synchronize(empty_queue_cb, this, line, 1);
	}
}

void exec_device::empty_event_queue(int line)
{
	input &in = inputs[line];
	for (int cur = 0; cur < in.qindex; cur++) {
		int32_t ev = in.queue[cur];
		in.curstate = uint8_t(ev & 0xff);
		in.curvector = ev >> 8;
		if (line == INPUT_LINE_RESET) {
			if (in.curstate == ASSERT_LINE)
				suspend_dev(SUSPEND_REASON_RESET, true);
			else if (suspended(SUSPEND_REASON_RESET)) {
				// device().reset(): interface_pre_reset, device_reset, interface_post_reset
				pre_reset();
				if (reset_device)
					reset_device(ctx);
				post_reset();
				resume(SUSPEND_REASON_RESET);
			}
		} else {
			switch (in.curstate) {
			case HOLD_LINE:
			case ASSERT_LINE:
				set_input(ctx, line, ASSERT_LINE);
				break;
			case CLEAR_LINE:
				set_input(ctx, line, CLEAR_LINE);
				break;
			}
			// signal_interrupt_trigger: no device waits on triggers on this board
		}
	}
	in.qindex = 0;
}

int exec_device::standard_irq_callback(int line)
{
	input &in = inputs[line];
	int vector = in.curvector;
	if (in.curstate == HOLD_LINE) {
		set_input(ctx, line, CLEAR_LINE);
		in.curstate = CLEAR_LINE;
	}
	return vector;
}

} // namespace kemu
