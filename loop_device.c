#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/hex.h>
#include <linux/slab.h> 

#define DEVICE_NAME     "loop_device"
#define OUTPUT_FILE     "/tmp/output"
#define ROW_LEN	        48
#define BYTES_PER_ROW   16

static int major_num;

static int chardev_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "chardev_example: Device opened\n");
    return 0;
}

static int chardev_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "chardev_example: Device closed\n");
    return 0;
}

static ssize_t chardev_write_in_file(unsigned char *hex_buffer,size_t length, loff_t *offset)
{
    struct file *output_file = NULL;
    int ret = 0;
    
    // Open the output file with the O_TRUNC flag
    output_file = filp_open(OUTPUT_FILE, O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, 0644);
    if (IS_ERR(output_file)) 
    {
        printk(KERN_ALERT "Failed to open output file\n");
        kfree(hex_buffer); 
        return PTR_ERR(output_file);
    }
    
    ret = kernel_write(output_file, hex_buffer, length, 0);
    
    if (ret < 0) 
    {
        printk(KERN_ALERT "Failed to write data to output file\n");
        filp_close(output_file, NULL);
        kfree(hex_buffer); 
        return ret;
    }
    
    // Close the output file
    filp_close(output_file, NULL);
    return ret;
}

static ssize_t chardev_write(struct file *file, const char *buffer, size_t length, loff_t *offset)
{

    unsigned char *hex_buffer = NULL; 
    hex_buffer = kmalloc(( length / BYTES_PER_ROW + 2) * ROW_LEN, GFP_KERNEL); 
    unsigned char row[ROW_LEN];
    if (!hex_buffer) 
    {
        printk(KERN_ALERT "Failed to allocate memory\n");
        return -ENOMEM;
    }
    
    size_t total_bytes_read = 0;
    size_t hex_buffer_index = 0;

    while(total_bytes_read < length)
    {
        sprintf(hex_buffer + hex_buffer_index, "%07zx", total_bytes_read);
        hex_buffer_index+=7;
        // Print hexadecimal representation
	size_t i = 0;
	size_t row_current_len = 0;
	while ( i < BYTES_PER_ROW && i + total_bytes_read < length) 
	{
            sprintf(row + row_current_len, " ");
            row_current_len++;
            if(i + 1 + total_bytes_read < length)
            {
                sprintf(row + row_current_len, "%02x", buffer[total_bytes_read + i + 1]);
            }
            else
            {
                sprintf(row + row_current_len, "00");
            }
		
            row_current_len += 2;
		
            sprintf(row + row_current_len, "%02x", buffer[total_bytes_read + i]);
            row_current_len += 2;	       
  
            if(i + 1 + total_bytes_read < length)
                i += 2;
            else
                i++;
        }
        total_bytes_read += i;
        
        memcpy(hex_buffer + hex_buffer_index, row, row_current_len);
        hex_buffer_index += row_current_len;
        
        int space_count = ROW_LEN - row_current_len - 8;
        if(space_count > 0)
        {
    	    sprintf(hex_buffer + hex_buffer_index, "%*s", space_count, " ");
    	}
    	
    	hex_buffer_index = hex_buffer_index + space_count;
        sprintf(hex_buffer + hex_buffer_index, "\n");
        hex_buffer_index++;

    }

        

    sprintf(hex_buffer + hex_buffer_index, "%07zx", total_bytes_read);
    hex_buffer_index+=7;
    
    sprintf(hex_buffer + hex_buffer_index, "\n");
    hex_buffer_index++;
    
    int ret = chardev_write_in_file(hex_buffer, hex_buffer_index, offset);
   
    kfree(hex_buffer); 

    return length;
}

static struct file_operations fops = {
    .open = chardev_open,
    .release = chardev_release,
    .read = NULL,
    .write = chardev_write,
};

static int __init chardev_init(void)
{
    major_num = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_num < 0) {
        printk(KERN_ALERT "Failed to register a major number\n");
        return major_num;
    }
    printk(KERN_INFO "Registered a major number %d\n", major_num);
    return 0;
}

static void __exit chardev_exit(void)
{
    unregister_chrdev(major_num, DEVICE_NAME);
    printk(KERN_INFO "Unregistered the major number %d\n", major_num);
}

module_init(chardev_init);
module_exit(chardev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Margo");
MODULE_DESCRIPTION("A loop character device driver");

