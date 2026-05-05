#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "string.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

static void valid_argument (const char *file);
static void valid_fd (const uint64_t fd);
static void sys_halt (void);
static void sys_exit (uint64_t status);
static int sys_wait (tid_t child_tid);
static int sys_write (int fd, const void *buffer, unsigned size);
static int sys_create (const char *file, unsigned initial_size);
static int sys_open (const char *file);
static int sys_close (int fd);

struct lock file_lock;

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR         0xc0000081 /* Segment selector msr */
#define MSR_LSTAR        0xc0000082 /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr (MSR_STAR, ((uint64_t) SEL_UCSEG - 0x10) << 48 |
	                             ((uint64_t) SEL_KCSEG) << 32);
	write_msr (MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr (MSR_SYSCALL_MASK,
	           FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	lock_init (&file_lock);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	uint64_t syscall_num = f->R.rax;
	uint64_t ret = -1; // default return value is -1, which indicates an error
	uint64_t arg[6] = { f->R.rdi, f->R.rsi, f->R.rdx,
		                f->R.r10, f->R.r8,  f->R.r9 }; // syscall arguments

	switch (syscall_num) {
	case SYS_HALT:
		sys_halt ();
		break;
	case SYS_EXIT:
		sys_exit (arg[0]);
		break;
	case SYS_WAIT:
		ret = sys_wait (arg[0]);
		break;
	case SYS_WRITE:
		ret = sys_write (arg[0], arg[1], arg[2]);
		break;
	case SYS_CREATE:
		ret = sys_create (arg[0], arg[1]);
		break;
	case SYS_OPEN:
		ret = sys_open (arg[0]);
		break;
	case SYS_CLOSE:
		ret = sys_close (arg[0]);
		break;
	default:
		sys_exit (-1);
		break;
	}

	f->R.rax = ret; // set return value

	// printf ("system call!\n");
	// thread_exit ();
}

static void
valid_argument (const char *file) {
	struct thread *curr = thread_current ();
	if (file == NULL || !is_user_vaddr (file) ||
	    pml4_get_page (curr->pml4, file) == NULL) {
		curr->exit_status = -1;
		thread_exit ();
	}
}

static void
valid_fd (const uint64_t fd) {
	struct thread *curr = thread_current ();
	if (fd < 2 || fd >= FD_SIZE) {
		curr->exit_status = -1;
		thread_exit ();
	}
}

static void
sys_halt () {
	power_off ();
}

static void
sys_exit (uint64_t status) {
	struct thread *curr = thread_current ();
	curr->exit_status = (int) status;
	thread_exit ();
}

static int
sys_wait (tid_t child_tid) {
	return process_wait (child_tid);
}

static int
sys_write (int fd, const void *buffer, unsigned size) {
	if (fd == 1) {
		putbuf ((const char *) buffer, size);
		return size;
	} else {
		valid_fd (fd);
		valid_argument (buffer);

		struct thread *curr = thread_current ();
		lock_acquire (&file_lock);
		off_t temp = file_write (curr->fd[fd], buffer, size);
		lock_release (&file_lock);

		return temp;
	}
}

static int
sys_create (const char *file, unsigned initial_size) {
	valid_argument (file);

	if (strlen (file) > 14) {
		return 0;
	}

	return filesys_create (file, initial_size);
}

static int
sys_open (const char *file) {
	valid_argument (file);

	if (strlen (file) > 14) {
		return -1;
	}

	lock_acquire (&file_lock);
	struct file *temp = filesys_open (file);
	lock_release (&file_lock);

	if (temp == NULL) {
		return -1;
	}

	struct thread *curr = thread_current ();
	for (int i = 3; i < FD_SIZE; i++) {
		if (curr->fd[i] == 0) {
			curr->fd[i] = temp;
			return i;
		}
	}
	return -1;
}

static int
sys_close (int fd) {
	valid_fd (fd);

	struct thread *curr = thread_current ();
	if (curr->fd[fd] == 0) {
		return -1;
	}
	lock_acquire (&file_lock);
	file_close (curr->fd[fd]);
	lock_release (&file_lock);
	curr->fd[fd] = 0;

	return 0;
}
