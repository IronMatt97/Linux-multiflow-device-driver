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
* @file queuing-service.c 
* @brief This is the main source for the Linux Kernel Module which implements
*       a blocking queuing service 
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
MODULE_DESCRIPTION("queuing service");



#define MODNAME "QUEUING"


unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0660);


unsigned long the_ni_syscall;

unsigned long new_sys_call_array[] = {0x0,0x0};//please set to sys_got_sleep and sys_awake at startup
#define HACKED_ENTRIES (int)(sizeof(new_sys_call_array)/sizeof(unsigned long))
int restore[HACKED_ENTRIES] = {[0 ... (HACKED_ENTRIES-1)] -1};


#define AUDIT if(1)

#define NO (0)
#define YES (NO+1)

static int enable_sleep = 1;// this can be configured at run time via the sys file system - 1 meas any sleeping thread is freezed
module_param(enable_sleep,int,0660);

unsigned long count __attribute__((aligned(8)));//this is used to audit how many threads are still sleeping onto the sleep/wakeup queue
module_param(count,ulong,0660);

typedef struct _elem{
        struct task_struct *task;
        int pid;
        int awake;
        int already_hit;
        struct _elem * next;
        struct _elem * prev;
} elem;

elem head = {NULL,-1,-1,-1,NULL,NULL};
elem tail = {NULL,-1,-1,-1,NULL,NULL};
spinlock_t queue_lock;




#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _goto_sleep, int, unused){
#else
asmlinkage long sys_goto_sleep(int unused){
#endif

	volatile elem me;
        elem *aux;
        DECLARE_WAIT_QUEUE_HEAD(the_queue);//here we use a private queue - wakeup is selective via wake_up_process

        me.next = NULL;
        me.prev = NULL;
        me.task = current;
        me.pid  = current->pid;
        me.awake = NO;
        me.already_hit = NO;

        AUDIT
        printk("%s: sys_goto_sleep on strong fifo sleep/wakeup queue called from thread %d\n",MODNAME,current->pid);


        if(!enable_sleep){
                printk("%s: sys_goto_sleep - sleeping not currently enabled\n",MODNAME);
                return -1;
        }

        preempt_disable();//this is redundant here
        spin_lock(&queue_lock);

        aux = &tail;
        if(aux->prev == NULL){
                spin_unlock(&queue_lock);
                preempt_enable();
                printk("%s: malformed sleep-wakeup-queue - service damaged\n",MODNAME);
                return -1;
        }

        aux->prev->next = &me;
        me.prev = aux->prev;
        aux->prev = &me;
        me.next = aux;

        spin_unlock(&queue_lock);
        preempt_enable();//this is redundant here

        atomic_inc((atomic_t*)&count);//a new sleeper 

        AUDIT
        printk("%s: thread %d actually going to sleep\n",MODNAME,current->pid);
                
        wait_event_interruptible(the_queue, me.awake == YES);
        
        preempt_disable();
        spin_lock(&queue_lock);

        //aux = &head;

        //if(aux == NULL){
         //       spin_unlock(&queue_lock);
          //      preempt_enable();
           //     printk("%s: malformed sleep-wakeup-queue upon wakeup - service damaged\n",MODNAME);
            //    return -1;
        //}

        me.prev->next = me.next;//we know where we are thanks to double linkage
        me.next->prev = me.prev;

        spin_unlock(&queue_lock);
        preempt_enable();

        AUDIT
        printk("%s: thread %d exiting sleep for a wakeup or signal\n",MODNAME, current->pid);

        atomic_dec((atomic_t*)&count);//finally awaken

        if(me.awake == NO){
                AUDIT
                printk("%s: thread %d exiting sleep for signal\n",MODNAME, current->pid);
                return -EINTR;
        }

        return 0;

}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _awake, int, unused){
#else
asmlinkage long sys_awake(int unused){
#endif
	        struct task_struct *the_task;
        int its_pid = -1;
        elem *aux;


        printk("%s: sys_awake called from thread %d\n",MODNAME,current->pid);

        aux = &head;

        preempt_disable();//this is redundant here
        spin_lock(&queue_lock);

        if(aux == NULL){
                spin_unlock(&queue_lock);
                preempt_enable();
                printk("%s: malformed sleep-wakeup-queue\n",MODNAME);
                return -1;
        }

        while(aux->next != &tail){

                AUDIT
                printk("%s: in cycle\n",MODNAME);

                if(aux->next->already_hit == NO){
                        the_task = aux->next->task;
                        aux->next->awake = YES;
                        aux->next->already_hit = YES;
                        its_pid = aux->next->pid;
                        wake_up_process(the_task);
                        goto awaken;
                }

                aux = aux->next;

        }

        spin_unlock(&queue_lock);
        preempt_enable();

	return 0;

awaken:
        spin_unlock(&queue_lock);
        preempt_enable();//this is redundant here

        AUDIT
        printk("%s: called the awake of thread %d\n",MODNAME,its_pid);

        return its_pid;

}



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
long sys_goto_sleep = (unsigned long) __x64_sys_goto_sleep;       
long sys_awake = (unsigned long) __x64_sys_awake;       
#else
#endif


int init_module(void) {

        int i;
        int ret;

        AUDIT{
           printk("%s: queuing example received sys_call_table address %px\n",MODNAME,(void*)the_syscall_table);
           printk("%s: initializing - hacked entries %d\n",MODNAME,HACKED_ENTRIES);
        }

	spin_lock_init(&queue_lock);

        head.next = &tail;// setup initial double linked list
        tail.prev = &head;


        new_sys_call_array[0] = (unsigned long)sys_goto_sleep;
        new_sys_call_array[1] = (unsigned long)sys_awake;

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


