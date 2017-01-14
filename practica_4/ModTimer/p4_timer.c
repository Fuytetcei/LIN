

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/random.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include "cbuffer.h"

// Estructura del timer
struct timer_list my_timer;

// Buffer circulas
cbuffer_t *buff;

// Lista enlazada
struct list_head mylist;

typedef struct {
    int data;
    struct list_head links;
}list_item_t;

// Datos de configuración
int timer_period_ms;
int max_randoms;
int emergency_threshold;

list_item_t nodes[max_randoms];
int flag;

// Workqueue
struct workqueue_struct myqueue;

// Mutex para sincronización
DEFINE_SPINLOCK(mtxCBuff);
DEFINE_SPINLOCK(mtxList);

// Semáforo
// struct semaphore mysem;

// Función de workqueue
void bottom_half (struct work_struct *work) {
    int _data, i;

    // Leo del buffer
    spin_lock(&mtxCBuff);

    i=0;
    while(!is_empty_cbuffer_t(buff)) {

        // Extraigo elemento
        remove_items_cbuffer_t(buff, (char*) &_data, 4);

        // Asigno datos
        nodes[flag].data = data;

        // Escribo en lista enlazada
        spin_lock(&mtxList);
            // Espero a que se consuman los elementos
            down(&mysem);
                list_add_tail(&nodes[i].links, &mylist);
        spin_unlock(&mtxList);
        flag += (flag)%max_randoms;
    }

    // Despierto al programa de usuario
    up(&mysem);

    spin_unlock(&mtxCBuff);

    // Planifico timer de nuevo
    mod_timer( &(my_timer), jiffies + HZ);
}


// Función del timer
static void fire_timer(unsigned long data) {
    unsigned int ran;
    int cpu;
    struct work_struct *newwork;

    // Genero un número aleatorio
    ran = get_random_int();

    // Escribo en el buffer circular
    /*************** SECCION CRITICA *******************/
    spin_lock(&mtxCBuff);
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

        INIT_WORK(newwork, bottom_half);
        queue_work_on(cpu, &myqueue, newwork);
    }
        
    // Miro si debo reactivar el timer
    if(!is_full_cbuffer_t(buff))
	   mod_timer( &(my_timer), jiffies + HZ);
    spin_unlock(&mtxCBuff);
    /************** FIN SECCION CRITICA *****************/
}

int init_timer_module( void ) {
    struct file *fd = NULL;

    // Leo los parámetros de configuración
    fd = filp_open("/proc/timerConf", NULL, O_RDONLY);
    if(!fd) {
        printk(KERNEL_INFO "timer: error al leer configuracion");
        return -1;
    }

    /********************** CARGAR CONFIGURACION ************************/

    // Creo el buffer
    buff = create_cbuffer_t(max_randoms*4);

    // Creo el timer
    init_timer(&my_timer);

    // Creo workqueue
    &myqueue = create_workqueue("p4");

    // Creo la lista
    INIT_LIST_HEAD(&mylist);
    flag=0;

    // Inicializo semáforo
    sema_init(&mysem, 0);

    // Inicializo el timer
    my_timer.data=0;
    my_timer.function=fire_timer;
    my_timer.expires=(jiffies + HZ) * (timer_period_ms/1000);

    // Por último activo el timer
    add_timer(&my_timer); 
    return 0;
}


void cleanup_timer_module( void ){
    // Destruyo workqueue
    flush_workqueue(&myqueue);
    destroy_workqueue(&myqueue);

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
