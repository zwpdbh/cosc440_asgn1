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
MODULE_AUTHOR("Zhao Wei");
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

struct proc_dir_entry *proc;                /*for proc entry*/

/**
 * Helper function which add a page and set the data_size
 */
void add_pages(int num) {
    int i;
    
    for (i = 0; i < num; i++) {
        page_node *pg;
        pg = kmalloc(sizeof(struct page_node_rec), GFP_KERNEL);
        pg->page = alloc_page(GFP_KERNEL);
        INIT_LIST_HEAD(&pg->list);
        printk(KERN_WARNING "before adding new page, num_pages = %d\n", asgn1_device.num_pages);
        list_add_tail(&pg->list, &asgn1_device.mem_list);
        asgn1_device.num_pages += 1;
        printk(KERN_WARNING "after adding new page, num_pages = %d\n", asgn1_device.num_pages);
    }
    
}


/**
 * This function frees all memory pages held by the module.
 */
void free_memory_pages(struct asgn1_dev_t *dev) {
    
    struct page_node_rec *page_node_current_ptr, *page_node_next_ptr;
    int count = 1;
    if (!list_empty(&dev->mem_list)) {
        list_for_each_entry_safe(page_node_current_ptr, page_node_next_ptr, &dev->mem_list, list) {
            __free_page(page_node_current_ptr->page);
            list_del(&page_node_current_ptr->list);
            kfree(page_node_current_ptr);
            
            printk(KERN_WARNING "freed %d: page\n", count);
            count += 1;
        }
        dev->num_pages = 0;
        dev->data_size = 0;
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
    //    struct asgn1_dev_t *dev;
    int nprocs;
    int max_nprocs;
    
    //    dev = container_of(inode->i_cdev, struct asgn1_dev_t, cdev);
    //    filp->private_data = dev;
    filp->private_data = &asgn1_device;
    
    
    atomic_inc(&asgn1_device.nprocs);
    
    nprocs = atomic_read(&asgn1_device.nprocs);
    max_nprocs = atomic_read(&asgn1_device.max_nprocs);
    if (nprocs > max_nprocs) {
        return -EBUSY;
    }
    
    if (filp->f_mode == FMODE_WRITE) {
        free_memory_pages(&asgn1_device);
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
    if(atomic_read(&asgn1_device.nprocs) > 0) {
        atomic_sub(1, &asgn1_device.nprocs);
    }
    
    return 0;
}



/**
 * This function reads contents of the virtual disk and writes to the user
 */
ssize_t asgn1_read(struct file *filp, char __user *buf, size_t count,
                   loff_t *f_pos) {
    /*record offset in current page*/
    size_t offset;
    
    size_t unfinished;
    size_t result;
    size_t finished;
    size_t total_finished;
    
    int page_index;
    int curr_page_index;
    
    struct list_head *ptr;
    struct page_node_rec *page_ptr;
    int processing_count;
    
    //dev = filp->private_data;
    total_finished = 0;
    processing_count = 0;
    
    printk(KERN_WARNING "======READING========\n");
    printk(KERN_WARNING "*f_pos = %ld\n", (long) *f_pos);
    
    /*if the seeking position is bigger than the data_size, return 0*/
    if (*f_pos >= asgn1_device.data_size) {
        return 0;
    }
    
    page_index = *f_pos / PAGE_SIZE;
    offset = *f_pos % PAGE_SIZE;
    curr_page_index = 0;
    
    /*check the limit of amount of work needed to be done*/
    if (*f_pos + count > asgn1_device.data_size) {
        unfinished = asgn1_device.data_size - *f_pos;
    } else {
        unfinished = count;
    }
    
    ptr = asgn1_device.mem_list.next;
    /*make sure the current operating page is the page computed from *f_pos / PAGE_SIZE*/
    while (curr_page_index < page_index) {
        ptr = ptr->next;
        curr_page_index += 1;
    }
    
    
    printk(KERN_WARNING "unfinished = %d\n", unfinished);
    
    do {
        page_index = *f_pos / PAGE_SIZE;
        offset = *f_pos % PAGE_SIZE;
        
        printk(KERN_WARNING "curr_page_index = %d\n", curr_page_index);
        printk(KERN_WARNING "page_index = %d\n", page_index);
        printk(KERN_WARNING "offset = %d\n", offset);
        
        if (page_index != curr_page_index) {
            printk(KERN_WARNING "curr_page_index = %d, *f_pos / PAGE_SIZE = page_index = %d", curr_page_index, page_index);
            ptr = ptr->next;
            curr_page_index += 1;
            printk(KERN_WARNING "go to next page: %d\n", curr_page_index);
        }
        
        page_ptr = list_entry(ptr, page_node, list);
        if (unfinished > PAGE_SIZE - offset) {
            printk(KERN_WARNING "processing %ld amout of data(PAGE_SIZE - offset)\n", (long)(PAGE_SIZE - offset));
            result = copy_to_user(buf + total_finished, (page_address(page_ptr->page) + offset), PAGE_SIZE - offset);
            finished = PAGE_SIZE - offset -result;
        } else {
            printk(KERN_WARNING "processing %ld amout of data(unfinished)\n", (long int)unfinished);
            result = copy_to_user(buf + total_finished, (page_address(page_ptr->page) + offset), unfinished);
            finished = unfinished - result;
        }
        
        if (result < 0) {
            break;
        }
        
        processing_count += 1;
        unfinished -= finished;
        total_finished += finished;
        *f_pos += finished;
        
        printk(KERN_WARNING "===processing_count = %d===\n", processing_count);
        printk(KERN_WARNING "finished = %d\n", finished);
        printk(KERN_WARNING "unfinished = %d\n", unfinished);
        printk(KERN_WARNING "total_finished = %d\n", total_finished);
        printk(KERN_WARNING "\n");
    } while (unfinished >0);
    
    printk(KERN_WARNING "===END of READING, return %d===\n", total_finished);
    printk(KERN_WARNING "\n\n\n");
    return total_finished;
}



static loff_t asgn1_lseek (struct file *file, loff_t offset, int whence)
{
    loff_t newpos = 0;
    
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
            newpos = asgn1_device.data_size + offset;
            break;
        default:
            break;
    }
    if (newpos < 0 || newpos > asgn1_device.num_pages * PAGE_SIZE) {
        return -EINVAL;
    } else {
        file->f_pos = newpos;
        printk (KERN_INFO "Seeking to pos=%ld\n", (long)newpos);
    }
    
    return newpos;
}





/**
 * This function writes from the user buffer to the virtual disk of this
 * module
 */
ssize_t asgn1_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    
    /*record offset in current page*/
    size_t offset;
    
    size_t unfinished;
    size_t result;
    size_t finished;
    size_t total_finished;
    
    
    /*page_no = *f_pos / PAGE_SIZE, while curr_page_index is used to track which page */
    int page_index;
    int curr_page_index;
    
    //struct asgn1_dev_t *dev;
    //dev = filp->private_data;
    struct list_head *ptr;
    struct page_node_rec *page_ptr;
    size_t orig_f_pos = *f_pos;
    int processing_count = 0;
    
    unfinished = count;
    total_finished = 0;
    
    printk(KERN_WARNING "======WRITING========\n");
    /*initializ*/
    page_index = *f_pos / PAGE_SIZE;
    offset = *f_pos % PAGE_SIZE;
    curr_page_index = 0;
    
    /*if the *f_pos exceed the max limit, return 0*/
    if (page_index >= asgn1_device.num_pages) {
        printk(KERN_WARNING "request page_index is: %d\n", page_index);
        printk(KERN_WARNING "currently, there are: %d pages\n", asgn1_device.num_pages);
        printk(KERN_WARNING "need to add %d pages\n", page_index - asgn1_device.num_pages + 1);
        add_pages(page_index - asgn1_device.num_pages + 1);
    }
    
    if(*f_pos > asgn1_device.num_pages * PAGE_SIZE) {
        printk(KERN_WARNING "*f_pos > asgn1_device.num_pages * PAGE_SIZE ?\n");
        printk(KERN_WARNING "*fpos = %ld\n", (long)*f_pos);
        printk(KERN_WARNING "asgn1_device.num_pages = %d\n", asgn1_device.num_pages);
        printk(KERN_WARNING "asgn1_device.num_pages * PAGE_SIZE = %ld\n", (long)asgn1_device.num_pages * PAGE_SIZE);
        return 0;
    }
    
    ptr = asgn1_device.mem_list.next;
    /*make sure the current operating page is the page computed from *f_pos*/
    while (curr_page_index < page_index && ptr){
        ptr = ptr->next;
        curr_page_index += 1;
    }
    
    printk(KERN_WARNING "===unfinished = %d...===\n", (int)unfinished);
    do {
        page_index = *f_pos / PAGE_SIZE;
        offset = *f_pos % PAGE_SIZE;
        
        printk(KERN_WARNING "offset = %d\n", offset);
        printk(KERN_WARNING "*f_pos = %ld\n", (long int)*f_pos);
        printk(KERN_WARNING "page_index = *f_pos / PAGE_SIZE = %d\n", page_index);
        
        if (page_index + 1 > asgn1_device.num_pages) {
            printk(KERN_WARNING "page_index = %d, while asgn1_device.num_pages = %d, need to add a new page.\n", page_index,  asgn1_device.num_pages);
            add_pages(1);
            ptr = ptr->next;
            curr_page_index += 1;
            printk(KERN_WARNING "go to next page: %d\n", curr_page_index);
        } else if (page_index + 1 <= asgn1_device.num_pages && page_index > curr_page_index) {
            printk(KERN_WARNING "page_index = %d, while curr_page_index = %d, need to move to next page.\n", page_index, curr_page_index);
            ptr = ptr->next;
            curr_page_index += 1;
            printk(KERN_WARNING "go to next page: %d\n", curr_page_index);
        }
        
        page_ptr = list_entry(ptr, page_node, list);
        
        if (unfinished < PAGE_SIZE - offset) {
            printk(KERN_WARNING "processing %ld amout of data(unfinished)\n", (long int)unfinished);
            result = copy_from_user(page_address(page_ptr->page) + offset, buf + total_finished, unfinished);
            finished = unfinished - result;
        } else {
            printk(KERN_WARNING "processing %ld amout of data(PAGE_SIZE - offset)\n", (long)(PAGE_SIZE - offset));
            result = copy_from_user(page_address(page_ptr->page) + offset, buf + total_finished, PAGE_SIZE - offset);
            finished = PAGE_SIZE - offset - result;
        }
        
        if (result < 0) {
            break;
        }
        
        unfinished -= finished;
        total_finished += finished;
        *f_pos += finished;
        processing_count += 1;
        
        printk(KERN_WARNING "===processing_count = %d===\n", processing_count);
        printk(KERN_WARNING "finished = %d\n", finished);
        printk(KERN_WARNING "unfinished = %d\n", unfinished);
        printk(KERN_WARNING "total_finished = %d\n", total_finished);
        printk(KERN_WARNING "\n");
    } while (unfinished > 0);
    
    asgn1_device.data_size = max(asgn1_device.data_size, orig_f_pos + total_finished);
    printk(KERN_WARNING "===END of WRITING, return %d===\n", total_finished);
    printk(KERN_WARNING "\n\n\n");
    
    return total_finished;
    
}

