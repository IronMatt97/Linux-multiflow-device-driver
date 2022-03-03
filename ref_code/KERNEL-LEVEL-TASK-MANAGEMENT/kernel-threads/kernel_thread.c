#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/signal.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#include <linux/sched/signal.h>
#endif


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Francesco Quaglia <francesco.quaglia@uniroma2.it>");
MODULE_DESCRIPTION("basic example usage of kernel threads");

#define MODNAME "KERNEL THREAD"




static int shutdown_daemon = 0;// this can be configured at run time via the sys file system - 1 lead to daemon thread shutdown 
module_param(shutdown_daemon,int,0660);

static int sleep_enabled = 1;// this can be configured at run time via the sys file system 
module_param(sleep_enabled,int,0660);

static int timeout = 1;// this can be configured at run time via the sys file system 
module_param(timeout,int,0660);


#define SCALING (1000)  // please thake this value from CONFIG_HZ in your kernel config file 


int thread_function(void* data){

	DECLARE_WAIT_QUEUE_HEAD(my_sleep_queue);

	allow_signal(SIGKILL);
	allow_signal(SIGTERM);

	
begin:
	printk("%s: this is the daemon thread you activated (pid = %d) - running on CPU-core %d\n",MODNAME,current->pid,smp_processor_id());

	if(shutdown_daemon){
		printk("%s: deamon thread (pid = %d) - ending exection\n",MODNAME,current->pid);
		module_put(THIS_MODULE);
		return 0;
	}

	wait_event_interruptible_timeout(my_sleep_queue,!sleep_enabled,timeout*SCALING);

	if(signal_pending(current)){
		printk("%s: deamon thread (pid = %d) - killed\n",MODNAME,current->pid);
		module_put(THIS_MODULE);
		return -1;
	
	}

	goto begin;

}



int init_module(void) {

	int ret = -1;
	char name[128] = "the_new_daemon";
	struct task_struct *the_new_daemon;

	printk("%s: initializing\n",MODNAME);

	if(!try_module_get(THIS_MODULE)) return -1;


	the_new_daemon = kthread_create(thread_function,NULL,name);
	if(the_new_daemon) {
		wake_up_process(the_new_daemon);
		ret = 0;
	}
	else{
		module_put(THIS_MODULE);
	}

	return ret;


}


void cleanup_module(void) {
        	
	printk("%s: shutting down\n",MODNAME);
	
}

