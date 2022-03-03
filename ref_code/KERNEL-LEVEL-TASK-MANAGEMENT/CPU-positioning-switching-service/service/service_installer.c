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
*       a service that can be used to ask the kernel what is the current CPU id 
*	and how many CPU switches the thread had
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
#include "../include/monitor.h"


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Francesco Quaglia <francesco.quaglia@uniroma2.it>");
MODULE_DESCRIPTION("CPU positioning and switching service");



#define MODNAME "CPU positioning/switching:"


unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0660);


unsigned long the_ni_syscall;

unsigned long new_sys_call_array[] = {0x0,0x0};//please set to sys_put_work at startup
#define HACKED_ENTRIES (int)(sizeof(new_sys_call_array)/sizeof(unsigned long))
int restore[HACKED_ENTRIES] = {[0 ... (HACKED_ENTRIES-1)] -1};


#define AUDIT if(0)


#ifdef CONFIG_THREAD_INFO_IN_TASK
int offset = sizeof(struct thread_info);
#else
int offset = 0; 
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _monitor_reset, int, unused){
#else
asmlinkage long sys_monitor_reset(int unused){
#endif

	monitor * p;

        printk("%s: sys_monitor_reset has been called - current is %d\n",MODNAME,current->pid);

        p = (void*)current->stack + offset;

        p->cpu_id = -1;
        p->switches = 0;

        return 0;

}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _get_cpu_id, monitor*, buff){
#else
asmlinkage long sys_get_cpu_id(monitor * buff){
#endif

	monitor * p;
        int ret;

        printk("%s: sys_get_cpu_id has been called - current is %d\n",MODNAME,current->pid);

        p = (void*)current->stack + offset;

        printk("%s: cpuid info is:  %d\n",MODNAME,p->cpu_id);

        ret = copy_to_user((char*)buff,(char*)p,sizeof(monitor));

        return 0;


}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
long sys_monitor_reset = (unsigned long) __x64_sys_monitor_reset;       
long sys_get_cpu_id = (unsigned long) __x64_sys_get_cpu_id;       
#else
#endif


int init_module(void) {

        int i;
        int ret;

        AUDIT{
           printk("%s: CPU positioning/switching service received sys_call_table address %px\n",MODNAME,(void*)the_syscall_table);
           printk("%s: initializing - hacked entries %d\n",MODNAME,HACKED_ENTRIES);
        }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
        printk("%s: kerrnel verison no mlinger supported\n",MODNAME);
	return -1;
#endif

        new_sys_call_array[0] = (unsigned long)sys_monitor_reset;
        new_sys_call_array[1] = (unsigned long)sys_get_cpu_id;

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


