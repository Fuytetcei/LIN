
/* PARTE OPCIONAL 1 */

#include "modlist.h"

#ifdef _MODLISTCHAR /* ---------------- Lista de cadenas -----------------*/

char *modname = "modlistChar";

// Declaro funciones axiliares
int insert (char *c, int len);
void remove (char *c, int len);
int cleanup(void);

/* Esta función devuelve ,por el buffer de usuario, un elemento de la lista
   o '\0' si ya no hay más */
ssize_t read_modlist(struct file *filp, char __user *buf, size_t len, loff_t *off) {
	char *auxbuff;
	int dev;
	list_char_t *node = NULL;

	/* Reservo memoria para el buffer local,
	   10 bytes ya que son el número máximo de cifras que
	   puede tener un tipo int 
	*/
	auxbuff = (char*)vmalloc(1024);
	if(!auxbuff)
		return -1;

	/* Miro si he terminado de recorrer la lista */
	if(read_head == &mylist){
		memset(auxbuff, '\0', 1024);
		copy_to_user(buf, auxbuff, 1024);
		(*off) = (loff_t) filp;
		return 0;
	}
	else {
		/* Extraigo el elemento que corresponda */
		node = list_entry(read_head, list_char_t, links);
		if(!node)
			return -EFAULT;

		/* Leo el carácter */
		dev = sprintf(auxbuff, "%s\n", node->data);

		/* Traspaso a espacio de usuario */
		if(copy_to_user(buf, auxbuff, dev) == 2)
			return -EFAULT;

		(*off)+=1;
	}
	read_head = read_head->next;

	return dev;
};

/* Esta función ejecuta operaciones sobre la lista
   según una serie de instrucciones previamente parseadas. */
ssize_t write_modlist(struct file *filp, const char __user *buf, size_t len, loff_t *off) {

	int dev;
	char c = '\0';

	// Primero hago una copia desde el espacio de usuario a espacio de kernel
		// Miro si me paso de tamaño
		if(len > BUFFER_LENGTH-1) {
			printk(KERN_INFO "moslistChar: Instrucción demasiado larga.\n");
			return -EFAULT;
		}

		// Transfiero los datos del espacio usuario al espacio kernel
		if(copy_from_user(&buff_modlist[0], buf, len))
			return -EFAULT;

	// Parseo la entrada y ejecuto la orden que corresponda
		// Insertar al final
		if(sscanf(buff_modlist, "add %s", buff_modlist)){
			dev = insert(buff_modlist, len-4);
			if(dev == 2) {
				dev = 0;
				printk(KERN_INFO "modlistChar: no hay suficiente memoria");
			}
			else if(!dev)
				printk(KERN_INFO "modlistChar: error al reservar memoria");
	
		}// Eliminar todas las apariciones de un elemento
		else if (sscanf(buff_modlist, "remove %s",  buff_modlist))
			remove(buff_modlist, len-7);
		// Eliminar todos los elementos
		else {
			sscanf(buff_modlist, "%s", buff_modlist);
			if(!strcmp(buff_modlist, "cleanup")) {
				dev = cleanup();
			}
			else {
				printk(KERN_INFO "moslistChar: Instrucción desconocida.\n");
				dev = 0;
			}
		}
	// Actualizo punteros
	(*off)+=len;

	// Devuelvo número de datos guardados
	return len;
	//return 1;
};

// Insertar
int insert (char *c, int len) {
	list_char_t *new = NULL;

	// Miro si me queda memoria para más elementos
	if(mem >= BUFFER_KERNEL){
		return 2;
	}
	else {
		// Reservo memoria para el nuevo nodo
		new = (list_char_t*)vmalloc(sizeof(list_char_t));
		if(!new)
			return 0;
		// Reservo memoria para la cadena
		new->data = (char*)vmalloc(len);
		if(!new->data)
			return 0;
		// Asigno el nuevo dato
		strcpy(new->data, c);

		// Añado el nuevo nodo
		list_add_tail(&new->links, &mylist);

		// Actualizo parámetros de control
		mem += sizeof(list_char_t+len);
	}

	read_head = mylist.next;

	return 1;
};

// Eliminar
void remove (char *c, int len) {
	struct list_head *pos = NULL;
	struct list_head *n = NULL;
	list_char_t *node;

	// Itero hasta reencontrarme con la cabeza
	list_for_each_safe(pos, n, &mylist){
		// Obtengo el puntero de la estructura del nodo
		node = list_entry(pos, list_char_t, links);
		// Miro si soinciden los elementos
		if(!strcmp(node->data, c)) {
			printk(KERN_INFO "modlistChar: cadena encontrada");
			// Elimino el elemento
			list_del(pos);
			// Actualizo parámetros de control
			mem -= sizeof(list_char_t+len);
			// Libero memoria de la cadena y del nodo
			vfree(node->data);
			vfree(node);
		}
	}
	read_head = mylist.next;
};

/* Limpiar lista */
int cleanup() {
	struct list_head *pos = NULL;
	struct list_head *n = NULL;
	list_char_t *node;
	int i = 0;

	// Itero hasta reencontrarme con la cabeza
	list_for_each_safe(pos, n, &mylist){
		// Obtengo el puntero de la estructura del nodo
		node = list_entry(pos, list_char_t, links);
		// Elimino el elemento
		list_del(pos);
		// Libero memoria
		vfree(node);
		if(!node)
		// Actualizo parámetros de control
		mem -= sizeof(list_char_t);
		i++;
	}
	read_head = &mylist;

	return i;
}

