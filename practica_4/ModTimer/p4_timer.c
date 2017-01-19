

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/random.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/unistd.h>
#include <linux/vmalloc.h>
#include <linux/semaphore.h>
#include <asm-generic/uaccess.h>
#include "cbuffer.h"
#include "list.h"

static struct proc_dir_entry *proc_entry_config;
static struct proc_dir_entry *proc_entry;

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

// Contador de referencias
int cr;

bool flag;

// Mutex para sincronización
DEFINE_SPINLOCK(mtxCBuff);
DEFINE_SPINLOCK(mtxList);

// Semáforo
struct semaphore mysem;

// Función de workqueue
void bottom_half (struct work_struct *work) {
    int _data, i;
    list_item_t *newnode;
    unsigned long flag;

    printk(KERN_INFO "modtimer: bottom_half\n");
    for(i=0;i<emergency_threshold;i++) {
        newnode = (list_item_t*) vmalloc(sizeof(list_item_t));
        if(!newnode)
            return;
        printk(KERN_INFO "modtimer: extraigo elemento\n");
        // Leo del buffer
        spin_lock_irqsave(&mtxCBuff, flag);

        // Extraigo elemento
        remove_items_cbuffer_t(buff, (char*) &_data, 4);

        spin_unlock_irqrestore(&mtxCBuff, flag);

        printk(KERN_INFO "modtimer: inserto elemento\n");

        // Asigno datos
        newnode->data = _data;

        // Escribo en lista enlazada
        spin_lock(&mtxList);
        // Espero a que se consuman los elementos
        while(list_empty(&mylist)) {
            spin_unlock(&mtxList);
            if(down_interruptible(&mysem))
                return -EINTR;
            spin_lock(&mtxList);
        }

        // Añado elemento
        list_add_tail(&newnode->links, &mylist);
        up(&mysem);

        spin_unlock(&mtxList);
        printk(KERN_INFO "modtimer: elemento guardado: %d\n", _data);
    }

    // Planifico timer de nuevo
    mod_timer( &(my_timer), (jiffies + HZ)*(timer_period_ms*1000));
    printk(KERN_INFO "modtimer: timer reprogramado\n");
}

// Función del timer
static void fire_timer(unsigned long data) {
    unsigned int ran;
    int cpu = 0;
    struct work_struct newwork;
    unsigned long flag;

    // Genero un número aleatorio
    ran = get_random_int();

    printk(KERN_INFO "modtimer: %d\n", (int)ran);

    // Escribo en el buffer circular
    /*************** SECCION CRITICA *******************/
    spin_lock_irqsave(&mtxCBuff, flag);
    printk(KERN_INFO "modtimer: Inserto en el buffer\n");
    insert_items_cbuffer_t(buff, (char*) &ran, 4);

    // Difiero tarea
    printk(KERN_INFO "modtimer: Miro si tengo que diferir\n");
    if(emergency_threshold <= size_cbuffer_t(buff)) {

        printk(KERN_INFO "modtimer: difiero\n");
        // Obtengo cpu actual
        cpu = smp_processor_id();

        // Elijo cpu
        if(cpu%2)
            cpu--;
        else
            cpu++;

        INIT_WORK(&newwork, bottom_half);
        schedule_work_on(cpu, &newwork);
    }

    // Miro si debo reactivar el timer
    printk(KERN_INFO "modtimer: Miro si reactivo timer\n");
    if(!is_full_cbuffer_t(buff)) {
        printk(KERN_INFO "modtimer: Reactivo timer\n");
        mod_timer( &(my_timer), (jiffies + HZ) * (timer_period_ms/1000));
    }
    spin_unlock_irqrestore(&mtxCBuff, flag);
    printk(KERN_INFO "modtimer: Fin timer\n");
}

// Funciones /proc/modtimer
ssize_t read_modtimer(struct file *filp, char __user *buf, size_t len, loff_t *off) {
    int data;
    list_item_t* node;

    printk(KERN_INFO "modtimer: read_modtimer\n");
    spin_lock(&mtxList);

    // Espero a que haya elementos
    printk(KERN_INFO "modtimer: espero a que haya elementos\n");
    /*if(!list_empty(&mylist)) {
        down(&mysem);
        printk(KERN_INFO "modtimer: esperado\n");
    }*/

    while(!list_empty(&mylist)) {
        spin_unlock(&mtxList);
        if(down_interruptible(&mysem)) {
            return -EINTR;
        }
        spin_lock(&mtxList);
    }

    printk(KERN_INFO "modtimer: extraigo elemento\n");
    // Extraigo dato
    node = list_entry(mylist.next, list_item_t, links);

    printk(KERN_INFO "modtimer: elimino elemento de la lista\n");
    // Elimino dato de la lista
    list_del(mylist.next);

    printk(KERN_INFO "modtimer: extraigo datos\n");
    // Paso al usuario
    data = node->data;

    up(&mysem);

    spin_unlock(&mtxList);

    printk(KERN_INFO "modtimer: devuelvo al usuario: %d\n", data);
    copy_to_user(buf, &data, 4);

    printk(KERN_INFO "modtimer: libero memoria\n");
    // Libero memoria
    vfree(node);

    (*off)+=4;

    return 4;
}

