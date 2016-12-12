/*

	.H PARA LOS #DEFINES E #INCLUDES

*/

#include <getopt.h>
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <err.h>
#include <errno.h>
#include <linux/module.h>	/* Requerido por todos los módulos */
#include <linux/unistd.h>
#include <linux/kernel.h>	/* Definición de KERN_INFO */
#include <linux/proc_fs.h>	/* Para cargar y descargar módulos del kernel */
#include <linux/string.h>	/* para tratar las cadenas */
#include <linux/vmalloc.h>	/* Para reservar y liberar memoria e el kernel */
#include <linux/semaphore.h>
#include <asm-generic/uaccess.h>
#include "cbuffer.h"


MODULE_LICENSE("GPL");		/* Licencia del módulo */
MODULE_DESCRIPTION("modFIFO Kernel Module - LIN-FDI-UCM");
MODULE_AUTHOR("Daniel y Manuel");

// Tamaño del buffer de entrada desde el espacio de usuario
#define BUFFER_LENGTH 512

/* Tamaño del buffer del kernel, para controlar el tamaño de la lista.
   Solo hasta la mitad */
#define BUFFER_KERNEL 1024


static struct proc_dir_entry *proc_entry; 	// Puntero al la entrada /Proc

/* Variables globales */

cbuffer_t* cbuffer; /* Buffer circular */
int prod_count = 0;
/* Número de procesos que abrieron la entrada
/proc para escritura (productores) */
int cons_count = 0;
/* Número de procesos que abrieron la entrada
/proc para lectura (consumidores) */
struct semaphore mtx;
/* para garantizar Exclusión Mutua */
struct semaphore sem_prod; /* cola de espera para productor(es) */
struct semaphore sem_cons; /* cola de espera para consumidor(es) */
int nr_prod_waiting=0; /* Número de procesos productores esperando */
int nr_cons_waiting=0; /* Número de procesos consumidores esperando */

