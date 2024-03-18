#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/hex.h>
#include <linux/slab.h> 
#include <linux/file.h>
#include <linux/buffer_head.h>

#define DEVICE_NAME     "loop_dev"
#define OUTPUT_FILE     "/tmp/output"
#define ROW_LEN	        48
#define BYTES_PER_ROW   16
#define HEAD_LEN        7

static int major_num;
struct file *last_file;
static size_t last_file_len = 0;

static int loop_dev_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "loop_dev: Device opened\n");
    return 0;
}

static int loop_dev_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "loop_dev: Device closed\n");
    return 0;
}

static ssize_t write_in_file(unsigned char *hex_buffer, size_t length, bool is_file_new) 
{
    struct file *output_file = NULL;
    int ret = 0;
    loff_t offset = 0;
    loff_t file_len = 0;
    // Open the output file with appropriate flags based on the offset
    if (is_file_new)
        output_file = filp_open(OUTPUT_FILE, O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, 0666);
    else 
    {
        output_file = filp_open(OUTPUT_FILE, O_WRONLY | O_LARGEFILE, 0666);
        if (!IS_ERR(output_file)) 
        {
            file_len = vfs_llseek(output_file, 0, SEEK_END);
            offset = (file_len > 8) ? file_len - 8 : 0;
        }
    }

    if (IS_ERR(output_file)) 
    {
        printk(KERN_ALERT "Failed to open output file\n");
        return PTR_ERR(output_file);
    }

    // Write data to the file at the specified offset
    ret = kernel_write(output_file, hex_buffer, length, &offset);

    if (ret < 0) 
    {
        printk(KERN_ALERT "Failed to write data to output file\n");
        filp_close(output_file, NULL);
        return ret;
    }

    // Close the output file
    filp_close(output_file, NULL);
    return ret;
}


static ssize_t loop_dev_write(struct file *file, const char *buffer, size_t length, loff_t *offset)
{
    bool new_file = true;
    if(last_file == file)
    {
       new_file = false;

    } else last_file_len = 0;
    
    last_file = file;


    unsigned char *hex_buffer = kmalloc(( length / BYTES_PER_ROW + 2) * ROW_LEN, GFP_KERNEL); 
    unsigned char row[ROW_LEN - HEAD_LEN];
    unsigned char last_writen_row[ROW_LEN - HEAD_LEN];

    if (!hex_buffer) 
    {
        printk(KERN_ALERT "Failed to allocate memory\n");
        return -ENOMEM;
    }
    
    size_t total_bytes_read = 0;
    size_t hex_buffer_index = 0;
    bool is_exist = false;
    
    while(total_bytes_read < length)
    {
    	size_t row_current_len = 0;
	size_t i = 0;
	
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

	if(total_bytes_read == 0 || memcmp(last_writen_row, row, ROW_LEN - HEAD_LEN) != 0)
	{
            if(is_exist)
            {
	        sprintf(hex_buffer + hex_buffer_index, "*\n");
                hex_buffer_index += 2;
                is_exist = false;
            }
            
            sprintf(hex_buffer + hex_buffer_index, "%07zx", total_bytes_read + last_file_len);
            hex_buffer_index+=7;
	    memcpy(hex_buffer + hex_buffer_index, row, row_current_len);
            hex_buffer_index += row_current_len;
            memcpy(last_writen_row,row, ROW_LEN - HEAD_LEN);
            int space_count = ROW_LEN - row_current_len - 1 - HEAD_LEN;
            if(space_count > 0)
            {
    	        sprintf(hex_buffer + hex_buffer_index, "%*s", space_count, " ");
    	    }
    	
    	    hex_buffer_index = hex_buffer_index + space_count;
            sprintf(hex_buffer + hex_buffer_index, "\n");
            hex_buffer_index++;
	}
        else
        {
            is_exist = true;
        }
        
        total_bytes_read += i;
        
    }

        

    sprintf(hex_buffer + hex_buffer_index, "%07zx", total_bytes_read + last_file_len);
    hex_buffer_index += HEAD_LEN;
    
    sprintf(hex_buffer + hex_buffer_index, "\n");
    hex_buffer_index++;
       last_file_len += total_bytes_read;
    int ret = write_in_file(hex_buffer, hex_buffer_index, new_file);
    if (ret < 0) 
    {
        printk(KERN_ALERT "Failed to write data to output file\n");
    }
    kfree(hex_buffer); 
 
    return length;
}

static struct file_operations fops = {
    .open = loop_dev_open,
    .release = loop_dev_release,
    .read = NULL,
    .write = loop_dev_write,
};

static int __init loop_dev_init(void)
{
    major_num = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_num < 0) {
        printk(KERN_ALERT "Failed to register a major number\n");
        return major_num;
    }
    printk(KERN_INFO "Registered a major number %d\n", major_num);
    return 0;
}

static void __exit loop_dev_exit(void)
{
    unregister_chrdev(major_num, DEVICE_NAME);
    printk(KERN_INFO "Unregistered the major number %d\n", major_num);
}

module_init(loop_dev_init);
module_exit(loop_dev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Margo");
MODULE_DESCRIPTION("A loop character device driver");

