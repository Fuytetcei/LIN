
/*
	PARTE OPCIONAL 2
 
NOTAS:
	- En push lo único que hacemos es lo mismo que en insert solo
	  que llamamos a la función básica de inserción y le pasamos
	  el nuevo nodo y la cabeza de la lista pata que lo ponga a la
	  cabeza.
	- En pop realizamos el mismo proceso que en remove solo que no
	  recorremos la lista en busca de ningún elemento, solo
	  borramos el primero.
	- Para sort hemos utilizado la función list_sort de la librería
	  linux/list_sort.h esta solución la hemos encontrado aquí:
	  	http://stackoverflow.com/questions/19282929/sorting-kernel-linux-linked-list
	  	http://stackoverflow.com/questions/35694862/sorting-a-linux-list-h-linked-list-with-list-sort
*/

#include "modlist.h"

#ifdef _MODLISTCHAR /* ---------------- Lista de cadenas -----------------*/

/* Estructura del nodo */
typedef struct {
	char *data;
	struct list_head links;
}list_char_t;

char *modname = "modlistChar";

// Declaro funciones axiliares
int insert (char *c, int len);
void remove (char *c, int len);
int cleanup(void);
int push (char *c, int len);
void pop (void);
int compare (void* priv, struct list_head *a, struct list_head *b);
void sort (void);

/* Esta función devuelve ,por el buffer de usuario, un elemento de la lista
   o '\0' si ya no hay más */