#define SET_NPROC_OP 1
#define TEM_SET_NPROC _IOW(MYIOC_TYPE, SET_NPROC_OP, int)

/**
 * The ioctl function, which nothing needs to be done in this case.
 *
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
    if(_IOC_TYPE(cmd) != MYIOC_TYPE) {
        return -EINVAL;
    }
    nr = _IOC_NR(cmd);
    
    switch (nr) {
        case SET_NPROC_OP:
            result=copy_from_user((int*)&new_nprocs,(int*)arg,sizeof(int));
            
            if(result!=0) return -EINVAL;
            if(new_nprocs > atomic_read(&asgn1_device.nprocs)){
                atomic_set(&asgn1_device.max_nprocs, new_nprocs);
                return 0;
            }
            break;
            
        default:
            break;
    }
    return -ENOTTY;
}


static int asgn1_mmap (struct file *filp, struct vm_area_struct *vma)
{
    unsigned long pfn;
    
    /*the page frame number is simply the physical address right-shifted by PAGE_SHIFT bits*/
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
    printk(KERN_WARNING "invoking mmap function\n");
    printk(KERN_WARNING "offset = %ld\n", offset);
    printk(KERN_WARNING "len = %ld\n", len);
    printk(KERN_WARNING "ramdisk_size = %ld\n", ramdisk_size);
    
    if (offset * PAGE_SIZE + len > ramdisk_size) {
        return -1;
    }
    
    list_for_each_entry(curr, &asgn1_device.mem_list, list) {
        printk(KERN_WARNING "looping until reach the offset page\n");
        if (index >= offset) {
            pfn = page_to_pfn(curr->page);
            
            if (PAGE_SIZE * (index - offset) >= len) {
                return 0;
            }
            
            if (remap_pfn_range(vma, vma->vm_start + PAGE_SIZE * (index - offset), pfn, PAGE_SIZE, vma->vm_page_prot)) {
                return -EAGAIN;
            }
        }
        index += 1;
    }
    return 0;
}


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


