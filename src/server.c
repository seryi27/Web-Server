#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

#include "daemons.h"
#include "server.h"
#include "picohttpparser.h"
#include "file.h"
#include "leerconfig.h"

#define MAX_CONNECTIONS 5
#define DEFAULT_MIME_TYPE "application/octet-stream"
// #define SERVER_ROOT "htmlfiles"
// #define MAX_CLIENTS 10
#define SERVER_NAME "mapache-server 3.0"
// #define LISTEN_PORT 8080
#define OK_RESPONSE "HTTP/1.1 200 OK"
#define	BAD_REQUEST 400
#define NOT_ALLOWED 405

int max_connections, max_clients, listen_port;
char server_root[128];
char server_signature[128];
char directorio[64];

/*
 * Funcion que, dado una ruta absoluta a un archivo, construye una cabecera HTTP
 * basica, y la devuelve en header
 * TODO: Añadir parametros de la cabecera, aumentando la funcionalidad.
 */
void build_header(char *header, char *filePath, int length) {

	char date[30], lastModified[30];
	time_t rawtime;
   	struct tm *info;

	time( &rawtime );
    info = localtime( &rawtime );

    //strftime con timestamp
    struct stat attrib;
    stat(filePath, &attrib);
    strftime(date, 50, "%a, %d %b %Y %X %Z", info);
    strftime(lastModified, 50, "%a, %d %b %Y %X %Z", localtime(&(attrib.st_ctime)));

	sprintf(header, "Date: %s\nServer: %s\nLast-Modified: %s\nContent-Length: %d\nContent-Type: %s\n\n", date, SERVER_NAME, lastModified, length, mime_type_get(filePath));
}

/*
 * Función que inicializa el servidor para escuchar por un puerto, que recibe como argumento
 *	TODO: sacar el listen al main, y eliminar el wait_request (se llamará mediante la pool de hilos)
 */
int initialize_server(int port) {

	int sockfd;
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        syslog(LOG_ERR, "ERROR opening socket");

    bzero((char *) &serv_addr, sizeof(serv_addr));


    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        syslog(LOG_ERR, "ERROR on binding");

    listen(sockfd,5);

    //Hasta aquí la parte de inicialización

    wait_request(sockfd, &cli_addr);
}

/*
 * Función principal de cada hilo: esperan a que algún cliente pida algo
 * y atienden la petición con respond
 */
int wait_request(int sockfd, struct sockaddr_in *cli_addr) {

	int client;
	int clilen = sizeof(struct sockaddr_in);

	while(1) {
		client = accept(sockfd, (struct sockaddr *) cli_addr, &clilen);
		if(client < 0) {
			syslog(LOG_ERR, "accept error");
		} else {
			handle_http_request(client);
		}
	}
}

/**
 * Funcion que envía finalmente la información pedida por el cliente
 */
int send_response(int fd, char *header, void *body, int content_length) {

	const int max_response_size = 262144;
	char response[max_response_size];
	int response_length;

	strcat(response, header);
	strcat(response, (char *) body);
	response_length = strlen(response);

	int rv = send(fd, response, response_length, 0);

    if (rv < 0) {
        syslog(LOG_ERR, "send");
    }

	return rv;
}
//Funcion que ejecuta los scripts
//filepath: ruta del archivo, tiene que ser ya absoluto
//interprete: python o php (tenemos que hacer la comprobacion del tipo antes de llamar a la funcion)

FILE * exec_cgi(const char* interprete, const char*filepath, const char* get_paramametros,
	 size_t get_params_len, const_char* post_parametros, size_t post_params_len)
	 {
		 char buff[10000];
		 sprintf(buff,"echo -n \"%.*s\" | %s %s \"%.*s\"",post_params_len,post_parametros,interprete,filepath,get_params_len,get_parametros);
		 return popen(buff,"r");

}

/**
 * Funcion con la que parseamos la peticion http y llamamos a las funciones pertinentes
 * para tratarlas.
 */
