#define EXPORT_SYMTAB
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>	
#include <linux/pid.h>
#include <linux/tty.h>
#include <linux/version.h>
#include <linux/spinlock.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Matteo Ferretti");

#define MODNAME "MULTI-FLOW-DEVICE"
#define DEVICE_NAME "mfdev"

#define get_major(session)	MAJOR(session->f_inode->i_rdev)
#define get_minor(session)	MINOR(session->f_inode->i_rdev)

#define OBJECT_MAX_SIZE  (4096)

typedef struct _object_state
{
   struct mutex mutex;
	int prio;   //0 for low priority, 1 for high priority
   int opMode; //0 for non-blocking rw ops, 1 for blocking rw ops
   unsigned long awake_timeout; //timeout regulating the awake of blocking operations
   int valid_bytes;
   char * low_priority_flow;
   char * high_priority_flow;
   long long int next_offset; 
} object_state;

static int Major;

#define MINORS 128
object_state objects[MINORS];

/* the actual driver */

static int dev_open(struct inode *inode, struct file *file)
{
   int minor = get_minor(file);
   if(minor >= MINORS) return -ENODEV;
   printk("%s: device file successfully opened for object with minor %d\n",MODNAME,minor);
   return 0;
}

static int dev_release(struct inode *inode, struct file *file)
{
   //int minor = get_minor(file);
   printk("%s: device file closed\n",MODNAME);
   //device closed by default nop
   return 0;
}

static ssize_t dev_write(struct file *filp, const char *buff, size_t len, loff_t *off)
{
   int minor = get_minor(filp);
   int ret;
   object_state *the_object = objects + minor;
   int prio = the_object->prio;
   printk("%s: somebody called a write on dev with [major,minor] number [%d,%d]\n",MODNAME,get_major(filp),get_minor(filp));
   //need to lock in any case
   if(the_object->opMode == 0)
      mutex_trylock(&(the_object->mutex));  
   else if(the_object->opMode == 1)
      mutex_lock(&(the_object->mutex));
   printk("%s: INITIAL VALUES\nprio=%d\n*off=%lld\nthe_object->next_offset=%lld\n",MODNAME,prio,*off,the_object->next_offset);
   *off += the_object->next_offset;
   printk("%s: OFFSET AFTER APPLYING NEXT OFFSET: %lld\n",MODNAME,*off);
   //if offset too large
   if(*off >= OBJECT_MAX_SIZE) 
   {
      mutex_unlock(&(the_object->mutex));
      printk("---ERROR: NO SPACE LEFT ON DEVICE\n");
	   return -ENOSPC;//no space left on device
   }
   //if offset beyond the current stream size
   if(*off > the_object->valid_bytes) {
      mutex_unlock(&(the_object->mutex));
      printk("---ERROR: OUT OF STREAM RESOURCES\n");
      //printk("VALUES:\nprio=%d\n*off=%lld\nvalid_bytes=%d\n",prio,*off,the_object->valid_bytes);
	   return -ENOSR;//out of stream resources
   }
   
   if((OBJECT_MAX_SIZE - *off) < len)
   {
      len = OBJECT_MAX_SIZE - *off;
   } 
      

   
   if(prio == 0)
      ret = copy_from_user(&(the_object->low_priority_flow[*off]),buff,len);
   else if(prio == 1)
      ret = copy_from_user(&(the_object->high_priority_flow[*off]),buff,len);
   *off += (len - ret);
   the_object->valid_bytes = *off;

   the_object->next_offset = *off;

   mutex_unlock(&(the_object->mutex));

   printk("%s: FLOWS BEFORE RETURNING\nLOWPRIOFLOW: %s\nHIGHPRIOFLOW: %s\n",MODNAME, the_object->low_priority_flow, the_object->high_priority_flow);
   
   printk("%s: VALS BEFORE RETURNING:\nOffset is : %lld\nthe_object->next_offset is %lld\n",MODNAME,*off, the_object->next_offset);
   return len - ret;
}

