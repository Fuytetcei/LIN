

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
#include <linux/slab.h>
#include "cbuffer.h"
#include "list.h"

#define BUFFER_LENGTH 40

static struct proc_dir_entry *proc_entry_config;
static struct proc_dir_entry *proc_entry;

// Estructura del timer
struct timer_list my_timer;

// Buffer circulas
cbuffer_t *buff;

// Lista enlazada
struct list_head parlist, imparlist;

typedef struct {
    unsigned int data;
    struct list_head links;
}list_item_t;

// Datos de configuración
int timer_period_ms;
int max_randoms;
int emergency_threshold;

// Contador de referencias
int cr;
int read_wait;
int par=0, impar=1;
bool parb=false, imparb=false, buffb=false;

bool flag;

struct work_struct newwork;
static struct workqueue_struct *my_wq;

// Mutex para sincronización
DEFINE_SPINLOCK(mtxCBuff);

// Semáforos
struct semaphore mysem, conssem;

// Función de workqueue
void bottom_half (struct work_struct *work) {
    int _size, i;
    list_item_t *newnode;
    unsigned long flag;
    unsigned int auxbuff[BUFFER_LENGTH];

    printk(KERN_INFO "modtimer: bottom_half\n");
    spin_lock_irqsave(&mtxCBuff, flag);

    _size = size_cbuffer_t(buff)/4;
    remove_items_cbuffer_t(buff, (char*)&auxbuff[0], _size*4);

    spin_unlock_irqrestore(&mtxCBuff, flag);

    printk(KERN_INFO "Vuelco elementos\n");
    // Vuelco los elementos a la lista
    for(i=0;i<_size;i++) {
        newnode = (list_item_t*) vmalloc(sizeof(list_item_t));
        if(!newnode)
            return;
        // Asigno datos
        newnode->data = auxbuff[i];

        // Espero para escribir
        if(down_interruptible(&mysem))
            return;

        // Añado elemento
        if(newnode->data%2)
            list_add_tail(&newnode->links, &imparlist);
        else
            list_add_tail(&newnode->links, &parlist);

        // Despierto a algún proceso en espera
        up(&mysem);
        printk(KERN_INFO "modtimer: bottom half: %d\n", newnode->data);
    }

    printk(KERN_INFO "elementos volcados\n");

    // Despierto a un posible consumidor
    if(down_interruptible(&mysem))
            return;
    if(read_wait>0) {
        up(&conssem);
        read_wait--;
    }
    up(&mysem);
}

// Función del timer
static void fire_timer(unsigned long data) {
    unsigned int ran;
    int cpu = 0;
    unsigned long flag = 0;
    int size;

    // Genero un número aleatorio
    ran = get_random_int()%max_randoms;

    // Escribo en el buffer circular
    spin_lock_irqsave(&mtxCBuff, flag);

    insert_items_cbuffer_t(buff, (char*) &ran, 4);
    size = size_cbuffer_t(buff);

    spin_unlock_irqrestore(&mtxCBuff, flag);

    printk(KERN_INFO "Random insertado\n");

    // Difiero tarea
    if(emergency_threshold <= size) {
        // Obtengo cpu actual
        cpu = smp_processor_id();

        // Elijo cpu
        if(cpu%2)
            cpu--;
        else
            cpu++;
        printk(KERN_INFO "Difiero\n");
        schedule_work_on(cpu, &newwork);
    }

    mod_timer(&(my_timer), (jiffies + HZ) * (timer_period_ms/1000));
}

// Funciones /proc/modtimer
ssize_t read_modtimer(struct file *filp, char __user *buf, size_t len, loff_t *off) {
    int data;
    list_item_t* node;
    char dev[50];
    int *r = filp->private_data;
    struct list_head *mylist;



    if(*r)
        mylist = &imparlist;
    else
        mylist = &parlist;

    // Espero a que haya elementos
    if(down_interruptible(&mysem))
        return -EINTR;
    while(list_empty(mylist) || (cr<2)) {
        read_wait++;
        up(&mysem);
        if(down_interruptible(&conssem)){
            down(&mysem);
            read_wait--;
            up(&mysem);
            return -EINTR;
        }
        if(down_interruptible(&mysem))
            return -EINTR;

    }

    node = list_entry(mylist->next, list_item_t, links);
    list_del(mylist->next);


    up(&mysem);

    // Paso al usuario
    data = node->data;
    data = sprintf(dev, "%d\n", data);
    copy_to_user(buf, (const void*)&dev, data);

    // Libero memoria
    vfree(node);

    (*off)+=data;

    return data;
}

