/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Ime Asamudo"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;
struct aesd_tmp_buffer data_stream;


int add_to_tmp_buffer(struct aesd_tmp_buffer *tmp_buffer, char *string, size_t str_len)
{
    PDEBUG("Appending to dynamic buffer");

    char *new_data;
    new_data = krealloc(tmp_buffer->data, tmp_buffer->size + str_len + 1, GFP_KERNEL);
    if(!new_data)
    {
        return -ENOMEM;
    }
    PDEBUG("Memory was allocated.");
    tmp_buffer->data = new_data;

    memcpy(tmp_buffer->data + tmp_buffer->size , string, str_len);
    tmp_buffer->size += str_len;
    tmp_buffer->data[tmp_buffer->size] = '\0';

    PDEBUG("String added : %s", tmp_buffer->data);

    return 0;
}

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");

    struct aesd_dev *dev;

    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    
    return 0; 
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count, *f_pos);
    /**
     * TODO: handle read
     */
    struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
    struct aesd_buffer_entry *entry;
    size_t char_offset;
    size_t bytes_to_read = count;


    mutex_lock_interruptible(&dev->lock);
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(dev->buffer, *f_pos, &char_offset); 
    if(!entry)
    {
        retval = 0;
        goto out;
    }

    if(entry->size - char_offset < bytes_to_read)
       bytes_to_read = entry->size - char_offset;

    if(copy_to_user(buf, entry->buffptr + char_offset, bytes_to_read))
    {
        retval = -EFAULT;
        goto out;   
    }

    *f_pos += bytes_to_read;
    retval = bytes_to_read;

    out:    
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);

    struct aesd_dev *dev = (struct aesd_dev *)filp->private_data;
    struct aesd_buffer_entry entry;
    char *string = kmalloc(count, GFP_KERNEL);

    if(!string)
    {
        retval = -ENOMEM;
        goto out;
    }

    if(copy_from_user(string, buf, count))
    {
        kfree(string);
        retval = -EFAULT;
        goto out;
    }

    
    if(add_to_tmp_buffer(&data_stream, string, count) < 0)
    {
        kfree(string);
        retval = -ENOMEM;
        goto out;
    }
    kfree(string);


    while(data_stream.size > 0)
    {
        char *newline_pos = memchr(data_stream.data, '\n', data_stream.size);
        if(!newline_pos)
        {
            retval = data_stream.size;
            break;
        }

        size_t entry_size = newline_pos - data_stream.data + 1; 

        entry.buffptr = kmalloc(entry_size, GFP_KERNEL);
        if(!entry.buffptr)
        {
            retval = -ENOMEM;
            break;
        }

        memcpy(entry.buffptr, data_stream.data, entry_size);
        entry.size = entry_size;

        memmove(data_stream.data, data_stream.data + entry_size, data_stream.size - entry_size);
        data_stream.size -= entry_size;

        if(mutex_lock_interruptible(&dev->lock))
        {
            kfree(entry.buffptr);
            retval = -ERESTARTSYS;
            break;
        }
        if(dev->buffer->full)
        {
            kfree(dev->buffer->entry[dev->buffer->out_offs].buffptr);
            dev->buffer->entry[dev->buffer->out_offs].buffptr = NULL;
            dev->buffer->entry[dev->buffer->out_offs].buffptr = 0;
        }
        aesd_circular_buffer_add_entry(dev->buffer, &entry);
        mutex_unlock(&dev->lock);


        *f_pos += entry.size;
        retval += entry.size;
    }

   out: 
   return retval;
}

struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}
/**
 * TODO: cleanup AESD specific poritions here as necessary
 */
void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);
    unregister_chrdev_region(devno, 1);

    if(aesd_device.buffer != NULL)
    {
        int i = 0;
        for(i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++)
        {
            if(aesd_device.buffer->entry[i].buffptr != NULL)
            {
                kfree(aesd_device.buffer->entry[i].buffptr);
            }
        }
        kfree(aesd_device.buffer);
        aesd_device.buffer = NULL;
    }

    if(data_stream.data != NULL)
    {
        kfree(data_stream.data);
        data_stream.data = NULL;
        data_stream.size = 0;
    }
    
    printk(KERN_ALERT "Goodbye from aesd char device\n");
    return;
}



/**
 * TODO: initialize the AESD specific portion of the device
 */
int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    /*Staticly setting major and minor numbers.
    Assume aesd_major and minor have already been set
    And just assigns them to dev
    reserves a range of character device numbers*/
    if(aesd_major)
    {
       dev = MKDEV(aesd_major, aesd_minor);
       result = register_chrdev_region(dev, 1, "aesdchar");
    }
   /*Dynamically set a major and minor number for character device*/
   else
    {
        result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
        aesd_major = MAJOR(dev);
    }
    if (result < 0) 
    {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }

    memset(&aesd_device,0,sizeof(struct aesd_dev));
    aesd_device.buffer = kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);
    if(!aesd_device.buffer)
    {
        result = -ENOMEM;
        goto fail;

    }
    aesd_circular_buffer_init(aesd_device.buffer);
    mutex_init(&aesd_device.lock);
    result = aesd_setup_cdev(&aesd_device);
    if( result != 0 ) {
       // unregister_chrdev_region(dev, 1);
        aesd_cleanup_module();
    }

    data_stream.data = NULL;
    data_stream.size = 0;
    printk(KERN_ALERT "Hello from aesd char device\n");
    return result;

fail:
    aesd_cleanup_module();
    return result;

}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
