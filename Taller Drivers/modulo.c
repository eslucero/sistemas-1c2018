#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

char * mapa;
static dev_t hello_devno;
struct cdev hello_dev;
static struct class *hello_class;
char * dato;

ssize_t hello_read(struct file *filp, char __user *data, size_t s, loff_t *off){
  if (data == NULL)
    return 0;
  copy_to_user(data, mapa, s);
  return 9;
}

ssize_t hello_write(struct file *filp, char __user *data, size_t s, loff_t *off){
  if (data == NULL){
   printk(KERN_ALERT "No hay datos!\n");
   return s;
  }
  dato = vmalloc(s);
  copy_from_user(dato, data, s);

  if (*dato == 'N'){
    if (mapa[4] == 48){
       printk(KERN_ALERT "No podes ir al norte!\n");
       return s;
    }
    mapa[4] = (char) (mapa[4] - 1);
    printk(KERN_ALERT "Te moviste al norte\n");
  }  
  else if (*dato == 'S'){
    if (mapa[4] == 57){
       printk(KERN_ALERT "No podes ir al sur!\n");
       return s;
    }
    mapa[4] = (char) (mapa[4] + 1);
    printk(KERN_ALERT "Te moviste al sur\n");
  } 

  if (*dato == 'W'){
    if (mapa[2] == 48){
       printk(KERN_ALERT "No podes ir al oeste!\n");
       return s;
    }
    mapa[2] = (char) (mapa[2] - 1);
    printk(KERN_ALERT "Te moviste al oeste\n");
  }  
  else if (*dato == 'E'){
    if (mapa[2] == 57){
       printk(KERN_ALERT "No podes ir al este!\n");
       return s;
    }
    mapa[2] = (char) (mapa[2] + 1);
    printk(KERN_ALERT "Te moviste al este\n");
  }

  vfree(dato);
  return s;
}

static struct file_operations mis_operaciones = {
  .owner = THIS_MODULE,
  .read = hello_read,
  .write = hello_write,
};

static int __init hello_init(void){
  printk(KERN_ALERT "Hola, Sistemas Operativos!\n");
  mapa = vmalloc(9);
  mapa[0] = 'A'; 
  mapa[1] = ' '; 
  mapa[2] = '0'; 
  mapa[3] = '-'; 
  mapa[4] = '0'; 
  mapa[5] = ':'; 
  mapa[6] = ' '; 
  mapa[7] = '_'; 
  mapa[8] = '\n'; 
  printk(KERN_ALERT "%s", mapa);
  if (alloc_chrdev_region(&hello_devno, 0, 1, "modulo") == -1){
    printk(KERN_ALERT  "LA CAGASTE\n");
    return 1;
  }
  cdev_init(&hello_dev, &mis_operaciones);
  if (cdev_add(&hello_dev, hello_devno, 1) == -1){
    printk(KERN_ALERT  "LA CAGASTE con el cdev\n");
    return 1;
  }
  hello_class = class_create(THIS_MODULE, "modulo");
  device_create(hello_class, NULL, hello_devno, NULL, "modulo");

  return 0;
}

static void __exit hello_exit(void){
  device_destroy(hello_class, hello_devno);
  class_destroy(hello_class);
  cdev_del(&hello_dev);
  unregister_chrdev_region(hello_devno, 1);
  vfree(mapa);
  printk(KERN_ALERT "Adios, mundo cruel...\n");
}


module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Bruno de los Palotes");
MODULE_DESCRIPTION("Aca va algo");
