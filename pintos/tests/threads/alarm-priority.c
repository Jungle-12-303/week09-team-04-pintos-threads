/* Checks that when the alarm clock wakes up threads, the
   higher-priority threads run first. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

static thread_func alarm_priority_thread;
static int64_t wake_time;
static struct semaphore wait_sema;

void
test_alarm_priority (void) 
{
  int i;
  
  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  //500틱 후 기상(모든 스레드 공통=>전역 변수)
  wake_time = timer_ticks () + 5 * TIMER_FREQ;
  sema_init (&wait_sema, 0);
  
  for (i = 0; i < 10; i++) 
    {
      /*priority 수식의 의도:
      i=0..9에 대해 5,6,7,8,9,0,1,2,3,4 순으로 순환값을 생성
      priority: 25, 24, 23,22,21,30,29...순으로 부여
       10개의 서로 다른 우선순위를 “섞어서” 만들기 위해 쓴 것
      */
      int priority = PRI_DEFAULT - (i + 5) % 10 - 1;
      char name[16];
      snprintf (name, sizeof name, "priority %d", priority);

      //스레드는 create되면서(alarm_priority_thread) wait_sema +1 => wait sema 최종 10
      thread_create (name, priority, alarm_priority_thread, NULL);
    }

  thread_set_priority (PRI_MIN);

  //10번 세마 다운
  for (i = 0; i < 10; i++)
    sema_down (&wait_sema);
}

static void
alarm_priority_thread (void *aux UNUSED) 
{
  /* Busy-wait until the current time changes. */
  int64_t start_time = timer_ticks ();
  while (timer_elapsed (start_time) == 0)
    continue;

  /* Now we know we're at the very beginning of a timer tick, so
     we can call timer_sleep() without worrying about races
     between checking the time and a timer interrupt. */
  timer_sleep (wake_time - timer_ticks ());

  /* Print a message on wake-up. */
  msg ("Thread %s woke up.", thread_name ());

  //스레드 1개 생성하며 세마 +1
  sema_up (&wait_sema);
}
