#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. 
   struct thread의 `magic` 멤버를 위한 랜덤 값입니다.
   스택 오버플로우를 감지하기 위해 사용합니다. 자세한 내용은 thread.h 상단
  의 큰 주석을 참고하세요. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
/* 기본 스레드용 랜덤 값입니다.
   이 값은 수정하면 안 됩니다. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. 
   THREAD_READY 상태(실행 가능하지만 실제로는 실행 중이 아닌) 스레드들의 목록입니다. */
static struct list ready_list;

//대기해야 하는 스레드들의 목록
static struct list sleep_list;


/* Idle thread. */
/* 아무 작업이 없는 동안 실행되는 유휴 스레드입니다. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
/* 초기 스레드. init.c의 main()을 실행하는 스레드입니다. */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
/* allocate_tid()에서 사용하는 락입니다. */
static struct lock tid_lock;

/* Thread destruction requests */
/* 스레드 소멸 요청 목록입니다. */
static struct list destruction_req;

/* Statistics. */
/* 통계값들입니다. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
							 /* idle 상태로 보낸 tick 수입니다. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
							 /* 커널 스레드에서 보낸 tick 수입니다. */
static long long user_ticks;    /* # of timer ticks in user programs. */
							 /* 사용자 프로그램에서 보낸 tick 수입니다. */

/* Scheduling. */
/* 스케줄링 관련 변수입니다. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
							 /* 각 스레드에 할당할 타이머 tick 수입니다. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */
							 /* 마지막 양보 후 경과한 tick 수입니다. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". 
   false(기본값)이면 라운드 로빈 스케줄러를 사용합니다.
   true이면 다단계 피드백 큐(MLFQS) 스케줄러를 사용합니다.
   커널 부팅 옵션 "-o mlfqs"로 제어됩니다. */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule (void);
static tid_t allocate_tid (void);
//스레드 elem을 priority에 대해서 오름차순으로 정렬 도움
static bool priority_greater_comparator(const struct list_elem *a,
		   const struct list_elem *b,
		   void *aux);

/* Returns true if T appears to point to a valid thread. 
 T가 유효한 스레드를 가리키는지 검사합니다. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. 
 * 현재 실행 중인 스레드를 반환합니다.
 * CPU의 스택 포인터 `rsp'를 읽고, 페이지 경계 시작으로 내립니다.
 * `struct thread`는 페이지의 시작에 항상 위치하고 스택 포인터는 중간 어딘가에 있으므로
 * 이를 이용해 현재 스레드를 찾아냅니다. */
#define running_thread() ((struct thread *) (pg_round_down (rrsp ())))

//슬립 리스트
struct list* get_sleepList() {
	return &sleep_list;
}


// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
// thread_start를 위한 글로벌 디스크립터 테이블(GDT)입니다.
// thread_init 실행 이후 GDT를 다시 구성하기 전에, 우선 임시 GDT를 준비해야 합니다.
static uint64_t gdt[3] = { 0, 0x00af9a000000ffff, 0x00cf92000000ffff };

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. 
   현재 실행 중인 코드를 스레드로 변환해 스레딩 시스템을 초기화합니다.
   일반적으로는 이런 방식이 동작하지 않지만, loader.S가 스택 하단을 페이지 경계에 배치했기 때문에
   이 환경에서만 가능한 방법입니다.

   또한 실행 큐와 tid용 락도 여기에서 초기화합니다.

   이 함수를 호출한 뒤 thread_create()로 스레드를 만들기 전에는
   페이지 할당기를 먼저 초기화해야 합니다.

   thread_current()는 이 함수가 끝나기 전까지 호출하면 안 됩니다. */
