/*
* @file multi-flow-device.c 
* @brief This is a linux kernel module developed for academic purpose. Using the module allows threads to
*        read and write data segments from high and low priority flows of device files.
*
* @author Matteo Ferretti
*
* @date March 1, 2022
*/

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

//Some helpful predefined strings
#define MODNAME "MULTI-FLOW-DEVICE"
#define DEVICE_NAME "mfdev"
#define LOW_PRIORITY "LOW"
#define HIGH_PRIORITY "HIGH"
#define EMPTY_BUFF "[empty]"

#define get_major(session) MAJOR(session->f_inode->i_rdev)  //MAJOR number
#define get_minor(session) MINOR(session->f_inode->i_rdev)  //MINOR number

#define DEFAULT_BLOCKING_OPS_TIMEOUT 20 //Default timeout (in microseconds) for blocking operations on a device
#define DEFAULT_BLOCKING_MODE 0  //Default blocking mode of RW operations for a device (0 = non-blocking - 1 = blocking)
#define DEFAULT_PRIORITY_MODE 0  //Default priority mode for a device (0 = low priority usage - 1 = high priority usage)

//Some useful pre-defined macros
#define ALLOCATION_FAILED(x) (x==NULL)
#define NOT(x) (x==0)
#define ASSIGN_ADDRESS(data,data_low,data_high,prio) if(NOT(prio)){data=&data_low;}else{data=&data_high;}
#define SELECT_EQUAL(data,data_low,data_high,prio) if(NOT(prio)){data=data_low;}else{data=data_high;}

typedef struct _object_state  // This struct represents a single device
{
   struct mutex mutex_hi;  // synchronization utility for high priority flows
   struct mutex mutex_lo;  // synchronization utility for low priority flows
   int *isEnabled;                    // pointer to module param representing if a device is enabled (openable sessions)
   int valid_bytes_lo;                // written bytes present in the low priority flow
   int valid_bytes_hi;                // written bytes present in the high priority flow
   char *low_priority_flow;           // low priority data stream
   char *high_priority_flow;          // high priority data stream
   wait_queue_head_t high_prio_queue; // wait event queue for threads waiting to perform actions on high priority streams
   wait_queue_head_t low_prio_queue;  // wait event queue for threads waiting to perform actions on low priority streams

} object_state;

typedef struct session  //This struct represents an opened session to a device
{
   int priorityMode;                  // 0 = low priority usage, 1 = high priority usage
   int blockingModeOn;                // 0 = non-blocking RW ops, 1 = blocking RW ops
   unsigned long awake_timeout;       // timeout regulating the awake of blocking operations
} session;

typedef struct _packed_work // Work structure to write on devices in deferred mode
{
   struct file *filp;   //pointer to session
   char *buff; //data string
   size_t len; //data size variable
   long long int off;   //session offset
   struct work_struct work;   //defined work for work queue
} packed_work;

static int Major;

#define MINORS 128   //Support up to 128 minors
object_state objects[MINORS];

//Arrays used for module parameters
int devices_state[MINORS];
int high_bytes[MINORS];
int low_bytes[MINORS];
int high_waiting[MINORS];
int low_waiting[MINORS];

module_param_array(devices_state,int,NULL,S_IWUSR|S_IRUGO); //Module parameter to expose devices state (0 = disabled - 1 = enabled)
module_param_array(high_bytes,int,NULL,S_IRUGO);   //Module parameter describing how many valid bytes are present in every high priority flow
module_param_array(low_bytes,int,NULL,S_IRUGO); //Module parameter describing how many valid bytes are present in every low priority flow
module_param_array(high_waiting, int,NULL,S_IRUGO);   //Module parameter representing how many threads are waiting on high priority stream for every device
module_param_array(low_waiting, int,NULL,S_IRUGO); //Module parameter representing how many threads are waiting on low priority stream for every device

