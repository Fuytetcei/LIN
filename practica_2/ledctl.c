/*

	PARTE B DE LA PRÁCTICA 2

*/

#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <asm-generic/errno.h>
#include <linux/init.h>
#include <linux/tty.h>      /* For fg_console */
#include <linux/kd.h>       /* For KDSETLED */
#include <linux/vt_kern.h>

/* Obtengo el handler del driver */
struct tty_driver* get_kbd_driver_handler(void){
   return vc_cons[fg_console].d->port.tty->driver;
}

/* Establezco los leds mediante la máscara */
static inline int set_leds(struct tty_driver* handler, unsigned int mask){
    return (handler->ops->ioctl) (vc_cons[fg_console].d->port.tty, KDSETLED,mask);
}

SYSCALL_DEFINE1 (lin_ledctl, unsigned int, leds) {
	/* Obtengo el manejador del driver
	   y lo paso a set_leds */
	if (leds > 7)
		return -EINVAL;
   	set_leds(get_kbd_driver_handler(),mask);
	return 0;
}

