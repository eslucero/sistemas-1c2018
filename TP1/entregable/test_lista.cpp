#include<pthread.h>
#include<stdio.h>
#include "ListaAtomica.hpp"

#define  CANT_THREADS  10

using namespace std;

Lista<int> list;

void *listar(void *p_minumero)
{
    int minumero = *((int *) p_minumero);
    for (int i = minumero * 100000; i < (minumero + 1) * 100000; i++){
    	list.push_front(i);
    }
    return NULL;
}

int main(int argc, char **argv) 
{
    pthread_t thread[CANT_THREADS];
    int tids[CANT_THREADS], tid;

    for (tid = 0; tid < CANT_THREADS; ++tid) {
         tids[tid] = tid;
         pthread_create(&thread[tid], NULL, listar, &tids[tid]);
    }

    for (tid = 0; tid < CANT_THREADS; ++tid)
         pthread_join(thread[tid], NULL);

    // En un principio los listaba, pero como eran pocos elementos (10 por thread)
    // no había concurrencia. Es decir, cada thread terminaba antes de ser desalojado por otro
    int cant = 0;
    for (auto it = list.CrearIt(); it.HaySiguiente(); it.Avanzar()) {
    	cant++;
    }

    printf("%d \n", cant);

    return 0;
}