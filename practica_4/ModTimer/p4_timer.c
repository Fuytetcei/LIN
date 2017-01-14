

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

// Workqueue
struct workqueue_struct myqueue;

// Contador de referencias
int cr;

// Mutex para sincronización
DEFINE_SPINLOCK(mtxCBuff);
DEFINE_SPINLOCK(mtxList);

// Semáforo
struct semaphore mysem;
struct semaphore waitsem;


// Funcion /proc/modconfig
ssize_t write_modconfig(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
    char aux[100];
    int i;

    // Copio desde usuario
    copy_from_user(&aux, buf, len);

    // Compruebo que variable hay que modificar
    if(!strncmp(&aux, "timer_period_ms", 15)) {
        timer_period_ms = strtol(&aux+15, NULL, 10);
    }

    if(!strncmp(&aux, "max_randoms", 11)) {
        max_randoms = strtol(&aux+11, NULL, 10);
    }

    if(!strncmp(&aux, "emergency_threshold", 19)) {
        emergency_threshold = strtol(&aux+19, NULL, 10);
    }

    (*off)+=len;

    return len;
}

ssize_t read_modtimer(struct file *filp, char __user *buf, size_t len, loff_t *off) {

    char aux[100];
    int i;

    i = vsnprintf(&aux, 100, "timer_period_ms=%d\nemergency_threshold=%d\nmax_random=%d\n", timer_period_ms, emergency_threshold, max_randoms);

    copy_to_user(buf, &aux, len)

    (*off)+=len;
}

// Funciones /proc/modtimer
ssize_t read_modtimer(struct file *filp, char __user *buf, size_t len, loff_t *off) {
    int data;
    list_item_t* node;

    // Extraigo dato
    node = list_entry(mylist.next, list_item_t, links);

    // Elimino dato de la lista
    list_del(mylist.next);
    // Paso al usuario
    data = node->data;
    copy_to_user(buf, &data, 4);

    // Libero memoria
    vfree(node);

    (*off)+=4;

    return 4;
}

ssize_t release_modtimer (struct inode *node, struct file * fd) {
    struct list_head *pos = NULL, *n = NULL;
    list_item_t *node = NULL;

    // Espero a que terminen los trabajos y destruyo workqueue
    flush_workqueue(&myqueue);
    destroy_workqueue(&myqueue);

    // Espero a que acabe el timer y lo apago
    del_timer_sync(&my_timer);

    // Destruyo el buffer ciercular
    destroy_cbuffer_t(buff);

    // Vacio la lista enlzada
    if(list_empty(&mylist)){
        list_for_each_safe(pos, n, &mylist){
            // Elimino el elemento
            list_del(pos);
            // Libero memoria
            vfree(pos);
        }
    }

    cr--;
}

ssize_t open_modtimer(struct inode *node, struct file * fd) {

    if(cr) {
        printk(KERN_INFO "timer_mod: ya ha un usuario\n");
        return -1;
    }

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

    // Inicio contador
    cr = 0;

    // Inicializo el timer
    my_timer.data=0;
    my_timer.function=fire_timer;
    my_timer.expires=(jiffies + HZ) * (timer_period_ms/1000);

    // Por último activo el timer
    add_timer(&my_timer); 

    cr++;
}

// Función de workqueue
void bottom_half (struct work_struct *work) {
    int _data;
    list_item_t newnode = (list_item_t*) vmalloc(sizeof(list_item_t));

    // Leo del buffer
    spin_lock_irqsave(&mtxCBuff);

    // Extraigo elemento
    remove_items_cbuffer_t(buff, (char*) &_data, 4);

    spin_unlock_irqrestore(&mtxCBuff);

    // Asigno datos
    newnode->data = data;

    // Escribo en lista enlazada
    spin_lock(&mtxList);
        // Espero a que se consuman los elementos
        if(list_empty(&mylist))
            down(&mysem);
        list_add_tail(newnode->links, &mylist);
        up(&mysem);

    spin_unlock(&mtxList);

    // Despierto al programa de usuario
    up(&mysem);

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

    return 0;
}


void cleanup_timer_module( void ){

}

module_init( init_timer_module );
module_exit( cleanup_timer_module );

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("P4-LIN");
MODULE_AUTHOR("Manuel y Dani");
