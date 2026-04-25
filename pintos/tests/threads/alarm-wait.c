/* Creates N threads, each of which sleeps a different, fixed
   duration, M times. Records the wake-up order and verifies
   that it is valid.
   N개의 스레드를 생성해 각각 고정된 서로 다른 시간 동안 M번 잠들게 하고,
   깨어나는 순서를 기록해 유효성을 검사한다. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

static void test_sleep (int thread_cnt, int iterations);

void
test_alarm_single (void) 
{
  test_sleep (5, 1);
}

void
test_alarm_multiple (void) 
{
  test_sleep (5, 7);
}

/* Information about the test.
   테스트에 대한 정보. */
struct sleep_test 
  {
    int64_t start;              /* Current time at start of test.
                                   테스트 시작 시점의 현재 시간. */
    int iterations;             /* Number of iterations per thread.
                                   스레드당 반복 횟수. */

    /* Output. */
    struct lock output_lock;    /* Lock protecting output buffer.
                                   출력 버퍼를 보호하는 락. */
    int *output_pos;            /* Current position in output buffer.
                                   출력 버퍼의 현재 위치. */
  };

/* Information about an individual thread in the test.
   테스트 내 개별 스레드의 정보. */
struct sleep_thread 
  {
    struct sleep_test *test;     /* Info shared between all threads.
                                   모든 스레드가 공유하는 정보. */
    int id;                     /* Sleeper ID. 잠자는 스레드 ID. */
    int duration;               /* Number of ticks to sleep.
                                   잠자는데 필요한 tick 수. */
    int iterations;             /* Iterations counted so far.
                                   현재까지 누적된 반복 횟수. */
  };

static void sleeper (void *);

/* Runs THREAD_CNT threads thread sleep ITERATIONS times each.
   THREAD_CNT개의 스레드를 각각 ITERATIONS번 sleep으로 실행한다. */
static void
test_sleep (int thread_cnt, int iterations) 
{
  struct sleep_test test;
  struct sleep_thread *threads;
  int *output, *op;
  int product;
  int i;

  /* This test does not work with the MLFQS.
     이 테스트는 MLFQS 모드에서는 동작하지 않습니다. */
  ASSERT (!thread_mlfqs);

  msg ("Creating %d threads to sleep %d times each.", thread_cnt, iterations);
  msg ("Thread 0 sleeps 10 ticks each time,");
  msg ("thread 1 sleeps 20 ticks each time, and so on.");
  msg ("If successful, product of iteration count and");
  msg ("sleep duration will appear in nondescending order.");

  /* Allocate memory.
     메모리를 할당한다. */
  threads = malloc (sizeof *threads * thread_cnt);
  output = malloc (sizeof *output * iterations * thread_cnt * 2);
  if (threads == NULL || output == NULL)
    PANIC ("couldn't allocate memory for test");

  /* Initialize test.
     테스트를 초기화한다. */
  test.start = timer_ticks () + 100;
  test.iterations = iterations;
  lock_init (&test.output_lock);
  test.output_pos = output;

  /* Start threads.
     스레드를 생성해 시작한다. */
  ASSERT (output != NULL);
  for (i = 0; i < thread_cnt; i++)
    {
      struct sleep_thread *t = threads + i;
      char name[16];
      
      t->test = &test;
      t->id = i;
      t->duration = (i + 1) * 10;
      t->iterations = 0;

      snprintf (name, sizeof name, "thread %d", i);

      thread_create (name, PRI_DEFAULT, sleeper, t);
     
    }
  
  /* Wait long enough for all the threads to finish.
     모든 스레드가 완료될 때까지 충분히 기다린다. */
  timer_sleep (100 + thread_cnt * iterations * 10 + 100);

  /* Acquire the output lock in case some rogue thread is still
     running.
     혹시 예상치 못한 스레드가 아직 실행 중일 수 있으므로
     출력 락을 획득한다. */
  lock_acquire (&test.output_lock);

  /* Print completion order.
     완료(기록) 순서를 출력한다. */
  product = 0;
  for (op = output; op < test.output_pos; op++) 
    {
      struct sleep_thread *t;
      int new_prod;

      ASSERT (*op >= 0 && *op < thread_cnt);
      t = threads + *op;

      new_prod = ++t->iterations * t->duration;
        
      msg ("thread %d: duration=%d, iteration=%d, product=%d",
           t->id, t->duration, t->iterations, new_prod);
      
      if (new_prod >= product)
        product = new_prod;
      else
        fail ("thread %d woke up out of order (%d > %d)!",
              t->id, product, new_prod);
    }

  /* Verify that we had the proper number of wakeups.
     깨어난 횟수가 기대한 횟수와 일치하는지 검증한다. */
  for (i = 0; i < thread_cnt; i++)
    if (threads[i].iterations != iterations)
      fail ("thread %d woke up %d times instead of %d",
            i, threads[i].iterations, iterations);
  
  lock_release (&test.output_lock);
  free (output);
  free (threads);
}

/* Sleeper thread.
   잠자기 동작을 수행하는 스레드. */
static void
sleeper (void *t_) 
{
  struct sleep_thread *t = t_;
  struct sleep_test *test = t->test;
  int i;

  for (i = 1; i <= test->iterations; i++) 
    {
      int64_t sleep_until = test->start + i * t->duration;
      timer_sleep (sleep_until - timer_ticks ());
      lock_acquire (&test->output_lock);
      *test->output_pos++ = t->id;
      lock_release (&test->output_lock);
    }
}
