#include <types.h>
#include <current.h>
#include <proc.h>
#include <lib.h>
#include <sys_functions.h>
#include <spl.h>
#include <thread.h>
#include <kern/fcntl.h>
#include <vnode.h>
#include <uio.h>
#include <vfs.h>
#include <copyinout.h>
#include <synch.h>
#include <kern/errno.h>
#include <thread.h>
#include <kern/unistd.h>

/**
 * All non-file related syscall functions for A2 goes here
 *
 * Functions:
 *
 * sys__exit
 * sys_write
 * sys_open
 */

void
sys__exit(int exitcode)
{
  int dbflags = DB_A2;

  (void) exitcode;
  struct proc *process = curthread->t_proc;

  KASSERT(process != NULL);

  int processCount = threadarray_num(&process->p_threads);

  //turn off all interrupts
  int original_spl = splhigh();

  //detach all threads used by this process
  for (int i = 0; i < processCount; i++)
    {
      threadarray_remove(&process->p_threads, i);
    }
  KASSERT(threadarray_num(&process->p_threads) == 0);

  //destroy process
  proc_destroy(process);

  DEBUG(DB_A2, "HERE");

  //turn spl back to original level
  splx(original_spl);

  thread_exit();

}
/**
 * Return value: nbytes read is returned.
 * This count should be positive.
 * A return value of 0 should be construed as signifying end-of-file.
 * On error, read returns -1 and sets errno to a suitable error code for the error condition encountered.
 */
int
sys_read(int fd, void* buf, size_t nbytes, int32_t *retval)
{

  int readBytes = 0;
  struct file_desc* file;
  struct iovec iov;
  struct uio uio_R; //uio for reading

  int old_spl = splhigh();
  //check if file is opened
  if (curthread->fd_table[fd] == NULL)
    {
      return EBADF;
    }

  file = curthread->fd_table[fd];
  uio_kinit(&iov, &uio_R, buf, nbytes, 0, O_RDONLY);
  readBytes = VOP_READ(file->f_vn, &uio_R);

  *retval = (int32_t) readBytes;
  splx(old_spl);

  return 0;
}

/**
 *  The count of bytes written is returned. This count should be positive.
 *  A return value of 0 means that nothing could be written, but that no error occurred;
 *  this only occurs at end-of-file on fixed-size objects. On error, write returns -1 and
 *  sets errno to a suitable error code for the error condition encountered.
 *  Note that in some cases, particularly on devices, fewer than buflen (but greater than zero) bytes may be written.
 *  This depends on circumstances and does not necessarily signify end-of-file.
 *  In most cases, one should loop to make sure that all output has actually been written.
 */
int
sys_write(int fd, const void *buf, size_t nbytes, int32_t *retval)
{
  int32_t wroteBytes = 0;
  struct file_desc* file; //file
  struct iovec iov;
  struct uio uio_W; //uio for writing

  //disable interrupts
  int old_spl = splhigh();

  wroteBytes = kprintf(buf);
  switch (fd)
    {
  case STDOUT_FILENO: //stdout
    wroteBytes = kprintf(buf); //temporary use kprintf
    break;
  case STDIN_FILENO: //stdin
    break;
  case STDERR_FILENO:
    break;
  default:
    lock_acquire(file->f_lock);
    //check if the file is opened
    if (curthread->fd_table[fd] == NULL)
      {
        return EBADF;
      }

    file = curthread->fd_table[fd];
    uio_kinit(&iov, &uio_W, &buf, nbytes, 0, UIO_WRITE);
    wroteBytes = VOP_WRITE(file->f_vn, &uio_W);

    lock_release(file->f_lock);
    break;
    }

  *retval = (int32_t) wroteBytes;
  splx(old_spl);

  return 0;
}

int
sys_open(const char *filename, int flags, int mode, int32_t *retval)
{
  int dbflags = DB_A2;
  int fd = 0; //filehandle to be returned
  bool opened = false;
  struct file_desc** table = curthread->fd_table;

  DEBUG(DB_A2, "now in sys_open\n");

  int old_spl = splhigh();

  //if file is the console: stdin, stdout, stderr
  if (filename == "con:" && flags == O_RDONLY)
    {
      fd = STDIN_FILENO;	//0
    }
  else if (filename == "con:" && flags == O_WRONLY && mode == 0664)
    {
      fd = STDOUT_FILENO; //1
    }
  else if (filename == "con:" && flags == O_WRONLY && mode == 0665)
    {
      fd = STDERR_FILENO; //2
    }
  else
    {

      //check in fd_table to see if file is already open
      KASSERT(MAX_OPEN_COUNT > 4);
      for (int i = 3; i < MAX_OPEN_COUNT; i++)
        {

          if (table[i] == NULL)
            {
              break;
            }

          /**
           * todo: f_name should have a max of MAX_LEN_FILENAME letters
           */
          if (table[i]->f_name == filename)
            {
              opened = true;
              table[i]->f_opencount++;
              fd = i;
              //should we also incrememnt reference count?
            }
        }

      //if not already opened
      if (!opened)
        {
          DEBUG(DB_A2, "file not already opened\n");
          //check if we reached MAX_OPEN_COUNT
          fd = 3;
          while (fd < MAX_OPEN_COUNT && table[fd] != NULL)
            {
              fd++;
            }
          DEBUG(DB_A2, "file will be opened with fd = %d\n", fd);

          //if there is room in table to add a fd entry
          if (fd < MAX_OPEN_COUNT)
            {
              table[fd] = kmalloc(sizeof(struct file_desc));
              table[fd]->f_name = (char *) filename;
              table[fd]->f_opencount = 1;
              table[fd]->f_refcount = 1;
              table[fd]->f_lock = lock_create(filename);
              table[fd]->f_vn = NULL;

              //open file and add entry to fdtable
              if (vfs_open((char*) filename, flags, mode, &table[fd]->f_vn))
                {
                  DEBUG(DB_A2, "going to report ENOENT\n");
                  //if there is a problem opening the file
                  return ENOENT;
                }

            }
        }
    }
  *retval = (int32_t) fd;
  splx(old_spl);

  return 0;
}

int
sys_close(int fd)
{

  KASSERT(curthread->fd_table[fd] != NULL);

  if (curthread->fd_table[fd] == NULL)
    {
      return EBADF;
    }

  struct file_desc* file = curthread->fd_table[fd];
  if (file->f_opencount > 0)
    {
      file->f_opencount--;
    }
  else
    {
      //permanently destroy the file_desc and the vnode
      kfree(file->f_name);
      lock_destroy(file->f_lock);
      vnode_cleanup(file->f_vn);
      kfree(file);
    }

  return 0;
}

