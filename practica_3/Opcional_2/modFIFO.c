/*

	PARTE OPCIONAL 2 DE LA PRÁCTICA 3

*/

#include "modFIFO.h"

// Llamada a open
ssize_t open_modFIFO(struct inode *node, struct file * fd) {

	if(down_interruptible(&mtx))
		return -EINTR;

	// Miro si es productor o consumidor
	if(fd->f_mode & FMODE_READ) {
		// Sumo uno al consumido
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
		clear_cbuffer_t(cbuffer);
	up(&mtx);

	return 0;
};

// Consumir del buffer
ssize_t read_modFIFO(struct file *filp, char __user *buf, size_t len, loff_t *off){

	char data[cbuffer->max_size];

	// Inicializo buffer auxiliar
	memset(data, '\0', cbuffer->max_size);

	// Miro si puedo leer los bytes requeridos
	if (len > cbuffer->max_size) { return -EINTR; }

// Seccion crítica
	if(down_interruptible(&mtx))
			return -EINTR;

	// Espero hasta que haya algo que leer y algún productor
	while((prod_count>0) && (len > size_cbuffer_t(cbuffer))){
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
	if ((prod_count==0) && (is_empty_cbuffer_t ( cbuffer ) < 0)) {up(&mtx);printk(KERN_INFO "modfifoREAD: fin de comunicacion\n"); return -EPIPE;}

	// Extraigo datos
	remove_items_cbuffer_t(cbuffer, data, len);
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

	char data[cbuffer->max_size];

	// Inicializo buffer auxiliar
	memset(data, '\0', cbuffer->max_size);

	// Copio los datos del usuario
	if(copy_from_user(data, buf, len))
		return -EFAULT;

	if (len > cbuffer->max_size) { return -EFAULT;}

// Seccion crítica
	if(down_interruptible(&mtx))
			return -EINTR;
	// Miro si puedo leer los bytes requeridos
	// Espero hasta que haya algo que leer y algún productor
	while((cons_count>0) && (len > nr_gaps_cbuffer_t(cbuffer))){
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
	insert_items_cbuffer_t(cbuffer, data, len);

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

static long do_ioctl(struct file *filp, u_int cmd, u_long arg){
	return 0;
};

static loff_t do_llseek (struct file *file, loff_t offset, int orig){
	loff_t ret;
 
	switch (orig)
	{
		case SEEK_SET:
			ret = offset;
			break;
		case SEEK_CUR:
			ret = file->f_pos + offset;
			break;
		case SEEK_END:
			ret = sizeof (BUFFER_LENGTH) - offset;
			break;
		default:
			ret = -EINVAL;
	}

	if (ret >= 0)
		file->f_pos = ret;
	else
		ret = -EINVAL;

	return ret;
};

// -- GESTIÓN DE LA ENTRADA /PROC ---------------------------------

// Asigno las funciones necesarias para operar con la entrada
static const struct file_operations fops = {
	.read = read_modFIFO,
	.write = write_modFIFO,
	.open = open_modFIFO,
	.release = release_modFIFO,
	.unlocked_ioctl = do_ioctl,
	.llseek = do_llseek,
};

// Carga del módulo
int modulo_modFIFO_init(void) {

	// Creo la entrada al dispositivo
	int result;
 
	if (register_chrdev (DRIVER_MAJOR, DRIVER_NAME, &fops)) {
		printk ("No se pudo registrar el módulo\n");
		return result;
	}
	printk(KERN_INFO "modfifo: dispositivo de caracteres creado\n");

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
	unregister_chrdev (DRIVER_MAJOR, DRIVER_NAME);
	printk(KERN_INFO "modfifo: dispositivo de caracteres descargado.\n");

};



/* Declaración de funciones init y cleanup */
module_init(modulo_modFIFO_init);
module_exit(modulo_modFIFO_clean);