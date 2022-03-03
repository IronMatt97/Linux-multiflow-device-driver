
// baseline example of proc file usage

#define EXPORT_SYMTAB
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/pid.h>          /* For pid types */
#include <linux/tty.h>          /* For the tty declarations */
#include <linux/version.h>      /* For LINUX_VERSION_CODE */
#include <linux/signal_types.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>

#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/types.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Francesco Quaglia");


#define SIZE (4096)

typedef struct _the_file{
	ssize_t valid;
	char file_data[SIZE];
} the_file;

the_file f;
DEFINE_MUTEX(lock);

struct proc_dir_entry *proc;

ssize_t write_proc(struct file *filp, const char *buf, size_t count, loff_t *offp ) {
	the_file *data;
	int ret;
	data=(the_file*)PDE_DATA(file_inode(filp));//getting the pointer to the data area
	if(!(data)){
		printk(KERN_INFO "Null data");
		return -1;
	}

	mutex_lock(&lock);
	if(*offp > data->valid) {//already at the end of file
		mutex_unlock(&lock);
		return 0;
	}

	if(count > (SIZE - *offp)) {
		count = SIZE - *offp;
	}

	ret = copy_from_user(f.file_data,buf,count);
	*offp += (count - ret);

	if(*offp > data->valid) data->valid = *offp;

	mutex_unlock(&lock);

	return count-ret;
}

ssize_t read_proc(struct file *filp, char *buf, size_t count, loff_t *offp ) {
	the_file *data;
	int ret;
	data=(the_file*)PDE_DATA(file_inode(filp));//getting the pointer to the data area
	if(!(data)){
		printk(KERN_INFO "Null data");
		return -1;
	}

	mutex_lock(&lock);
	if(*offp > data->valid) {//already at the end of file
		mutex_unlock(&lock);
		return 0;
	}

	if(count > (data->valid - *offp)) {
		count = data->valid - *offp;
	}

	ret = copy_to_user(buf,data->file_data, count);
	*offp += (count - ret);


	mutex_unlock(&lock);

	return count - ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0)
struct file_operations proc_fops = {
        owner: THIS_MODULE,
        read: read_proc,
        write: write_proc
};
#else
struct proc_ops proc_fops = {
        proc_read: read_proc,
        proc_write: write_proc
};
#endif


void create_new_proc_entry(void) {

	//here we create a single file but we could do more
	f.valid = 0;
	proc=proc_create_data("this-is-the-new-proc-file",0,NULL,&proc_fops,&f);
}


int proc_init (void) {
 create_new_proc_entry();
 return 0;
}

void proc_cleanup(void) {
 remove_proc_entry("this-is-the-new-proc-file",NULL);
}

MODULE_LICENSE("GPL"); 
module_init(proc_init);
module_exit(proc_cleanup);

