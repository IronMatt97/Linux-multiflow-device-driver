/*
* 
* This is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
* 
* This module is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
* 
* @file kt-startup-service.c 
* @brief This is the main source for the Linux Kernel Module which runs a kernel daemon 
*
* @author Francesco Quaglia
*
* @date November 13, 2021
*/

#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/kprobes.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/apic.h>
#include <asm/io.h>
#include <linux/syscalls.h>
#include <linux/kthread.h>
#include "lib/include/scth.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Francesco Quaglia <francesco.quaglia@uniroma2.it>");
MODULE_DESCRIPTION("kernel daemon starter");



#define MODNAME "DAEMON"


unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0660);


unsigned long the_ni_syscall;

unsigned long new_sys_call_array[] = {0x0};//please set to sys_put_work at startup
#define HACKED_ENTRIES (int)(sizeof(new_sys_call_array)/sizeof(unsigned long))
int restore[HACKED_ENTRIES] = {[0 ... (HACKED_ENTRIES-1)] -1};


#define AUDIT if(1)


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
        printk("%s: this is the daemon thread you activated (pid = %d) - running on CPU-core %d\n" ,MODNAME,current->pid,smp_processor_id());

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


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _run_daemon, unsigned long, unused){
#else
asmlinkage long sys_run_daemon(unsigned long unused){
#endif

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



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
long sys_run_daemon = (unsigned long) __x64_sys_run_daemon;       
#else
#endif


int init_module(void) {

        int i;
        int ret;

        AUDIT{
           printk("%s: daemon manager received sys_call_table address %px\n",MODNAME,(void*)the_syscall_table);
           printk("%s: initializing - hacked entries %d\n",MODNAME,HACKED_ENTRIES);
        }

        new_sys_call_array[0] = (unsigned long)sys_run_daemon;

        ret = get_entries(restore,HACKED_ENTRIES,(unsigned long*)the_syscall_table,&the_ni_syscall);


        if (ret != HACKED_ENTRIES){
                printk("%s: could not hack %d entries (just %d)\n",MODNAME,HACKED_ENTRIES,ret);
                return -1;
        }

        unprotect_memory();

        for(i=0;i<HACKED_ENTRIES;i++){
                ((unsigned long *)the_syscall_table)[restore[i]] = (unsigned long)new_sys_call_array[i];
        }

        protect_memory();

        printk("%s: all new system-calls correctly installed on sys-call table\n",MODNAME);

        return 0;

}


void cleanup_module(void) {

        int i;

        printk("%s: shutting down\n",MODNAME);

        unprotect_memory();
        for(i=0;i<HACKED_ENTRIES;i++){
                ((unsigned long *)the_syscall_table)[restore[i]] = the_ni_syscall;
        }
        protect_memory();
        printk("%s: sys-call table restored to its original content\n",MODNAME);

}


