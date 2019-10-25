/**
 * @file threadpool.c
 * @brief Threadpool implementation file
 */

#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "thread-pool.h"
#include <syslog.h>


typedef struct {
    void (*function)(void *); //puntero a funcion que va a realizar la tarea
    void *argumento; //argumento de la funcion
} tpool_tarea_t;


struct tpool_t {
  int t_count; //numero de hilos
  tpool_tarea_t *queue; //array de tareas
  int queue_tam; //numero de tareas maximo del pool
  int head; //indice del primer elemento
  int tail; //indice del siguiente elmento
  int q_count; //numero de tareas pendientes
  int started; //numero de hilos iniciados
  int apagado; //bandera para apagar el pool
  pthread_mutex_t bloqueo; //indica si el pool esta bloqueado
  pthread_cond_t notificar; //variable para notificar los hilos de trabajo
  pthread_t *threads; //array de ids de los hilos que estan trabajando


};

//hilo princiapl, asgina las tareas a los hilos
static void *tpool_main(void *threadpool);

//funcion para liberar recursos
int tpool_free(tpool_t *pool);

tpool_t *tpool_create(int t_count, int queue_tam) {
    tpool_t *pool;
    int i;

    if(t_count <= 0 || t_count > MAX_THREADS || queue_tam <= 0 || queue_tam > MAX_QUEUE) {
        return NULL;
    }

    //Reserva de memoria para la estructura threadpool
    if((pool = (tpool_t *)malloc(sizeof(tpool_t))) == NULL) {
      return NULL;
    }

    // Inicializacion
    pool->t_count = 0;
    pool->queue_tam = queue_tam;
    pool->head = pool->tail = pool->q_count = 0;
    pool->started = 0;
    pool->apagado = 0;

    // reserva de memoria para los hilos y la cola de tareas
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * t_count);
    pool->queue = (tpool_tarea_t *)malloc(sizeof(tpool_tarea_t) * queue_tam);

    if ((pool->threads == NULL) || (pool->queue == NULL)) {
         if(pool) {
             tpool_free(pool);
         }
         syslog(LOG_DEBUG,"Error en tpool_create, error alocando memoria para los hilos o la cola de tareas.");
         return NULL;
    }

    //Inicializacion del mutex y de la condicion
    if((pthread_mutex_init(&(pool->bloqueo), NULL) != 0) || (pthread_cond_init(&(pool->notificar), NULL) != 0)) {
         if(pool) {
             tpool_free(pool);
         }
         return NULL;
    }

    // Creamos los hilos
    for(i = 0; i < t_count; i++) {
        //Creacion de los hilos como funcion le pasamos una funcion generica y como argumento la estructura del pool
        if(pthread_create(&(pool->threads[i]), NULL, tpool_main, (void*)pool) != 0) {
            tpool_destroy(pool);
            return NULL;
        }
        pool->t_count++;
        pool->started++;
    }

    return pool;
}

int tpool_add(tpool_t *pool, void (*function)(void *), void *argumento)
{
    int next;

    if(pool == NULL || function == NULL) {
        syslog(LOG_DEBUG,"Error en tpool_add, puntero a pool o a funcion nulo.");
        return -1;
    }

    //bloqueo del semaforo del pool para evitar accesos concurrentes a la zona critica
    if(pthread_mutex_lock(&(pool->bloqueo)) != 0) {
      syslog(LOG_DEBUG,"Error en tpool_add,error al bloquear el pool");
      return -1;
    }

    next = (pool->tail + 1) % pool->queue_tam;

    if(pool->apagado){
      return -1;
    }

        //Comprobacion de que el numero de tareas pendientes no ha llegado al maximo del tamaño de la cola
        if(pool->q_count == pool->queue_tam) {
          syslog(LOG_DEBUG,"Error en tpool_add, cola llena");
          return -1;
        }

        //Añadimos una tarea a la cola
        pool->queue[pool->tail].function = function;
        pool->queue[pool->tail].argumento = argumento;
        pool->tail = next;
        pool->q_count += 1;

        //despertarmos un hilo
        if(pthread_cond_signal(&(pool->notificar)) != 0) {
          syslog(LOG_DEBUG,"Error en tpool_add, error al despertar");
          return -1;
        }

    //desbloqueamo el pool
    if(pthread_mutex_unlock(&pool->bloqueo) != 0) {
      syslog(LOG_DEBUG,"Error en tpool_add, error al desbloquear el pool");
      return -1;
    }

    return 0;
}

int tpool_destroy(tpool_t *pool){
    int i;

    if(pool == NULL) {
      syslog(LOG_DEBUG,"Error en tpool_destroy, puntero a pool o a funcinulo.");
      return -1;
    }

    //Comprobamos que no esta bloqueado
    //igual no es necesario, preguntar
    if(pthread_mutex_lock(&(pool->bloqueo)) != 0) {
      syslog(LOG_DEBUG,"Error en tpool_destroy, error al bloquear el pool.");
      return -1;
    }

    //si ya esta apagado salimos
    if(pool->apagado){
      return -1;
    }

    pool->apagado = 1;


    //despertamos los hilos
    if((pthread_cond_broadcast(&(pool->notificar)) != 0) || (pthread_mutex_unlock(&(pool->bloqueo)) != 0)) {
      syslog(LOG_DEBUG,"Error en tpool_destroy, puntero a pool o a funcion nulo.");
      tpool_free(pool);
      return -1;
    }

        // Hacemos el Join de los hilos
        for(i = 0; i < pool->t_count; i++) {
            if(pthread_join(pool->threads[i], NULL) != 0) {
              syslog(LOG_DEBUG,"Error en tpool_destroy, error en pthread_join.");
              return -1;
            }
          }

    tpool_free(pool);

    return 0;
}

int tpool_free(tpool_t *pool)
{
    if(pool == NULL || pool->started > 0) {
      syslog(LOG_DEBUG,"Error en tpool_free, puntero a pool nulo o ningun hilo lanzado");
        return -1;
    }


    if(pool->threads) {
        free(pool->threads);
        free(pool->queue);
        // bloqueamos el pool por si acaso en ese momento un hilo lo bloquease y nos impidiese destruirlo

        pthread_mutex_lock(&(pool->bloqueo));
        pthread_mutex_destroy(&(pool->bloqueo));
        pthread_cond_destroy(&(pool->notificar));
    }
    free(pool);
    return 0;
}


static void *tpool_main(void *threadpool)
{
    tpool_t *pool = (tpool_t *)threadpool;
    tpool_tarea_t tarea;

    while(1) {
        //bloqueamos el pool
        pthread_mutex_lock(&(pool->bloqueo));

        /* Wait on condition variable, check for spurious wakeups.
           When returning from pthread_cond_wait(), we own the lock. */
        //esperamos a que haya alguna tarea
        while(pool->q_count == 0 && (!pool->apagado)) {
            pthread_cond_wait(&(pool->notificar), &(pool->bloqueo));
        }

        if(pool->apagado && (pool->q_count == 0)){
          break;
        }


        //asignamos una tarea a un hilo
        tarea.function = pool->queue[pool->head].function;
        tarea.argumento = pool->queue[pool->head].argumento;
        pool->head = (pool->head + 1) % pool->queue_tam;
        pool->q_count -= 1;

        //desbloqueamos el pool
        pthread_mutex_unlock(&(pool->bloqueo));

        // llamamos a la funcion
        (*(tarea.function))(tarea.argumento);
    }

    pool->started--;

    pthread_mutex_unlock(&(pool->bloqueo));
    pthread_exit(NULL);
    return(NULL);
}
