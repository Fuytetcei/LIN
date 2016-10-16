/*

	PARTE PRINCIPAL DE LA PRÁCTICA 1

*/

/*
	APUNTES, ACUERDATE DE BORRARLOS!!!

	sscanf(kbuf, "add %i", &num); devuelve el número de tokens leídos
	al pasarle una cadena y una expresión regular

	dest += sprintf(dest, %i\n, item->data);

	return dest-kbuf;

*/

#include "modlist.h"

/* Esta función devuelve ,por el buffer de usuario, un elemento de la lista
   o '\0' si ya no hay más */
ssize_t read_modlist(struct file *filp, char __user *buf, size_t len, loff_t *off) {
	char *auxbuff;
	int dig, num, i;
	list_item_t *node = NULL;

	/* Reservo memoria para el buffer local,
	   10 bytes ya que son el número máximo de cifras que
	   puede tener un tipo int 
	*/
	trace_printk(KERN_INFO "moslist: leyendo elemento.\n");
	auxbuff = (char*)vmalloc(10);
	memset(auxbuff, '\n', 10);
	// Busco el siguiente elemento
	read_head = read_head->next;
	// Miro si he llegado al final
	if(read_head == &mylist){
		copy_to_user(buf, '\0', 1);
		printk(KERN_INFO "moslist: fin de lista.\n");
		// Actualizo el puntero del fichero ------------ !!!!
		//off = 0;
	}
	else {
		// Extraigo el número
		node = list_entry(read_head, list_item_t, links);
		if(!node) {
			printk(KERN_INFO "moslist: error al leer de la lista.\n");
			return 0;
		}
		num = node->data;
		
		// Lo transformo en cadena de carancteres
		i = 0;
		while ((num >= 1) && (i < 10)) {
	        dig = num % 10;

            auxbuff[i] = (char) (dig + 48);            
            num -= dig;
            num /= 10;
	        i++;
	    }
		// Lo copio a espacio usuario
		copy_to_user(buf, auxbuff, 10);
		// Actualizo el puntero del fichero
		(*off)+=len;
		printk(KERN_INFO "moslist: len: %i\n", (int)len);
	}

	printk(KERN_INFO "moslist: elemento leido.\n");
	// Devuelvo 1
	return 1;
};

/* Esta función ejecuta operaciones sobre la lista
   según una serie de instrucciones previamente parseadas. */
ssize_t write_modlist(struct file *filp, const char __user *buf, size_t len, loff_t *off) {

	int dev;
	int num = 0;

	// Primero hago una copia desde el espacio de usuario a espacio de kernel
		// Miro si me paso de tamaño
		if(len > BUFFER_LENGTH-1) {
			printk(KERN_INFO "moslist: Instrucción demasido larga.\n");
			return 0;
		}

		// Transfiero los datos del espacio usuario al espacio kernel
		if(copy_from_user(&buff_modlist[0], buf, len))
			return -EFAULT;

	// Parseo la entrada y ejecuto la orden que corresponda
		// Insertar al final
		if(sscanf(buff_modlist, "add %i", &num) == 1){
			dev = insert(num);
			if(dev == 2) {
				dev = 0;
				printk(KERN_INFO "modlist: no hay suficiente memoria");
			}
			else if(!dev)
				printk(KERN_INFO "modlist: error al reservar memoria");
	
		}// Eliminar todas las apariciones de un elemento
		//else if (sscanf(buf_modlist, "remove %i",  &num)) {
			//if(!err = delete(num))
				//dev = 1;
		//}// Eliminar todos los elementos
		//else if (sscanf(buf_modlist, "cleanup")) {
			//while(/* LISTA NO VACÍA */){
				/* ELIMINAR CABEZA */
			//}
		//}
		else {
			printk(KERN_INFO "moslist: Instrucción desconocida.\n");
			dev = 0;
		}

	// Actualizo punteros

	// Devuelvo número de datos guardados
		return dev;
};

