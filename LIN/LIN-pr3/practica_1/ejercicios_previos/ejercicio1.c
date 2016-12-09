
/*
	MÓDULO BÁSICO DE PRUEBA
	LA DIFERENCIA ENTRE KERN_INFO Y KERN_ALERT ES LA PRIORIDAD CON LA QUE
	SE ESCRIBE EN EL LOG DEL KERNEL
*/

#include <linux/module.h> /* Requerido por todos los módulos */
#include <linux/kernel.h> /* Definición de KERN_INFO */

MODULE_LICENSE("GPL"); /* Licencia del módulo */

/* Función que se invoca cuando se carga el módulo en el kernel */
int modulo_lin_init(void){
	printk(KERN_INFO "INFO. Modulo LIN cargado. Hola kernel.\n");

	printk(KERN_ALERT "ALERT. Hola kernel.\n");

	/* Devolver 0 para indicar una carga correcta del módulo */
	return 0;
}


/* Función que se invoca cuando se descarga el módulo del kernel */
void modulo_lin_clean(void)
{
	printk(KERN_INFO "Modulo LIN descargado. Adios kernel.\n");
}

/* Declaración de funciones init y cleanup */
module_init(modulo_lin_init);
module_exit(modulo_lin_clean);

/*
	EJERCICIO 2

	SI DEVUELVO UN NÚMERO NEGATIVO INDICANDO ERROR AL CARGAR EL MÓDULO
	EL SISTEMA INTERRUMPE SU CARGA EN EL KERNEL Y MUESTRA EL MENSAJE DE
	ERROR POR EL SHELL
*/