ssize_t release_modtimer (struct inode *node, struct file * fd) {
    struct list_head *pos = NULL, *n = NULL;

    // Espero a que acabe el timer y lo apago
    del_timer_sync(&my_timer);

    // Espero a que terminen los trabajos
    flush_scheduled_work();

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

    return 0;
}

ssize_t open_modtimer(struct inode *node, struct file * fd) {

    if(cr) {
        printk(KERN_INFO "timer_mod: ya hay un usuario\n");
        return -1;
    }

    // Creo el buffer
    buff = create_cbuffer_t(max_randoms*4);

    // Creo la lista
    INIT_LIST_HEAD(&mylist);

    // Inicializo semáforo
    sema_init(&mysem, 0);

    // Inicio contador
    cr = 0;

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

ssize_t read_modconfig(struct file *filp, char __user *buf, size_t len, loff_t *off) {

    char aux[100];
    int i = 0;


    if(flag) {
        memset(aux, '\0', 100);
        i = sprintf(aux, "timer_period_ms=%d\nemergency_threshold=%d\nmax_random=%d\n", timer_period_ms, emergency_threshold, max_randoms);
        copy_to_user(buf, aux, 100);
        flag=!flag;
        (*off)+=i;
    }
    else {
        flag = !flag;
        (*off) = (loff_t) filp;
        i=0;
    }

    return i;
}

// Funcion /proc/modconfig
ssize_t write_modconfig(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
    char aux[100];

    // Copio desde usuario
    memset(aux, '\0', 100);
    copy_from_user(aux, buf, len);

    // Compruebo que variable hay que modificar
    if(!strncmp(aux, "timer_period_ms", 15)) {
        timer_period_ms = simple_strtol(&aux[0]+16, NULL, 10);
    }

    if(!strncmp(aux, "max_randoms", 11)) {
        max_randoms = simple_strtol(&aux[0]+12, NULL, 10);
    }

    if(!strncmp(aux, "emergency_threshold", 19)) {
        emergency_threshold = simple_strtol(&aux[0]+20, NULL, 10);
    }

    (*off)+=len;

    return len;
}

static const struct file_operations fops = {
    .read = read_modtimer,
    .release = release_modtimer,
    .open = open_modtimer,
};

static const struct file_operations fopsconf = {
    .read = read_modconfig,
    .write = write_modconfig,
};

int init_timer_module( void ) {

    // Asigno una configuración por defecto
    max_randoms = 10;
    timer_period_ms = 1000;
    emergency_threshold = 8;

    // Registro el modtimer
    proc_entry = proc_create("modtimer", 0666, NULL, &fops);
    if(!proc_entry) {
        // Si hay error libero memoria y salgo con error
        printk(KERN_INFO "modtimer: No se pudo crear entrada /proc.\n");
        return -ENOMEM;
    }
    printk(KERN_INFO "modtimer: entrada /proc creada.\n");

    // Registro la configuración
    // Creo la entrada a /Proc
    proc_entry_config = proc_create("modconfig", 0666, NULL, &fopsconf);
    if(!proc_entry_config) {
        // Si hay error libero memoria y salgo con error
        printk(KERN_INFO "modconfig: No se pudo crear entrada /proc.\n");
        return -ENOMEM;
    }
    printk(KERN_INFO "modconfig: entrada /proc creada.\n");

    flag = true;

    return 0;
}

void cleanup_timer_module( void ){

    remove_proc_entry("modtimer", NULL);
    printk(KERN_INFO "modtimer: descargado.\n");

    remove_proc_entry("modconfig", NULL);
    printk(KERN_INFO "modconfig: descargado.\n");
}

module_init( init_timer_module );
module_exit( cleanup_timer_module );

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("P4-LIN");
MODULE_AUTHOR("Manuel y Dani");
