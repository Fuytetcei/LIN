/*

	PARTE PRINCIPAL DE LA PRÁCTICA 1

*/

#include "modFIFO.h"

// Llamada a open
ssize_t open_modFIFO(struct inode *node, struct file * fd) {

	// Miro si es productor o consumidor
	if(fd->f_mode & FMODE_READ) {
		printk(KERN_INFO "modfifoOPEN: abriendo consumidor\n");
		// Sumo uno al consumidor
		if(down_interruptible(&mtx))
			return -EINTR;
		cons_count++;
		
		// Espero a que haya un productor
		while(prod_count==0) {
			nr_cons_waiting++;
			up(&mtx);
			if(down_interruptible(&sem_cons)) {
				down(&mtx);
				nr_cons_waiting--;
				up(&mtx);
				return -EINTR;
			}
			if(down_interruptible(&mtx))
				return -EINTR;

		}
		up(&mtx);
		printk(KERN_INFO "modfifoOPEN: abierto para lectura. %d %d\n", prod_count, cons_count);
	}
	else if(fd->f_mode & FMODE_WRITE){
		printk(KERN_INFO "modfifoOPEN: abriendo productor\n");
		// Sumo uno al productor
		if(down_interruptible(&mtx))
			return -EINTR;
		prod_count++;

		// Espero a que haya un consumidor
		while(cons_count==0) {
			nr_prod_waiting++;
			up(&mtx);
			printk(KERN_INFO "modfifoOPEN: esperando");
			if(down_interruptible(&sem_prod)) {
				down(&mtx);
				nr_prod_waiting--;
				up(&mtx);
				return -EINTR;
			}
			if(down_interruptible(&mtx))
				return -EINTR;
			printk(KERN_INFO "modfifoOPEN: ...\n");

		}
		up(&mtx);
		printk(KERN_INFO "modfifoOPEN: abierto para escritura %d %d\n", prod_count, cons_count);
	} else {
		printk(KERN_INFO "modfifoOPEN: error de apertura\n");
		return -EINTR;
	}

	// Devolver descriptor /proc/modFIFO
	return 0;
}

// Llamada a release
ssize_t release_modFIFO (struct inode *node, struct file * fd) {
	// Miro si es productor o consumidor
	if(fd->f_mode & FMODE_READ) {
		printk(KERN_INFO "modfifoRELEASE: cerrando para lectura\n");
		// Consumidor
		// Resto uno al consumidor
		if(down_interruptible(&mtx))
			return -EINTR;
		cons_count--;
		up(&mtx);
		printk(KERN_INFO "modfifoRELEASE: cerrado para lectura\n");
	}
	else {
		// Productor
		printk(KERN_INFO "modfifoRELEASE: cerrando para escritura\n");
		// Resto uno al productor
		if(down_interruptible(&mtx))
			return -EINTR;
		prod_count--;
		up(&mtx);
		printk(KERN_INFO "modfifoRELEASE: cerrado para escritura\n");
	}

	return 0;
};

// Consumir del buffer
ssize_t read_modFIFO(struct file *filp, char __user *buf, size_t len, loff_t *off){

	char data[cbuffer->max_size];
	int err;

	// Inicializo buffer auxiliar
	memset(data, '\0', cbuffer->max_size);

// Seccion crítica
	printk(KERN_INFO "modfifo READ: cojo mutex\n");
	if(down_interruptible(&mtx))
			return -EINTR;
	// Miro si puedo leer los bytes requeridos
	if (len > cbuffer->max_size) { return -EINTR; }
	// Espero hasta que haya algo que leer y algún productor
	while((prod_count>0) && (len > cbuffer->size)){
		nr_cons_waiting++;
		up(&mtx);
		printk(KERN_INFO "modfifo READ: suelto mutex\n");
		if(down_interruptible(&sem_cons)){
			down(&mtx);
			nr_cons_waiting--;
			up(&mtx);
			return -EINTR;
		}
		printk(KERN_INFO "modfifo READ: cojo mutex\n");
		if(down_interruptible(&mtx))
			return -EINTR;
	}


	// Compruebo fin de comunicación
	if (prod_count==0) {up(&mtx); return -EPIPE;}

	// Extraigo datos
	remove_items_cbuffer_t(cbuffer, data, len);
	// Despierto a un posible productor
	up(&sem_prod);

	printk(KERN_INFO "modfifo READ: suelto mutex\n");
	up(&mtx);
// Fin sección crítica

	// Mando los datos al usuario y devuelvo bytes leídos
	err = copy_to_user(buf, data, len);
	if(err)
		return -EINTR;

	(*off)+=len;

	return len;

};


// Producir en el buffer
ssize_t write_modFIFO(struct file *filp, const char __user *buf, size_t len, loff_t *off){

	char data[cbuffer->max_size];
	int err;

	printk(KERN_INFO "modfifoWRITE: empezando a escribir\n");
	// Inicializo buffer auxiliar
	memset(data, '\0', cbuffer->max_size);

	// Copio los datos del usuario
	err = copy_from_user(data, buf, len);
	if(err)
		return -EINTR;

	if (len > cbuffer->max_size) { return -EINTR;}

// Seccion crítica
	printk(KERN_INFO "modfifoWRITE: cojo mutex\n");
	if(down_interruptible(&mtx))
			return -EINTR;
	// Miro si puedo leer los bytes requeridos
	// Espero hasta que haya algo que leer y algún productor
	while((cons_count>0) && (len > nr_gaps_cbuffer_t(cbuffer))){
		nr_prod_waiting++;
		printk(KERN_INFO "modfifoWRITE: dejo mutex\n");
		up(&mtx);
		if(down_interruptible(&sem_prod)){
			down(&mtx);
			nr_prod_waiting--;
			up(&mtx);
			return -EINTR;
		}
		printk(KERN_INFO "modfifoWRITE: cojo mutex\n");
		if(down_interruptible(&mtx))
			return -EINTR;
	}


	// Compruebo fin de comunicación
	if (cons_count==0) {up(&mtx); return -EPIPE;}

	// Inserto datos datos
	insert_items_cbuffer_t(cbuffer, data, len);

	// Despierto a un posible productor
	up(&sem_cons);
	printk(KERN_INFO "modfifoWRITE: dejo mutex\n");
	up(&mtx);
// Fin sección crítica

	// Devuelvo bytes escritos
	return len;
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
	proc_entry = proc_create("fifomod", 0666, NULL, &fops);
	if(!proc_entry) {
		// Si hay error libero memoria y salgo con error
		printk(KERN_INFO "modfifo: No se pudo crear entrada /proc.\n");
		return -ENOMEM;
	}
	printk(KERN_INFO "modfifo: entrada /proc creada.\n");

	// Inicializo los semáforos
	sema_init(&mtx, 1);
	sema_init(&sem_prod, 0);
	sema_init(&sem_cons, 0);

	// Inicializo el buffer
	cbuffer = create_cbuffer_t(BUFFER_LENGTH);


	/* Devolver 0 para indicar una carga correcta del módulo */
	return 0;
};

// Descarga del módulo
void modulo_modFIFO_clean(void) {

	// Destruyo el buffer
	destroy_cbuffer_t(cbuffer);

	// Elimino la entrada a /Proc
	remove_proc_entry("modifo", NULL);
	printk(KERN_INFO "modfifo: descargado.\n");

};

/* Declaración de funciones init y cleanup */
module_init(modulo_modFIFO_init);
module_exit(modulo_modFIFO_clean);