static void *my_seq_start(struct seq_file *s, loff_t *pos)
{
    if(*pos >= 1) return NULL;
    else return &asgn1_dev_count + *pos;
}

static void *my_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
    (*pos)++;
    if(*pos >= 1) return NULL;
    else return &asgn1_dev_count + *pos;
}

static void my_seq_stop(struct seq_file *s, void *v)
{
    /* There's nothing to do here! */
}

int my_seq_show(struct seq_file *s, void *v) {
    /* COMPLETE ME */
    /**
     * use seq_printf to print some info to s
     */
    
    seq_printf(s, "dev->nprocs = %d\n", atomic_read(&asgn1_device.nprocs));
    seq_printf(s, "dev->max_nprocs = %d\n", atomic_read(&asgn1_device.max_nprocs));
    seq_printf(s, "dev->num_pages = %d\n", asgn1_device.num_pages);
    seq_printf(s, "dev->data_size = %d\n", asgn1_device.data_size);
    
    return 0;
}


static struct seq_operations my_seq_ops = {
    .start = my_seq_start,
    .next = my_seq_next,
    .stop = my_seq_stop,
    .show = my_seq_show
};


static int my_proc_open(struct inode *inode, struct file *filp)
{
    return seq_open(filp, &my_seq_ops);
}



