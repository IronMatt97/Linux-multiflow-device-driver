//@TODO - fix the 'char removal' feature
//@TODO - implement a test disabling some devices or do it before running tests

#define EXPORT_SYMTAB
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/tty.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/jiffies.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Matteo Ferretti <matti1097@gmail.com>");
MODULE_DESCRIPTION("A basic device driver implementing multi-flow devices realized for academic purpose.");

#define MODNAME "MULTI-FLOW-DEVICE"
#define DEVICE_NAME "mfdev"

#define get_major(session) MAJOR(session->f_inode->i_rdev)  //MAJOR number
#define get_minor(session) MINOR(session->f_inode->i_rdev)  //MINOR number

#define DEFAULT_BLOCKING_OPS_TIMEOUT 1 //Default timeout (in seconds) for blocking operations on a device
#define DEFAULT_BLOCKING_MODE 0  //Default blocking mode of RW operations for a device (0 = non-blocking - 1 = blocking)
#define DEFAULT_PRIORITY_MODE 0  //Default priority mode for a device (0 = low priority usage - 1 = high priority usage)

#define OBJECT_MAX_SIZE (4096)

typedef struct _object_state  // This struct represents a single device
{
   struct mutex mutex_hi;  // synchronization utility for high priority flows
   struct mutex mutex_lo;  // synchronization utility for low priority flows
   int priorityMode;                  // 0 = low priority usage, 1 = high priority usage
   int blockingModeOn;                // 0 = non-blocking RW ops, 1 = blocking RW ops
   int *isEnabled;                    // pointer to module param representing if a device is enabled (openable sessions)
   unsigned long awake_timeout;       // timeout regulating the awake of blocking operations
   int valid_bytes_lo;                // written bytes present in the low priority flow
   int valid_bytes_hi;                // written bytes present in the high priority flow
   char *low_priority_flow;           // low priority data stream
   char *high_priority_flow;          // high priority data stream
   wait_queue_head_t high_prio_queue; // wait event queue for threads waiting to perform actions on high priority streams
   wait_queue_head_t low_prio_queue;  // wait event queue for threads waiting to perform actions on low priority streams

} object_state;

typedef struct _packed_work // Work structure to write on devices
{
   struct file *filp;   //pointer to session
   char *buff; //data string
   size_t len; //data size variable
   long long int off;   //session offset
   struct work_struct work;   //defined work for work queue
} packed_work;

static int Major;

#define MINORS 128
object_state objects[MINORS];

//Arrays used for module parameters
int devices_state[MINORS];
int high_bytes[MINORS];
int low_bytes[MINORS];
int high_waiting[MINORS];
int low_waiting[MINORS];

module_param_array(devices_state,int,NULL,S_IWUSR|S_IRUSR); //Module parameter to expose devices state (0 = disabled - 1 = enabled)
module_param_array(high_bytes,int,NULL,S_IRUGO);   //Module parameter describing how many valid bytes are present in every high priority flow
module_param_array(low_bytes,int,NULL,S_IRUGO); //Module parameter describing how many valid bytes are present in every low priority flow
module_param_array(high_waiting, int,NULL,S_IRUGO);   //Module parameter representing how many threads are waiting on high priority stream for every device
module_param_array(low_waiting, int,NULL,S_IRUGO); //Module parameter representing how many threads are waiting on low priority stream for every device

MODULE_PARM_DESC(devices_state, "Array of devices states (0 = disabled - 1 = enabled)");
MODULE_PARM_DESC(high_bytes, "Array reporting the number of current valid bytes in the high priority stream of every device.");
MODULE_PARM_DESC(low_bytes, "Array reporting the number of current valid bytes in the low priority stream of every device.");
MODULE_PARM_DESC(high_waiting, "Array describing the number of threads waiting on the high priority stream of every device.");
MODULE_PARM_DESC(low_waiting, "Array describing the number of threads waiting on the low priority stream of every device.");

