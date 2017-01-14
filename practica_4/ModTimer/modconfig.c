
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <asm-generic/uaccess.h>

static struct proc_dir_entry *proc_entry;

int timer_period_ms;
int max_randoms;
int emergency_threshold;

// Funcion /proc/modconfig
ssize_t write_modconfig(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
    char aux[100];

    // Copio desde usuario
    copy_from_user(buf, aux, len);

    // Compruebo que variable hay que modificar
    if(!strncmp(&aux[0], "timer_period_ms", 15)) {
        timer_period_ms = simple_strtol(&aux[0]+15, NULL, 10);
    }

    if(!strncmp(&aux[0], "max_randoms", 11)) {
        max_randoms = simple_strtol(&aux[0]+11, NULL, 10);
    }

    if(!strncmp(&aux[0], "emergency_threshold", 19)) {
        emergency_threshold = simple_strtol(&aux[0]+19, NULL, 10);
    }

    (*off)+=len;

    return len;
}

ssize_t read_modconfig(struct file *filp, char __user *buf, size_t len, loff_t *off) {

    char aux[100];
    int i;

    i = vsnprintf(&aux[0], 100, "timer_period_ms=%d\nemergency_threshold=%d\nmax_random=%d\n", timer_period_ms, emergency_threshold, max_randoms);

    copy_to_user(buf, &aux[0], len)

    (*off)+=len;
}

static const struct file_operations fops = {
	.read = read_modconfig,
	.write = write_modconfig,
};

int init_config_module( void ) {

	// Creo la entrada a /Proc
	proc_entry = proc_create("modconfig", 0666, NULL, &fops);
	if(!proc_entry) {
		// Si hay error libero memoria y salgo con error
		printk(KERN_INFO "modconfig: No se pudo crear entrada /proc.\n");
		return -ENOMEM;
	}
	printk(KERN_INFO "modconfig: entrada /proc creada.\n");

    return 0;
}


void cleanup_config_module( void ){
	remove_proc_entry("modconfig", NULL);
	printk(KERN_INFO "modconfig: descargado.\n");
}

module_init( init_config_module );
module_exit( cleanup_config_module );

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("P4-LIN");
MODULE_AUTHOR("Manuel y Dani");