// Insertar
int insert(int num) {
	list_item_t *new = NULL;

	printk(KERN_INFO "modlist: añadiendo elemento.\n");
	// Miro si me queda memoria para más elementos
	if(mem >= BUFFER_KERNEL){
		printk(KERN_INFO "modlist: no queda espacio en la pila.\n");
		return 2;
	}
	else {
		// Reservo memoria para el nuevo nodo
		new = (list_item_t*)vmalloc(sizeof(list_item_t));
		if(!new){
			return 0;
		}
		// Asigno el nuevo dato
		new->data = num;
		printk(KERN_INFO "modlist: memoria reservada %i\n", new->data);

		// Añado el nuevo nodo
		list_add(&new->links, &mylist);
		printk(KERN_INFO "modlist: dato guardado\n", new->data);

		// Actualizo parámetros de control
		mem += sizeof(list_item_t);
		numElem++;
	}

	printk(KERN_INFO "modlist: elemento añadido.\n");

	return 1;
};

// Eliminar
int remove (int num) {
	struct list_head *pos = NULL;
	struct list_head *n = NULL;
	list_item_t *node;

	printk(KERN_INFO "modlist: eliminando elemento.\n");
	// Itero hasta reencontrarme con la cabeza
	list_for_each_safe(pos, n, &mylist){
		// Obtengo el puntero de la estructura del nodo
		node = list_entry(pos, list_item_t, links);
		// Miro si soinciden los elementos
		if(node->data == num) {
			// Elimino el elemento
			list_del(pos);
			// Libero memoria
			vfree(pos);
			// Actualizo parámetros de control
			mem -= BUFFER_KERNEL;
			numElem--;
		}
	}

	printk(KERN_INFO "modlist: elemento borrado.\n");
	return 1;
};

// -- GESTIÓN DE LA ENTRADA /PROC ---------------------------------

// Asigno las funciones necesarias para operar con la entrada
static const struct file_operations fops = {
	.read = read_modlist,
	.write = write_modlist,
};

// Carga del módulo
int modulo_modlist_init(void) {
	// Reservo memoria para el buffer
	buff_modlist = (char *)vmalloc(BUFFER_LENGTH);
	if(!buff_modlist) {
		printk(KERN_INFO "modlist: No se pudo reservar memoria.\n");
		return -ENOMEM;
	}

	// PARA QUE VALÍA ESTO??!!
	memset(buff_modlist, '\0', BUFFER_LENGTH);

	// Creo la entrada a /Proc
	proc_entry = proc_create("modlist", 0666, NULL, &fops);
	if(!proc_entry) {
		// Si hay error libero memoria y salgo con error
		vfree(buff_modlist);
		printk(KERN_INFO "modlist: No se pudo crear entrada /proc.\n");
		return -ENOMEM;
	}
	printk(KERN_INFO "modlist: Entrada /proc creada.\n");

	// Inicializo la cabeza de la lista
	LIST_HEAD(mylist);
	INIT_LIST_HEAD(&mylist);
	// Inicializo la cabeza de lectura
	read_head = &mylist;

	/* Devolver 0 para indicar una carga correcta del módulo */
	printk(KERN_INFO "Modulo modlist cargado.");
	return 0;
};

// Descarga del módulo
void modulo_modlist_clean(void) {
	struct list_head *pos = NULL;
	struct list_head *n = NULL;

	// Libero la memoria de la lista si no está vacía
	if(list_empty(&mylist)){
		printk(KERN_INFO "eliminando lista.\n");
		list_for_each_safe(pos, n, &mylist){
			//clear Elimino el elemento
			list_del(pos);
			// Libero memoria
			vfree(pos);
			// Actualizo parámetros de control
			mem -= BUFFER_KERNEL;
			numElem--;
		}
	}
	// Elimino la entrada a /Proc
	remove_proc_entry("modlist", NULL);
	// Libero memoria
	vfree(buff_modlist);
	printk(KERN_INFO "Modulo modlist descargado.\n");
};

/* Declaración de funciones init y cleanup */
module_init(modulo_modlist_init);
module_exit(modulo_modlist_clean);