void work_function(struct work_struct *work)
{
   // Implementation of deferred work
   int minor;
   int ret;
   int ok;
   int len;
   char * buff;
   long long int off;
   packed_work *info;
   object_state *the_object;

   info = container_of(work, packed_work, work);
   minor = get_minor(info->filp);
   the_object = objects + minor;


   printk("%s: KWORKER DAEMON RUNNING - PID = %d - CPU-core = %d\n",MODNAME,current->pid,smp_processor_id());
   if (the_object->blockingModeOn)
   {
      __atomic_fetch_add(&low_waiting[minor], 1, __ATOMIC_SEQ_CST);
      ok = wait_event_timeout(the_object->low_prio_queue, mutex_trylock(&(the_object->mutex_lo)), the_object->awake_timeout);
      __atomic_fetch_sub(&low_waiting[minor], 1, __ATOMIC_SEQ_CST);
      if (!ok)
      {
         printk("%s: KWORKER DAEMON: REQUEST TIMEOUT - the requested write ('%s') on minor %d will not be performed.\n", MODNAME, info->buff,minor);
         free_page((unsigned long)info->buff);
         kfree(info);
         return;
      }
   }
   else
   {
      ok = mutex_trylock(&(the_object->mutex_lo));
      if (ok == EBUSY)
      {
         printk("%s: KWORKER DAEMON: RESOURCE BUSY - the requested write ('%s') on minor %d will not be performed.\n", MODNAME, info->buff,minor);
         free_page((unsigned long)info->buff);
         kfree(info);
         return;
      }
   }
   //Lock acquired
   (info->off) += the_object->valid_bytes_lo;

   // Only low priority condition possible here
   // if offset too large
   if ((info->off) >= OBJECT_MAX_SIZE)
   {
      free_page((unsigned long)info->buff);
      kfree(info);
      mutex_unlock(&(the_object->mutex_lo));
      wake_up(&(the_object->low_prio_queue));
      printk("%s: KWORKER DAEMON: ERROR - No space left on device\n", MODNAME);
      return;
   }
   // if offset beyond the current stream size
   if (((info->off) > the_object->valid_bytes_lo))
   {
      free_page((unsigned long)info->buff);
      kfree(info);
      mutex_unlock(&(the_object->mutex_lo));
      wake_up(&(the_object->low_prio_queue));
      printk("%s: KWORKER DAEMON: ERROR - Out of stream resources\n", MODNAME);
      return;
   }
   
   len = info->len;
   buff = info->buff;
   off = info->off;

   if ((OBJECT_MAX_SIZE - (info->off)) < len)
      len = OBJECT_MAX_SIZE - (info->off);

   strncat(the_object->low_priority_flow,buff,len);
   info->off +=len;
   the_object->valid_bytes_lo = (info->off);
   low_bytes[minor] = the_object->valid_bytes_lo;
   printk("%s: KWORKER DAEMON: WRITE OPERATION COMPLETED - uncopied bytes = %d\n",MODNAME,ret);
   printk("%s: KWORKER DAEMON: UPDATED FLOWS\nLOW_PRIORITY_FLOW: %s\nHIGH_PRIORITY_FLOW: %s\n", MODNAME, the_object->low_priority_flow, the_object->high_priority_flow);
   free_page((unsigned long)info->buff);
   kfree(info);
   mutex_unlock(&(the_object->mutex_lo));
   wake_up(&(the_object->low_prio_queue));
   return;
}

/*
      DRIVER
*/

static int dev_open(struct inode *inode, struct file *file)
{
   int minor = get_minor(file);
   object_state *the_object;

   if (minor >= MINORS)
   {
      printk("%s: ERROR - minor number %d out of handled range.\n", MODNAME, minor);
      return -1;
   }
   the_object = objects + minor;
   if(devices_state[minor] == 1)
   {
      printk("%s: OPENED DEVICE FILE WITH MINOR %d\n", MODNAME, minor);
      return 0;
   }
   else
   {
      printk("%s: ERROR - requested device with minor number %d is currently disabled.\n", MODNAME, minor);
      return -1;
   }
}

static int dev_release(struct inode *inode, struct file *file)
{
   int minor = get_minor(file);
   printk("%s: CLOSED DEVICE FILE WITH MINOR %d\n", MODNAME, minor);
   return 0;
}

