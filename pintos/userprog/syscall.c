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
#include "include/lib/stdio.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

static int sys_halt (void);
static void sys_exit (uint64_t status);
static tid_t sys_fork (uint64_t thread_name);
static int sys_exec (uint64_t file);
static int sys_wait (tid_t child_tid);

static int sys_create (uint64_t name, uint64_t size);
static int sys_remove (uint64_t file);
static int sys_open (uint64_t name);
static int sys_filesize (uint64_t fd);
static int sys_read (uint64_t fd, uint64_t buffer, uint64_t size);
static int sys_write (uint64_t fd, uint64_t buffer, uint64_t size);
static void sys_seek (uint64_t fd, uint64_t position);
static unsigned sys_tell (uint64_t fd);
static void sys_close (uint64_t fd);

static bool is_valid_user_ptr (void *uaddr);
// static bool is_valid_string (void *uaddr);
// static bool is_valid_user_buffer (void *buffer, unsigned size);
// static bool is_readable_address (void *buffer);
// static bool is_writable_address (void *buffer);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall`
 * instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR         0xc0000081 /* Segment selector msr */
#define MSR_LSTAR        0xc0000082 /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

#define FAIL_NO    -1
#define Success_NO 0

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
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	uint64_t syscall_num = f->R.rax;
	uint64_t ret = -1; // default return value is -1, which indicates an error
	uint64_t arg[6] = { f->R.rdi, f->R.rsi, f->R.rdx,
		                f->R.r10, f->R.r8, f->R.r9 }; // syscall arguments

	struct thread *t = thread_current ();

	////@breakpoint syscall_num
	switch (syscall_num) {
	case SYS_HALT:
		ret = sys_halt ();
		break;
	case SYS_EXIT:
		sys_exit (arg[0]);
		break;

	case SYS_FORK:
		ret = sys_fork (arg[0]);
		break;

	case SYS_EXEC:
		break;

	case SYS_WAIT:
		ret = sys_wait (arg[0]);
		break;

	case SYS_CREATE:
		ret = sys_create (arg[0], arg[1]);
		break;

	case SYS_OPEN:
		ret = sys_open (arg[0]);
		break;

	case SYS_FILESIZE:
		ret = sys_filesize (arg[0]);
		break;

	case SYS_READ:
		ret = sys_read (arg[0], arg[1], arg[2]);
		break;

	case SYS_WRITE:
		ret = sys_write (arg[0], arg[1], arg[2]);
		break;

	case SYS_CLOSE:
		sys_close (arg[0]);
		break;

	default:
		sys_exit (-1);
		break;
	}

	f->R.rax = ret; // set return value

	// printf ("system call!\n");
	// thread_exit ();
}

static int
sys_halt () {
	printf ("!! System Call - Halt\n");
	power_off ();
	return 0;
}

static void
sys_exit (uint64_t status) {
	struct thread *curr = thread_current ();
	curr->exit_status = (int) status;
	thread_exit ();
}

/* @note
    현재 프로세스를 인자 이름으로 하는 프로세스를 만들기
    callee-saved registers인 %RBX, %RSP, %RBP, and %R12 - %R15외에는 복사할 필요 없음
    반환값은 pid

    자식 프로세스는 0을 반환해야 하고
    자식은 복사된 file descriptor and virtual memory space를 가져야 함

    부모는 절대 자식 프로세스가 성공적으로 복제되는 동안에 반환되면 안된다
    만약 자식 프로세스가 복제되는게 실패하면 TID_ERROR를 반환한다.

    유저 메모리를 복사하는데에 threads/mmu.c의 pml4_for_each()를 사용
    또한 pte_for_each_func으로 놓친 구간을 채워야 한다

*/
static tid_t
sys_fork (uint64_t thread_name) {
	tid_t pid = 0;
	struct thread *t = thread_current ();

	return pid;
}

static int
sys_exec (uint64_t file) {
}

static int
sys_wait (tid_t child_tid) {
	return process_wait (child_tid);
}