ssize_t read_modlist(struct file *filp, char __user *buf, size_t len, loff_t *off) {
	char auxbuff[512];
	int dev;
	list_char_t *node = NULL;

	/* Reservo memoria para el buffer local,
	   10 bytes ya que son el número máximo de cifras que
	   puede tener un tipo int 
	*/

	/* Miro si he terminado de recorrer la lista */
	if(read_head == &mylist){
		memset(auxbuff, '\0', 512);
		copy_to_user(buf, auxbuff, 512);
		(*off) = (loff_t) filp;
		read_head = read_head->next;
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
	char buff_modlist[512];

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
		else if(sscanf(buff_modlist, "push %s", buff_modlist)) {
			dev = push(buff_modlist, len-5);
			if(dev == 2) {
				dev = 0;
				printk(KERN_INFO "modlistChar: no hay suficiente memoria");
			}
			else if(!dev)
				printk(KERN_INFO "modlistChar: error al reservar memoria");
		}
		else {
			sscanf(buff_modlist, "%s", buff_modlist);
			if(!strcmp(buff_modlist, "cleanup")) {
				dev = cleanup();
			}
			else if(!strcmp(buff_modlist, "pop")) {
				pop();
			}
			else if(!strcmp(buff_modlist, "sort")) {
				sort();
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
	if((mem + sizeof(list_char_t)) >= BUFFER_KERNEL)
		return 2;
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
		mem += sizeof(list_char_t)+len;
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
			// Elimino el elemento
			list_del(pos);
			// Actualizo parámetros de control
			mem -= sizeof(list_char_t)+len;
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

/* --------------------- OP2 ---------------------------*/

/* Inserción en la cima */
int push (char *c, int len) {
	list_char_t *new = NULL;

	// Miro si me queda memoria para más elementos
	if((mem + sizeof(list_char_t)) >= BUFFER_KERNEL)
		return 2;
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

		// Añado el nuevo nodo en la cabeza
		list_add(&new->links, &mylist);

		// Actualizo parámetros de control
		mem += sizeof(list_char_t)+len;
	}

	read_head = mylist.next;

	return 1;
};

/* Eliminar el primer elemento de la lista */
void pop (void) {
	list_char_t *node;

	printk(KERN_INFO "modlistChar: pop\n");

	/* Miro si la lista está vacía */
	if(list_empty(&mylist)){
		printk(KERN_INFO "modlistInt: lista vacía\n");
		return;
	}

	node = list_entry(mylist.next, list_char_t, links);
	// Elimino el primer elemento
	list_del(mylist.next);
	// Actualizo parámetros de control
	mem -= sizeof(list_char_t)+strlen(node->data);
	// Libero memoria de la cadena y del nodo
	vfree(node->data);
	vfree(node);

	read_head = mylist.next;
};

/* Ordenación y comparación */
int compare (void* priv, struct list_head *a, struct list_head *b) {
	/* Obtengo los nodos */
	list_char_t *nodeA = list_entry(a, list_char_t, links);
	list_char_t *nodeB = list_entry(b, list_char_t, links);
	int i;

	/* Comparo las cadenas */
	i = strcmp(nodeA->data, nodeB->data);

	/* Calculo el resultado */
	if(i<0)
		return -1;
	else if(i>0)
		return 1;
	return 0;
};

void sort (void) {
	/* El primer argumento puede ser null, no lo usa la función */
	list_sort(NULL, &mylist, compare);
	read_head = mylist.next;
};

/* --------------------- OP2 ---------------------------*/

#else /* ---------------- Lista de enteros ----------------------*/

/* Nodos de la lista */
typedef struct {
	int data;
	struct list_head links;
}list_int_t;

char *modname = "modlistInt";

// Declaro funciones axiliares
int insert (int num);
void remove (int num);
int cleanup(void);
int push (int num);
void pop (void);
int compare (void* priv, struct list_head *a, struct list_head *b);
void sort (void);


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
		read_head = read_head->next;
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

	vfree(auxbuff);

	return dev;
};

/* Esta función ejecuta operaciones sobre la lista
   según una serie de instrucciones previamente parseadas. */
ssize_t write_modlist(struct file *filp, const char __user *buf, size_t len, loff_t *off) {

	int dev;
	int num = 0;
	char buff_modlist[512];

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
		else if(sscanf(buff_modlist, "push %d", &num)) {
			dev = push(num);
			if(dev == 2) {
				dev = 0;
				printk(KERN_INFO "modlistChar: no hay suficiente memoria");
			}
			else if(!dev)
				printk(KERN_INFO "modlistChar: error al reservar memoria");
		}
		else {
			sscanf(buff_modlist, "%s", buff_modlist);
			if(!strcmp(buff_modlist, "cleanup")) {
				dev = cleanup();
			}
			else if(!strcmp(buff_modlist, "pop")) {
				pop();
			}
			else if(!strcmp(buff_modlist, "sort")) {
				sort();
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
int insert (int num) {
	list_int_t *new = NULL;

	// Miro si me queda memoria para más elementos
	if((mem + sizeof(list_int_t)) >= BUFFER_KERNEL){
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
};

/* ------------------------------ OP2 ------------------------*/
/* Insertar en la cabeza */
int push (int num) {
	list_int_t *new = NULL;

	// Miro si me queda memoria para más elementos
	if((mem + sizeof(list_int_t)) >= BUFFER_KERNEL){
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
		list_add(&new->links, &mylist);

		// Actualizo parámetros de control
		mem += sizeof(list_int_t);
	}

	read_head = mylist.next;

	return 1;
};

/* Eliminar de la cabeza de la lista */
void pop (void) {
	list_int_t *node = NULL;

	printk(KERN_INFO "modlistInt: pop\n");
	/* Miro si la lista está vacía */
	if(list_empty(&mylist)){
		printk(KERN_INFO "modlistInt: lista vacía\n");
		return;
	}
		

	node = list_entry(mylist.next, list_int_t, links);

	// Elimino el elemento
	list_del(mylist.next);
	// Libero memoria
	vfree(node);
	// Actualizo parámetros de control
	mem -= sizeof(list_int_t);
	read_head = mylist.next;
};

/* Ordenar la lista */
int compare (void* priv, struct list_head *a, struct list_head *b){
	/* Obtengo los nodos */
	list_int_t *nodeA = list_entry(a, list_int_t, links);
	list_int_t *nodeB = list_entry(b, list_int_t, links);
	int x, y;

	/* Busco los datos */
	x = nodeA->data;
	y = nodeB->data;

	/* Calculo el resultado */
	if(x>y)
		return 1;
	else if(x<y)
		return -1;
	return 0;
};

void sort (void) {
	list_sort(NULL, &mylist, compare);
	read_head = mylist.next;
};

/* ------------------------------ OP2 ------------------------*/

#endif /* _MODLISTOP */