static ssize_t dev_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
   // Synchronous for high and asynchronous (deferred work) for low priority
   // Blocking mode = waits for lock to go on, Non-blocking mode = doesn't wait for lock if busy and returns
   int ret;
   int ok;
   int minor = get_minor(filp);
   object_state *the_object = objects + minor;
   int highPriority = the_object->priorityMode;

   printk("%s: WRITE CALLED ON [MAJ-%d,MIN-%d]\n", MODNAME, get_major(filp), get_minor(filp));
   printk("%s: \nPriority mode = %d\nBlocking mode = %d\nValid bytes (low priority stream) = %d\nValid bytes (high priority stream) = %d\n", MODNAME, highPriority, the_object->blockingModeOn, the_object->valid_bytes_lo, the_object->valid_bytes_hi);

   // Blocking mode
   if (the_object->blockingModeOn)
   {
      if (highPriority)
      {
         __atomic_fetch_add(&high_waiting[minor], 1, __ATOMIC_SEQ_CST);
         ok = wait_event_timeout(the_object->high_prio_queue, mutex_trylock(&(the_object->mutex_hi)), the_object->awake_timeout);
         __atomic_fetch_sub(&high_waiting[minor], 1, __ATOMIC_SEQ_CST);
         if (!ok)
         {
            printk("%s: REQUEST TIMEOUT - the requested write ('%s') on minor %d will not be performed.\n", MODNAME, buff, minor);
            return -1;
         }
      }
      else
      {
         // Deferred work
         packed_work *info = kzalloc(sizeof(packed_work), GFP_ATOMIC);
         if (info == NULL)
         {
            printk("%s: ERROR - deferred work structure allocation failed.\n", MODNAME);
            return -1;
         }
         info->filp = filp;
         //info->buff = buff;
         info->len = len;
         info->buff = (char *)__get_free_page(GFP_KERNEL);
         ret = copy_from_user((info->buff), buff, len);
         info->off = *off;

         __INIT_WORK(&(info->work), work_function, (unsigned long)(&(info->work)));
         schedule_work(&(info->work));
         return 0;
      }

   } // Non-blocking mode
   else
   {
      if (highPriority)
      {
         ok = mutex_trylock(&(the_object->mutex_hi));
         if (ok == EBUSY)
         {
            printk("%s: RESOURCE BUSY - the requested write ('%s') on minor %d will not be performed.\n", MODNAME, buff, minor);
            return -1;
         }
      }
      else
      {
         // Deferred work
         packed_work *info = kzalloc(sizeof(packed_work), GFP_ATOMIC);
         if (info == NULL)
         {
            printk("%s: ERROR - deferred work structure allocation failed.\n", MODNAME);
            return -1;
         }
         info->filp = filp;
         //info->buff = buff;
         info->len = len;
         info->buff = (char *)__get_free_page(GFP_KERNEL);
         ret = copy_from_user((info->buff), buff, len);
         info->off = *off;

         __INIT_WORK(&(info->work), work_function, (unsigned long)(&(info->work)));
         schedule_work(&(info->work));
         return 0;
      }
   }

   if (highPriority)
      *off += the_object->valid_bytes_hi;
   else
      *off += the_object->valid_bytes_lo;


   // Only high priority condition possible here
   // if offset too large
   if (*off >= OBJECT_MAX_SIZE)
   {
      mutex_unlock(&(the_object->mutex_hi));
      wake_up(&(the_object->high_prio_queue));
      printk("%s: ERROR - No space left on device\n", MODNAME);
      return -ENOSPC;
   }
   // if offset beyond the current stream size
   if ((!highPriority && *off > the_object->valid_bytes_lo) || (highPriority && *off > the_object->valid_bytes_hi))
   {
      mutex_unlock(&(the_object->mutex_hi));
      wake_up(&(the_object->high_prio_queue));
      printk("%s: ERROR - Out of stream resources\n", MODNAME);
      return -ENOSR;
   }

   if ((OBJECT_MAX_SIZE - *off) < len)
      len = OBJECT_MAX_SIZE - *off;

   if (highPriority)
   {
      ret = copy_from_user(&(the_object->high_priority_flow[*off]), buff, len);
      *off += (len - ret);
      the_object->valid_bytes_hi = *off;
      high_bytes[minor] = the_object->valid_bytes_hi;
   }
   else
   {
      ret = copy_from_user(&(the_object->low_priority_flow[*off]), buff, len);
      *off += (len - ret);
      the_object->valid_bytes_lo = *off;
      low_bytes[minor] = the_object->valid_bytes_lo;
   }

   printk("%s: WRITE OPERATION COMPLETED - uncopied bytes = %d\n",MODNAME,ret);
   printk("%s: UPDATED FLOWS\nLOW_PRIORITY_FLOW: %s\nHIGH_PRIORITY_FLOW: %s\n", MODNAME, the_object->low_priority_flow, the_object->high_priority_flow);
   mutex_unlock(&(the_object->mutex_hi));
   wake_up(&(the_object->high_prio_queue));
   return len - ret;
}

