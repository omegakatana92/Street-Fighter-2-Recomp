// Minimal machine framework (MAME 0.264 scheduler subset); first written for
// the Kaiser Knuckle port and shared with this one.
//
// The CPU interleave of the CPS-1 board (68000 main CPU, Z80 sound CPU, the
// sound-latch synchronize() calls and the YM2151 timers) decides what each CPU
// sees of the other, so this file reproduces MAME 0.264's scheduler: attotime arithmetic
// (src/emu/attotime.h), the timer list and timeslice loop
// (src/emu/schedule.cpp) and the execute interface's suspend, cycle-eating and
// synchronised input-line logic (src/emu/diexec.cpp).  Behaviour follows the
// MAME code; it is reimplemented here with only what this board uses.
#ifndef SF2_EMU_KEMU_H
#define SF2_EMU_KEMU_H

#include <cstdint>
#include <cstddef>
#include <vector>

namespace kemu {

typedef int64_t attoseconds_t;
typedef int32_t seconds_t;

constexpr attoseconds_t ATTOSECONDS_PER_SECOND = 1000000000000000000LL;
constexpr attoseconds_t ATTOSECONDS_PER_MICROSECOND = 1000000000000LL;
constexpr seconds_t ATTOTIME_MAX_SECONDS = 1000000000;

inline attoseconds_t HZ_TO_ATTOSECONDS(double hz) { return attoseconds_t(double(ATTOSECONDS_PER_SECOND) / hz); }
inline attoseconds_t HZ_TO_ATTOSECONDS_U(uint32_t hz) { return ATTOSECONDS_PER_SECOND / hz; }

struct attotime {
	seconds_t sec = 0;
	attoseconds_t atto = 0;
	constexpr attotime() = default;
	constexpr attotime(seconds_t s, attoseconds_t a) : sec(s), atto(a) {}
	static constexpr attotime never() { return attotime(ATTOTIME_MAX_SECONDS, 0); }
	bool is_never() const { return sec >= ATTOTIME_MAX_SECONDS; }
	bool is_zero() const { return sec == 0 && atto == 0; }
	attoseconds_t as_attoseconds() const {
		return sec == 0 ? atto : sec == -1 ? atto - ATTOSECONDS_PER_SECOND : sec > 0 ? ATTOSECONDS_PER_SECOND : -ATTOSECONDS_PER_SECOND;
	}
	static attotime from_hz_u(uint32_t f) { return f > 1 ? attotime(0, HZ_TO_ATTOSECONDS_U(f)) : f == 1 ? attotime(1, 0) : never(); }
	// attotime::from_ticks(ticks, frequency)
	static attotime from_ticks(uint64_t ticks, uint32_t frequency);
	uint64_t as_ticks(uint32_t frequency) const;
	attotime &operator+=(const attotime &r);
	attotime &operator-=(const attotime &r);
};

attotime operator+(const attotime &l, const attotime &r);
attotime operator-(const attotime &l, const attotime &r);
inline bool operator==(const attotime &l, const attotime &r) { return l.sec == r.sec && l.atto == r.atto; }
inline bool operator!=(const attotime &l, const attotime &r) { return !(l == r); }
inline bool operator<(const attotime &l, const attotime &r) { return l.sec < r.sec || (l.sec == r.sec && l.atto < r.atto); }
inline bool operator<=(const attotime &l, const attotime &r) { return l.sec < r.sec || (l.sec == r.sec && l.atto <= r.atto); }
inline bool operator>(const attotime &l, const attotime &r) { return r < l; }
inline bool operator>=(const attotime &l, const attotime &r) { return r <= l; }

// device_t::clocks_to_attotime for an integer clock
attotime clocks_to_attotime(uint64_t clocks, uint32_t clock);

class scheduler;
struct exec_device;

// emu_timer
struct timer {
	typedef void (*callback_fn)(void *ctx, int32_t param);
	scheduler *sched = nullptr;
	timer *next = nullptr, *prev = nullptr;
	callback_fn cb = nullptr;
	void *ctx = nullptr;
	int32_t param = 0;
	bool enabled = false;
	bool temporary = false;
	bool in_active_list = false;
	attotime period = attotime::never();
	attotime start, expire = attotime::never();
	int kind = 0;           // identifies the callback for save states
	void adjust(attotime start_delay, int32_t param = 0, attotime period = attotime::never());
	bool enable(bool en);
	attotime remaining() const;
};

enum { CLEAR_LINE = 0, ASSERT_LINE = 1, HOLD_LINE = 2 };
enum { INPUT_LINE_RESET = 32 + 1, MAX_INPUT_LINES = 32 + 3 };
enum {
	SUSPEND_REASON_HALT = 0x0001, SUSPEND_REASON_RESET = 0x0002, SUSPEND_REASON_SPIN = 0x0004,
	SUSPEND_REASON_TRIGGER = 0x0008, SUSPEND_REASON_DISABLE = 0x0010, SUSPEND_REASON_TIMESLICE = 0x0020,
	SUSPEND_REASON_CLOCK = 0x0040, SUSPEND_ANY_REASON = ~0u
};

// device_execute_interface (only what the board uses)
struct exec_device {
	struct input {
		int linenum = 0;
		int32_t stored_vector = 0, curvector = 0;
		uint8_t curstate = CLEAR_LINE;
		int qindex = 0;
		int32_t queue[32] = {};
	};
	scheduler *sched = nullptr;
	exec_device *nextexec = nullptr;
	const char *tag = "";
	uint32_t clock = 0;
	uint32_t suspend = SUSPEND_REASON_RESET, nextsuspend = 0;
	uint8_t eatcycles = 0, nexteatcycles = 0;
	int *icountptr = nullptr;
	int32_t cycles_running = 0, cycles_stolen = 0;
	uint64_t totalcycles = 0;
	attotime localtime;
	uint32_t divisor = 0;
	uint8_t divshift = 0;
	uint32_t cycles_per_second = 0;
	attoseconds_t attoseconds_per_cycle = 0;
	bool disabled = false;
	input inputs[MAX_INPUT_LINES];

