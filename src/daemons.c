#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>

#include "daemons.h"

int demonizar(char *servicio) {
	int sessionId;
	pid_t pid;

	// Creamos un proceso hijo y terminamos el padre
	pid = fork();
	if(pid < 0) exit(EXIT_FAILURE);
	if(pid > 0) exit(EXIT_SUCCESS);

	// Abrimos syslog para el futuro
	openlog(servicio, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL3);

	// Creamos una nueva sesión
	sessionId = setsid();
	if(sessionId < 0) {
		syslog(LOG_ERR,"Error creating a new SID for the child process.");
		exit(-1);
	}

	// Cambiamos la máscara
	umask(S_IRWXU | S_IRWXG | S_IRWXO);
	setlogmask(LOG_UPTO(LOG_INFO));

	//Establecemos el directorio raíz
	if(chdir("/") < 0) {
		syslog(LOG_ERR,"Error changing the current working directory = \"/\"");
		exit(-1);
	}

	//Cerramos todos los descriptores de fichero
	int n_max = getdtablesize();
	for(int i = 0;  i < n_max; i++) {
		close(i);
	}

	return(0);
}


//Scripts de prueba
float cambio_temperatura(float celsius) {
	return (celsius*9/5 + 32);
}

char * saludar(char *nombre) {
	char saludo[]="Hola";
	return strcat(saludo, nombre);
}