static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off)
{
   // Synchronous for both priorities
   // Blocking mode = waits for lock to go on, Non-blocking mode = doesn't wait for lock if busy and returns
   int minor = get_minor(filp);
   int ret;
   int ok;
   object_state *the_object = objects + minor;
   int highPriority = the_object->priorityMode;

   printk("%s: READ CALLED ON [MAJ-%d,MIN-%d]\n", MODNAME, get_major(filp), get_minor(filp));
   printk("%s: \nPriority mode = %d\nBlocking mode = %d\nValid bytes (low priority stream) = %d\nValid bytes (high priority stream) = %d\n", MODNAME, highPriority, the_object->blockingModeOn, the_object->valid_bytes_lo, the_object->valid_bytes_hi);

   // Blocking mode
   if (the_object->blockingModeOn)
   {
      if (highPriority)
      {
         __atomic_fetch_add(&high_waiting[minor], 1, __ATOMIC_SEQ_CST);
         ok = wait_event_timeout(the_object->high_prio_queue, mutex_trylock(&(the_object->mutex_hi)), the_object->awake_timeout);
         __atomic_fetch_sub(&high_waiting[minor], 1, __ATOMIC_SEQ_CST);
         if (!ok)
         {
            printk("%s: REQUEST TIMEOUT - the requested read (%ld bytes) on minor %d will not be performed.\n", MODNAME, len, minor);
            return -1;
         }
      }
      else
      {
         __atomic_fetch_add(&low_waiting[minor], 1, __ATOMIC_SEQ_CST);
         ok = wait_event_timeout(the_object->low_prio_queue, mutex_trylock(&(the_object->mutex_lo)), the_object->awake_timeout);
         __atomic_fetch_sub(&low_waiting[minor], 1, __ATOMIC_SEQ_CST);
         if (!ok)
         {
            printk("%s: REQUEST TIMEOUT - the requested read (%ld bytes) on minor %d will not be performed.\n", MODNAME, len, minor);
            return -1;
         }
      }

   } // Non-blocking mode
   else
   {
      if (highPriority)
      {
         ok = mutex_trylock(&(the_object->mutex_hi));
         if (ok == EBUSY)
         {
            printk("%s: RESOURCE BUSY - the requested read (%ld bytes) on minor %d will not be performed.\n", MODNAME, len, minor);
            return -1;
         }
      }
      else
      {
         ok = mutex_trylock(&(the_object->mutex_lo));
         if (ok == EBUSY)
         {
            printk("%s: RESOURCE BUSY - the requested read (%ld bytes) on minor %d will not be performed.\n", MODNAME, len, minor);
            return -1;
         }
      }
   }
   //Lock acquired

   // if offset beyond the current stream size
   if ((!highPriority && *off > the_object->valid_bytes_lo))
   {
      mutex_unlock(&(the_object->mutex_lo));
      wake_up(&(the_object->low_prio_queue));
      return 0;
   }
   else if ((highPriority && *off > the_object->valid_bytes_hi))
   {
      mutex_unlock(&(the_object->mutex_hi));
      wake_up(&(the_object->high_prio_queue));
      return 0;
   }
   
   if ((!highPriority && the_object->valid_bytes_lo - *off < len))
      len = the_object->valid_bytes_lo - *off;
   else if ((highPriority && the_object->valid_bytes_hi - *off < len))
      len = the_object->valid_bytes_hi - *off;

   if (highPriority)
   {
      ret = copy_to_user(buff, &(the_object->high_priority_flow[*off]), len);
      the_object->high_priority_flow += len;
      the_object->valid_bytes_hi -= len;
      high_bytes[minor] = the_object->valid_bytes_hi;

      printk("%s: READ OPERATION COMPLETED - unread bytes = %d\n",MODNAME,ret);
      printk("%s: UPDATED FLOWS\nLOW_PRIORITY_FLOW: %s\nHIGH_PRIORITY_FLOW: %s\n", MODNAME, the_object->low_priority_flow, the_object->high_priority_flow);
      mutex_unlock(&(the_object->mutex_hi));
      wake_up(&(the_object->high_prio_queue));
   }
   else
   {
      ret = copy_to_user(buff, &(the_object->low_priority_flow[*off]), len);
      the_object->low_priority_flow += len;
      the_object->valid_bytes_lo -= len;
      low_bytes[minor] = the_object->valid_bytes_lo;

      printk("%s: READ OPERATION COMPLETED - unread bytes = %d\n",MODNAME,ret);
      printk("%s: UPDATED FLOWS\nLOW_PRIORITY_FLOW: %s\nHIGH_PRIORITY_FLOW: %s\n", MODNAME, the_object->low_priority_flow, the_object->high_priority_flow);
      mutex_unlock(&(the_object->mutex_lo));
      wake_up(&(the_object->low_prio_queue));
   }

   return len - ret;
}

