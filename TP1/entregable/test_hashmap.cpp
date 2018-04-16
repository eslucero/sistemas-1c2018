#include <iostream>
#include <pthread.h>
#include "ConcurrentHashMap.hpp"

#define  CANT_THREADS  10

using namespace std;

ConcurrentHashMap h;

void *listar(void *p_minumero)
{
    int minumero = *((int *) p_minumero);
    if (minumero < 5){
        for (int i = 0; i < 10000; i++){
            h.addAndInc("perro");
            h.addAndInc("gato");
            h.addAndInc("asteroide");
            h.addAndInc("perro");
            h.addAndInc("flor");
            h.addAndInc("parasito");
            h.addAndInc("astilla");
            h.addAndInc("florencia");
            h.addAndInc("flaco");
            h.addAndInc("gastronomico");
            h.addAndInc("gato");
        }
    }else{
        pair<string, unsigned int> maximo = h.maximum(4);
        cout << maximo.first << " " << maximo.second << endl;
    }
    return NULL;
}

int main(){
    pthread_t thread[CANT_THREADS];
    int tids[CANT_THREADS], tid;

    for (tid = 0; tid < CANT_THREADS; ++tid) {
         tids[tid] = tid;
         pthread_create(&thread[tid], NULL, listar, &tids[tid]);
    }

    for (tid = 0; tid < CANT_THREADS; ++tid)
         pthread_join(thread[tid], NULL);

	for (int i = 0; i < 26; i++) {
		for (auto it = h.tabla[i]->CrearIt(); it.HaySiguiente(); it.Avanzar()) {
			auto t = it.Siguiente();
			cout << t.first << " " << t.second << endl;
		}
	}

    pair<string, unsigned int> maximo = h.maximum(4);
    cout << "Maximo: " << maximo.first << " " << maximo.second << endl;
    
	return 0;
}