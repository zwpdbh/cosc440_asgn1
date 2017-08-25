
/**
 * File: asgn1.c
 * Date: 13/03/2011
 * Author: Your Name
 * Version: 0.1
 *
 * This is a module which serves as a virtual ramdisk which disk size is
 * limited by the amount of memory available and serves as the requirement for
 * COSC440 assignment 1 in 2012.
 *
 * Note: multiple devices and concurrent modules are not supported in this
 *       version.
 */

/* This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/device.h>
#include <linux/sched.h>

#define MYDEV_NAME "asgn1"
#define MYIOC_TYPE 'k'

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("COSC440 asgn1");


/**
 * The node structure for the memory page linked list.
 */
typedef struct page_node_rec {
    struct list_head list;
    struct page *page;
} page_node;


typedef struct asgn1_dev_t {
    dev_t dev;            /* the device */
    struct cdev *cdev;
    struct list_head mem_list;
    int num_pages;        /* number of memory pages this module currently holds */
    size_t data_size;     /* total data size in this module */
    atomic_t nprocs;      /* number of processes accessing this device */
    atomic_t max_nprocs;  /* max number of processes accessing this device */
    struct kmem_cache *cache;      /* cache memory */
    struct class *class;     /* the udev class */
    struct device *device;   /* the udev device node */
} asgn1_dev;

asgn1_dev asgn1_device;


int asgn1_major = 0;                      /* major number of module */
int asgn1_minor = 0;                      /* minor number of module */
int asgn1_dev_count = 1;                  /* number of devices */


/**
 * Helper function which add a page and set the data_size
 */
void add_pages(asgn1_dev *asgn1_device_ptr, int num) {
    int i;
    
    printk(KERN_WARNING "before adding pages, there are: %d pages\n", asgn1_device_ptr->num_pages);
    
    for (i = 0; i < num; i++) {
        page_node *pg;
        pg = kmalloc(sizeof(struct page_node_rec), GFP_KERNEL);
        pg->page = alloc_page(GFP_KERNEL);
        
        list_add_tail(&pg->list, &asgn1_device_ptr->mem_list);
        asgn1_device_ptr->num_pages = num;
    }
    printk(KERN_WARNING "after adding pages, there are: %d pages\n", asgn1_device_ptr->num_pages);
}


/**
 * This function frees all memory pages held by the module.
 */
void free_memory_pages(struct asgn1_dev_t *asgn1_device_ptr) {
    /* COMPLETE ME */
    /**
     * Loop through the entire page list {
     *   if (node has a page) {
     *     free the page
     *   }
     *   remove the node from the page list
     *   free the node
     * }
     * reset device data size, and num_pages
     */
    
    struct page_node_rec *page_node_current_ptr, *page_node_next_ptr;
    int count = 1;
    if (!list_empty(&asgn1_device_ptr->mem_list)) {
        list_for_each_entry_safe(page_node_current_ptr, page_node_next_ptr, &asgn1_device.mem_list, list) {
            __free_page(page_node_current_ptr->page);
            list_del(&page_node_current_ptr->list);
            kfree(page_node_current_ptr);
            
            printk(KERN_WARNING "freed %d: page\n", count);
            count += 1;
        }
        asgn1_device_ptr->num_pages = 0;
        asgn1_device_ptr->data_size = 0;
    }
}


/**
 * This function opens the virtual disk, if it is opened in the write-only
 * mode, all memory pages will be freed.
 */
/**
 * From book: should perform the following tasks:
 * 1. check for device-specific erros
 * 2. initialize the device if it is being opened for the first time
 * 3. update the f_op pointer, if necessary
 * 4. allocate and fill any data structure to be put in filp->private_data
 */
int asgn1_open(struct inode *inode, struct file *filp) {
    /* COMPLETE ME */
    /**
     * Increment process count, if exceeds max_nprocs, return -EBUSY
     *
     * if opened in write-only mode, free all memory pages
     *
     */
    struct asgn1_dev_t *asgn1_device_ptr;
    int nprocs;
    int max_nprocs;
    
    asgn1_device_ptr = container_of(inode->i_cdev, struct asgn1_dev_t, cdev);
    filp->private_data = asgn1_device_ptr;
    
    atomic_inc(&asgn1_device_ptr->nprocs);
    
    nprocs = atomic_read(&asgn1_device_ptr->nprocs);
    max_nprocs = atomic_read(&asgn1_device_ptr->max_nprocs);
    
    if (nprocs > max_nprocs) {
        return -EBUSY;
    }
    
    if (filp->f_mode == FMODE_WRITE) {
        free_memory_pages(asgn1_device_ptr);
    }
    
    return 0; /* success */
}