static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off)
{
   int minor = get_minor(filp);
   int ret;
   object_state *the_object = objects + minor;;
   int prio = the_object->prio;
   
   printk("%s: somebody called a read on dev with [major,minor] number [%d,%d]\n",MODNAME,get_major(filp),get_minor(filp));

   //need to lock in any case
   if(the_object->opMode == 0)
      mutex_trylock(&(the_object->mutex));  
   else if(the_object->opMode == 1)
      mutex_lock(&(the_object->mutex));
   if(*off > the_object->valid_bytes)
   {
      mutex_unlock(&(the_object->mutex));
	   return 0;
   } 
   if(the_object->valid_bytes - *off < len) 
      len = the_object->valid_bytes - *off;
   
   if(prio==0)
      ret = copy_to_user(buff,&(the_object->low_priority_flow[*off]),len);
   else if(prio==1)
      ret = copy_to_user(buff,&(the_object->high_priority_flow[*off]),len);
   *off += (len - ret);
   mutex_unlock(&(the_object->mutex));

   return len - ret;
}

static long dev_ioctl(struct file *filp, unsigned int command, unsigned long param)
{
   int minor = get_minor(filp);
   object_state *the_object;

   the_object = objects + minor;
   printk("%s: somebody called an ioctl on dev with [major,minor] number [%d,%d] and command %u \n",MODNAME,get_major(filp),get_minor(filp),command);
   
   //do here whathever you would like to control the state of the device
   if(command == 0)
      the_object->prio = 0;
   else if(command == 1)
      the_object->prio = 1;
   else if(command == 2)
      the_object->opMode = 0;
   else if(command == 3)
      the_object->opMode = 1;
   else if (command == 4)
      the_object->awake_timeout = param;
   
   return 0;
}

static struct file_operations fops = {
  .owner = THIS_MODULE,//do not forget this
  .write = dev_write,
  .read = dev_read,
  .open =  dev_open,
  .release = dev_release,
  .unlocked_ioctl = dev_ioctl
};

int init_module(void)
{
   int i;
	//initialize the drive internal state
	for(i=0;i<MINORS;i++)
   {
		mutex_init(&(objects[i].mutex));
      objects[i].next_offset = 0;
      objects[i].awake_timeout=500;
      objects[i].opMode=0;
      objects[i].prio=0;
		objects[i].valid_bytes = 0;
      objects[i].low_priority_flow = NULL;
		objects[i].low_priority_flow = (char*)__get_free_page(GFP_KERNEL);
      objects[i].high_priority_flow = NULL;
		objects[i].high_priority_flow = (char*)__get_free_page(GFP_KERNEL);
		if(objects[i].low_priority_flow == NULL || objects[i].high_priority_flow == NULL) goto revert_allocation;
	}

	Major = __register_chrdev(0, 0, 128, DEVICE_NAME, &fops);
	//actually allowed minors are directly controlled within this driver

	if (Major < 0) 
   {
	  printk("%s: registering device failed\n",MODNAME);
	  return Major;
	}
	printk(KERN_INFO "%s: new device registered, it is assigned major number %d\n",MODNAME, Major);
	return 0;

revert_allocation:
	for(;i>=0;i--)
   {
		free_page((unsigned long)objects[i].low_priority_flow);
      free_page((unsigned long)objects[i].high_priority_flow);
	}
	return -ENOMEM;
}

void cleanup_module(void)
{
	int i;
	for(i=0;i<MINORS;i++)
   {
		free_page((unsigned long)objects[i].low_priority_flow);
		free_page((unsigned long)objects[i].high_priority_flow);
	}
	unregister_chrdev(Major, DEVICE_NAME);
	printk(KERN_INFO "%s: new device unregistered, it was assigned major number %d\n",MODNAME, Major);
	return;
}
