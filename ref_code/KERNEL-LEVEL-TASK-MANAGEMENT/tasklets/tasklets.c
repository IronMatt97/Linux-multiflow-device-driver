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
* @file tasklet.c 
* @brief This is the main source for the Linux Kernel Module which implements
*       a system call that can be used to ask the SoftIRQ daemon to execute a tasklet 
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
MODULE_DESCRIPTION("basic example usage of kernel tasklets");



#define MODNAME "TASKLET"


unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0660);


unsigned long the_ni_syscall;

unsigned long new_sys_call_array[] = {0x0};//please set to sys_put_work at startup
#define HACKED_ENTRIES (int)(sizeof(new_sys_call_array)/sizeof(unsigned long))
int restore[HACKED_ENTRIES] = {[0 ... (HACKED_ENTRIES-1)] -1};



#define AUDIT if(1)

typedef struct _packed_task{
        void* buffer;
        long code;
        struct tasklet_struct the_tasklet;
} packed_task;


//in a real usage this should ever run as interrupt-context
void audit(unsigned long data){

        AUDIT{
                printk("%s: ------------------------------\n",MODNAME);
                printk("%s: this print comes from softirq daemon with PID=%d - running on CPU-core %d\n",MODNAME,current->tgid,smp_processor_id());
        }

        AUDIT
        printk("%s: running task with code  %ld\n",MODNAME,((packed_task*)data)->code);

        AUDIT
        printk("%s: releasing the task buffer at address %p - container of task is at %p\n",MODNAME,(void*)data,container_of((void*)data,packed_task,buffer));

        kfree((void*)data);

        module_put(THIS_MODULE);

}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _put_work, int, request_code){
#else
asmlinkage long sys_put_work(int request_code){
#endif

	packed_task *the_task;

        if(!try_module_get(THIS_MODULE)) return -ENODEV;

        AUDIT{
                printk("%s: ------------------------\n",MODNAME);
                printk("%s: requested deferred work with request code: %d\n",MODNAME,request_code);
        }

        the_task = kzalloc(sizeof(packed_task),GFP_ATOMIC);//non blocking memory allocation

        if (the_task == NULL) {
                printk("%s: tasklet buffer allocation failure\n",MODNAME);
                module_put(THIS_MODULE);
                return -1;
        }

        the_task->buffer = the_task;
        the_task->code = request_code;

        AUDIT
        printk("%s: tasklet buffer allocation success - address is %p\n",MODNAME,the_task);

        tasklet_init(&the_task->the_tasklet,audit,(unsigned long)the_task);

        tasklet_schedule(&the_task->the_tasklet);

        return 0;
}




#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
long sys_put_work = (unsigned long) __x64_sys_put_work;       
#else
#endif


int init_module(void) {

        int i;
        int ret;

        AUDIT{
           printk("%s: tasklet example received sys_call_table address %px\n",MODNAME,(void*)the_syscall_table);
           printk("%s: initializing - hacked entries %d\n",MODNAME,HACKED_ENTRIES);
        }

        new_sys_call_array[0] = (unsigned long)sys_put_work;

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