/**
 * This function releases the virtual disk, but nothing needs to be done
 * in this case.
 */
int asgn1_release (struct inode *inode, struct file *filp) {
    /* COMPLETE ME */
    /**
     * decrement process count
     */
    return 0;
}



/**
 * This function reads contents of the virtual disk and writes to the user
 */
ssize_t asgn1_read(struct file *filp, char __user *buf, size_t count,
                   loff_t *f_pos) {
    
    /* the offset from the beginning of a page to start reading */
    size_t begin_offset;
    
    /* the amout of data need to read*/
    size_t unfinished;
    /* the amout of data need to read in one page*/
    size_t finished_in_one_page;
    size_t unfinished_in_this_page;
    size_t should_finish_in_this_page;
    size_t unfinished_in_this_page_record;
    
    /* the first page which contains the requested data */
    int begin_page_no = *f_pos / PAGE_SIZE;
    
    /* the current page number */
    int curr_page_no = 0;
    
    struct asgn1_dev_t *dev = filp->private_data;
    struct list_head *ptr = dev->mem_list.next;
    
    unsigned int first_one = 1;
    
    page_node *curr_page_node_ptr;
    
    printk(KERN_WARNING "\n\n\n");
    printk(KERN_WARNING "============Before reading==========\n");
    printk(KERN_WARNING "dev->num_pages = %d\n", dev->num_pages);
    printk(KERN_WARNING "count = %zu\n", count);
    
    printk(KERN_WARNING "comparing *f_pos with dev->data_size...\n");
    printk(KERN_WARNING "*f_pos = %lld, data_size = %zu\n", *f_pos, dev->data_size);
    /*if beyong data size, return 0*/
    if (*f_pos >= dev->data_size) {
        printk(KERN_WARNING "because *f_pos >= dev->data_size, SO return 0");
        return 0;
    }
    
    unfinished = count;
    /*the data need to be sent to user should not exceed data_size*/
    printk(KERN_WARNING "computing valid unfinished work");
    if (*f_pos + count > dev->data_size) {
        printk(KERN_WARNING "because *f_pos + count = %lld, dev->data_size = %zu\n", *f_pos + count, dev->data_size);
        unfinished = dev->data_size - *f_pos;
        printk(KERN_WARNING "SO set unfinished = %zu\n", unfinished);
        count = unfinished;
    }

    
    /*1. find the first requested page*/
    while (curr_page_no < begin_page_no) {
        ptr = ptr->next;
        curr_page_no += 1;
    }
    
    while (unfinished >0) {
        curr_page_node_ptr = list_entry(ptr, page_node, list);
        
        if (first_one == 1) {
            begin_offset = *f_pos % PAGE_SIZE;
        } else {
            begin_offset = 0;
        }
        
        /*process in one page*/
        printk(KERN_WARNING "\n");
        printk(KERN_WARNING "==processing in the %dth page==\n", curr_page_no);
        printk(KERN_WARNING "unfinished = %zu\n", unfinished);
        printk(KERN_WARNING "begin_offset = %zu\n", begin_offset);
        printk(KERN_WARNING "PAGE_SIZE - begin_offset = %lu\n", PAGE_SIZE - begin_offset);
        
        printk(KERN_WARNING "==computing the work needed to be done in this page...\n");
        if (unfinished > PAGE_SIZE - begin_offset) {
            printk(KERN_WARNING "because unfinished > PAGE_SIZE - begin_offset, SO\n");
            unfinished_in_this_page = PAGE_SIZE - begin_offset;
        } else {
            printk(KERN_WARNING "because unfinished <= PAGE_SIZE - begin_offset, SO\n");
            unfinished_in_this_page = unfinished;
        }
        
        printk(KERN_WARNING "unfinished_in_this_page = %zu\n", unfinished_in_this_page);
        
        should_finish_in_this_page = unfinished_in_this_page;
        unfinished_in_this_page_record = unfinished_in_this_page;
        
        printk(KERN_WARNING "\n\n");
        printk(KERN_WARNING "we need to read %zu amout of data from the %dth page", should_finish_in_this_page, curr_page_no);
        
        do {
            printk(KERN_WARNING "we are going to read %d amout of data from page %d, offset is %zu\n", unfinished_in_this_page, curr_page_no, begin_offset);
            unfinished_in_this_page = copy_to_user(buf, page_address(curr_page_node_ptr->page) + begin_offset, unfinished_in_this_page);
            printk(KERN_WARNING "finished reading for %d amount of data\n", unfinished_in_this_page_record - unfinished_in_this_page);
            buf += (unfinished_in_this_page_record - unfinished_in_this_page);
            
            begin_offset += (unfinished_in_this_page_record - unfinished_in_this_page);
            unfinished_in_this_page_record = unfinished_in_this_page;
        } while (unfinished_in_this_page > 0);
        /*end of processing in one page*/
        
        *f_pos += should_finish_in_this_page;
        printk(KERN_WARNING "change *f_pos to: %lld\n", *f_pos);
        filp->f_pos = *f_pos;
        
        printk(KERN_WARNING "we succeed in read %d amout of data from the the %dth page\n", should_finish_in_this_page, curr_page_no);
        
        unfinished -= should_finish_in_this_page;
        printk(KERN_WARNING "there are %d amout of data left to read\n\n", unfinished);
        
        ptr = ptr->next;
        curr_page_no += 1;
        first_one += 1;
    }
    
    
    printk(KERN_WARNING "=======after reading...========\n");
    printk(KERN_WARNING "unfinished reading amout of data is: %zu\n", unfinished);
    printk(KERN_WARNING "finished reading %zu - %zu = total %zu amout of data\n\n", count, unfinished, count - unfinished);
    printk(KERN_WARNING "*f_pos = %lld\n", *f_pos);
    printk(KERN_WARNING "filp->f_pos = %lld\n", filp->f_pos);
    printk(KERN_WARNING "return %zu\n", count - unfinished);
    printk(KERN_WARNING "\n\n\n");
    return count - unfinished;
}