void
thread_init (void) {
	ASSERT (intr_get_level () == INTR_OFF);

	/* Reload the temporal gdt for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). 
	 * 커널용 임시 GDT를 다시 로드합니다.
	 * 이 GDT에는 사용자 컨텍스트 정보가 들어있지 않습니다.
	 * 커널은 gdt_init()에서 사용자 컨텍스트를 포함한 GDT를 다시 구성합니다. */
	struct desc_ptr gdt_ds = {
		.size = sizeof (gdt) - 1,
		.address = (uint64_t) gdt
	};
	lgdt (&gdt_ds);

	/* Init the globla thread context */
	/* 전역 스레드 문맥을 초기화합니다. */
	lock_init (&tid_lock);
	list_init (&ready_list);
	list_init (&sleep_list);
	list_init (&destruction_req);

	/* Set up a thread structure for the running thread. */
	/* 현재 실행 중인 스레드의 스레드 구조체를 설정합니다. */
	initial_thread = running_thread ();
	init_thread (initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. 
   인터럽트를 활성화해 선점형 스레드 스케줄링을 시작합니다.
   동시에 유휴(idle) 스레드를 생성합니다. */
void
thread_start (void) {
	/* Create the idle thread. */
	/* 유휴 스레드를 생성합니다. */
	struct semaphore idle_started;
	sema_init (&idle_started, 0);
	thread_create ("idle", PRI_MIN, idle, &idle_started);

	/* Start preemptive thread scheduling. */
	/* 선점형 스케줄링을 시작합니다. */
	intr_enable ();

	/* Wait for the idle thread to initialize idle_thread. */
	/* 유휴 스레드가 idle_thread를 초기화할 때까지 대기합니다. */
	sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. 
   timer_interrupt 핸들러에서 매 tick마다 호출됩니다.
   따라서 외부 인터럽트 컨텍스트에서 실행됩니다. */
void
thread_tick (void) {
	struct thread *t = thread_current ();

	/* Update statistics. */
	/* 통계를 갱신합니다. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption. */
	/* 선점 정책을 적용합니다. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return ();
}

/* Prints thread statistics. 
   스레드 통계를 출력합니다. */
void
thread_print_stats (void) {
	printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
			idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. 
    새 커널 스레드를 생성합니다.
   NAME은 이름, PRIORITY는 초기 우선순위, FUNCTION은 새 스레드가 시작할 때 실행할 함수의 포인터, AUX는 인자입니다.
   생성한 스레드는 ready 큐에 들어가고, 성공 시 TID를 반환하며 실패 시 TID_ERROR를 반환합니다.

   thread_start()가 이미 호출된 경우, 새 스레드는 thread_create()가 반환되기 전에
   스케줄될 수 있고, 심지어 반환 전에 종료될 수도 있습니다.
   반대로 기존 스레드는 새 스레드가 스케줄되기 전까지 임의의 시간 동안 실행될 수 있습니다.
   실행 순서가 필요하면 세마포어 같은 동기화 수단을 사용하세요.

   현재 코드는 새 스레드의 `priority`를 PRIORITY로 설정하지만,
   실제 우선순위 스케줄링은 아직 구현되어 있지 않습니다.
   우선순위 스케줄링은 문제 1-3에서 구현할 부분입니다. */
tid_t
thread_create (const char *name, int priority,
		thread_func *function, void *aux) {
	struct thread *t;
	tid_t tid;

	ASSERT (function != NULL);

	/* Allocate thread. */
	/* 스레드 구조체를 할당합니다. */
	t = palloc_get_page (PAL_ZERO);
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	/* 스레드를 초기화합니다. */
	init_thread (t, name, priority);
	tid = t->tid = allocate_tid ();

	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	/* 스케줄링 시 kernel_thread()가 실행되도록 설정합니다.
	 * rdi가 1번째 인자, rsi가 2번째 인자입니다. */
	t->tf.rip = (uintptr_t) kernel_thread;
	t->tf.R.rdi = (uint64_t) function;
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. */
	/* 실행 큐에 추가합니다. */
	thread_unblock (t);

	return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. 
   현재 스레드를 대기 상태로 전환합니다.
   thread_unblock()으로 깨워지기 전까지 다시 스케줄되지 않습니다.

   이 함수는 인터럽트가 꺼진 상태에서 호출해야 합니다.
   보통은 synch.h의 동기화 원시 자료구조를 쓰는 편이 더 좋습니다. */
void
thread_block (void) {
	ASSERT (!intr_context ()); 
	ASSERT (intr_get_level () == INTR_OFF);

	thread_current ()->status = THREAD_BLOCKED;

	schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. 
   BLOCKED 상태의 스레드 T를 READY 상태로 전환합니다.
   T가 블록 상태가 아니면 오류입니다. (실행 중인 스레드를 READY로 바꾸려면
   thread_yield()를 사용하세요.)

   이 함수는 현재 실행 중인 스레드를 선점하지 않습니다.
   인터럽트를 이미 꺼 둔 상태에서 호출했을 경우,
   스레드를 원자적으로 깨우고 다른 데이터도 함께 갱신할 수 있다고 가정할 수 있기 때문입니다. */
void
thread_unblock (struct thread *t) {
	enum intr_level old_level;

	ASSERT (is_thread (t));

	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED);

	//list_push_back (&ready_list, &t->elem);
	list_insert_ordered(&ready_list, &t->elem, priority_greater_comparator, NULL);
	t->status = THREAD_READY;
	intr_set_level (old_level);
}

/* Returns the name of the running thread. */
/* 현재 실행 중인 스레드의 이름을 반환합니다. */
const char *
thread_name (void) {
	return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. 
   현재 실행 중인 스레드를 반환합니다.
   running_thread()에서 추가적인 안전성 검사를 수행한 버전입니다.
   자세한 내용은 thread.h 상단 주석을 참고하세요. */
struct thread *
thread_current (void) {
	struct thread *t = running_thread ();

	/* Make sure T is really a thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow. 
	   t가 진짜 스레드인지 확인합니다.
	   여기서 assert가 실패하면 스택 오버플로우가 의심됩니다.
	   스레드 스택이 4KB 미만이므로 큰 자동 배열이나 깊은 재귀는 오버플로우를 유발할 수 있습니다. */
	ASSERT (is_thread (t));

	
	ASSERT (t->status == THREAD_RUNNING);

	return t;
}

/* Returns the running thread's tid. */
/* 현재 스레드의 tid를 반환합니다. */
tid_t
thread_tid (void) {
	return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
/* 현재 스레드를 디스패치에서 제거하고 소멸 처리합니다.
   이 함수는 호출한 위치로 복귀하지 않습니다. */
void
thread_exit (void) {
	ASSERT (!intr_context ());

#ifdef USERPROG
	process_exit ();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	/* 상태를 THREAD_DYING으로 바꾼 뒤 다른 스레드를 스케줄합니다.
	   schedule_tail()에서 실제 소멸이 진행됩니다. */
	intr_disable ();
	do_schedule (THREAD_DYING);
	NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
/* CPU를 양보합니다. 현재 스레드는 수면 상태가 아니므로
   스케줄러 판단에 따라 즉시 다시 실행될 수도 있습니다. */
void
thread_yield (void) {

	//curr: 현재 실행중인 리스트
	struct thread *curr = thread_current ();

	//인터럽트 끄기
	enum intr_level old_level;
	ASSERT (!intr_context ());
	old_level = intr_disable ();

	//현 스레드가 유휴 스레드가 아니면 ready에 넣기
	//유휴 스레드는 레디리스트에 안넣는다
	if (curr != idle_thread)
		//list_insert_ordered(&ready_list, &curr->elem, priority_greater_comparator, NULL);
		list_push_back (&ready_list, &curr->elem);
	do_schedule (THREAD_READY);
	intr_set_level (old_level);
}

/* Sets the current thread's priority to NEW_PRIORITY. */
/* 현재 스레드의 우선순위를 NEW_PRIORITY로 설정합니다. */
void
thread_set_priority (int new_priority) {
	thread_current ()->priority = new_priority;
}

/* Returns the current thread's priority. */
/* 현재 스레드의 우선순위를 반환합니다. */
int
thread_get_priority (void) {
	return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
/* 현재 스레드의 nice 값을 NICE로 설정합니다. */
void
thread_set_nice (int nice UNUSED) {
	/* TODO: Your implementation goes here */
}

/* Returns the current thread's nice value. */
/* 현재 스레드의 nice 값을 반환합니다. */
int
thread_get_nice (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the system load average. */
/* 시스템 부하 평균(load average)의 100배 값을 반환합니다. */
int
thread_get_load_avg (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the current thread's recent_cpu value. 
 현재 스레드의 recent_cpu 값의 100배를 반환합니다. */
int
thread_get_recent_cpu (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. 
   유휴 스레드입니다. 다른 스레드가 실행할 게 없을 때 실행됩니다.

   유휴 스레드는 thread_start()에서 처음 ready list에 들어갑니다.
   처음 스케줄될 때 idle_thread를 초기화하고 전달된 세마포어를 up 하여
   thread_start()가 계속 진행할 수 있게 만든 뒤 즉시 block됩니다.
   이후 유휴 스레드는 ready list에 다시 올라가지 않으며,
   ready list가 비어있을 때만 next_thread_to_run()에서 특수 케이스로 반환됩니다. */
static void
idle (void *idle_started_ UNUSED) {
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current ();
	sema_up (idle_started);

	for (;;) {
		/* Let someone else run. */
		/* 다른 스레드가 실행될 시간을 줍니다. */
		intr_disable ();
		thread_block ();

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		/* 인터럽트를 다시 켜고 다음 인터럽트를 기다립니다.

		   `sti` 명령은 다음 명령이 끝날 때까지 인터럽트를 비활성화하므로,
		   이어지는 두 명령은 원자적으로 실행됩니다. 이 원자성은 중요합니다.
		   만약 인터럽트가 다시 켜진 뒤 다음 인터럽트 대기 사이에 들어오면
		   최대 한 tick 정도의 시간을 낭비할 수 있습니다.

		   자세한 내용은 [IA32-v2a] "HLT", [IA32-v2b] "STI", [IA32-v3a] 7.11.1 "HLT Instruction"을 참고하세요. */
		asm volatile ("sti; hlt" : : : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
/* 커널 스레드의 진입점 함수입니다. */
static void
kernel_thread (thread_func *function, void *aux) {
	ASSERT (function != NULL);

	intr_enable ();       /* The scheduler runs with interrupts off. */
						 /* 스케줄러는 인터럽트가 꺼진 상태에서 동작합니다. */
	function (aux);       /* Execute the thread function. */
						 /* 스레드 함수를 실행합니다. */
	thread_exit ();       /* If function() returns, kill the thread. */
						 /* function()가 반환하면 스레드를 종료합니다. */
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
/* 스레드 T를 이름이 NAME인 BLOCKED 상태로 기본 초기화합니다. */
static void
init_thread (struct thread *t, const char *name, int priority) {
	ASSERT (t != NULL);
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT (name != NULL);

	memset (t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;
	strlcpy (t->name, name, sizeof t->name);
	t->tf.rsp = (uint64_t) t + PGSIZE - sizeof (void *);
	t->priority = priority;
	t->magic = THREAD_MAGIC;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. 
   다음으로 스케줄링할 스레드를 선택해 반환합니다.
   run queue가 비어 있지 않다면 run queue에서 하나를 꺼내 반환하고,
   비어 있으면 idle_thread를 반환합니다. */
static struct thread *
next_thread_to_run (void) {
	if (list_empty (&ready_list))
		return idle_thread;
	else
		return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread */
/* iretq로 스레드 실행을 시작합니다. */
void
do_iret (struct intr_frame *tf) {
	__asm __volatile(
			"movq %0, %%rsp\n"
			"movq 0(%%rsp),%%r15\n"
			"movq 8(%%rsp),%%r14\n"
			"movq 16(%%rsp),%%r13\n"
			"movq 24(%%rsp),%%r12\n"
			"movq 32(%%rsp),%%r11\n"
			"movq 40(%%rsp),%%r10\n"
			"movq 48(%%rsp),%%r9\n"
			"movq 56(%%rsp),%%r8\n"
			"movq 64(%%rsp),%%rsi\n"
			"movq 72(%%rsp),%%rdi\n"
			"movq 80(%%rsp),%%rbp\n"
			"movq 88(%%rsp),%%rdx\n"
			"movq 96(%%rsp),%%rcx\n"
			"movq 104(%%rsp),%%rbx\n"
			"movq 112(%%rsp),%%rax\n"
			"addq $120,%%rsp\n"
			"movw 8(%%rsp),%%ds\n"
			"movw (%%rsp),%%es\n"
			"addq $32, %%rsp\n"
			"iretq"
			: : "g" ((uint64_t) tf) : "memory");
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function. 
   새 스레드의 페이지 테이블을 활성화해 스위칭하고,
   이전 스레드가 종료 상태면 소멸시킵니다.

   이 함수 호출 시점에는 PREV에서 새 스레드로 이미 전환됐으며,
   새 스레드가 실행 중이고 인터럽트는 계속 꺼져 있습니다.

   thread switch가 완료되기 전에는 printf()를 안전하게 호출할 수 없습니다.
   실제로는 함수 마지막 부분에서 출력문을 넣는 편이 안전합니다. */
static void
thread_launch (struct thread *th) {
	uint64_t tf_cur = (uint64_t) &running_thread ()->tf;
	uint64_t tf = (uint64_t) &th->tf;
	ASSERT (intr_get_level () == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. 
	 * 실제 컨텍스트 스위칭 핵심 로직입니다.
	 * 먼저 실행 컨텍스트를 intr_frame에 복구한 뒤 do_iret를 호출해 다음 스레드로 이동합니다.
	 * 전환이 완료될 때까지 이 스택은 사용해서는 안 됩니다. */
	__asm __volatile (
			/* Store registers that will be used. */
			/* 사용할 레지스터 값을 저장합니다. */
			"push %%rax\n"
			"push %%rbx\n"
			"push %%rcx\n"
			/* Fetch input once */
			/* 입력 인자를 한 번에 읽어옵니다. */
			"movq %0, %%rax\n"
			"movq %1, %%rcx\n"
			"movq %%r15, 0(%%rax)\n"
			"movq %%r14, 8(%%rax)\n"
			"movq %%r13, 16(%%rax)\n"
			"movq %%r12, 24(%%rax)\n"
			"movq %%r11, 32(%%rax)\n"
			"movq %%r10, 40(%%rax)\n"
			"movq %%r9, 48(%%rax)\n"
			"movq %%r8, 56(%%rax)\n"
			"movq %%rsi, 64(%%rax)\n"
			"movq %%rdi, 72(%%rax)\n"
			"movq %%rbp, 80(%%rax)\n"
			"movq %%rdx, 88(%%rax)\n"
			"movq %%rcx, 96(%%rax)\n"
			"movq %%rbx, 104(%%rax)\n"
			"pop %%rbx\n"              // Saved rcx
			"movq %%rbx, 96(%%rax)\n"
			"pop %%rbx\n"              // Saved rbx
			"movq %%rbx, 104(%%rax)\n"
			"pop %%rbx\n"              // Saved rax
			"movq %%rbx, 112(%%rax)\n"
			"addq $120, %%rax\n"
			"movw %%es, (%%rax)\n"
			"movw %%ds, 8(%%rax)\n"
			"addq $32, %%rax\n"
			"call __next\n"         // read the current rip.
			"__next:\n"
			"pop %%rbx\n"
			"addq $(out_iret -  __next), %%rbx\n"
			"movq %%rbx, 0(%%rax)\n" // rip
			"movw %%cs, 8(%%rax)\n"  // cs
			"pushfq\n"
			"popq %%rbx\n"
			"mov %%rbx, 16(%%rax)\n" // eflags
			"mov %%rsp, 24(%%rax)\n" // rsp
			"movw %%ss, 32(%%rax)\n"
			"mov %%rcx, %%rdi\n"
			"call do_iret\n"
			"out_iret:\n"
			: : "g"(tf_cur), "g" (tf) : "memory"
			);
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). 
 * 새 프로세스를 스케줄링합니다. 진입 시 인터럽트는 꺼져 있어야 합니다.
 * 현재 스레드 상태를 status로 바꾸고, 다른 실행 스레드를 찾아 전환합니다.
 * schedule() 내에서는 printf()를 호출하면 안 됩니다. */
static void
do_schedule(int status) {
	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (thread_current()->status == THREAD_RUNNING);
	while (!list_empty (&destruction_req)) {
		struct thread *victim =
			list_entry (list_pop_front (&destruction_req), struct thread, elem);
		palloc_free_page(victim);
	}
	thread_current ()->status = status;
	schedule ();
}

static void
schedule (void) {
	struct thread *curr = running_thread ();
	struct thread *next = next_thread_to_run ();

	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (curr->status != THREAD_RUNNING);
	ASSERT (is_thread (next));
	/* Mark us as running. */
	/* 새로 선택된 스레드를 실행 상태로 표시합니다. */
	next->status = THREAD_RUNNING;

	/* Start new time slice. */
	/* 시간 슬라이스를 초기화합니다. */
	thread_ticks = 0;

#ifdef USERPROG
	/* Activate the new address space. */
	/* 새 스레드의 주소 공간을 활성화합니다. */
	process_activate (next);
#endif

	if (curr != next) {
		/* If the thread we switched from is dying, destroy its struct
		   thread. This must happen late so that thread_exit() doesn't
		   pull out the rug under itself.
		   We just queuing the page free reqeust here because the page is
		   currently used by the stack.
		   The real destruction logic will be called at the beginning of the
		   schedule(). */
		/* 이전 스레드가 종료 상태면 해당 thread 구조체를 해제합니다.
		   이는 thread_exit()가 자기 자신을 망치지 않도록 늦게 수행되어야 합니다.
		   스택이 사용하는 페이지를 즉시 해제하지 않고 여기서 삭제 요청으로만 큐에 넣고,
		   실제 해제는 schedule() 시작부에서 수행합니다. */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread) {
			ASSERT (curr != next);
			list_push_back (&destruction_req, &curr->elem);
		}

		/* Before switching the thread, we first save the information
		 * of current running. */
		/* 스레드 전환 전, 현재 실행중인 스레드 정보를 먼저 저장합니다. */
		thread_launch (next);
	}
}

/* Returns a tid to use for a new thread. */
/* 새 스레드에 사용할 tid를 반환합니다. */
static tid_t
allocate_tid (void) {
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire (&tid_lock);
	tid = next_tid++;
	lock_release (&tid_lock);

	return tid;
}

//스레드 elem을 priority에 대해서 오름차순으로 정렬 도움
static bool 
priority_greater_comparator(const struct list_elem *a,
           const struct list_elem *b,
		   void *aux) {
	
	return list_entry(a, struct thread, elem)-> priority >
		   list_entry(b, struct thread, elem)-> priority;
}          