MODULE_PARM_DESC(devices_state, "Array of devices states (0 = disabled - 1 = enabled)");
MODULE_PARM_DESC(high_bytes, "Array reporting the number of current valid bytes in the high priority stream of every device.");
MODULE_PARM_DESC(low_bytes, "Array reporting the number of current valid bytes in the low priority stream of every device.");
MODULE_PARM_DESC(high_waiting, "Array describing the number of threads waiting on the high priority stream of every device.");
MODULE_PARM_DESC(low_waiting, "Array describing the number of threads waiting on the low priority stream of every device.");

void print_streams(char *low_stream,char *high_stream,int low_bytes, int high_bytes)   //Streams printing function
{
   char * l;
   char * h;
   if(low_bytes == 0)
      l=EMPTY_BUFF;
   else
      l=low_stream;
   if(high_bytes == 0)
      h=EMPTY_BUFF;
   else
      h=high_stream;

   printk("%s: --------------------\nFLOWS UPDATED:\nLOW_PRIORITY_FLOW: %s\nHIGH_PRIORITY_FLOW: %s\n---------------------------------------\n", MODNAME,l,h);
}

void do_write(ssize_t len, const char *buff, int *valid_bytes, char** flow, int minor, char* prioMode)   //write logic function
{
   *flow = krealloc(*flow,*valid_bytes+len,GFP_KERNEL);  //allocate new space for the write
   memset(*flow + *valid_bytes,0,len); //initialize the new memory to a clean state
   strncat(*flow,buff,len);   //attach the new string to the flow
   *valid_bytes += len; //Increment the valid bytes quantity in the object
   if(strcmp(prioMode,LOW_PRIORITY) == 0) //update kernel params
      low_bytes[minor] = *valid_bytes;
   else
      high_bytes[minor] = *valid_bytes;
   printk("%s: ||| WRITE OPERATION COMPLETED. |||\n",MODNAME);
}
int do_read(int len, int ret, char **buff, char **selected_flow, int *selected_valid_bytes, int minor, char *selected_prio)   //read logic function
{
   int delivered_bytes;
  
   //If the read request size is bigger than valid bytes present only read how many bytes possible
   if (*selected_valid_bytes < len)
      len = *selected_valid_bytes;

   // In order to perform a read the sequence is: copy to user, move the remaining string to the beginning of the stream, clean the final part.
   ret = copy_to_user(*buff, selected_flow[0], len);  //copy the information to user space
   if (ret!=0)
      printk("%s: The read was partial due to a problem.\n",MODNAME);
   delivered_bytes = len-ret;
   memmove(*selected_flow, *selected_flow + delivered_bytes, *selected_valid_bytes - delivered_bytes);   //copy the remaining unread flow to the beginning
   memset(*selected_flow + *selected_valid_bytes - delivered_bytes,0,delivered_bytes); //clean the final redundant part
   if(delivered_bytes != 0)   //do not reallocate memory size if no bytes were actually delivered
      *selected_flow = krealloc(*selected_flow,*selected_valid_bytes-delivered_bytes,GFP_KERNEL);  //resize the memory in order to deallocate the final part
   *selected_valid_bytes -= delivered_bytes; //update valid bytes
   if(strcmp(selected_prio,LOW_PRIORITY) == 0)  //update module params
      low_bytes[minor] = *selected_valid_bytes;
   else
      high_bytes[minor] = *selected_valid_bytes;
   printk("%s: ||| READ OPERATION COMPLETED. |||\n",MODNAME);
   return delivered_bytes;
}

