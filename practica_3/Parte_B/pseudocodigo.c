
// Llamada a open
ssize_t open_modFIFO(struct inode *node, struct file * fd) {

	lock(mtx);
	// Miro si es productor o consumidor
	if(fd->f_mode & FMODE_READ) {
		// Sumo uno al consumidor
		cons_count++;

		cond_signal(prod);
		// Espero a que haya un productor
		while(prod_count==0) {
			cond_wait(cons,mtx);
		}
	}
	else{
		// Sumo uno al productor

		prod_count++;

		cond_signal(cons)

		// Espero a que haya un consumidor
		while(cons_count==0) {
			cond_wait(proc,mtx)
		}
	}
	unlock(mtx)
	return 0;
}

// Llamada a release
ssize_t release_modFIFO (struct inode *node, struct file * fd) {

	// Miro si es productor o consumidor
	lock(mtx)	
	if(fd->f_mode & FMODE_READ) {
		// Consumidor
		// Resto uno al consumidor
		cons_count--;
		signal_cond(prod, mtx);
	}
	else {
		// Productor
		// Resto uno al productor
		prod_count--;
		signal_cond(cons, mtx);
	}
	if(cons_count== 0 && prod_count ==0)
		clear_cbuffer(cbuffer);
	unlock(mtx)
	return 0;
};

// Consumir del buffer
ssize_t read_modFIFO(struct file *filp, char __user *buf, size_t len, loff_t *off){

	char data[cbuffer->max_size];

	// Inicializo buffer auxiliar
	memset(data, '\0', cbuffer->max_size);

	// Miro si puedo leer los bytes requeridos
	if (len > cbuffer->max_size) { return -EINTR; }

// Seccion crítica
	lock(mtx)

	// Espero hasta que haya algo que leer y algún productor
	while((prod_count>0) && (len > size_cbuffer_t(cbuffer))){
		cond_wait(prod,mtx);
	}

	// Compruebo fin de comunicación
	if ((prod_count==0) && (is_empty_cbuffer_t ( cbuffer ) < 0)) {unlock(mtx); return 0;}

	// Extraigo datos
	remove_items_cbuffer_t(cbuffer, data, len);
	// Despierto a un posible productor
	cond_signal(prod);

	unlock(mtx);
// Fin sección crítica

	// Mando los datos al usuario y devuelvo bytes leídos
	if(copy_to_user(buf, data, len))
		return -EINTR;

	(*off)+=len;

	return len;

};


// Producir en el buffer
ssize_t write_modFIFO(struct file *filp, const char __user *buf, size_t len, loff_t *off){

	char data[cbuffer->max_size];

	// Inicializo buffer auxiliar
	memset(data, '\0', cbuffer->max_size);

	// Copio los datos del usuario
	if(copy_from_user(data, buf, len))
		return -EFAULT;

	if (len > cbuffer->max_size) { return -EFAULT;}

// Seccion crítica
	lock(mtx)
	// Miro si puedo leer los bytes requeridos
	// Espero hasta que haya algo que leer y algún productor
	while((cons_count>0) && (len > nr_gaps_cbuffer_t(cbuffer))){
		cond_wait(prod,mtx)
	}


	// Compruebo fin de comunicación
	if (cons_count==0) {unlock(&mtx); return -EPIPE;}

	// Inserto datos datos
	insert_items_cbuffer_t(cbuffer, data, len);

	// Despierto a un posible productor
	cond_signal(cons);

	unlock(mtx);
// Fin sección crítica

	(*off)+=len;

	// Devuelvo bytes escritos
	return len;
};