static long dev_ioctl(struct file *filp, unsigned int command, unsigned long param)
{
   int minor = get_minor(filp);
   object_state *the_object;
   the_object = objects + minor;

   printk("%s: IOCTL CALLED ON [MAJ-%d,MIN-%d] - command = %u \n", MODNAME, get_major(filp), get_minor(filp), command);

   // Device state control

   switch (command)
   {
      case 0:
         the_object->priorityMode = 0;
         break;
      case 1:
         the_object->priorityMode = 1;
         break;
      case 2:
         the_object->blockingModeOn = 0;
         break;
      case 3:
         the_object->blockingModeOn = 1;
         break;
      case 4:
         the_object->awake_timeout = param*HZ;
         break;
      case 5:
         if(devices_state[minor] == 1)
         {
            devices_state[minor] = 0;
         }
         else
         {
            devices_state[minor] = 1;
         }
         break;
      default:
         printk("%s: IOCTL ERROR - unhandled command code (%d)\n",MODNAME,command);
  }
   return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = dev_write,
    .read = dev_read,
    .open = dev_open,
    .release = dev_release,
    .unlocked_ioctl = dev_ioctl};

int init_module(void)
{
   int i;
   // Driver internal state initialization
   for (i = 0; i < MINORS; i++)
   {
      mutex_init(&(objects[i].mutex_hi));
      mutex_init(&(objects[i].mutex_lo));
      init_waitqueue_head(&(objects[i].high_prio_queue));
      init_waitqueue_head(&(objects[i].low_prio_queue));

      objects[i].awake_timeout = DEFAULT_BLOCKING_OPS_TIMEOUT*HZ;
      objects[i].blockingModeOn = DEFAULT_BLOCKING_MODE;
      objects[i].priorityMode = DEFAULT_PRIORITY_MODE;

      devices_state[i] = 1;
      objects[i].isEnabled = &devices_state[i];

      objects[i].valid_bytes_hi = 0;
      objects[i].valid_bytes_lo = 0;
      objects[i].low_priority_flow = NULL;
      objects[i].low_priority_flow = (char *)__get_free_page(GFP_KERNEL);
      objects[i].high_priority_flow = NULL;
      objects[i].high_priority_flow = (char *)__get_free_page(GFP_KERNEL);
      if (objects[i].low_priority_flow == NULL || objects[i].high_priority_flow == NULL)
         goto revert_allocation;

      low_bytes[i] = objects[i].valid_bytes_lo;
      high_bytes[i] = objects[i].valid_bytes_hi;

      low_waiting[i] = 0;
      high_waiting[i] = 0;
   }

   Major = __register_chrdev(0, 0, 128, DEVICE_NAME, &fops);

   if (Major < 0)
   {
      printk("%s: ERROR - device registration failed\n", MODNAME);
      return Major;
   }
   printk(KERN_INFO "%s: DEVICE REGISTERED - Assigned MAJOR = %d\n", MODNAME, Major);
   return 0;

revert_allocation:
   for (; i >= 0; i--)
   {
      free_page((unsigned long)objects[i].low_priority_flow);
      free_page((unsigned long)objects[i].high_priority_flow);
   }
   printk("%s: ERROR - Requested memory is not available\n", MODNAME);
   return -ENOMEM;
}

void cleanup_module(void)
{
   int i;
   for (i = 0; i < MINORS; i++)
   {
      free_page((unsigned long)objects[i].low_priority_flow);
      free_page((unsigned long)objects[i].high_priority_flow);
   }
   unregister_chrdev(Major, DEVICE_NAME);
   printk(KERN_INFO "%s: DEVICE WITH MAJOR = %d WAS SUCCESSFULLY UNREGISTERED\n", MODNAME, Major);
   return;
}
