#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

/*** Jack ***/
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/palloc.h"
#include "lib/string.h"

/*** GrilledSalmon ***/
#include "threads/init.h"	
#include "userprog/process.h"
#include "devices/input.h"			// for 'input_getc()'
#include "kernel/stdio.h"

/* eleshock */
#include "vm/file.h"
#include "vm/vm.h"


void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void halt (void);						/*** GrilledSalmon ***/
void exit (int status);					/*** GrilledSalmon ***/

/*** Phase 1 ***/
/*** Jack ***/
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int filesize (int fd);
void seek (int fd, unsigned position);

int read (int fd, void *buffer, unsigned size); 	/*** GrilledSalmon ***/
int write (int fd, void *buffer, unsigned size);    /*** GrilledSalmon ***/
unsigned tell (int fd);                             /*** GrilledSalmon ***/

typedef int pid_t;
int wait (pid_t pid);                               /*** Jack ***/
int exec (const char *cmd_line);                    /*** Jack ***/

/*** hyeRexx : phase 3 ***/
pid_t fork(const char *thread_name, struct intr_frame *intr_f);

static struct lock filesys_lock;                    /*** GrilledSalmon ***/

/* eleshock */
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset);
void munmap (void *addr);


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
syscall_init (void)
{
    lock_init(&filesys_lock);       /*** GrilledSalmon ***/

	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/*** hyeRexx ***/
/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) 
{
    int64_t syscall_case = f->R.rax;
    ASSERT(is_user_vaddr(f->rsp)); // rsp ?????? ????????? ????????? ?????? 

    /* eleshock */
    thread_current()->if_rsp = f->rsp;

	switch (syscall_case)
    {
        case SYS_HALT :
            halt();
            break;
        
        case SYS_EXIT :
            exit(f->R.rdi);
            break;
        
        case SYS_FORK : 
            f->R.rax = fork(f->R.rdi, f);
            break;
        
        case SYS_EXEC :
            f->R.rax = exec(f->R.rdi);
            break;
        
        case SYS_WAIT :
            f->R.rax = wait(f->R.rdi);
            break;
        
        case SYS_CREATE : 
            f->R.rax = create(f->R.rdi, f->R.rsi);
            break;

        case SYS_REMOVE :
            f->R.rax = remove(f->R.rdi);
            break;
        
        case SYS_OPEN :
            f->R.rax = open(f->R.rdi); // returns new file descriptor
            break;

        case SYS_FILESIZE : /*** debugging genie : phase 2 ***/
            f->R.rax = filesize(f->R.rdi);
            break;

        case SYS_READ :
            f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
            break;
        
        case SYS_WRITE :
            f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
            break;
        
        case SYS_SEEK : // Jack
            seek(f->R.rdi, f->R.rsi);
            break;
        
        case SYS_TELL :
            f->R.rax = tell(f->R.rdi);
            break;
        
        case SYS_CLOSE :
            close(f->R.rdi);
            break;  

        case SYS_MMAP : // eleshock
            f->R.rax = mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8); // debugging sanori - ?????? ?????? ??????..?
            break;

        case SYS_MUNMAP : // eleshock
            munmap(f->R.rdi);
            break;
    }
}

void 
check_address(void *vaddr) 
{
	// if (is_kernel_vaddr(vaddr) || vaddr == NULL || pml4_get_page (thread_current()->pml4, vaddr) == NULL)
    if (!is_user_vaddr(vaddr) || vaddr == NULL)
        exit(-1);
}

/*** Jack ***/
bool create (const char *file, unsigned initial_size)
{
	check_address(file);
	return filesys_create(file, initial_size);
}

/*** Jack ***/
bool remove (const char *file)
{
	check_address(file);
	return filesys_remove(file);
}

/*** Jack ***/
int filesize (int fd)
{
	struct file *f = process_get_file(fd);
    if (f == NULL)
        return -1;

	return file_length(f); 
}

/*** GrilledSalmon ***/
/* Power off the Pintos system.
 * The user will barely use this syscall function. */
void halt (void)
{
	power_off();			/* Power off */
}

