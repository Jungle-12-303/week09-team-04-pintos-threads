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
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "filesys/file.h"
#include "devices/input.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

static int sys_halt (void);
static void sys_exit (uint64_t status);
static int sys_wait (tid_t child_tid);
static int sys_write (uint64_t fd, uint64_t buffer, uint64_t size);
static bool sys_create(const char *file, unsigned initial_size);
static bool is_valid_user_ptr(const void*uaddr);
static bool is_valid_user_string(const char *str);
static int sys_open(const char *file);
static void sys_close(int fd);
static int sys_read(uint64_t fd, uint64_t buffer, uint64_t size);
static bool is_valid_user_buffer(uint64_t buffer, uint64_t size, bool writable);
static int sys_filesize(uint64_t fd);
/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	uint64_t syscall_num = f->R.rax;
	uint64_t ret = -1; // default return value is -1, which indicates an error
	uint64_t arg[6] = {f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8, f->R.r9}; // syscall arguments

	switch (syscall_num) {
		case SYS_HALT:
			ret = sys_halt();
			break;
		case SYS_EXIT:
			sys_exit(arg[0]);
			break;
		case SYS_WAIT:
			ret = sys_wait(arg[0]);
			break;
		case SYS_WRITE:
			ret = sys_write(arg[0], arg[1], arg[2]);
			break;
		case SYS_CREATE:
			ret = sys_create((const char*)arg[0], (unsigned)arg[1]);
			break;
		case SYS_OPEN:
			ret = sys_open((const char*)arg[0]);
			break;
		case SYS_CLOSE:
			sys_close((int)arg[0]);
			break;
		case SYS_READ:
			ret = sys_read(arg[0], arg[1],arg[2]);
			break;
		case SYS_FILESIZE:
			ret = sys_filesize(arg[0]);
			break;
		default:
			sys_exit (-1);
			 break;
	}

	f->R.rax = ret; // set return value

	// printf ("system call!\n");
	// thread_exit ();
}
	
static bool
is_valid_user_ptr(const void *uaddr){
	if((uaddr == NULL) || !is_user_vaddr(uaddr) || (pml4_get_page(thread_current()->pml4, uaddr) == NULL)){
		return false;
	}
	return true;
}

static bool is_valid_user_string(const char *str){
	const char *p = str;

	while(true){
		if (!is_valid_user_ptr(p)){
			return false;
		}

		if (*p == '\0'){
			return true;
		}
		p++;
	}
}

static bool 
is_valid_user_buffer(uint64_t buffer, uint64_t size, bool writable){
	if (size == 0){
		return true;
	}
	if (buffer == 0){
		return false;
	}
	
	uint64_t start = buffer;
	uint64_t end = buffer + size -1;

	if(end < start){
		return false;
	}

	if (!is_user_vaddr((void*)start)){
		return false;
	}
	if (!is_user_vaddr((void*)end)){
		return false;
	}

	for(uint64_t addr = start; addr <= end; addr = (uint64_t)pg_round_down((void*)addr) + PGSIZE){
		if(!is_user_vaddr((void*)addr)){
			return false;
		}
		if(pml4_get_page(thread_current()->pml4, (void*)addr)==NULL){
			return false;
		}
	}
	return true;
}

static int
sys_halt (){
	printf("!! System Call - Halt\n");
	power_off();
	return 0;
}

static void
sys_exit (uint64_t status){
	struct thread *curr = thread_current();
	curr->exit_status = (int) status;
	thread_exit();
}

static int
sys_wait (tid_t child_tid){
	return process_wait(child_tid);
}

static int
sys_write (uint64_t fd, uint64_t buffer, uint64_t size){
	if (fd == 1){
		putbuf((const char *) buffer, size);
		return size;
	}
	return -1;
}

static bool
sys_create(const char *file, unsigned initial_size){
	if (!is_valid_user_string(file)){
		sys_exit(-1);
	}

	return filesys_create(file, initial_size);
}

static int 
sys_open(const char *file){
	if(!is_valid_user_string(file)){
		sys_exit(-1);
	}

	struct file *opened_file = filesys_open(file);

	if(opened_file == NULL){
		return -1;
	}
	struct thread *cur = thread_current();

	for(int i=2; i<FD_MAX; i++){
		if (cur->fd_table[i] == NULL){
			cur->fd_table[i] = opened_file;
			return i;
		}
	}	
	file_close(opened_file);
	return -1;
}

static void
sys_close(int fd){
	struct thread *cur = thread_current();

	if(fd < 2){
		return;
	}
	if(fd >= FD_MAX){
		return;
	}
	if(cur->fd_table[fd] == NULL){
		return;
	}

	file_close(cur->fd_table[fd]);
	cur->fd_table[fd] = NULL;	
}

static int
sys_read(uint64_t fd, uint64_t buffer, uint64_t size){
	if(size == 0){
		return 0;
	}
	if(!is_valid_user_buffer(buffer, size, true)){
		sys_exit(-1);
	}
	struct thread *cur = thread_current();
	if(fd == 0){
		uint8_t* buf = (uint8_t*)buffer;
		for(uint64_t i=0; i<size; i++){
			buf[i] = input_getc();
		}
		return size;
	}
	if(fd == 1){
		return -1;
	}
	if(fd >= FD_MAX){
		return -1;
	}

	if(cur->fd_table[fd] !=NULL){
		return file_read(cur->fd_table[fd], (void*)buffer, size);
	}
	else{
		return -1;
	}
}

static int
sys_filesize(uint64_t fd){
	if (fd < 2){
		return -1;
	}
	if (fd >= FD_MAX){
		return -1;
	}

	struct thread *cur = thread_current();

	if(cur->fd_table[fd] == NULL){
		return -1;
	}

	return file_length(cur->fd_table[fd]);
}