#else /* ---------------- Lista de enteros ----------------------*/

char *modname = "modlistInt";

// Declaro funciones axiliares
int insert (int num);
void remove (int num);
int cleanup(void);

/* Esta función devuelve ,por el buffer de usuario, un elemento de la lista
   o '\0' si ya no hay más */
ssize_t read_modlist(struct file *filp, char __user *buf, size_t len, loff_t *off) {
	char *auxbuff;
	int dev;
	list_int_t *node = NULL;

	/* Reservo memoria para el buffer local,
	   10 bytes ya que son el número máximo de cifras que
	   puede tener un tipo int 
	*/
	auxbuff = (char*)vmalloc(11);
	if(!auxbuff)
		return -1;

	/* Miro si he terminado de recorrer la lista */
	if(read_head == &mylist){
		memset(auxbuff, '\0', 11);
		copy_to_user(buf, auxbuff, 11);
		(*off) = (loff_t) filp;
		return 0;
	}
	else {
		/* Extraigo el elemento que corresponda */
		node = list_entry(read_head, list_int_t, links);
		if(!node)
			return -EFAULT;

		/* Paso de entero a carácter */
		dev = sprintf(auxbuff, "%d\n", node->data);

		/* Traspaso a espacio de usuario */
		if(copy_to_user(buf, auxbuff, 11) == 11)
			return -EFAULT;

		(*off)+=1;
	}
	read_head = read_head->next;

	return dev;
};

/* Esta función ejecuta operaciones sobre la lista
   según una serie de instrucciones previamente parseadas. */
ssize_t write_modlist(struct file *filp, const char __user *buf, size_t len, loff_t *off) {

	int dev;
	int num = 0;

	// Primero hago una copia desde el espacio de usuario a espacio de kernel
		// Miro si me paso de tamaño
		if(len > BUFFER_LENGTH-1) {
			printk(KERN_INFO "moslistInt: Instrucción demasiado larga.\n");
			return -EFAULT;
		}

		// Transfiero los datos del espacio usuario al espacio kernel
		if(copy_from_user(&buff_modlist[0], buf, len))
			return -EFAULT;

	// Parseo la entrada y ejecuto la orden que corresponda
		// Insertar al final
		if(sscanf(buff_modlist, "add %i", &num)){
			dev = insert(num);
			if(dev == 2) {
				dev = 0;
				printk(KERN_INFO "modlistInt: no hay suficiente memoria");
			}
			else if(!dev)
				printk(KERN_INFO "modlistInt: error al reservar memoria");
	
		}// Eliminar todas las apariciones de un elemento
		else if (sscanf(buff_modlist, "remove %i",  &num))
			remove(num);
		// Eliminar todos los elementos
		else {
			sscanf(buff_modlist, "%s", buff_modlist);
			if(!strcmp(buff_modlist, "cleanup")) {
				dev = cleanup();
			}
			else {
				printk(KERN_INFO "moslistInt: Instrucción desconocida.\n");
				dev = 0;
			}
		}
	// Actualizo punteros
	(*off)+=len;

	// Devuelvo número de datos guardados
	return len;
	//return 1;
};

// Insertar
int insert (int num) {
	list_int_t *new = NULL;

	// Miro si me queda memoria para más elementos
	if(mem >= BUFFER_KERNEL){
		return 2;
	}
	else {
		// Reservo memoria para el nuevo nodo
		new = (list_int_t*)vmalloc(sizeof(list_int_t));
		if(!new)
			return 0;
		// Asigno el nuevo dato
		new->data = num;

		// Añado el nuevo nodo
		list_add_tail(&new->links, &mylist);

		// Actualizo parámetros de control
		mem += sizeof(list_int_t);
	}

	read_head = mylist.next;

	return 1;
};

// Eliminar
void remove (int num) {
	struct list_head *pos = NULL;
	struct list_head *n = NULL;
	list_int_t *node;

	// Itero hasta reencontrarme con la cabeza
	list_for_each_safe(pos, n, &mylist){
		// Obtengo el puntero de la estructura del nodo
		node = list_entry(pos, list_int_t, links);
		// Miro si soinciden los elementos
		if(node->data == num) {
			// Elimino el elemento
			list_del(pos);
			// Libero memoria
			vfree(node);
			// Actualizo parámetros de control
			mem -= sizeof(list_int_t);
		}
	}
	read_head = mylist.next;
};

/* Limpiar lista */
int cleanup() {
	struct list_head *pos = NULL;
	struct list_head *n = NULL;
	list_int_t *node;
	int i = 0;

	// Itero hasta reencontrarme con la cabeza
	list_for_each_safe(pos, n, &mylist){
		// Obtengo el puntero de la estructura del nodo
		node = list_entry(pos, list_int_t, links);
		// Elimino el elemento
		list_del(pos);
		// Libero memoria
		vfree(node);
		if(!node)
		// Actualizo parámetros de control
		mem -= sizeof(list_int_t);
		i++;
	}
	read_head = &mylist;

	return i;
}

#endif /* _MODLISTOP */