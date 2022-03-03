
//baseline usage of the sys file system

#include <linux/module.h> 
#include <linux/moduleparam.h> 
#include <linux/printk.h> 
#include <linux/kobject.h> 
#include <linux/sysfs.h> 
#include <linux/init.h> 
#include <linux/fs.h> 
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Francesco Quaglia");


#define NAME "SYS BASELINE MODULE"

static struct kobject *example_kobject;
static int var;
module_param(var,int,0660);//this is redundant

static ssize_t var_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {//just one page of storage from buff

        return sprintf(buf, "%d\n", var);
}

static ssize_t var_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {//just one page of storage from buff

        sscanf(buf, "%du", &var);
        return count;
}


static struct kobj_attribute the_attribute =__ATTR(var, 0660, var_show, var_store);

typedef struct _data{
	char data[10];
	struct kobj_attribute file;
} data;

data the_file;

static int __init mymodule_init (void) {

        int error = 0;

        printk("%s: initialization\n",NAME);


        example_kobject = kobject_create_and_add(NAME, kernel_kobj);//the kernel_kobj variable points to the /sys/kernel object
        if(!example_kobject)
                return -ENOMEM;

	memcpy(the_file.data , &the_attribute, sizeof(the_attribute));
        error = sysfs_create_file(example_kobject, &the_file.file.attr);//&the_attribute.attr);//actually creating an attribute 

        if (error) {
                printk("%s: failed to create the target kobj\n",NAME);
        }

        return error;
}

static void __exit mymodule_exit (void)
{
        printk("%s: shutdown \n",NAME);
        kobject_put(example_kobject);
}

MODULE_LICENSE("GPL"); 
module_init(mymodule_init);
module_exit(mymodule_exit);
