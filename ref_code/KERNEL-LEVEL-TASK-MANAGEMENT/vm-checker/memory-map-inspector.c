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
MODULE_DESCRIPTION("memory map inspector");



#define MODNAME "MMINSP"


unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0660);


unsigned long the_ni_syscall;

unsigned long new_sys_call_array[] = {0x0};//please set to sys_put_work at startup
#define HACKED_ENTRIES (int)(sizeof(new_sys_call_array)/sizeof(unsigned long))
int restore[HACKED_ENTRIES] = {[0 ... (HACKED_ENTRIES-1)] -1};


#define AUDIT if(1)



#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(2, _mm_inspect, unsigned long, vaddr, int, mode){
#else
asmlinkage long sys_mm_inspect(unsigned long vaddr, int mode){
#endif

	struct vm_area_struct *map;
        pgprot_t prot;
        int ret = 0;
        unsigned long mask;



        AUDIT{
        printk("%s: \n------------------------------\n",MODNAME);
        printk("%s: mm inspector asked to tell memory at virtual address %p is accessible in mode %d\n",MODNAME,(void*)vaddr,mode);
        }


        down_read(&current->mm->mmap_sem);//this is just a traversal with reads
        map = current->mm->mmap;

        while( map!=NULL ){

                printk("%s: traversing entry for zone [%lu,%lu]\n",MODNAME,map->vm_start,map->vm_end);

                if((map->vm_start <= vaddr) && ((map->vm_end-1)>= vaddr)){

                        printk("%s: target address found in entry for zone [%ld,%ld]\n",MODNAME,map->vm_start,map->vm_end);

                        prot = map->vm_page_prot;
                        mask = (unsigned long)pgprot_val(prot);

                        switch (mode) {

                                case 0:
                                        printk("%s: read only query\n",MODNAME);
                                        if( !(mask & 0x2)) ret = 1;
                                        break;

                                case 1:
                                        printk("%s: read/write query\n",MODNAME);
                                        if( !(mask & 0x2)) ret = 1;
                                        break;

                                case 2:
                                        printk("%s: exec query\n",MODNAME);
                                        if( !(mask & 0x8000000000000000) ) ret = 1;
                                        break;

                        }

                        up_read(&current->mm->mmap_sem);//it was just a traversal with reads

                        return ret;
                }

                map = map->vm_next;

        }
        up_read(&current->mm->mmap_sem);//it was just a traversal with reads
        return -1;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
long sys_mm_inspect = (unsigned long) __x64_sys_mm_inspect;       
#else
#endif


int init_module(void) {

        int i;
        int ret;

        AUDIT{
           printk("%s: mm inspect received sys_call_table address %px\n",MODNAME,(void*)the_syscall_table);
           printk("%s: initializing - hacked entries %d\n",MODNAME,HACKED_ENTRIES);
        }

        new_sys_call_array[0] = (unsigned long)sys_mm_inspect;

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