/*** GrilledSalmon ***/
/* Process exit */
void exit (int status)
{	
	struct thread *curr_thread = thread_current();

#ifdef USERPROG
    curr_thread->exit_status = status;                  /*** Jack ***/
#endif

	/*** Develope Genie ***/
	/* ????????? ???????????? ????????? ?????? ?????? status??? ?????? ?????? ???????????? ???!! */
    /* thread_exit -> process_exit ?????? sema up ???????????? ????????? */

	thread_exit();			/* ?????? ???????????? ????????? DYING ?????? ????????? schedule(?????? ??????????????? ?????????) */
}

/*** hyeRexx ***/
/*** debugging genie : do we need to check sysout, sysin? ***/
int open(const char *file)
{
    check_address(file);                           // check validity of file ptr
    struct file *new_file = filesys_open(file);    // file open, and get file ptr

    if(!new_file) // fail
    {
        return -1;
    }

    int fd = process_add_file(new_file);
    if (fd == -1)
        file_close(new_file);
    
    return fd; // return file descriptor for 'file'
}

/*** hyeRexx ***/
void close(int fd)
{
    process_close_file(fd);
    return;
}

/*** Jack ***/
/* Change offset from origin to 'position' */
void seek (int fd, unsigned position)
{
    ASSERT(fd >= 0);
    ASSERT (position >= 0);

    struct file* f = process_get_file(fd);
    ASSERT (f != NULL);
    
    file_seek(f, position);
    return;
}

/*** GrilledSalmon ***/
int read (int fd, void *buffer, unsigned size)
{
    check_address(buffer);

    // buffer??? read only ??? ???????????? ?????????????????? ?????? - Jack Debug
    struct page *p = spt_find_page(&thread_current()->spt, buffer);
    if (p != NULL && !p->writable)
        exit(-1);

    uint64_t read_len = 0;              // ????????? ??????

	if (fd == 0) { 			            /* fd??? stdin??? ????????? ?????? */
        
        /*** extra??? ??? ??????????????? ***/

        char *buffer_cursor = buffer;
        lock_acquire(&filesys_lock);    // debugging genie
        while (read_len < size)
        {
            *buffer_cursor++ = input_getc();
            read_len++;
        }
        *buffer_cursor = '\0';
        lock_release(&filesys_lock);
        return read_len;
	}

	struct file *now_file = process_get_file(fd);

    if (now_file == NULL || fd == 1){   // fd??? stdout??? ??????????????? file??? ?????? ??????
        return -1;
    }

    lock_acquire(&filesys_lock);
    read_len = file_read(now_file, buffer, size);
    lock_release(&filesys_lock);
    return read_len;
}

/*** GrilledSalmon ***/
int write (int fd, void *buffer, unsigned size)
{
    check_address(buffer);

    if (fd == 1) {                      // fd == stdout??? ??????
        lock_acquire(&filesys_lock);
        putbuf(buffer, size);
        lock_release(&filesys_lock);
        return size;
    }

    struct file *now_file = process_get_file(fd);

    if (now_file == NULL || fd == 0){   // fd??? stdin??? ??????????????? file??? ?????? ??????
        return -1;
    }

    lock_acquire(&filesys_lock);
    uint64_t read_len = file_write(now_file, buffer, size);
    lock_release(&filesys_lock);

    return read_len;
}

/*** GrilledSalmon ***/
unsigned tell (int fd)
{
    struct file *now_file = process_get_file(fd);
    if (now_file == NULL) {
        return -1;
    }
    return file_tell(now_file);
}

/*** Jack ***/
int wait (pid_t pid)
{
    return process_wait(pid);
}

/*** Jack ***/
int exec (const char *cmd_line)
{
    check_address(cmd_line);

    char *cmd_copy = malloc(strlen(cmd_line)+2); // ????????? ????????? ?????? malloc?????? ??????
    strlcpy(cmd_copy, cmd_line, strlen(cmd_line)+2);

    return process_exec(cmd_copy);
}

/*** hyeRexx ***/
pid_t fork (const char *thread_name, struct intr_frame *intr_f) // ???????????? ?????????
{
    check_address(thread_name);

    tid_t child = process_fork(thread_name, intr_f);
    return (child == TID_ERROR) ? TID_ERROR : child; 
}


/* eleshock */
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset)
{
    struct file *now_file = process_get_file(fd);
    bool chk_addr = (addr != NULL) && is_user_vaddr(addr); // debug
    return now_file && chk_addr? do_mmap(addr, length, writable, now_file, offset): NULL;
}

/* eleshock */
void munmap (void *addr)
{
    check_address(addr);
    do_munmap(addr);
}
