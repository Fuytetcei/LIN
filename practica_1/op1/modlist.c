/*

	CARGA/DESCARGA DEL MÓDULO

*/

#include "modlistOp.h"

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
	proc_entry = proc_create(modname, 0666, NULL, &fops);
	if(!proc_entry) {
		// Si hay error libero memoria y salgo con error
		vfree(buff_modlist);
		printk(KERN_INFO "%s: No se pudo crear entrada /proc.\n", modname);
		return -ENOMEM;
	}
	printk(KERN_INFO "%s: entrada /proc creada.\n", modname);

	// Inicializo la cabeza de la lista
	//LIST_HEAD(mylist);
	//INIT_LIST_HEAD(&mylist);
	mylist.prev = &mylist;
	mylist.next = &mylist;
	// Inicializo la cabeza de lectura
	read_head = &mylist;

	/* Devolver 0 para indicar una carga correcta del módulo */
	return 0;
};

// Descarga del módulo
void modulo_modlist_clean(void) {
	struct list_head *pos = NULL;
	struct list_head *n = NULL;

	// Libero la memoria de la lista si no está vacía
	if(list_empty(&mylist)){
		list_for_each_safe(pos, n, &mylist){
			//clear Elimino el elemento
			list_del(pos);
			// Libero memoria
			vfree(pos);
		}
	}
	// Elimino la entrada a /Proc
	remove_proc_entry(modname, NULL);
	// Libero memoria
	vfree(buff_modlist);
	printk(KERN_INFO "%s: descargado.\n", modname);
};

/* Declaración de funciones init y cleanup */
module_init(modulo_modlist_init);
module_exit(modulo_modlist_clean);