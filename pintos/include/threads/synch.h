#ifndef THREADS_SYNCH_H
#define THREADS_SYNCH_H

#include <list.h>
#include <stdbool.h>

/* A counting semaphore. */
/* 카운팅 세마포어: value가 0이면 대기하고, 양수이면 획득할 수 있다. */
struct semaphore {
	unsigned value;             /* Current value. */
	                            /* 현재 세마포어 값. */
	struct list waiters;        /* List of waiting threads. */
	                            /* 세마포어를 기다리는 스레드 목록. */
};

void sema_init (struct semaphore *, unsigned value);
void sema_down (struct semaphore *);
bool sema_try_down (struct semaphore *);
void sema_up (struct semaphore *);
void sema_self_test (void);

/* Lock. */
/* 한 번에 하나의 스레드만 소유할 수 있는 이진 세마포어 기반 락. */
struct lock {
	struct thread *holder;      /* Thread holding lock (for debugging). */
	                            /* 현재 락을 보유한 스레드. */
	struct semaphore semaphore; /* Binary semaphore controlling access. */
	                            /* 락 획득을 제어하는 이진 세마포어. */
};

void lock_init (struct lock *);
void lock_acquire (struct lock *);
bool lock_try_acquire (struct lock *);
void lock_release (struct lock *);
bool lock_held_by_current_thread (const struct lock *);

/* Condition variable. */
/* 특정 조건이 만족될 때까지 락과 함께 기다리는 조건 변수. */
struct condition {
	struct list waiters;        /* List of waiting threads. */
	                            /* 조건 신호를 기다리는 스레드 목록. */
};

void cond_init (struct condition *);
void cond_wait (struct condition *, struct lock *);
void cond_signal (struct condition *, struct lock *);
void cond_broadcast (struct condition *, struct lock *);

/* Optimization barrier.
 * 최적화 배리어: 컴파일러가 이 지점을 넘어 명령을 재배치하지 못하게 한다.
 *
 * The compiler will not reorder operations across an
 * optimization barrier.  See "Optimization Barriers" in the
 * reference guide for more information.*/
#define barrier() asm volatile ("" : : : "memory")

#endif /* threads/synch.h */