void work_function(struct work_struct *work) // Implementation of deferred work
{
   int minor;
   packed_work *info;
   object_state *the_object;
   session *s;

   printk("%s: [KWORKER DAEMON RUNNING - PID = %d - CPU-core = %d]\n",MODNAME,current->pid,smp_processor_id());

   info = container_of(work, packed_work, work);   //retrieve the info struct in order to use all the prepared informations
   minor = get_minor(info->filp);
   the_object = objects + minor;
   s = (info->filp)->private_data;
   
   // Actual logic: only low priority operations possible here
   mutex_lock(&(the_object->mutex_lo));
   do_write(info->len,info->buff, &the_object->valid_bytes_lo, &the_object->low_priority_flow, minor,LOW_PRIORITY);  //Perform the write operation
   print_streams(the_object->low_priority_flow,the_object->high_priority_flow,the_object->valid_bytes_lo, the_object->valid_bytes_hi); //Print results
   mutex_unlock(&(the_object->mutex_lo));
   wake_up(&(the_object->low_prio_queue));
   free_page((unsigned long)info->buff);
   kfree(info);
}

void prepare_deferred_work(struct file *filp, size_t len, char** temp_buff, int ret, packed_work *info)  //Function used to prepare work structs before queuing the work
{
   info->filp = filp;   //struct preparation
   info->len = len;
   info->buff = (char *)__get_free_page(GFP_KERNEL);
   strncpy(info->buff,*temp_buff,info->len);
   info->len -= ret;//if some bytes were not written decrease the len to read in order to avoid memory sizing problems
   
   __INIT_WORK(&(info->work), work_function, (unsigned long)(&(info->work))); //Init the work before queuing it
   schedule_work(&(info->work)); //Queue the work
   kfree(*temp_buff);
}

/*
      DRIVER
*/

static int dev_open(struct inode *inode, struct file *file) //What to do when a session to an object is opened
{
   int minor = get_minor(file);
   file -> private_data = kzalloc(sizeof(session),GFP_ATOMIC); //Allocate a session object to store related informations
   ((session*)(file -> private_data)) -> awake_timeout = DEFAULT_BLOCKING_OPS_TIMEOUT*((HZ)/1000000);//HZ = 250 increments every second
   ((session*)(file -> private_data)) -> blockingModeOn = DEFAULT_BLOCKING_MODE;
   ((session*)(file -> private_data)) -> priorityMode = DEFAULT_PRIORITY_MODE; 
   if (minor >= MINORS) //Error handling
   {
      printk("%s: ERROR - minor number %d out of handled range.\n", MODNAME, minor);
      return -ENODEV;
   }
   if(devices_state[minor] == 1)
   {
      printk("%s: OPENED DEVICE FILE WITH MINOR %d\n", MODNAME, minor);
      return 0;
   }
   else
   {
      printk("%s: ERROR - requested device with minor number %d is currently disabled.\n", MODNAME, minor);
      return -EACCES;
   }
}

static int dev_release(struct inode *inode, struct file *file) //What to do when a session is closed
{
   int minor = get_minor(file);
   kfree(file->private_data); //deallocate the session
   printk("%s: CLOSED DEVICE FILE WITH MINOR %d\n", MODNAME, minor);
   return 0;
}