static loff_t asgn1_lseek (struct file *file, loff_t offset, int whence)
{
    loff_t newpos = 0;
    
    //    size_t buffer_size = asgn1_device.num_pages * PAGE_SIZE;
    struct asgn1_dev_t *dev = file->private_data;
    size_t buffer_size = dev->num_pages * PAGE_SIZE;
    
    /* COMPLETE ME */
    /**
     * set testpos according to the command
     *
     * if testpos larger than buffer_size, set testpos to buffer_size
     *
     * if testpos smaller than 0, set testpos to 0
     *
     * set file->f_pos to testpos
     */
    switch (whence) {
        case SEEK_SET:
            newpos = offset;
            break;
            
        case SEEK_CUR:
            newpos = file->f_pos + offset;
            break;
            
        case SEEK_END:
            newpos = dev->data_size + offset;
            break;
        default:
            break;
    }
    if (newpos < 0 || newpos > buffer_size) {
        return -EINVAL;
    } else {
        file->f_pos = newpos;
        printk (KERN_INFO "Seeking to pos=%ld\n", (long)newpos);
    }
    
    return newpos;
}



void allocate_new_pages_based_on(struct asgn1_dev_t *dev, size_t count, loff_t *f_pos) {
    int num_pages_needed_allocate;
    size_t differences;
    
    printk(KERN_WARNING "=======preparing writing=====\n");
    printk(KERN_WARNING "the request *f_pos is: %lld\n", *f_pos);
    printk(KERN_WARNING "the request count is: %zu\n", count);
    
    differences = *f_pos + count - dev->data_size;
    printk(KERN_WARNING "the differences between need and already have is: %zu\n", differences);
    
    if (differences > 0) {
        num_pages_needed_allocate = differences / PAGE_SIZE;
        printk(KERN_WARNING "the pages needed to allocate is: %d\n", num_pages_needed_allocate);
        
        if (dev->num_pages == 0 && num_pages_needed_allocate == 0) {
            printk(KERN_WARNING "we will allocate: %d pages\n", 1);
            add_pages(dev, 1);
        } else if (num_pages_needed_allocate > 0) {
            printk(KERN_WARNING "we will allocate: %d pages\n", num_pages_needed_allocate + 1);
            add_pages(dev, num_pages_needed_allocate + 1);
        }
    }
    
}



