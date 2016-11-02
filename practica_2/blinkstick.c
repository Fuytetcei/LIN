/*

	PARTE B DE LA PRÁCTICA 2, LLAMADA AL SISTEMA

*/

#include <linux/syscalls.h> /* For SYSCALL_DEFINEi() */
#include <linux/kernel.h>

SYSCALL_DEFINE2(lin_blinkstick, char*, color, int , num) {

	int fd;

	// Abro fichero
	if(!fd = open("/dev/usb/blinkstick0", O_WRONLY))
		return -ENOENT;

	// Escribo en fichero
	if(0 > write(fd, color, (num*10)+(num-1)))
		return -EIO;

	// Cierro fichero
	close(fd);

	return 0;
}