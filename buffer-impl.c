/* Implemente aqui el driver para /dev/buffer */
/* Necessary includes for device drivers */
#include <linux/init.h>
/* #include <linux/config.h> */
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <linux/uaccess.h> /* copy_from/to_user */
#include "kmutex.h"

MODULE_LICENSE("Dual BSD/GPL");


/* Declaration of memory.c functions */
static int buffer_open(struct inode *inode, struct file *filp);
static int buffer_release(struct inode *inode, struct file *filp);
static ssize_t buffer_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
static ssize_t buffer_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);

void buffer_exit(void);
int buffer_init(void);

static struct file_operations buffer_fops = {
	read: buffer_read,
	write: buffer_write,
	open: buffer_open,
	release: buffer_release
};
/*declaracion de inicio y fin de modulos*/
module_init(buffer_init);
module_exit(buffer_exit);

/*Variables globales*/
#define BUFF_SIZE 3
static int buffer_major = 61;
#define MAX_SIZE 8192
static char *buffer_buffer[BUFF_SIZE];

static int cicloWrite;
static int writings;
static int readers;
static size_t curr_size[BUFF_SIZE]={0,0,0};
static int cicloRead;


/* El mutex y la condicion para buffer */
static KMutex mutex;
static KCondition cond;


int buffer_init(void){
	int result;

	result = register_chrdev(buffer_major,"buffer",&buffer_fops);
	if(result < 0 ){
		printk(
			"<1>buffer: cannot obtain major number %d\n",buffer_major);
		return result;
	}

	/*inicializo las cosas*/
	cicloWrite=0;
	cicloRead=0;
	writings = 0;
	readers = 0; 

	m_init(&mutex);
	c_init(&cond);


	buffer_buffer[0] = kmalloc(MAX_SIZE,GFP_KERNEL);
	if(!buffer_buffer[0]){
		result= -ENOMEM;
	}

	buffer_buffer[1] = kmalloc(MAX_SIZE,GFP_KERNEL);
	if(!buffer_buffer[1]){
		result= -ENOMEM;
	}

	buffer_buffer[2] = kmalloc(MAX_SIZE,GFP_KERNEL);
	if(!buffer_buffer[2]){
		result= -ENOMEM;
	}


	memset(buffer_buffer[0],0,MAX_SIZE);
	memset(buffer_buffer[1],0,MAX_SIZE);
	memset(buffer_buffer[2],0,MAX_SIZE);

	printk("<1>Inserting buffer module\n");
	return 0;
}
void buffer_exit(void){
	unregister_chrdev(buffer_major,"buffer");

	if(buffer_buffer[0]){
		kfree(buffer_buffer[0]);
	}

	if(buffer_buffer[1]){
		kfree(buffer_buffer[1]);
	}
	if(buffer_buffer[2]){
		kfree(buffer_buffer[2]);
	}
	printk("<1>Removing Buffer module\n");

}

// TODO: ver cantidad de escritores
static int buffer_open(struct inode *inode, struct file *filp) {
	char *mode=   filp->f_mode & FMODE_WRITE ? "write" :
	filp->f_mode & FMODE_READ ? "read" :
	"unknown";
	printk("<1>open %p for %s\n", filp, mode);
	return 0;
}

static int buffer_release(struct inode *inode, struct file *filp) {
	printk("<1>release %p\n", filp);
	return 0;
}

static ssize_t buffer_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) {
	ssize_t rc;
	printk("<BR> vamos a leer de nuevo, writings: %d",writings);
	m_lock(&mutex);
	printk("<BR>entramos al mutex");
	if(*f_pos>0){
		printk("<BR> se acabo la lectura");
		rc=0;
		goto epilog;
		
	}
	printk("<BR> tenemos que liberar mutex si hay 0 writings: %d \n",writings);
	while(writings==0){
		printk("<BR> tenemos que liberar mutex, writings: %d\n ",writings);
		if(c_wait(&cond,&mutex)){
			printk("<1>read interrupted\n");
			rc= -EINTR;
			goto epilog;
		}
	}
	if (count > curr_size[cicloRead]) {
		count= curr_size[cicloRead];
	}
	printk("<1> read %d bytes at %d\n",(int)count,(int)*f_pos);

	if (copy_to_user(buf,buffer_buffer[cicloRead],count)!=0){
		rc= -EFAULT;
		goto epilog;
	}
	*f_pos+=cicloRead - (cicloRead-count);
	rc = count;
	cicloRead = (cicloRead+1)%BUFF_SIZE;
	writings--;
	printk("<BR>llegue al final de read");
	printk("<BR>quedan %d mensajes por leer\n",writings );

	epilog:
	printk("<BR>libero mutex");
	c_broadcast(&cond);
	m_unlock(&mutex);
	return rc;
	
}
static ssize_t buffer_write( struct file *filp, const char *buf, size_t count, loff_t *f_pos){
	ssize_t rc;
	printk("<BW> vamos a escribir de nuevo, writings: %d \n",writings);
	m_lock(&mutex);
	printk("<BW> tomamos el mutex ");
	if(count>MAX_SIZE){
		count= MAX_SIZE;
	}
	printk("<BW> check cant de writings: %d", writings);
	while(writings>2){
		printk("<BW> tenemos mas de 3 writings");
		if(c_wait(&cond,&mutex)){
			printk("<1>write interrupted\n");
			rc= -EINTR;
			goto epilog;
		}
	}
	curr_size[cicloWrite] = count;


	printk("<1>write %d bytes at %d\n",(int)count,(int)*f_pos);
	if(copy_from_user(buffer_buffer[cicloWrite],buf,count)!=0){
		rc= -EFAULT;
		goto epilog;
	}
	cicloWrite=(cicloWrite+1)%BUFF_SIZE;
	*f_pos = count;
	c_broadcast(&cond);
	rc= count;
	printk("<1>llegue al final de write");
	writings++;
	printk("<W> hay %d mensajes en el buffer\n",writings);

	epilog:
	printk("<BW>libero mutex");
	m_unlock(&mutex);
	return rc;
}
/*
mknod /dev/buffer c 61 0
chmod a+rw /dev/buffer
insmod buffer.ko
dmesg | tail

*/