/**
 * This function writes from the user buffer to the virtual disk of this
 * module
 */
ssize_t asgn1_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    size_t orig_f_pos = *f_pos;
    size_t size_written = 0;
    size_t begin_offset;
    size_t unfinished_in_this_page;
    size_t unfinished_in_this_page_record;
    size_t should_finish_in_this_page;
    size_t unfinished = count;
    
    /* the first page this finction should start writing to */
    int begin_page_no = *f_pos / PAGE_SIZE;
    int curr_page_no = 0;     /* the current page number */
    
    struct asgn1_dev_t *dev = filp->private_data;
    struct list_head *ptr = dev->mem_list.next;
    page_node *curr_page_node_ptr;
    
    unsigned int first_one = 1;
    
    printk(KERN_WARNING "\n\n\n");
    printk(KERN_WARNING "==============before writing==========");
    printk(KERN_WARNING "the data size stored in device is %zu\n", dev->data_size);
    printk(KERN_WARNING "before writing, there are %d pages available\n", dev->num_pages);
    printk(KERN_WARNING "data need to write is count = %zu\n", count);
    printk(KERN_WARNING "the *f_pos passed in is: %lld\n", *f_pos);
    
    allocate_new_pages_based_on(dev, unfinished, f_pos);
    
    printk(KERN_WARNING "literate to get the first page");
    while (curr_page_no < begin_page_no) {
        ptr = ptr->next;
        curr_page_no += 1;
    }
    
    printk(KERN_WARNING "we need to write %zu amout of data\n", count);
    while (unfinished > 0) {
        curr_page_node_ptr = list_entry(ptr, page_node, list);
        
        if (first_one == 1) {
            begin_offset = *f_pos % PAGE_SIZE;
        } else {
            begin_offset = 0;
        }
        
        /*process in one page*/
        printk(KERN_WARNING "\n");
        printk(KERN_WARNING "==processing in the %dth page==\n", curr_page_no);
        printk(KERN_WARNING "unfinished = %zu\n", unfinished);
        printk(KERN_WARNING "begin_offset = %zu\n", begin_offset);
        printk(KERN_WARNING "PAGE_SIZE - begin_offset = %lu\n", PAGE_SIZE - begin_offset);
        
        if (unfinished > PAGE_SIZE - begin_offset) {
            printk(KERN_WARNING "because unfinished > PAGE_SIZE - begin_offset, SO\n");
            unfinished_in_this_page = PAGE_SIZE - begin_offset;
            printk(KERN_WARNING "set unfinished_in_this_page = PAGE_SIZE - begin_offset = %zu\n", unfinished_in_this_page);
        } else {
            printk(KERN_WARNING "because unfinished < PAGE_SIZE - begin_offset, SO\n");
            unfinished_in_this_page = unfinished;
            printk(KERN_WARNING "set unfinished_in_this_page = unfinished = %zu\n", unfinished);
        }
        
        should_finish_in_this_page = unfinished_in_this_page;
        unfinished_in_this_page_record = unfinished_in_this_page;
        
        printk(KERN_WARNING "\n");
        printk(KERN_WARNING "we need to write %zu amout of data into the %dth page", should_finish_in_this_page, curr_page_no);
        
        do {
            printk(KERN_WARNING "we are going to write %d amout of data at page %d, offset is %zu\n", unfinished_in_this_page, curr_page_no, begin_offset);
            unfinished_in_this_page = copy_from_user(page_address(curr_page_node_ptr->page) + begin_offset, buf, unfinished_in_this_page);
            printk(KERN_WARNING "finished writing for %d amount of data\n", unfinished_in_this_page_record - unfinished_in_this_page);
            buf += (unfinished_in_this_page_record - unfinished_in_this_page);
            
            begin_offset += (unfinished_in_this_page_record - unfinished_in_this_page);
            unfinished_in_this_page_record = unfinished_in_this_page;
        } while (unfinished_in_this_page > 0);
        /*end of processing in one page*/
        
        
        printk(KERN_WARNING "we succeed in writing %d amout of data into the the %dth page\n", should_finish_in_this_page, curr_page_no);
        
        
        
        unfinished -= should_finish_in_this_page;
        printk(KERN_WARNING "there are %d amout of data left to write\n", unfinished);
        
        ptr = ptr->next;
        curr_page_no += 1;
        first_one += 1;
    }
    
    
    printk(KERN_WARNING "======after writing...\n");
    printk(KERN_WARNING "size_written = count - unfinished\n");
    printk(KERN_WARNING "size_written = %zu - %zu\n", count, unfinished);
    size_written = count - unfinished;
    printk(KERN_WARNING "size_written = %zu\n", size_written);

    printk(KERN_WARNING "*f_pos += size_written\n");
    printk(KERN_WARNING "*f_pos = %lld\n", *f_pos);
    printk(KERN_WARNING "size_written = %zu\n", size_written);
    *f_pos = *f_pos + size_written;
    printk(KERN_WARNING "So set *f_pos = %lld\n\n", *f_pos);
    printk(KERN_WARNING "dev->data_size = %zu\n", dev->data_size);
    printk(KERN_WARNING "orig_f_pos + size_written = %zu + %zu = %zu\n", orig_f_pos, size_written, orig_f_pos + size_written);
    printk(KERN_WARNING "while dev->data_size = %zu\n", dev->data_size);
    printk(KERN_WARNING "Since, dev->data_size = max(dev->data_size, orig_f_pos + size_written)");
    dev->data_size = max(dev->data_size, orig_f_pos + size_written);
    printk(KERN_WARNING "SO dev->data_size = %d\n", dev->data_size);
    printk(KERN_WARNING "\n\n\n");
    
    return size_written;
}

