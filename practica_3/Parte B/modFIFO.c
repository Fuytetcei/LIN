/*

	PARTE PRINCIPAL DE LA PRÁCTICA 1

*/

#include "modFIFO.h"
#include "cbuffer.h"

// Llamada a open
ssize_t open_modFIFO(struct inode *node, struct file * fd) {

	// Miro si es productor o consumidor
	if(file->f_mode & FMODE_READ) {
		// Consumidor
		// Sumo uno al consumidor
		down_down_interruptible(mtx);
		cons_count++;
		up(mtx);
	}
	else {
		// Productor
		// Sumo uno al productor
		down_interruptible(mtx);
		prod_count++;
		up(mtx);
	}
};

// Llamada a release
ssize_t release_modFIFO (struct inode *node, struct file * fd) {
	// Miro si es productor o consumidor
	if(file->f_mode & FMODE_READ) {
		// Consumidor
		// Resto uno al consumidor
		down_interruptible(mtx);
		cons_count--;
		up(mtx);
	}
	else {
		// Productor
		// Resto uno al productor
		down_interruptible(mtx);
		prod_count--;
		up(mtx);
	}
	return 0;
};

// Consumir del buffer
ssize_t read_modFIFO(struct file *filp, char __user *buf, size_t len, loff_t *off) {

};


// Producir en el buffer
ssize_t write_modFIFO(struct file *filp, const char __user *buf, size_t len, loff_t *off) {

};

// -- GESTIÓN DE LA ENTRADA /PROC ---------------------------------

// Asigno las funciones necesarias para operar con la entrada
static const struct file_operations fops = {
	.read = read_modFIFO,
	.write = write_modFIFO,
	.open = open_modFIFO,
	.release = release_modFIFO,
};

// Carga del módulo
int modulo_modFIFO_init(void) {

	// Creo la entrada a /Proc
	proc_entry = proc_create("modFIFO", 0666, NULL, &fops);
	if(!proc_entry) {
		// Si hay error libero memoria y salgo con error
		printk(KERN_INFO "modFIFO: No se pudo crear entrada /proc.\n");
		return -ENOMEM;
	}
	printk(KERN_INFO "modFIFO: entrada /proc creada.\n");

	// Inicializo los semáforos
	sema_init(mtx,1)
	sema_init(sem_prod,0)
	sema_init(sem_cons,0)

	// Inicializo el buffer
	cbuffer = create_cbuffer_t(50);


	/* Devolver 0 para indicar una carga correcta del módulo */
	return 0;
};

// Descarga del módulo
void modulo_modFIFO_clean(void) {
	// Elimino la entrada a /Proc
	remove_proc_entry("modFIFO", NULL);
	printk(KERN_INFO "modFIFO: descargado.\n");

	// Destruyo el buffer
	destroy_buffer_t(cbuffer);
};

/* Declaración de funciones init y cleanup */
module_init(modulo_modFIFO_init);
module_exit(modulo_modFIFO_clean);
