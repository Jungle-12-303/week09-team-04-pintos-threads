#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* See [8254] for hardware details of the 8254 timer chip.
   8254 타이머 칩의 하드웨어 동작은 [8254] 문서를 참고한다. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted.
   운영체제가 부팅된 이후 경과한 타이머 tick 수. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate().
   타이머 tick당 수행할 수 있는 반복 횟수(루프 수).
   timer_calibrate()에서 계산되어 초기화된다. */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);

/* Sets up the 8254 Programmable Interval Timer (PIT) to
   interrupt PIT_FREQ times per second, and registers the
   corresponding interrupt.
   8254 PIT를 초당 PIT_FREQ번 동작하도록 설정하고,
   해당 인터럽트를 등록한다. */
void
timer_init (void) {
	/* 8254 input frequency divided by TIMER_FREQ, rounded to
	   nearest.
	   8254 입력 주파수를 TIMER_FREQ로 나눠 반올림한 값을 계산한다. */
	uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ;

	outb (0x43, 0x34);    /* CW: counter 0, LSB then MSB, mode 2, binary.
	                         제어워드: 카운터 0, LSB 다음 MSB, 모드 2, 바이너리 모드. */
	outb (0x40, count & 0xff);
	outb (0x40, count >> 8);

	intr_register_ext (0x20, timer_interrupt, "8254 Timer");
}

/* Calibrates loops_per_tick, used to implement brief delays.
   짧은 지연 처리를 위해 loops_per_tick 값을 보정한다. */
void
timer_calibrate (void) {
	unsigned high_bit, test_bit;

	ASSERT (intr_get_level () == INTR_ON);
	printf ("Calibrating timer...  ");

	/* Approximate loops_per_tick as the largest power-of-two
	   still less than one timer tick.
	   한 타이머 tick보다 작은 최대 2의 거듭제곱 값으로
	   loops_per_tick을 근사한다. */
	loops_per_tick = 1u << 10;
	while (!too_many_loops (loops_per_tick << 1)) {
		loops_per_tick <<= 1;
		ASSERT (loops_per_tick != 0);
	}

	/* Refine the next 8 bits of loops_per_tick.
	   loops_per_tick의 하위 8비트(나머지 정밀도)를 보정한다. */
	high_bit = loops_per_tick;
	for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
		if (!too_many_loops (high_bit | test_bit))
			loops_per_tick |= test_bit;

	printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted.
   운영체제가 부팅된 후 경과한 타이머 tick 수를 반환한다. */
int64_t
timer_ticks (void) {
	enum intr_level old_level = intr_disable ();
	int64_t t = ticks;
	intr_set_level (old_level);
	barrier ();
	return t;
}

/* Returns the number of timer ticks elapsed since THEN, which
   should be a value once returned by timer_ticks().
   THEN 시점(일반적으로 timer_ticks() 반환값) 이후 경과한
   tick 수를 반환한다. */
int64_t
timer_elapsed (int64_t then) {
	return timer_ticks () - then;
}

/* Suspends execution for approximately TICKS timer ticks.
   대략 TICKS만큼의 타이머 tick 동안 실행을 일시 중단한다.
   (현재 구현에서는 busy wait + yield 기반 동작.) */
void
timer_sleep (int64_t ticks) {

	//ticks는 양수여야만 한다
	// ASSERT(ticks > 0);
	if(ticks <= 0) return;

	// //스레드에 잠든 시각에 대한 틱(sleep_tick) 설정
	// thread_current()->sleep_tick = timer_ticks(); 
	
	//깨어날 ticks(wake_tick) 설정
	
	int64_t start = timer_ticks ();
	
	//가정: 인터럽트 on이다
	ASSERT (intr_get_level () == INTR_ON);
	
	//thread_block하기 전에 인터럽트를 꺼야 한다
	enum intr_level old_level = intr_disable ();

	thread_add_to_sleeping_list(start+ticks);

	//thread_block한 후에 인터럽트를 켜야 한다.
	intr_set_level (old_level);

}

/* Suspends execution for approximately MS milliseconds.
   약 MS 밀리초 동안 실행을 멈춘다. */
void
timer_msleep (int64_t ms) {
	real_time_sleep (ms, 1000);
}

/* Suspends execution for approximately US microseconds.
   약 US 마이크로초 동안 실행을 멈춘다. */
void
timer_usleep (int64_t us) {
	real_time_sleep (us, 1000 * 1000);
}

/* Suspends execution for approximately NS nanoseconds.
   약 NS 나노초 동안 실행을 멈춘다. */
void
timer_nsleep (int64_t ns) {
	real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics.
   타이머 통계값을 출력한다. */
void
timer_print_stats (void) {
	printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}


/* Timer interrupt handler.
   타이머 인터럽트 처리 함수. */
static void
timer_interrupt (struct intr_frame *args UNUSED) {
	ticks++;
	thread_tick ();

	thread_wakeup(ticks);

}

/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false.
   LOOPS만큼 반복했을 때 1개 tick 이상 소요되면 true,
   아니면 false를 반환한다. */
static bool
too_many_loops (unsigned loops) {
	/* Wait for a timer tick.
	   타이머 tick이 바뀔 때까지 대기한다. */
	int64_t start = ticks;
	while (ticks == start)
		barrier ();

	/* Run LOOPS loops.
	   LOOPS 횟수만큼 짧은 루프를 수행한다. */
	start = ticks;
	busy_wait (loops);

	/* If the tick count changed, we iterated too long.
	   루프 수행 중 tick이 바뀌면, 수행 시간이 너무 길었다는 뜻이다. */
	barrier ();
	return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.
   간단한 루프를 LOOPS번 돌려 짧은 지연을 구현한다.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict.
   코드 정렬에 따라 실행 타이밍이 달라질 수 있어
   NO_INLINE으로 강제한다. 다른 위치에서 인라인 방식이 달라지면
   시간 측정 결과가 예측하기 어려워질 수 있기 때문이다. */
static void NO_INLINE
busy_wait (int64_t loops) {
	while (loops-- > 0)
		barrier ();
}

/* Sleep for approximately NUM/DENOM seconds.
   대략 NUM/DENOM초 동안 잠을 잔다(정지/지연). */
static void
real_time_sleep (int64_t num, int32_t denom) {
	/* Convert NUM/DENOM seconds into timer ticks, rounding down.
	   NUM/DENOM 초를 타이머 tick 단위로 내림 변환한다.

	   (NUM / DENOM) s
	   ---------------------- = NUM * TIMER_FREQ / DENOM ticks.
	   1 s / TIMER_FREQ ticks
	   */
	int64_t ticks = num * TIMER_FREQ / denom;

	ASSERT (intr_get_level () == INTR_ON);
	if (ticks > 0) {
		/* We're waiting for at least one full timer tick.  Use
		   timer_sleep() because it will yield the CPU to other
		   processes.
		   최소 1개 tick 이상 기다려야 하므로, 다른 스레드에 CPU를
		   양보할 수 있는 timer_sleep()를 사용한다. */
		timer_sleep (ticks);
	} else {
		/* Otherwise, use a busy-wait loop for more accurate
		   sub-tick timing.  We scale the numerator and denominator
		   down by 1000 to avoid the possibility of overflow.
		   1 tick보다 짧은 시간은 busy-wait 루프로 더 정확히 맞춘다.
		   오버플로우를 방지하기 위해 분자/분모를 1000으로 먼저 줄인다. */
		ASSERT (denom % 1000 == 0);
		busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
	}
}
       