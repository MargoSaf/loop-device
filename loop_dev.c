

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
#define ROW_LEN         41
#define BYTES_PER_ROW   16
#define HEAD_LEN        7

static int major_num;
// For large files, the loop_dev_write function may be called multiple times until reaching the end of the file.
// To handle large files effectively, the last_file and last_file_len variables are utilized to keep track of the last file written to and its length.
struct file *last_file; 
static size_t last_file_len = 0; 


// Function to handle opening of the device
static int loop_dev_open(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "loop_dev: Device opened\n");
    return 0;
}

// Function to handle releasing of the device
static int loop_dev_release(struct inode *inode, struct file *file)
{
    printk(KERN_INFO "loop_dev: Device closed\n");
    return 0;
}

// Function to open the file and return the offset
static loff_t open_file(struct file *input_file, struct file **output_file) 
{
    bool is_file_new = true;

    // Check if the current file is the same as the last one
    if (last_file == input_file)
    {
        is_file_new = false;
    }
    else
    {
        // Reset last file length when switching to a new file
        last_file_len = 0;
    }

    last_file = input_file;
    
    loff_t offset = 0;
    loff_t file_len = 0;

    // Open the output file with appropriate flags based on the offset
    if (is_file_new)
        *output_file = filp_open(OUTPUT_FILE, O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, 0644);
    else 
    {
        *output_file = filp_open(OUTPUT_FILE, O_WRONLY | O_LARGEFILE, 0644);
        if (!IS_ERR(*output_file)) 
        {
            file_len = vfs_llseek(*output_file, 0, SEEK_END);
            offset = (file_len > 8) ? file_len - 8 : 0;
        }
    }

    if (IS_ERR(*output_file)) 
    {
        printk(KERN_ALERT "Failed to open output file\n");
        return PTR_ERR(*output_file);
    }

    return offset;
}

// Function to write data to the file
static ssize_t write_file(struct file *output_file, unsigned char *hex_buffer, size_t length, loff_t offset ) 
{
    int ret = 0;

    // Write data to the file at the specified offset
    ret = kernel_write(output_file, hex_buffer, length, &offset);

    if (ret < 0) 
    {
        printk(KERN_ALERT "Failed to write data to output file\n");
        filp_close(output_file, NULL);
        return ret;
    }

    return ret;
}
// Function to handle write operations to the loop device
static ssize_t loop_dev_write(struct file *file, const char *buffer, size_t length, loff_t *offset)
{
    struct file *output_file;
    loff_t file_offset = open_file(file, &output_file);
    
    unsigned char row[ROW_LEN];
    unsigned char last_written_row[ROW_LEN];

    size_t total_bytes_read = 0;
    bool is_row_repeating = false;

    while (total_bytes_read < length)
    {
        size_t row_current_len = 0;
        size_t i = 0;

        while (i < BYTES_PER_ROW && i + total_bytes_read < length)
        {
            // Convert bytes to hex format and store in row
            sprintf(row + row_current_len, " ");
            row_current_len++;
            if (i + 1 + total_bytes_read < length)
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

            if (i + 1 + total_bytes_read < length)
                i += 2;
            else
                i++;
        }

        // Check if the current row is different from the last written row
        if (total_bytes_read == 0 || memcmp(last_written_row, row, ROW_LEN) != 0)
        {
            unsigned char temp[ROW_LEN + HEAD_LEN];
            if (is_row_repeating)
            {
                
                sprintf(temp, "*\n");
                int ret = write_file(output_file, temp, 2 , file_offset);
                file_offset+=2;
                is_row_repeating = false;
            }
            
            // Write the offset of the current row
            sprintf(temp, "%07zx", total_bytes_read + last_file_len);

            // Copy the current row to the hex buffer
            memcpy(temp + HEAD_LEN, row, row_current_len);

            memcpy(last_written_row, row, ROW_LEN);

            // Add spaces to fill up the row length
            int space_count = ROW_LEN - row_current_len - 1;
            if (space_count > 0)
            {
                sprintf(temp + HEAD_LEN + row_current_len, "%*s", space_count, " ");
            }

            // Mark the end of the current row
            sprintf(temp + ROW_LEN + HEAD_LEN - 1, "\n");
            int ret = write_file(output_file, temp, ROW_LEN + HEAD_LEN, file_offset);
            file_offset += ROW_LEN + HEAD_LEN;
            if (ret < 0)
            {
              printk(KERN_ALERT "Failed to write data to output file\n");
            }
        }
        else
        {
            is_row_repeating = true;
        }

        total_bytes_read += i;
    }
    unsigned char temp[HEAD_LEN + 1];
    // Write the offset of the end of the data
    sprintf(temp, "%07zx", total_bytes_read + last_file_len);
    //file_offset += HEAD_LEN;

    // Mark the end of the data
    sprintf(temp + HEAD_LEN, "\n");
    int ret = write_file(output_file, temp, HEAD_LEN+1 , file_offset);
    last_file_len += total_bytes_read;

    return length;
}

// Structure defining file operations for the loop device
static struct file_operations fops = {
    .open = loop_dev_open,
    .release = loop_dev_release,
    .read = NULL,
    .write = loop_dev_write,
};

// Initialization function for the loop device module
static int __init loop_dev_init(void)
{
    // Register a character device
    major_num = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_num < 0)
    {
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