void handle_http_request(int fd) {

    char buf[4096], absolute_path[4096], header[4096], *method, *path;
	int pret, minor_version;
	struct phr_header headers[100];
	struct file_data *filedata;
	size_t buflen = 0, prevbuflen = 0, method_len, path_len, num_headers;
	ssize_t rret;

	while (1) {
	    /* read the request */
	    while ((rret = read(fd, buf + buflen, sizeof(buf) - buflen)) == -1 && errno == EINTR)
	    	;
	    if (rret < 0) {
	    	syslog(LOG_ERR, "IOError\n");
	    	return;
	    }

	    prevbuflen = buflen;
	    buflen += rret;
	    /* parse the request */
	    num_headers = sizeof(headers) / sizeof(headers[0]);

	    pret = phr_parse_request(buf, buflen, (const char **) &method, &method_len, (const char **) &path, &path_len,
	                             &minor_version, headers, &num_headers, prevbuflen);

	    if (pret > 0)
	        break; /* successfully parsed the request */
	    else if (pret == -1) {
	        syslog(LOG_ERR, "ParseError\n");
	    	return;
	    }
	    /* request is incomplete, continue the loop */
	    //assert(pret == -2);
	    if (buflen == sizeof(buf)) {
	        syslog(LOG_ERR, "RequestIsTooLongError\n");
	    	return;
	    }
	}

	if(strncmp(method, "GET", method_len)) {

		snprintf(absolute_path, sizeof absolute_path, "%s%.*s", SERVER_ROOT, (int) path_len, path);

		if(strcmp(mime_type_get(absolute_path), "python3") != 0 && strcmp(mime_type_get(absolute_path), "php")) {

			filedata = file_load(absolute_path);

			build_header(header, absolute_path, filedata->size);
			snprintf(header, sizeof header, "%s\n%s", OK_RESPONSE, header);
			send_response(fd, header, filedata->data, filedata->size);

			file_free(filedata);

		} else {

		}

	}
	else if (strncmp(method, "POST", method_len)) {


		int fichero; //descriptor
		char *interprete=strcmp(mime_type_get(absolute_path);
		FILE* py_result;
		char buf[1024];

		//comprobacion de si el fichero existe, se lo estoy copiando a carlos, mañana lo hablamos
		if((fichero=open(path, O_RDONLY))== -1){
			syslog(LOG_ERR, "Error en exec_cgi abriendo el fichero");
			//TO DO mandar un condigo de error 404, integralo
		}

		//TO DO obtener el resto de parametros de la funcion
		if(strcmp(interprete, "python3") == 0 || strcmp(interprete,"php")==0) {
			py_result=exec_cgi(interprete, const char*filepath, const char* get_paramametros,
				 size_t get_params_len, const_char* post_params, size_t post_params_len);

			if(py_result == null){
				//TODO devolver error 500
			}

			//mandamos el Fichero
			while (fgets(buf,py_result,1024)){
				write(sockfd,buf,strlen(buf));
				if(strcmp(buf,"/r/n")==0){
					break;
				}
			}

		} else {
			resp_error(fd, NOT_ALLOWED);
		}

	}
	else {
		resp_error(fd, BAD_REQUEST);
		return;
	}
}

/*
 * Funcion que devuelve los archivos de error correspondientes al fallo
 * durante la atencion de una peticion http.
 */
void resp_error(int fd, int code) {

    char filepath[4096];
    struct file_data *filedata;
    char header[1024];

    // Fetch the 404.html file
    snprintf(filepath, sizeof filepath, "%s/%d.html", SERVER_ROOT, code);
    filedata = file_load(filepath);

    if (filedata == NULL) {
        fprintf(stderr, "cannot find system %d file\n", code);
        exit(3);
    }

    build_header(header, filepath, 0);

    send_response(fd, strcat("HTTP/1.1 404 NOT FOUND\n", header), filedata->data, filedata->size);

    file_free(filedata);
}


/**
 * Lowercase a string
 */
char *strlower(char *s) {

    for(int i = 0; s[i]; i++){
  		s[i] = tolower(s[i]);
	}

    return s;
}

/**
 * Return a MIME type for a given filename
 */
char *mime_type_get(char *filename) {

    char *ext = strrchr(filename, '?');

    if(ext != NULL) {
    	return "script";
    } else {
    	ext = strrchr(filename, '.');

	    if (ext == NULL) {
	        return DEFAULT_MIME_TYPE;
	    }

	    ext++;

	    ext = strlower(ext);

	    if (strcmp(ext, "txt") == 0) { return "text/plain"; }
	    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) { return "text/html"; }
	    if (strcmp(ext, "gif") == 0) { return "image/gif"; }
	    if (strcmp(ext, "jpeg") == 0 || strcmp(ext, "jpg") == 0) { return "image/jpg"; }
	    if (strcmp(ext, "mpeg") == 0 || strcmp(ext, "mpg") == 0) {return "video/mpeg"; }
	    if (strcmp(ext, "doc") == 0 || strcmp(ext, "docx") == 0) {return "application/msword"; }
	    if (strcmp(ext, "pdf") == 0) { return "application/pdf"; }
	    if (strcmp(ext, "py") == {return "python3"; }
			if (strcmp(ext, "php") == 0) {return "php"; }

	    //En principio los anteriores son suficientes para esta práctica
	    if (strcmp(ext, "css") == 0) { return "text/css"; }
	    if (strcmp(ext, "js") == 0) { return "application/javascript"; }
	    if (strcmp(ext, "json") == 0) { return "application/json"; }
	    if (strcmp(ext, "png") == 0) { return "image/png"; }

	    return DEFAULT_MIME_TYPE;
	}
}


int main () {
	char *fichero="server.conf";
	ini_config(char *fichero);

	//preguntale a carlos el por que de esto
	getcwd(directorio,64);
	initialize_server(listen_port);
	return 0;
}