	// hooks into the CPU core
	void (*run)(void *ctx) = nullptr;
	void (*set_input)(void *ctx, int line, int state) = nullptr;   // execute_set_input
	void (*reset_device)(void *ctx) = nullptr;                      // device().reset() core part
	void *ctx = nullptr;

	void configure(scheduler &s, const char *tag, uint32_t clock);
	bool executing() const;
	attotime local_time() const;
	void abort_timeslice();
	void suspend_dev(uint32_t reason, bool eat);
	void resume(uint32_t reason);
	bool suspended(uint32_t reason = SUSPEND_ANY_REASON) const { return (nextsuspend & reason) != 0; }
	void pre_reset();   // interface_pre_reset
	void post_reset();  // interface_post_reset (input vectors)
	void set_input_line(int line, int state);
	int standard_irq_callback(int line);
	void empty_event_queue(int line);
	attotime cycles_to_attotime(uint64_t cycles) const { return clocks_to_attotime(cycles, clock); }
};

class scheduler {
public:
	scheduler();
	attotime time() const;
	void timeslice();
	timer *timer_alloc(timer::callback_fn cb, void *ctx, int kind);
	void synchronize(timer::callback_fn cb, void *ctx, int32_t param, int kind);
	void timer_list_insert(timer &t);
	void timer_list_remove(timer &t);
	timer *first_timer() const { return m_timer_list; }
	void abort_timeslice();
	void suspend_resume_changed() { m_suspend_changes_pending = true; }
	exec_device *currently_executing() const { return m_executing_device; }
	void add_exec(exec_device *d) { m_devices.push_back(d); }
	void rebuild_execute_list();
	void set_quantum(attoseconds_t q) { m_quantum = q; }

	attotime m_basetime;
	timer *m_timer_list = nullptr;
	timer *m_inactive_timers = nullptr;
	timer *m_callback_timer = nullptr;
	bool m_callback_timer_modified = false;
	attotime m_callback_timer_expire_time;
	bool m_suspend_changes_pending = true;
	exec_device *m_execute_list = nullptr;
	exec_device *m_executing_device = nullptr;
	std::vector<exec_device *> m_devices;    // enumeration order
	attoseconds_t m_quantum = 0;             // quantum list's single entry (actual)
	std::vector<timer *> m_temp_pool;        // recycled temporary timers

private:
	void apply_suspend_changes();
	void execute_timers();
};

} // namespace kemu

#endif