static ssize_t dev_write(struct file *filp, const char *buff, size_t len, loff_t *off) //device driver write operation
{
   // Synchronous for high and asynchronous (deferred work) for low priority
   // Blocking mode = waits for lock to go on, Non-blocking mode = doesn't wait for lock if busy and returns
   int ret;
   int written_bytes;
   int lock_taken;
   packed_work *info;
   int minor = get_minor(filp);
   object_state *the_object = objects + minor;
   session *s = filp->private_data;
   int highPriority = s->priorityMode;
   int blocking = s->blockingModeOn;
   
   char *temp_buff = kzalloc(sizeof(char)*len, GFP_ATOMIC);//Prepare a buffer to use to avoid sleeping in mutex
   if (ALLOCATION_FAILED(temp_buff))
   {
      printk("%s: ERROR - temporary structure allocation failed.\n", MODNAME);
      return -EINVAL;   //Could not allocate temporary buffer to perform the write operation
   }
   if(NOT(highPriority))
   {
      info = kzalloc(sizeof(packed_work), GFP_ATOMIC);
      if (ALLOCATION_FAILED(info))
      {
         printk("%s: ERROR - deferred work structure allocation failed.\n", MODNAME);
         kfree(temp_buff);
         return -EINVAL;   //Could not allocate temporary buffer to perform the write operation
      }
   }
   ret = copy_from_user(temp_buff, buff, len);  //Write in a temporary buffer to avoid blocked kernel threads
   written_bytes = len-ret;
   printk("%s: WRITE CALLED ON [MAJ-%d,MIN-%d]\n", MODNAME, get_major(filp), get_minor(filp));
   if (ret!=0)
      printk("%s: The write was partial due to a problem.\n",MODNAME);
   // Lock acquisition phase
   
   //Low priority
   if(NOT(highPriority))   
   {
      // Deferred work
      prepare_deferred_work(filp, len, &temp_buff, ret, info);
      return written_bytes;   //Deferred work should not fail
   }

   //High priority
   if (blocking)  //Blocking mode
   {
      __atomic_fetch_add(&high_waiting[minor], 1, __ATOMIC_SEQ_CST);
      lock_taken = wait_event_timeout(the_object->high_prio_queue, mutex_trylock(&(the_object->mutex_hi)), s->awake_timeout);
      __atomic_fetch_sub(&high_waiting[minor], 1, __ATOMIC_SEQ_CST);
      if (NOT(lock_taken))
      {
         printk("%s: REQUEST TIMEOUT - the requested write ('%s') on minor %d will not be performed.\n", MODNAME, buff, minor);
         kfree(temp_buff);
         return 0;   //No bytes have been written on fd
      }
   }
   else  // Non-blocking mode
   {
      lock_taken = mutex_trylock(&(the_object->mutex_hi));
      if (NOT(lock_taken))
      {
         printk("%s: RESOURCE BUSY - the requested write ('%s') on minor %d will not be performed.\n", MODNAME, buff, minor);
         kfree(temp_buff);
         return 0;   //No bytes have been written on fd
      }
   }

   // Only high priority operations possible here
   do_write(len,buff, &the_object->valid_bytes_hi, &the_object->high_priority_flow, minor,HIGH_PRIORITY);   //----The actual write operation
   print_streams(the_object->low_priority_flow,the_object->high_priority_flow,the_object->valid_bytes_lo, the_object->valid_bytes_hi);
   mutex_unlock(&(the_object->mutex_hi));
   wake_up(&(the_object->high_prio_queue));

   kfree(temp_buff);
   return written_bytes;
}

static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off)  //device driver read operation
{
   // Synchronous for both priorities
   // Blocking mode = waits for lock to go on, Non-blocking mode = doesn't wait for lock if busy and returns
   int *selected_waiting_array;
   int *selected_valid_bytes;
   char *selected_flow;
   char *selected_prio;
   struct mutex *selected_mutex;
   wait_queue_head_t *selected_wait_queue;
   int delivered_bytes;
   int lock_taken;
   int ret;
   int minor = get_minor(filp);
   object_state *the_object = objects + minor;
   session *s = filp->private_data;
   int highPriority = s->priorityMode;
   int blocking = s->blockingModeOn;
   printk("%s: READ CALLED ON [MAJ-%d,MIN-%d]\n", MODNAME, get_major(filp), get_minor(filp));
 
   //Assign each variable depending on priority mode
   SELECT_EQUAL(selected_waiting_array,low_waiting,high_waiting,highPriority);
   SELECT_EQUAL(selected_prio,LOW_PRIORITY,HIGH_PRIORITY,highPriority);
   SELECT_EQUAL(selected_flow,the_object->low_priority_flow,the_object->high_priority_flow,highPriority);
   ASSIGN_ADDRESS(selected_mutex,the_object->mutex_lo,the_object->mutex_hi,highPriority);
   ASSIGN_ADDRESS(selected_valid_bytes,the_object->valid_bytes_lo,the_object->valid_bytes_hi,highPriority);
   ASSIGN_ADDRESS(selected_wait_queue,the_object->low_prio_queue,the_object->high_prio_queue,highPriority);
   
   // Lock acquisition phase
   
   if (blocking)  // Blocking mode
   {
      __atomic_fetch_add(&selected_waiting_array[minor], 1, __ATOMIC_SEQ_CST);
      lock_taken=wait_event_timeout(*(selected_wait_queue), mutex_trylock(selected_mutex), s->awake_timeout);
      __atomic_fetch_sub(&selected_waiting_array[minor], 1, __ATOMIC_SEQ_CST);
      if (NOT(lock_taken))
      {
         printk("%s: REQUEST TIMEOUT - the requested read (%ld bytes) on minor %d will not be performed.\n", MODNAME, len, minor);
         return 0; //No bytes have been read from fd
      }
   } 
   else  // Non-blocking mode
   {
         lock_taken = mutex_trylock((selected_mutex));
         if (NOT(lock_taken))
         {
            printk("%s: RESOURCE BUSY - the requested read (%ld bytes) on minor %d will not be performed.\n", MODNAME, len, minor);
            return 0;   //No bytes have been read from fd
         }
   }
   //Lock acquired
   delivered_bytes=do_read(len, ret, &buff, &selected_flow, selected_valid_bytes,minor,selected_prio);   //----The actual read operation
   print_streams(the_object->low_priority_flow,the_object->high_priority_flow,the_object->valid_bytes_lo, the_object->valid_bytes_hi);
   mutex_unlock(selected_mutex);
   wake_up(selected_wait_queue);
   return delivered_bytes;
}

