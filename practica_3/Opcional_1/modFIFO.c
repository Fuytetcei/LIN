/*

	PARTE OPCIONAL 1 DE LA PRÁCTICA 3

*/

#include "modFIFO.h"

// Llamada a open
ssize_t open_modFIFO(struct inode *node, struct file * fd) {

	if(down_interruptible(&mtx))
		return -EINTR;

	// Miro si es productor o consumidor
	if(fd->f_mode & FMODE_READ) {
		// Sumo uno al consumidor
		cons_count++;

		up(&sem_prod);
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

		printk(KERN_INFO "modfifoOPEN: abierto para lectura. %d %d\n", prod_count, cons_count);
	}
	else {
		// Sumo uno al productor
		prod_count++;

		up(&sem_cons);

		// Espero a que haya un consumidor
		while(cons_count==0) {
			nr_prod_waiting++;
			up(&mtx);
			if(down_interruptible(&sem_prod)) {
				down(&mtx);
				nr_prod_waiting--;
				up(&mtx);
				return -EINTR;
			}
			if(down_interruptible(&mtx))
				return -EINTR;
		}

		printk(KERN_INFO "modfifoOPEN: abierto para escritura %d %d\n", prod_count, cons_count);
	}

	up(&mtx);

	return 0;
}

// Llamada a release
ssize_t release_modFIFO (struct inode *node, struct file * fd) {

	if(down_interruptible(&mtx))
		return -EINTR;

	// Miro si es productor o consumidor
	if(fd->f_mode & FMODE_READ) {
		// Consumidor
		// Resto uno al consumidor
		cons_count--;
		if(prod_count>0)
			up(&sem_prod);
		printk(KERN_INFO "modfifoRELEASE: cerrado para lectura\n");
	}
	else {
		// Productor
		// Resto uno al productor
		prod_count--;
		if(cons_count>0)
			up(&sem_cons);
		printk(KERN_INFO "modfifoRELEASE: cerrado para escritura\n");
	}

	if(cons_count==0 && prod_count==0)
			kfifo_reset(&cbuffer);
	up(&mtx);

	return 0;
};

// Consumir del buffer
ssize_t read_modFIFO(struct file *filp, char __user *buf, size_t len, loff_t *off){

	char data[BUFFER_LENGTH];

	// Inicializo buffer auxiliar
	memset(data, '\0', BUFFER_LENGTH);

	// Miro si puedo leer los bytes requeridos
	if (len > BUFFER_LENGTH) { return -EINTR; }

// Seccion crítica
	if(down_interruptible(&mtx))
			return -EINTR;

	// Espero hasta que haya algo que leer y algún productor
	while((prod_count>0) && (len >  kfifo_len(&cbuffer))){
		nr_cons_waiting++;
		up(&mtx);
		if(down_interruptible(&sem_cons)){
			down(&mtx);
			nr_cons_waiting--;
			up(&mtx);
			return -EINTR;
		}
		if(down_interruptible(&mtx))
			return -EINTR;
	}

	// Compruebo fin de comunicación
	if ((prod_count==0) && (kfifo_is_empty(&cbuffer))) {up(&mtx); return 0;}

	// Extraigo datos
	if(!kfifo_out(&cbuffer, data, len))
		return -EINTR;
	// Despierto a un posible productor
	if(nr_prod_waiting>0) {
		up(&sem_prod);
		nr_prod_waiting--;
	}

	up(&mtx);
// Fin sección crítica

	// Mando los datos al usuario y devuelvo bytes leídos
	if(copy_to_user(buf, data, len))
		return -EINTR;

	(*off)+=len;

	return len;

};


// Producir en el buffer
ssize_t write_modFIFO(struct file *filp, const char __user *buf, size_t len, loff_t *off){

	char data[BUFFER_LENGTH];

	// Inicializo buffer auxiliar
	memset(data, '\0', BUFFER_LENGTH);

	// Copio los datos del usuario
	if(copy_from_user(data, buf, len))
		return -EFAULT;

	if (len > BUFFER_LENGTH) { return -EFAULT;}

// Seccion crítica
	if(down_interruptible(&mtx))
			return -EINTR;
	// Miro si puedo leer los bytes requeridos
	// Espero hasta que haya algo que leer y algún productor
	while((cons_count>0) && (len > kfifo_avail(&cbuffer))){
		nr_prod_waiting++;
		up(&mtx);
		if(down_interruptible(&sem_prod)){
			down(&mtx);
			nr_prod_waiting--;
			up(&mtx);
			return -EINTR;
		}
		if(down_interruptible(&mtx))
			return -EINTR;
	}


	// Compruebo fin de comunicación
	if (cons_count==0) {up(&mtx); return -EPIPE;}

	// Inserto datos datos
	kfifo_in(&cbuffer, data, len);

	// Despierto a un posible productor
	if(nr_cons_waiting>0) {
		up(&sem_cons);
		nr_cons_waiting--;
	}
	up(&mtx);
// Fin sección crítica

	(*off)+=len;

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
	INIT_KFIFO(cbuffer);


	/* Devolver 0 para indicar una carga correcta del módulo */
	return 0;
};

// Descarga del módulo
void modulo_modFIFO_clean(void) {

	// Destruyo el buffer
	kfifo_free(&cbuffer);

	// Elimino la entrada a /Proc
	remove_proc_entry("modifo", NULL);
	printk(KERN_INFO "modfifo: descargado.\n");

};

/* Declaración de funciones init y cleanup */
module_init(modulo_modFIFO_init);
module_exit(modulo_modFIFO_clean);
