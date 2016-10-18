/*

	.H PARA LOS #DEFINES E #INCLUDES

*/

#include <linux/module.h>	/* Requerido por todos los módulos */
#include <linux/kernel.h>	/* Definición de KERN_INFO */
#include <linux/proc_fs.h>	/* Para cargar y descargar módulos del kernel */
#include <linux/string.h>	/* para tratar las cadenas */
#include <linux/vmalloc.h>	/* Para reservar y liberar memoria e el kernel */
#include <asm-generic/uaccess.h> /* PARA QUE VALE ESTO?!!*/
#include "list.h"

MODULE_LICENSE("GPL");		/* Licencia del módulo */
MODULE_DESCRIPTION("modlist Kernel Module - LIN-FDI-UCM");
MODULE_AUTHOR("Daniel y Manuel");

// Tamaño del buffer de entrada desde el espacio de usuario
#define BUFFER_LENGTH 256

/* Tamaño del buffer del kernel, para controlar el tamaño de la lista.
   Solo hasta la mitad */
#define BUFFER_KERNEL 4096


static struct proc_dir_entry *proc_entry; 	// Puntero al la entrada /Proc
static char *buff_modlist;				// Buffer de alamcenamiento

// -- GESTIÓN DE LA LISTA -----------------------------------------

// Declaro la lista a gestionar
/* Lista enlazada */
struct list_head mylist;

/* Guardo la cabeza de la lista además del puntero de lectura
   que me den para ahorrarme recorrer la lista cada vez que
   quiewra un elemento */
struct list_head *read_head;

/* Nodos de la lista */
typedef struct {
	int data;
	struct list_head links;
}list_item_t;

// Memoria utilizada (bytes)
int mem;

// Declaro funciones axiliares
int insert (int num);
int remove (int num);