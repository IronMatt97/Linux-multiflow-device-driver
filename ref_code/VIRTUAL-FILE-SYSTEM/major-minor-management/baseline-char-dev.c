
/*  
 *  baseline char device driver with limitation on minor numbers - no actual operations 
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


static int Major;            /* Major number assigned to broadcast device driver */


static DEFINE_MUTEX(device_state);

/* the actual driver */


static int dev_open(struct inode *inode, struct file *file) {

// this device file is single instance
   if (!mutex_trylock(&device_state)) {
		return -EBUSY;
   }

   printk("%s: device file successfully opened by thread %d\n",MODNAME,current->pid);
//device opened by a default nop
   return 0;
}


static int dev_release(struct inode *inode, struct file *file) {

   mutex_unlock(&device_state);

   printk("%s: device file closed by thread %d\n",MODNAME,current->pid);
//device closed by default nop
   return 0;

}



static ssize_t dev_write(struct file *filp, const char *buff, size_t len, loff_t *off) {


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
   printk("%s: somebody called a write on dev with [major,minor] number [%d,%d]\n",MODNAME,MAJOR(filp->f_inode->i_rdev),MINOR(filp->f_inode->i_rdev));
#else
   printk("%s: somebody called a write on dev with [major,minor] number [%d,%d]\n",MODNAME,MAJOR(filp->f_dentry->d_inode->i_rdev),MINOR(filp->f_dentry->d_inode->i_rdev));
#endif
//  return len;
  return 1;

}

static ssize_t dev_read(struct file *filp, char *buff, size_t len, loff_t *off) {

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
   printk("%s: somebody called a read on dev with [major,minor] number [%d,%d]\n",MODNAME,MAJOR(filp->f_inode->i_rdev),MINOR(filp->f_inode->i_rdev));
#else
   printk("%s: somebody called a read on dev with [major,minor] number [%d,%d]\n",MODNAME,MAJOR(filp->f_dentry->d_inode->i_rdev),MINOR(filp->f_dentry->d_inode->i_rdev));
#endif
   return 0;

}


static struct file_operations fops = {
  .write = dev_write,
  .read = dev_read,
  .open =  dev_open,
  .release = dev_release
};



int init_module(void) {

	Major = __register_chrdev(0, 0, 256, DEVICE_NAME, &fops);

	if (Major < 0) {
	  printk("%s: registering device failed\n",MODNAME);
	  return Major;
	}

	printk(KERN_INFO "%s: new device registered, it is assigned major number %d\n",MODNAME, Major);

	return 0;
}

void cleanup_module(void) {

	unregister_chrdev(Major, DEVICE_NAME);

	printk(KERN_INFO "%s: new device unregistered, it was assigned major number %d\n",MODNAME, Major);

	return;

}
