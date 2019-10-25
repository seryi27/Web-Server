#include <stdio.h>
#include <stdlib.h>
#include "thread-pool.h"

void thread_function(void *arg){
  int value = *(int *)arg;

  printf("Valor: %d\n", value);
}

int main(int argc, char *argv[]){
  tpool_t *pool;
  int n_threads=1;
  int queue_size=50;

  pool=tpool_create(n_threads, queue_size);

  for(int i = 0; i < 50; i++){
    int *var=(int *)malloc(sizeof(int));
    *var=i;
    wait(1);
    if(tpool_add(pool, *thread_function, (void *)var) != 0){
      printf("%s\n", "tpool_add error");
      tpool_destroy(pool);
      return EXIT_FAILURE;
    }
  }
  tpool_destroy(pool);
  printf("FIN\n");
  return EXIT_SUCCESS;
}
