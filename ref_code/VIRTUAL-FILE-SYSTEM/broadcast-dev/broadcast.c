
/*  
 *  broadcast dev file driver - you can broadcast streams of bytes on all the registered ttys 
 *  by simply writing to the broadcast dev file
 *  preliminary you need to mknod the dev file and assign the major retrived while mounting this module
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


#define MODNAME "BROADCAST"


static ssize_t print_stream_everywhere(const char *, size_t );

static int broadcast_open(struct inode *, struct file *);
static int broadcast_release(struct inode *, struct file *);
static ssize_t broadcast_write(struct file *, const char *, size_t, loff_t *);

#define DEVICE_NAME "broadcast"  /* Device file name in /dev/ - not mandatory  */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
#define get_major(session)      MAJOR(session->f_inode->i_rdev)
#define get_minor(session)      MINOR(session->f_inode->i_rdev)
#else
#define get_major(session)      MAJOR(session->f_dentry->d_inode->i_rdev)
#define get_minor(session)      MINOR(session->f_dentry->d_inode->i_rdev)
#endif


static int Major;            /* Major number assigned to broadcast device driver */

//registered ttys
char * targets[] = {	"/dev/pts/0",
		 	"/dev/pts/1",
		 	"/dev/pts/2",
			"/dev/pts/3",
			"/dev/pts/4",
			"/dev/pts/5",
			"/dev/pts/6",
			"/dev/pts/7",
			"/dev/pts/8",
			"/dev/pts/9",
			NULL
		};



/* the actual driver */


static int broadcast_open(struct inode *inode, struct file *file) {

	printk("%s: broadcast dev closed\n",MODNAME);
	//device opened by a default nop
  	 return 0;
}


static int broadcast_release(struct inode *inode, struct file *file) {

	printk("%s: broadcast dev closed\n",MODNAME);
	//device closed by default nop
   	return 0;

}



static ssize_t broadcast_write(struct file *filp, const char *buff, size_t len, loff_t *off) {

   printk("%s: somebody called a write on broadcast dev with [major,minor] number [%d,%d]\n",MODNAME,get_major(filp),get_minor(filp));
   return print_stream_everywhere(buff, len);

}



static struct file_operations fops = {
  .owner = THIS_MODULE,
  .write = broadcast_write,
  .open =  broadcast_open,
  .release = broadcast_release
};


static ssize_t print_stream_everywhere(const char *stream, size_t size ) {

	int i;
	struct file * f = NULL;

	printk("%s: print stream function of broadcast dev called - tageting all registered ttys\n",MODNAME);

	for (i = 0; targets[i] != NULL; i++){
		printk("%s: tageting %s\n",MODNAME,targets[i]);
        	f = filp_open(targets[i],O_WRONLY,0666);	
		if(IS_ERR(f)) continue;
		vfs_write(f,stream,size,&f->f_pos);
		filp_close(f,NULL);

	}

	return size;


}



int init_module(void)
{

	Major = register_chrdev(0, DEVICE_NAME, &fops);

	if (Major < 0) {
	  printk("Registering noiser device failed\n");
	  return Major;
	}

	printk(KERN_INFO "Broadcast device registered, it is assigned major number %d\n", Major);


	return 0;
}

void cleanup_module(void)
{

	unregister_chrdev(Major, DEVICE_NAME);

	printk(KERN_INFO "Broadcast device unregistered, it was assigned major number %d\n", Major);
}
