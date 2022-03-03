
/*  
 *  baseline char device driver with limitation on minor numbers - configurable in terms of concurrency 
 */

#define EXPORT_SYMTAB
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/sched.h>	
#include <linux/pid.h>		/* For pid types */
#include <linux/tty.h>		/* For the tty declarations */
#include <linux/version.h>	/* For LINUX_VERSION_CODE */


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Francesco Quaglia");

#define MODNAME "CHAR DEV"



static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

#define DEVICE_NAME "my-new-dev"  /* Device file name in /dev/ - not mandatory  */

//#define SINGLE_INSTANCE //just one session at a time across all I/O node 
#define SINGLE_SESSION_OBJECT //just one session per I/O node at a time


static int Major;            /* Major number assigned to broadcast device driver */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define get_major(session)	MAJOR(session->f_inode->i_rdev)
#define get_minor(session)	MINOR(session->f_inode->i_rdev)
#else
#define get_major(session)	MAJOR(session->f_dentry->d_inode->i_rdev)
#define get_minor(session)	MINOR(session->f_dentry->d_inode->i_rdev)
#endif

#ifdef SINGLE_INSTANCE
static DEFINE_MUTEX(device_state);
#endif


typedef struct _object_state{
#ifdef SINGLE_SESSION_OBJECT
	struct mutex object_busy;
#endif
	struct mutex operation_synchronizer;
	int valid_bytes;
	char * stream_content;//the I/O node is a buffer in memory

} object_state;

#define MINORS 8
object_state objects[MINORS];

#define OBJECT_MAX_SIZE  (4096) //just one page

/* the actual driver */

static int dev_open(struct inode *inode, struct file *file) {

   int minor;
   minor = get_minor(file);

   if(minor >= MINORS){
	return -ENODEV;
   }
#ifdef SINGLE_INSTANCE
// this device file is single instance
   if (!mutex_trylock(&device_state)) {
		return -EBUSY;
   }
#endif

#ifdef SINGLE_SESSION_OBJECT
   if (!mutex_trylock(&(objects[minor].object_busy))) {
		goto open_failure;
   }
#endif

   printk("%s: device file successfully opened for object with minor %d\n",MODNAME,minor);
//device opened by a default nop
   return 0;


#ifdef SINGLE_SESSION_OBJECT
open_failure:
#ifdef SINGE_INSTANCE
   mutex_unlock(&device_state);
#endif
   return -EBUSY;
#endif

}


static int dev_release(struct inode *inode, struct file *file) {

  int minor;
  minor = get_minor(file);

#ifdef SINGLE_SESSION_OBJECT
   mutex_unlock(&(objects[minor].object_busy));
#endif

#ifdef SINGLE_INSTANCE
   mutex_unlock(&device_state);
#endif

   printk("%s: device file closed\n",MODNAME);
//device closed by default nop
   return 0;

}



static ssize_t dev_write(struct file *filp, const char *buff, size_t len, loff_t *off) {

  int minor = get_minor(filp);
  int ret;
  object_state *the_object;

  the_object = objects + minor;
  printk("%s: somebody called a write on dev with [major,minor] number [%d,%d]\n",MODNAME,get_major(filp),get_minor(filp));

  //need to lock in any case
  mutex_lock(&(the_object->operation_synchronizer));
  if(*off >= OBJECT_MAX_SIZE) {//offset too large
 	 mutex_unlock(&(the_object->operation_synchronizer));
	 return -ENOSPC;//no space left on device
  } 
  if(*off > the_object->valid_bytes) {//offset bwyond the current stream size
 	 mutex_unlock(&(the_object->operation_synchronizer));
	 return -ENOSR;//out of stream resources
  } 
  if((OBJECT_MAX_SIZE - *off) < len) len = OBJECT_MAX_SIZE - *off;
  ret = copy_from_user(&(the_object->stream_content[*off]),buff,len);
  
  *off += (len - ret);
  the_object->valid_bytes = *off;
  mutex_unlock(&(the_object->operation_synchronizer));

  return len - ret;

}

static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off) {

  int minor = get_minor(filp);
  int ret;
  object_state *the_object;

  the_object = objects + minor;
  printk("%s: somebody called a read on dev with [major,minor] number [%d,%d]\n",MODNAME,get_major(filp),get_minor(filp));

  //need to lock in any case
  mutex_lock(&(the_object->operation_synchronizer));
  if(*off > the_object->valid_bytes) {
 	 mutex_unlock(&(the_object->operation_synchronizer));
	 return 0;
  } 
  if((the_object->valid_bytes - *off) < len) len = the_object->valid_bytes - *off;
  ret = copy_to_user(buff,&(the_object->stream_content[*off]),len);
  
  *off += (len - ret);
  mutex_unlock(&(the_object->operation_synchronizer));

  return len - ret;
   printk("%s: somebody called a read on dev with [major,minor] number [%d,%d]\n",MODNAME,get_major(filp),get_minor(filp));

  return 0;

}

static long dev_ioctl(struct file *filp, unsigned int command, unsigned long param) {

  int minor = get_minor(filp);
  object_state *the_object;

  the_object = objects + minor;
  printk("%s: somebody called an ioctl on dev with [major,minor] number [%d,%d] and command %u \n",MODNAME,get_major(filp),get_minor(filp),command);

  //do here whathever you would like to control the state of the device
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



int init_module(void) {

	int i;

	//initialize the drive internal state
	for(i=0;i<MINORS;i++){
#ifdef SINGLE_SESSION_OBJECT
		mutex_init(&(objects[i].object_busy));
#endif
		mutex_init(&(objects[i].operation_synchronizer));
		objects[i].valid_bytes = 0;
		objects[i].stream_content = NULL;
		objects[i].stream_content = (char*)__get_free_page(GFP_KERNEL);
		if(objects[i].stream_content == NULL) goto revert_allocation;


	}

	Major = __register_chrdev(0, 0, 256, DEVICE_NAME, &fops);
	//actually allowed minors are directly controlled within this driver

	if (Major < 0) {
	  printk("%s: registering device failed\n",MODNAME);
	  return Major;
	}

	printk(KERN_INFO "%s: new device registered, it is assigned major number %d\n",MODNAME, Major);

	return 0;

revert_allocation:
	for(;i>=0;i--){
		free_page((unsigned long)objects[i].stream_content);
	}
	return -ENOMEM;
}

void cleanup_module(void) {

	int i;
	for(i=0;i<MINORS;i++){
		free_page((unsigned long)objects[i].stream_content);
	}

	unregister_chrdev(Major, DEVICE_NAME);

	printk(KERN_INFO "%s: new device unregistered, it was assigned major number %d\n",MODNAME, Major);

	return;

}