static long dev_ioctl(struct file *filp, unsigned int command, unsigned long param) //ioctl function: codes exposed
{
   int minor;
   object_state *the_object;
   session *s;
   minor = get_minor(filp);
   the_object = objects + minor;
   s = filp->private_data;

   printk("%s: IOCTL CALLED ON [MAJ-%d,MIN-%d] - command = %u \n", MODNAME, get_major(filp), get_minor(filp), command);

   // Device state control

   switch (command)
   {
      case 0:  //set the priority mode to low
         s->priorityMode = 0;
         break;
      case 1:  //set the priority mode to high
         s->priorityMode = 1;
         break;
      case 6:  //set operational mode to non-blocking
         s->blockingModeOn = 0;
         break;
      case 3:  //set operational mode to blocking
         s->blockingModeOn = 1;
         break;
      case 4:  //set the awake timeout for blocking operations
         s->awake_timeout = param*((HZ)/1000000);//HZ = 250 increments every second
         break;
      case 5:  //enable or disable a device
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

int init_module(void)   //What to do when the module is loaded
{
   int i;
   // Driver internal state initialization
   for (i = 0; i < MINORS; i++)
   {
      mutex_init(&(objects[i].mutex_hi));
      mutex_init(&(objects[i].mutex_lo));
      init_waitqueue_head(&(objects[i].high_prio_queue));
      init_waitqueue_head(&(objects[i].low_prio_queue));

      devices_state[i] = 1;

      objects[i].isEnabled = &devices_state[i];
      objects[i].valid_bytes_hi = 0;
      objects[i].valid_bytes_lo = 0;
      objects[i].low_priority_flow = NULL;
      objects[i].low_priority_flow = kzalloc(0,GFP_ATOMIC); //The stream allocation will be dynamic, and operated on demand. Empty-initialized
      objects[i].high_priority_flow = NULL;
      objects[i].high_priority_flow = kzalloc(0,GFP_ATOMIC);

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
   printk("%s: DEVICE REGISTERED - Assigned MAJOR = %d\n", MODNAME, Major);
   return 0;
}

void cleanup_module(void)  //What to do when the module is removed
{
   int i;
   for (i = 0; i < MINORS; i++)
   {
      kfree(objects[i].low_priority_flow);   //deallocate flows
      kfree(objects[i].high_priority_flow);
   }
   unregister_chrdev(Major, DEVICE_NAME);
   printk("%s: DEVICE WITH MAJOR = %d WAS SUCCESSFULLY UNREGISTERED\n", MODNAME, Major);
   return;
} 

