

#include <linux/module.h>	/* Requerido por todos los módulos */
#include <linux/kernel.h>	/* Definición de KERN_INFO */
#include <linux/proc_fs.h>	/* Para cargar y descargar módulos del kernel */
#include <linux/string.h>	/* para tratar las cadenas */
#include <linux/vmalloc.h>	/* Para reservar y liberar memoria e el kernel */
#include <asm-generic/uaccess.h> /* PARA QUE VALE ESTO?!!*/

#define BUFFER_LENGTH 256

MODULE_LICENSE("GPL");		/* Licencia del módulo */
MODULE_DESCRIPTION("Modlist Kernel Module - LIN-FDI-UCM");
MODULE_AUTHOR("Daniel y Manuel");

static struct proc_dir_entry *proc_entry; // Puntero al la entrada /Proc
static char *buff_clipboard;			// Buffer de alamcenamiento

// Funciones de lectura/escritura de la nueva entrada /Proc
ssize_t read_clipboard(struct file *filp, char __user *buf, size_t len, loff_t *off){

	int _aux;

	if((*off) > 0)
		return 0;

	_aux = strlen(buff_clipboard);

	if (len<_aux)
    	return -ENOSPC;

    // Transfiero los datos al espacio usuario
    if(copy_to_user(buf, buff_clipboard, _aux))
    	return -EINVAL;

    // Actualizo el puntero de lectura para que el usuario sepa que se ha desplazado
    (*off)+=len;

    return _aux;
};

ssize_t write_clipboard(struct file *filp, const char __user *buf, size_t len, loff_t *off){

	if((*off) > 0)
		return 0;

	// Miro si me paso de tamaño
	if(len > BUFFER_LENGTH-1) {
		printk(KERN_INFO "Clipboard: Se ha intentado escribir de  más.\n");
		return 0;
	}

	// Transfiero los datos del espacio usuario al espacio kernel
	if(copy_from_user(&buff_clipboard[0], buf, len))
		return -EFAULT;

	// Situo el final de fichero
	buff_clipboard[len] = '\0';
	// Actualizo el puntero al fichero para que el usuario sepa que se ha desplazado
	*off+=len;

	return len;
}

static const struct file_operations fops = {
	.read = read_clipboard,
	.write = write_clipboard,
};

/* Función que se invoca cuando se carga el módulo en el kernel */
int modulo_clipboard_init(void) {
	// Reservo memoria para el buffer
	buff_clipboard = (char *)vmalloc(BUFFER_LENGTH);
	if(!buff_clipboard) {
		printk(KERN_INFO "Clipboard: No se pudo reservar memoria.\n");
		return -ENOMEM;
	}

	// PARA QUE VALÏA ESTO??!!
	memset(buff_clipboard, 0, BUFFER_LENGTH);

	// Creo la entrada a /Proc
	proc_entry = proc_create("clipboard", 0666, NULL, &fops);
	if(proc_entry == NULL) {
		// Si hay error libero memoria y salgo con error
		vfree(buff_clipboard);
		printk(KERN_INFO "Clipboard: No se pudo crear entrada /proc.\n");
		return -ENOMEM;
	}
	printk(KERN_INFO "Clipboard: Entrada /proc creada.\n");

	/* Devolver 0 para indicar una carga correcta del módulo */
	printk(KERN_INFO "Modulo clipboard cargado.");
	return 0;
}
/* Función que se invoca cuando se descarga el módulo del kernel */
void modulo_clipboard_clean(void)
{
	// Elimino la entrada a /Proc
	 remove_proc_entry("clipboard", NULL);
	// Libero memoria
	vfree(buff_clipboard);
	printk(KERN_INFO "Modulo clipboard descargado.\n");
}

/* Declaración de funciones init y cleanup */
module_init(modulo_clipboard_init);
module_exit(modulo_clipboard_clean);