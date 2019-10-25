

#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

#ifdef __cplusplus
extern "C" {
#endif


#define MAX_THREADS 16
#define MAX_QUEUE 65536

typedef struct tpool_t tpool_t;



/**
  Creacion de un objeto de tipo tpool_t

 t_count Numero de hilos que van a trabajar
 queue_tam   Tamaño de la cola
 */
tpool_t *tpool_create(int t_count, int queue_tam);

/**
 Añadimos una nueva tarea

 pool     pool al que añadimos la tarea
 function puntero a funcion que va a realizar la tarea
 argumento argumento que le pasamos a la funcion

 devolvemos 0 si funciona bien o 1 en caso de error
 */
int tpool_add(tpool_t *pool, void (*function)(void *), void *argumento);

/**
 Funcion que destruye el pool de hilos
 pool  Pool a destruir
 */
int tpool_destroy(tpool_t *pool);

#ifdef __cplusplus
}
#endif

#endif /* _THREADPOOL_H_ */
