#include "userprog/syscall.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "lib/kernel/stdio.h"
#include <syscall-nr.h>
#include <debug.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/input.h"

static void syscall_handler(struct intr_frame *);
void syscall_exit(int status, uint32_t *eax);
int syscall_read(int fd, void *buffer, unsigned size);
int syscall_write(int fd, void *buffer, unsigned size);
static bool put_user(uint8_t *udst, uint8_t byte);
static int get_user(const uint8_t *uaddr);

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f)
{
  int syscall = *(int *)(f->esp);

  /* Check if argument is fully in user address range */
  if(!is_user_vaddr((void *)(f->esp + 4))) {
    syscall_exit(-1, &f->eax);
  }
  
  switch (syscall)
  {
  case (SYS_EXIT):
    syscall_exit(*(int *)(f->esp + 4), &f->eax);
    break;

  case (SYS_READ):
    f->eax = (uint32_t)syscall_read(*(int *)(f->esp + 4), (char*)*(int*)(f->esp + 8), *(unsigned *)(f->esp + 12));

    /* Check if read was successful */
    if( f->eax == -1 ) {
      syscall_exit( -1, &f->eax );
    }
    break;

  case (SYS_WRITE):
    f->eax = (uint32_t)syscall_write(*(int *)(f->esp + 4), (uint8_t*)*(int*)(f->esp + 8), *(unsigned *)(f->esp + 12));
    
    /* Check if write was successful */
    if( f->eax == -1 ) {
      syscall_exit( -1, &f->eax );
    }
    break;

  default:
    printf("Invalid System Call!\n");
    thread_exit();
    break;
  }
}

void syscall_exit(int status, uint32_t *eax)
{
  printf("%s: exit(%d)\n", thread_current()->name, status);
  *eax = status;
  thread_exit();
}

int syscall_read(int fd, void *buffer, unsigned size)
{
  // Can only be stdin (PA2)
  if(fd != STDIN_FILENO)
  {
    return -1;
  }

  // Initialize the input buffer (not sure if needed)
  input_init();

  for (unsigned i = 0; i < size; i++)
  {
    if (!put_user(((uint8_t *)buffer + i), input_getc()))
    {
      return -1;
    }
  }
  return size;
}

#define WRITE_LEN 500

int syscall_write(int fd, void *buffer, unsigned size)
{
  // Can only be stdout (PA2)
  if(fd != STDOUT_FILENO)
  {
    return -1;
  }

  // Store a copy of the length to return on success
  unsigned initial_length = size;
  // Write in chunks of WRITE_LEN bytes defined above
  if (size > WRITE_LEN)
  {
    // Calculate how many bytes remain after dividing into even chunks, and write the excess
    int r = size % WRITE_LEN;
    putbuf((char *)buffer, r);
    size -= r;

    // After writing excess, this assertion should pass
    ASSERT(size % WRITE_LEN == 0);

    // While there's still remaining, write in chunks of WRITE_LEN bytes
    while (size > 0)
    {
      putbuf((char *)buffer, WRITE_LEN);
      size -= WRITE_LEN;
    }
  }

  // Simple case with a single write
  else
  {
    putbuf((char *)buffer, size);
  }
  return initial_length;
}

/* From userprog.texi
   Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user(uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm("movl $1f, %0; movb %b2, %1; 1:"
      : "=&a"(error_code), "=m"(*udst)
      : "q"(byte));
  return error_code != -1;
}