#define SET_NPROC_OP 1
#define TEM_SET_NPROC _IOW(MYIOC_TYPE, SET_NPROC_OP, int)

/**
 * The ioctl function, which nothing needs to be done in this case.
 */
long asgn1_ioctl (struct file *filp, unsigned cmd, unsigned long arg) {
    int nr;
    int new_nprocs;
    int result;
    
    /* COMPLETE ME */
    /**
     * check whether cmd is for our device, if not for us, return -EINVAL
     *
     * get command, and if command is SET_NPROC_OP, then get the data, and
     set max_nprocs accordingly, don't forget to check validity of the
     value before setting max_nprocs
     */
    
    return -ENOTTY;
}


static int asgn1_mmap (struct file *filp, struct vm_area_struct *vma)
{
    unsigned long pfn;
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
    unsigned long len = vma->vm_end - vma->vm_start;
    unsigned long ramdisk_size = asgn1_device.num_pages * PAGE_SIZE;
    page_node *curr;
    unsigned long index = 0;
    
    /* COMPLETE ME */
    /**
     * check offset and len
     *
     * loop through the entire page list, once the first requested page
     *   reached, add each page with remap_pfn_range one by one
     *   up to the last requested page
     */
    return 0;
}

// the method this driver support
struct file_operations asgn1_fops = {
    .owner = THIS_MODULE,
    .read = asgn1_read,
    .write = asgn1_write,
    .unlocked_ioctl = asgn1_ioctl,
    .open = asgn1_open,
    .mmap = asgn1_mmap,
    .release = asgn1_release,
    .llseek = asgn1_lseek
};


//struct file_operations asgn1_proc_ops = {
//    .owner = THIS_MODULE,
//    .open = my_proc_open,
//    .llseek = seq_lseek,
//    .read = seq_read,
//    .release = seq_release,
//};
//
//static struct seq_operations my_seq_ops = {
//    .start = my_seq_start,
//    .next = my_seq_next,
//    .stop = my_seq_stop,
//    .show = my_seq_show
//};


//static void *my_seq_start(struct seq_file *s, loff_t *pos)
//{
//    if(*pos >= 1) return NULL;
//    else return &asgn1_dev_count + *pos;
//}
//
//static void *my_seq_next(struct seq_file *s, void *v, loff_t *pos)
//{
//    (*pos)++;
//    if(*pos >= 1) return NULL;
//    else return &asgn1_dev_count + *pos;
//}
//
//static void my_seq_stop(struct seq_file *s, void *v)
//{
//    /* There's nothing to do here! */
//}
//
//int my_seq_show(struct seq_file *s, void *v) {
//    /* COMPLETE ME */
//    /**
//     * use seq_printf to print some info to s
//     */
//    return 0;
//
//
//}

