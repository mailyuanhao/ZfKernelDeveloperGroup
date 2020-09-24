#include <linux/init.h>          
#include <linux/module.h>         
#include <linux/device.h>        
#include <linux/kernel.h>         
#include <linux/fs.h>            
#include <asm/uaccess.h>          
#include <linux/mutex.h>	       
#define  DEVICE_NAME "zfchar"   
#define  CLASS_NAME  "zf"       
#define BUFFER_LENGTH 256        

MODULE_LICENSE("GPL");           
MODULE_AUTHOR("Great Wall");    
MODULE_DESCRIPTION("A simple Linux char driver for the ZF");  
MODULE_VERSION("0.1");            

static int    majorNumber;                  ///< Store the device number -- determined automatically
static char   message[BUFFER_LENGTH] = {0}; ///< Memory for the string that is passed from userspace
static short  size_of_message = 0;          ///< Used to remember the size of the string stored
static short  head = 0;                     ///< Head of the message array
static short  tail = 0;                     ///< Tail of the message array
static int    numberOpens = 0;              ///< Counts the number of times the device is opened
static struct class*  zfcharClass  = NULL; ///< The device-driver class struct pointer
static struct device* zfcharDevice = NULL; ///< The device-driver device struct pointer

static DEFINE_MUTEX(zfchar_mutex);	    ///< Macro to declare a new mutex

/// The prototype functions for the character driver -- must come before the struct definition
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

static int __init zfchar_init(void){
   printk(KERN_INFO "ZFChar: Initializing the ZFChar LKM\n");

   // Try to dynamically allocate a major number for the device
   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "ZFChar failed to register a major number\n");
      return majorNumber;
   }
   printk(KERN_INFO "ZFChar: registered correctly with major number %d\n", majorNumber);

   // Register the device class
   zfcharClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(zfcharClass)){           // Check for error and clean up if there is
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to register device class\n");
      return PTR_ERR(zfcharClass);     // Correct way to return an error on a pointer
   }
   printk(KERN_INFO "ZFChar: device class registered correctly\n");

   // Register the device driver
   zfcharDevice = device_create(zfcharClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(zfcharDevice)){          // Clean up if there is an error
      class_destroy(zfcharClass);      // Repeated code but the alternative is goto statements
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to create the device\n");
      return PTR_ERR(zfcharDevice);
   }
   printk(KERN_INFO "ZFChar: device class created correctly\n"); // Made it! device was initialized
   mutex_init(&zfchar_mutex);          // Initialize the mutex dynamically
   return 0;
}

static void __exit zfchar_exit(void){
   mutex_destroy(&zfchar_mutex);                       // destroy the dynamically-allocated mutex
   device_destroy(zfcharClass, MKDEV(majorNumber, 0)); // remove the device
   class_unregister(zfcharClass);                      // unregister the device class
   class_destroy(zfcharClass);                         // remove the device class
   unregister_chrdev(majorNumber, DEVICE_NAME);         // unregister the major number
   printk(KERN_INFO "ZFChar: Goodbye from the LKM!\n");
}

static int dev_open(struct inode *inodep, struct file *filep){
   numberOpens++;
   printk(KERN_INFO "ZFChar: Device has been opened %d time(s)\n", numberOpens);
   return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset){
   int error_count1 = 0, error_count2 = 0;
   int rdlen = len <= size_of_message ? len : size_of_message;
   int rdlen1 = tail + rdlen < BUFFER_LENGTH ? rdlen : BUFFER_LENGTH - tail;
   int rdlen2 = rdlen - rdlen1;

   printk(KERN_INFO "ZFChar: tail is %d, rdlen is %d, rdlen1 is %d, rdlen2 is %d\n", tail , rdlen, rdlen1, rdlen2);

   // copy_to_user has the format ( * to, *from, size) and returns 0 on success
   error_count1 = copy_to_user(buffer, message + tail, rdlen1);
   error_count2 = copy_to_user(buffer + rdlen1, message, rdlen2);
   
   tail += rdlen;
   tail %= BUFFER_LENGTH;
   size_of_message -= rdlen;
   printk(KERN_INFO "ZFChar: Sent %d characters to the user and remain len is %d\n", rdlen, size_of_message);
   return rdlen; // clear the position to the start and return 0

}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
   int i = 0, j = 0;
   int wtlen = len <= (BUFFER_LENGTH - size_of_message) ? len : (BUFFER_LENGTH - size_of_message);
   
   int wtlen1 = BUFFER_LENGTH - head <= wtlen ? BUFFER_LENGTH - head : wtlen;
   int wtlen2 = wtlen - wtlen1;

   printk(KERN_INFO "ZFChar: head is %d\n", head);
   for (i = head; i < head + wtlen1; i++){
      message[i] = buffer[j] ^ 0x55;
      j++;
   }

   for (i = 0; i < wtlen2; i++){
      message[i] = buffer[j] ^ 0x55;
      j++;
   }

   head += wtlen;
   head %= BUFFER_LENGTH;

   size_of_message += wtlen;                         // store the length of the stored message
   printk(KERN_INFO "ZFChar: Received %zu characters from the user, stored %d characters in array and total len is %d\n", len, wtlen, size_of_message);
   return wtlen;
}

static int dev_release(struct inode *inodep, struct file *filep){
   // mutex_unlock(&zfchar_mutex);                      // release the mutex (i.e., lock goes up)
   printk(KERN_INFO "ZFChar: Device successfully closed\n");
   return 0;
}

module_init(zfchar_init);
module_exit(zfchar_exit);