static int
sys_read (uint64_t fd, uint64_t buffer, uint64_t size) {
	/*
	Reads size bytes from the file open as fd into buffer.
	Returns the number of bytes actually read (0 at end of file), or
	-1 if the file could not be read
	(due to a condition other than end of file).
	fd 0 reads from the keyboard using input_getc().
	*/

	if (fd == STDIN_FILENO) {
		size = input_getc (buffer);
	} else if (fd == STDOUT_FILENO) {
		sys_exit (FAIL_NO);
	} else {
		struct file *f;

		if (fdt_find_fd (fd, &f) == false || !is_valid_user_ptr ((void *) buffer)) {
			sys_exit (FAIL_NO);
		}

		// printf ("!! sysread 파일 주소: %lld \n", f);
		return file_read (f, (void *) buffer, (off_t) size);
	}

	return Success_NO;
}

static int
sys_write (uint64_t fd, uint64_t buffer, uint64_t size) {
	if (fd == STDOUT_FILENO) {
		putbuf ((const char *) buffer, size);
		return size;
	} else {
		struct file *f;

		if (fdt_find_fd (fd, &f) == false || !is_valid_user_ptr ((void *) buffer))
			sys_exit (FAIL_NO);

		// printf ("!! sysread 파일 주소: %lld \n", f);
		return file_write (f, (void *) buffer, (off_t) size);
	}
}

static int
sys_create (uint64_t buffer, uint64_t size) {
	if (!is_valid_user_ptr ((void *) buffer))
		sys_exit (FAIL_NO);

	return filesys_create (buffer, size);
}

static int
sys_open (uint64_t name) {
	if (!is_valid_user_ptr (name))
		sys_exit (FAIL_NO);

	struct file *f = filesys_open (name);
	if (f == NULL)
		return FAIL_NO;

	if (thread_current ()->fdt == 0)
		fdt_init ();

	return fd_alloc (f);
}

static int
sys_filesize (uint64_t fd) {
	struct file *f;
	if (fdt_find_fd (fd, &f) == false)
		return FAIL_NO;

	return file_length (f);
}

static void
sys_close (uint64_t fd) {
	if (fdt_close_fd ((int) fd) == true)
		thread_current ()->exit_status = Success_NO;
}

static bool
is_valid_user_ptr (void *uaddr) {
	if (uaddr == NULL ||
	    pml4_get_page (thread_current ()->pml4, uaddr) == NULL ||
	    !is_user_vaddr (uaddr)) {
		// printf ("!! [is_valid_user_ptr] 통과 실패\n");
		return false;
	}

	// printf ("!! [is_valid_user_ptr] 통과\n");
	return true;
}

// static bool
// is_valid_string (void *uaddr) {
// 	if (is_valid_user_ptr (uaddr))
// 		return false;

// 	char *currP = (char *) uaddr;
// 	while (currP != '\0') {
// 		if (is_valid_user_ptr (currP))
// 			return false;

// 		currP += 1;
// 	}

// 	return true;
// }

// static bool
// is_valid_user_buffer (void *buffer, unsigned size) {
// 	//@note: 제시한 버퍼주소부터 체크
// 	if (is_valid_user_ptr (buffer))
// 		return false;

// 	uintptr_t *uaddr = buffer;
// 	uintptr_t *end_uaddr = (uintptr_t) buffer + size;
// 	uintptr_t diff;

// 	while (uaddr <= end_uaddr) {
// 		diff = end_uaddr - uaddr;

// 		// @note: 버퍼 크기가 페이지 단위를 안 넘을 때
// 		if (diff < PGSIZE) {
// 			return (is_valid_user_ptr ((void *) (buffer + size)));
// 		} else {
// 			uaddr += PGSIZE;
// 			if (!is_valid_user_ptr (uaddr))
// 				return false;
// 		}
// 	}

// 	return true;
// }

// static bool
// is_readable_address (void *buffer) {
// }

// static bool
// is_writable_address (void *buffer) {
// }