//static int my_proc_open(struct inode *inode, struct file *filp)
//{
//    return seq_open(filp, &my_seq_ops);
//}



/**
 * Initialise the module and create the master device
 */
int __init asgn1_init_module(void){
    int result;
    
    // 1. allocate major number
    
    result = alloc_chrdev_region(&asgn1_device.dev, 0, 1, "asgn1");
    asgn1_major = MAJOR(asgn1_device.dev);
    
    if (result < 0) {
        printk(KERN_WARNING "asgn1: can't get major number %d\n", asgn1_major);
        return result;
    }
    
    // 2. allocate cdev, and set ops and owner field
    INIT_LIST_HEAD(&asgn1_device.mem_list);
    asgn1_device.num_pages = 0;
    asgn1_device.data_size = 0;
    
    atomic_set(&asgn1_device.nprocs, 0);
    atomic_set(&asgn1_device.max_nprocs, 5);
    
    
    //    memset(asgn1_device, 0, sizeof(asgn1_dev));
    asgn1_device.cdev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
    cdev_init(asgn1_device.cdev, &asgn1_fops);
    asgn1_device.cdev->owner = THIS_MODULE;
    
    result = cdev_add(asgn1_device.cdev, asgn1_device.dev, 1);
    if (result) printk(KERN_WARNING "Error %d adding device temp", result);
    
    // 3. create class
    asgn1_device.class = class_create(THIS_MODULE, MYDEV_NAME);
    if (IS_ERR(asgn1_device.class)) {
        cdev_del(asgn1_device.cdev);
        unregister_chrdev_region(asgn1_device.dev, 1);
        printk(KERN_WARNING "%s: can't create udev class\n", "asgn1");
        result = -ENOMEM;
        return result;
    }
    
    // 4. create device
    asgn1_device.device = device_create(asgn1_device.class, NULL,
                                        asgn1_device.dev, "%s", MYDEV_NAME);
    if (IS_ERR(asgn1_device.device)) {
        printk(KERN_WARNING "%s: can't create udev device\n", MYDEV_NAME);
        result = -ENOMEM;
        goto fail_device;
    }
    
    
    printk(KERN_WARNING "set up udev entry\n");
    printk(KERN_WARNING "Hello world from %s\n", MYDEV_NAME);
    return 0;
    
    /* cleanup code called when any of the initialization steps fail */
fail_device:
    class_destroy(asgn1_device.class);
    
    /* COMPLETE ME */
    /* PLEASE PUT YOUR CLEANUP CODE HERE, IN REVERSE ORDER OF ALLOCATION */
    cdev_del(asgn1_device.cdev);
    unregister_chrdev_region(asgn1_device.dev, 1);
    printk(KERN_WARNING "%s: can't create udev class\n", "asgn1");
    result = -ENOMEM;
    
    return result;
}


/**
 * Finalise the module
 */
void __exit asgn1_exit_module(void){
    device_destroy(asgn1_device.class, asgn1_device.dev);
    class_destroy(asgn1_device.class);
    printk(KERN_WARNING "cleaned up udev entry\n");
    
    /* COMPLETE ME */
    /**
     * free all pages in the page list
     * cleanup in reverse order                             // what is the right order?
     */
    // 1. delete device
    printk(KERN_WARNING "unregistering and cleaning up a device that was created with a call to device_create...");
    device_destroy(asgn1_device.class, asgn1_device.dev);
    
    // 2. delete class
    printk(KERN_WARNING "destorying the pointer which have been created with a call to class_create...");
    class_destroy(asgn1_device.class);
    
    // 3. delete each data of allocated in each field
    free_memory_pages(&asgn1_device);
    
    cdev_del(asgn1_device.cdev);
    kfree(asgn1_device.cdev);
    
    printk(KERN_WARNING "unregistering a range of count device numbers...");
    unregister_chrdev_region(asgn1_device.dev, 1);
    printk(KERN_WARNING "Good bye from %s\n", MYDEV_NAME);
}


module_init(asgn1_init_module);
module_exit(asgn1_exit_module);


