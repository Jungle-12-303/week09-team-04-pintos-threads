#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

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
#define MAX_BUFFER_SIZE 1<<7 //버퍼 최대 입력 사이즈(128바이트)

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

	/*
	syscall_handler() gets control, 
	the system call number is in the rax, 
	and arguments are passed with the 
	order %rdi, %rsi, %rdx, %r10, %r8, and %r9.
	*/
	uint64_t sys_call_num = f->R.rax;
	uint64_t ARG0 = f->R.rdi;
	uint64_t ARG1 = f->R.rsi;
	uint64_t ARG2 =  f->R.rdx;
	uint64_t ARG3 =  f->R.r10;
	uint64_t ARG4 =  f->R.r8;
	uint64_t ARG5 =  f->R.r9;
	
	switch (sys_call_num)
	{
		case SYS_WRITE:
			/*
				Writes size bytes from buffer to the open file fd. 
				Returns the number of bytes actually written, 
				which may be less than size if some bytes could not be written. 
				Writing past end-of-file would normally extend the file, 
				but file growth is not implemented by the basic file system. 
				The expected behavior is to write as many bytes as possible up to end-of-file and return the actual number written, 
				or 0 if no bytes could be written at all. fd 1 writes to the console. 
				Your code to write to the console should write all of buffer in one call to putbuf(), 
				at least as long as size is not bigger than a few hundred bytes 
				(It is reasonable to break up larger buffers). 
				Otherwise, lines of text output by different processes may end up interleaved on the console, 
				onfusing both human readers and our grading scripts.s
				
				int
				write (int fd, const void *buffer, unsigned size) {
					return syscall3 (SYS_WRITE, fd, buffer, size);
				}

			*/

			int fd = (int)ARG0;
			void *buffer = (void *)ARG1;
			size_t size = (size_t)ARG2;

			f->R.rax = write_sys(fd, buffer, size);

			break;
	
		case SYS_EXIT:
			/*
			void exit (int status);
			Terminates the current user program, returning status to the kernel. 
			If the process's parent waits for it (see below), 
			this is the status that will be returned. 
			Conventionally, a status of 0 indicates success and nonzero values indicate errors.
			*/
			thread_exit();
			int status = thread_current()->status;

			f->R.rax = status;
			break;


	default:
		break;
	}

	printf ("system call!\n");
	thread_exit ();
}

int write_sys(int fd, void *buffer, size_t size){

	int written_size = 0;

	//fd 1 writes to the console
	if(fd == 1){

		/*
			Your code to write to the console 
			should write all of buffer 
			in one call to putbuf(), 
			at least as long as size is not bigger 
			than a few hundred bytes 
		*/

		if(size < MAX_BUFFER_SIZE){
			putbuf((char *) buffer, size);
			written_size = size;
		}
		else{

			// TODO: MAX_BUFFER_SIZE 로 나눠서 출력하기
			char *p = buffer;
			int rest_size = size;
			
			while(rest_size != 0){

				if(rest_size > MAX_BUFFER_SIZE){
					putbuf((char *)p, MAX_BUFFER_SIZE);
					p += MAX_BUFFER_SIZE;
					rest_size -= MAX_BUFFER_SIZE;
					
				}
				else
					putbuf((char *)p, rest_size);

				
			}

			written_size = size - rest_size;
		}

	}
	else{
		// TODO: 파일 출력
		/*
		Terminates the current user program, 
		returning status to the kernel. 
		If the process's parent waits for it (see below), 
		this is the status that will be returned. 
		Conventionally, a status of 0 indicates success and nonzero values indicate errors.
		*/


	}

	return written_size;
}
