
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/random.h>
#include <linux/fs.h>

#include "cbuffer.h"

struct timer_list my_timer; /* Structure that describes the kernel timer */

cbuffer_t *buff;

int timer_period_ms;
int max_randoms;
int emergency_threshold;


/* Function invoked when timer expires (fires) */
static void fire_timer(unsigned long data)
{
    unsigned int ran;
    int cpu;

    // Genero un número aleatorio
    ran = get_random_int();

    // Escribo en el buffer circular
    insert_items_cbuffer_t(buff, (char*) &ran, 4);

    // Difiero tarea
    if(emergency_threshold <= size_cbuffer_t(buff)) {
        // Obtengo cpu actual
        cpu = smp_processor_id();

        // Elijo cpu
        if(cpu%2)
            cpu = cpu--;
        else
            cpu = cpu++;



    }
        
    // Miro si debo reactivar el timer
    if(!is_full_cbuffer_t(buff))
	   mod_timer( &(my_timer), jiffies + HZ); 
}

int init_timer_module( void )
{
    struct file *fd = NULL;

    // Leo los parámetros de configuración
    fd = filp_open("/proc/timerConf", NULL, O_RDONLY);
    if(!fd) {
        printk(KERNEL_INFO "timer: error al leer configuracion");
        return -1;
    }

    /***************************************************************/

    // Creo el buffer
    buff = create_cbuffer_t(max_randoms*4);

    // Creo el timer
    init_timer(&my_timer);

    // Inicializo el timer
    my_timer.data=0;
    my_timer.function=fire_timer;
    my_timer.expires=(jiffies + HZ) * (timer_period_ms/1000);

    // Por último activo el timer
    add_timer(&my_timer); 
    return 0;
}


void cleanup_timer_module( void ){
  // Espero a que acabe el timer y lo apago
  del_timer_sync(&my_timer);

  // Destruyo el buffer ciercular
  destroy_cbuffer_t(buff);

}

module_init( init_timer_module );
module_exit( cleanup_timer_module );

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("P4-LIN");
MODULE_AUTHOR("Manuel y Dani");