ssize_t release_modtimer (struct inode *node, struct file * fd) {
    int *r = fd->private_data;

    // Espero a que acabe el timer y lo apago
    del_timer_sync(&my_timer);

    // Miro si soy el último
    if(down_interruptible(&mysem))
        return -EINTR;

    // Miro que lista debo vaciar
    if(!(*r))
        parb = false;
    else
        imparb = false;

    cr--;

    up(&mysem);

    return 0;
}

ssize_t open_modtimer(struct inode *node, struct file * fd) {

    // Miro so soy el primero en llegar
    if(down_interruptible(&mysem))
        return -EINTR;
    if(cr>2) {
        up(&mysem);
        printk(KERN_INFO "timer_mod: ya hay dos usuarios\n");
        return -1;
    }
    cr++;
    // Si soy el primero me pongo a la espera
    if(cr==1) {
        // Miro si hay una lista ya escogida
        if(!parb){
            fd->private_data = &par;
            parb = true;
        } else {
            fd->private_data = &impar;
            imparb = true;
        }
        up(&mysem);
        return 0;
    }

    // Si soy el segundo cojo la lilsta restante
    if(!parb) {
        fd->private_data = &par;
        parb = true;
    } else {
        fd->private_data = &impar;
        imparb = true;
    }

    up(&mysem);

    // Creo el timer
    init_timer(&my_timer);

    // Inicializo el timer
    my_timer.data=0;
    my_timer.function=fire_timer;
    my_timer.expires=(jiffies + HZ) * (timer_period_ms/1000);

    // Por último activo el timer
    add_timer(&my_timer);

    // Despierto al otro lector que estará esperando
    up(&conssem);

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
        emergency_threshold = emergency_threshold/BUFFER_LENGTH;
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
    emergency_threshold = 30;

    // Creo cola privada
    my_wq = create_workqueue("my_queue");

    INIT_WORK(&newwork, bottom_half);

    buff = create_cbuffer_t(BUFFER_LENGTH);

    INIT_LIST_HEAD(&parlist);
    INIT_LIST_HEAD(&imparlist);

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

    cr = 0;

    // Inicializo semáforo
    sema_init(&mysem, 1);
    sema_init(&conssem, 0);

    // Inicio flags de control
    parb = false;
    imparb = false;
    buffb = false;

    read_wait = 0;

    return 0;
}

void cleanup_timer_module( void ){

    struct list_head *pos = NULL, *n = NULL;
    struct list_item_t *node=NULL;


    remove_proc_entry("modtimer", NULL);
    remove_proc_entry("modconfig", NULL);
    

    // Espero a que terminen los trabajos
    flush_workqueue( my_wq );
    destroy_workqueue( my_wq );

    list_for_each_safe(pos, n, &parlist){
        // Obtengo el puntero de la estructura del nodo
        node = list_entry(pos, list_item_t, links);
        // Elimino el elemento
        list_del(pos);
        // Libero memoria
        vfree(node);
    }
    printk(KERN_INFO "modconfig: lista par eliminada\n");

    list_for_each_safe(pos, n, &imparlist){
        // Obtengo el puntero de la estructura del nodo
        node = list_entry(pos, list_item_t, links);
        // Elimino el elemento
        list_del(pos);
        // Libero memoria
        vfree(node);
    }
    printk(KERN_INFO "modconfig: lista impar eliminado\n");

    destroy_cbuffer_t(buff);
    printk(KERN_INFO "modconfig: buffer eliminada\n");


    printk(KERN_INFO "modconfig: descargado.\n");
}

module_init( init_timer_module );
module_exit( cleanup_timer_module );

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("P4-LIN");
MODULE_AUTHOR("Manuel y Dani");
