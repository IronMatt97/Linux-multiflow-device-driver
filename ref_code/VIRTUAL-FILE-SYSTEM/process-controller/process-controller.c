
/*  
*  address space hit dev file driver - you can update memory location of target processes by writing to this dev 
*  preliminarly you need to mknod the dev file and assign the major retrived while mounting this module
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
#include <linux/signal_types.h>
#include <linux/syscalls.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Francesco Quaglia email: francesco.quglia@uniroma2.it");



static int hit_open(struct inode *, struct file *);
static int hit_release(struct inode *, struct file *);
static ssize_t hit_write(struct file *, const char *, size_t, loff_t *);

#define DEVICE_NAME "hit"  /* Device file name in /dev/ - not mandatory  */


static int Major;            /* Major number assigned to hit device driver */

/* auxiliary stuff */
static inline void _write_cr3(unsigned long val) {

          asm volatile("mov %0,%%cr3": : "r" (val));

}

static inline unsigned long _read_cr3(void) {

	  unsigned long val;
          asm volatile("mov %%cr3,%0":  "=r" (val) : );
	  return val;

}


/* the actual driver */


static int hit_open(struct inode *inode, struct file *file) {

//device opened by a default nop
   return 0;
}


static int hit_release(struct inode *inode, struct file *file) {

//device closed by default nop
   return 0;

}

#define LINE_SIZE 128
#define AUDIT if(1)

static ssize_t hit_write(struct file *filp, const char *buff, size_t len, loff_t *off) {

   char buffer[LINE_SIZE];
   long pid;
   unsigned long addr;
   long val;
   int format = 0;
   int i,j;
   int ret = len;

   char* args[3];

   struct task_struct *the_task;
   void** restore_pml4;
   unsigned long restore_cr3;
   struct mm_struct *restore_mm;


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
   printk("%s: somebody called a write on hit dev with [major,minor] number [%d,%d]\n",DEVICE_NAME,MAJOR(filp->f_inode->i_rdev),MINOR(filp->f_inode->i_rdev));
#else
   printk("%s: somebody called a write on hit dev with [major,minor] number [%d,%d]\n",DEVICE_NAME,MAJOR(filp->f_dentry->d_inode->i_rdev),MINOR(filp->f_dentry->d_inode->i_rdev));
#endif
 
  if(len >= LINE_SIZE) return -1;
  ret = copy_from_user(buffer,buff,len);

  j = 1;
  for(i=0;i<len;i++){
	if(buffer[i] == ' ') {
		buffer[i] = '\0';
		args[j++] = buffer + i + 1;
		format++;
	}
  }

  if(format != 2) return -EBADMSG;//bad message

  args[0] = buffer;

  buffer[len] = '\0';
  ret = kstrtol(args[0],10,&pid);
  ret = kstrtol(args[1],10,&addr);
  ret = kstrtol(args[2],10,&val);

  AUDIT
  printk("%s: args are: %ld - %lu - %ld\n",DEVICE_NAME,pid,addr,val);


  restore_pml4 = (unsigned long)current->mm->pgd;
  restore_mm = (unsigned long)current->mm;
  restore_cr3 = _read_cr3();

  rcu_read_lock();

  the_task = pid_task(find_vpid(pid),PIDTYPE_PID);

  if(!the_task){ 
  	rcu_read_unlock();
	return -ESRCH;//no such process
  }


  if(!(the_task->mm)){
  	rcu_read_unlock();
        return -ESRCH;//no such process
  }
  atomic_inc(&(the_task->mm->mm_count));//to be released on failures of below parts


  if(!(the_task->mm->pgd)){
  	atomic_dec(&(the_task->mm->mm_count));
  	rcu_read_unlock();
        return  -ENXIO;//no such device or address
  }

  AUDIT
  printk("%s: process %ld is there with its memory map\n",DEVICE_NAME,pid);

  current->mm = the_task->mm;//this is needed to correctly handle empty zero memory access or other faults
  _write_cr3(__pa(the_task->mm->pgd));

  AUDIT
  printk("%s: current moved to the address space of process %ld\n",DEVICE_NAME,pid);
  
  //do the hack
  ret = copy_to_user((void*)addr,(void*)&val,sizeof(val));

  //resume my own face
  current->mm = restore_mm;
  _write_cr3(restore_cr3);

  AUDIT
  printk("%s: current moved back to its own address space\n",DEVICE_NAME);

  atomic_dec(&(the_task->mm->mm_count));
  rcu_read_unlock();

  return len;
 
}



static struct file_operations fops = {
  .owner = THIS_MODULE,//do not forget this
  .write = hit_write,
  .open =  hit_open,
  .release = hit_release
};



int init_module(void) {

	Major = register_chrdev(0, DEVICE_NAME, &fops);

	if (Major < 0) {
	  printk("%s: registering device failed\n",DEVICE_NAME);
	  return Major;
	}

	printk(KERN_INFO "%s: device registered, it is assigned major number %d\n",DEVICE_NAME,Major);


	return 0;
}

void cleanup_module(void)
{

	unregister_chrdev(Major, DEVICE_NAME);

	printk(KERN_INFO "%s: device unregistered, it was assigned major number %d\n",DEVICE_NAME,Major);
}