struct file_operations asgn1_proc_ops = {
    .owner = THIS_MODULE,
    .open = my_proc_open,
    .llseek = seq_lseek,
    .read = seq_read,
    .release = seq_release,
};


/**
 * Initialise the module and create the master device
 */
int __init asgn1_init_module(void){
    int rv;
    dev_t devno = MKDEV(asgn1_major, 0);
    
    if (asgn1_major) {
        rv = register_chrdev_region(devno, 1, MYDEV_NAME);
        if (rv < 0) {
            printk(KERN_WARNING "Can't use the major number %d; try atomatic allocation...\n", asgn1_major);
            rv = alloc_chrdev_region(&devno, 0, 1, MYDEV_NAME);
            asgn1_major = MAJOR(devno);
        }
    } else {
        rv = alloc_chrdev_region(&devno, 0, 1, MYDEV_NAME);
        asgn1_major = MAJOR(devno);
    }
    
    if (rv < 0) {
        return rv;
    }
    /*Initialize each fields of asgn1_device*/
    memset(&asgn1_device, 0, sizeof(struct asgn1_dev_t));
    asgn1_device.cdev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
    cdev_init(asgn1_device.cdev, &asgn1_fops);
    asgn1_device.cdev->owner = THIS_MODULE;
    INIT_LIST_HEAD(&asgn1_device.mem_list);
    asgn1_device.num_pages = 0;
    asgn1_device.data_size = 0;
    atomic_set(&asgn1_device.nprocs, 0);
    atomic_set(&asgn1_device.max_nprocs, 5);
    
    rv = cdev_add(asgn1_device.cdev, devno, 1);
    if (rv) {
        printk(KERN_WARNING "Error %d adding device %s", rv, MYDEV_NAME);
    }
    
    asgn1_device.class = class_create(THIS_MODULE, MYDEV_NAME);
    if (IS_ERR(asgn1_device.class)) {
        cdev_del(asgn1_device.cdev);
        unregister_chrdev_region(devno, 1);
        printk(KERN_WARNING "%s: can't create udev class\n", MYDEV_NAME);
        rv = -ENOMEM;
        return rv;
    }
    
    asgn1_device.device = device_create(asgn1_device.class, NULL, MKDEV(asgn1_major, 0), "%s", MYDEV_NAME);
    if (IS_ERR(asgn1_device.device)) {
        class_destroy(asgn1_device.class);
        cdev_del(asgn1_device.cdev);
        unregister_chrdev_region(devno, 1);
        printk(KERN_WARNING "%s: can't create udev device\n", MYDEV_NAME);
        rv = -ENOMEM;
        return rv;
    }
    
    /*create proc entry*/
    proc = proc_create_data(MYDEV_NAME, 0, NULL, &asgn1_proc_ops, NULL);
    
    printk(KERN_WARNING "\n\n\n");
    printk(KERN_WARNING "===Create %s driver succeed.===\n", MYDEV_NAME);
    
    return 0;
}


/**
 * Finalise the module
 */
void __exit asgn1_exit_module(void){
    device_destroy(asgn1_device.class, MKDEV(asgn1_major, 0));
    class_destroy(asgn1_device.class);
    free_memory_pages(&asgn1_device);
    
    cdev_del(asgn1_device.cdev);
    kfree(asgn1_device.cdev);
    
    remove_proc_entry(MYDEV_NAME, NULL);
    unregister_chrdev_region(MKDEV(asgn1_major, 0), 1);
    printk(KERN_WARNING "===GOOD BYE from %s===\n", MYDEV_NAME);
    
}


module_init(asgn1_init_module);
module_exit(asgn1_exit_module);

