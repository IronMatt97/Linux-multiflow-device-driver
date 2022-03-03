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
* @file usleep.c 
* @brief This is the main source for the Linux Kernel Module which implements
*       a system call that can be used to ask the kernel to sleep for fine grain periods 
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
#include "lib/include/scth.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Francesco Quaglia <francesco.quaglia@uniroma2.it>");
MODULE_DESCRIPTION("basic example usage of high resolution timers");



#define MODNAME "USLEEP"


unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0660);


unsigned long the_ni_syscall;

unsigned long new_sys_call_array[] = {0x0};//please set to sys_put_work at startup
#define HACKED_ENTRIES (int)(sizeof(new_sys_call_array)/sizeof(unsigned long))
int restore[HACKED_ENTRIES] = {[0 ... (HACKED_ENTRIES-1)] -1};


#define AUDIT if(1)

#define NO (0)
#define YES (NO+1)


typedef struct _control_record{
        struct task_struct *task;       
        int pid;
        int awake;
        struct hrtimer hr_timer;
} control_record;


static enum hrtimer_restart my_hrtimer_callback( struct hrtimer *timer ){

        control_record *control;
        struct task_struct *the_task;

        control = (control_record*)container_of(timer,control_record, hr_timer);
        control->awake = YES;
        the_task = control->task;
        wake_up_process(the_task);

        return HRTIMER_NORESTART;
}

#define CLASSIC

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _goto_sleep, unsigned long, microsecs){
#else
asmlinkage long sys_goto_sleep(unsigned long microsecs){
#endif

	control_record data;
        control_record* control;
        ktime_t ktime_interval;
        DECLARE_WAIT_QUEUE_HEAD(the_queue);//here we use a private queue - wakeup is selective via wake_up_process

        if(microsecs == 0) return 0;

	control = &data;//set the pointer to the current stack area

        AUDIT
        printk("%s: thread %d going to usleep for %lu microsecs\n",MODNAME,current->pid,microsecs);

        ktime_interval = ktime_set( 0, microsecs*1000 );

#ifdef CLASSIC
        control->awake = NO;
        wait_event_interruptible_hrtimeout(the_queue, control->awake == YES, ktime_interval);
#else
        control->task = current;
        control->pid  = current->pid;
        control->awake = NO;

        hrtimer_init(&(control->hr_timer), CLOCK_MONOTONIC, HRTIMER_MODE_REL);

        control->hr_timer.function = &my_hrtimer_callback;
        hrtimer_start(&(control->hr_timer), ktime_interval, HRTIMER_MODE_REL);

                
        wait_event_interruptible(the_queue, control->awake == YES);

        hrtimer_cancel(&(control->hr_timer));
        
#endif
        AUDIT
        printk("%s: thread %d exiting usleep\n",MODNAME, current->pid);

	if(unlikely(control->awake != YES)) return -1;

        return 0;

}



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
long sys_goto_sleep = (unsigned long) __x64_sys_goto_sleep;       
#else
#endif


int init_module(void) {

        int i;
        int ret;

        AUDIT{
           printk("%s: usleep example received sys_call_table address %px\n",MODNAME,(void*)the_syscall_table);
           printk("%s: initializing - hacked entries %d\n",MODNAME,HACKED_ENTRIES);
        }

        new_sys_call_array[0] = (unsigned long)sys_goto_sleep;

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


