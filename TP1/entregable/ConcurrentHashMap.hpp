#include <string>
#include <list>
#include <pthread.h>
#include <stdio.h>
#include <semaphore.h>
#include <atomic>
#include "ListaAtomica.hpp"

using namespace std;

struct args_struct{
	void *c;
	unsigned int t_id;
	unsigned int n;
};

class ConcurrentHashMap{
	private:
		int hash_key(string key);
		static void *wrapper(void *context);

		sem_t semaforo[26];
		atomic_bool lock;
		atomic_int cantWords;
		pair<string, unsigned int> max;
	public:
		// La hago publica porque los tests acceden directamente a ella
		Lista<pair<string, unsigned int> >* tabla[26];

	    ConcurrentHashMap();
	    ~ConcurrentHashMap();

	    void addAndInc(string key);
	    bool member(string key);
	    pair<string, unsigned int> maximum(unsigned int nt);
	    // No quedaba otra mas que hacerla publica para accederla desde el wrapper
	    void *buscar_maximo(unsigned int id, unsigned int nt);
};

int ConcurrentHashMap::hash_key(string key){
	int res = key.at(0) - 97;
	return res;
}

ConcurrentHashMap::ConcurrentHashMap(){
	for (int i = 0; i < 26; i++){
		tabla[i] = new Lista<pair<string, unsigned int> >();
		sem_init(&semaforo[i], 0, 1);
	}
	max = make_pair("", 0);
}

ConcurrentHashMap::~ConcurrentHashMap(){
	for (int i = 0; i < 26; i++){
  		delete tabla[i];
  		sem_destroy(&semaforo[i]);
	}
}

void ConcurrentHashMap::addAndInc(string key){
	// pthread_mutex_t mutex; mutex.wait() para que no sea concurrente con maximum
	// El problema con esto es que detiene a todos los threads que hagan addAndInc
	// Cuando el locking tiene que ser al nivel del array. 
	int k = hash_key(key);
	// Obtengo acceso exclusivo de la lista a modificar
	sem_wait(&semaforo[k]);
	
	bool no_esta = true;
	for (auto it = tabla[k]->CrearIt(); it.HaySiguiente(); it.Avanzar()){
		auto& t = it.Siguiente();
		if (t.first == key){
			t.second++;
			no_esta = false;
		}
	}
	if (no_esta){
		pair<string, unsigned int> palabra(key, 1);
		tabla[k]->push_front(palabra);
	}
	cantWords++; // Operación atómica

	sem_post(&semaforo[k]);
	//mutex.signal()
}

bool ConcurrentHashMap::member(string key){
	bool esta = false;
	int k = hash_key(key);
	for (auto it = tabla[k]->CrearIt(); it.HaySiguiente(); it.Avanzar()){
		auto t = it.Siguiente();
		if (t.first == key)
			esta = true;
	}

	return esta;
}

void *ConcurrentHashMap::buscar_maximo(unsigned int id, unsigned int nt){
	// Cada thread procesa distintas filas dependiendo de su id
	// Por ej., si hay 4 threads, el 1 va a procesar la 1era, 5ta, 10ma, etc.
	// El 2 va a procesar la 2da, 6ta, 11va, etc.
	// Etc.
	for (unsigned int i = id; i < 26; i += nt){
		pair<string, unsigned int> max_fila("", 0);
		for (auto it = tabla[i]->CrearIt(); it.HaySiguiente(); it.Avanzar()){
			auto t = it.Siguiente();
			if (t.second > max_fila.second)
				max_fila = t;
		}
		// Hago un TTAS Spinlock
		while(true){
			while(lock.load());
			if (!lock.exchange(true))
				break;
		}
		// Tengo acceso exclusivo
		if (max_fila.second > max.second)
			max = max_fila;
		lock.store(false);
	}

	return NULL;
}

void *ConcurrentHashMap::wrapper(void* context){
	struct args_struct *args = (struct args_struct*) context;
	ConcurrentHashMap* clase = (ConcurrentHashMap*) args->c;
	return clase->buscar_maximo(args->t_id, args->n);
}

pair<string, unsigned int> ConcurrentHashMap::maximum(unsigned int nt){
	// pthread_mutex_t mutex; mutex.wait() para la no-concurrencia con addAndInc
	// Pero no quiero detener a los threads que corran maximum!
	pthread_t thread[nt];
    unsigned int tid;
    //A cada thread le paso su tid y la cant. de threads, para saber qué filas procesar
    args_struct tids[nt];
    lock.store(false);

    for (tid = 0; tid < nt; ++tid) {
		tids[tid].c = this;
		tids[tid].t_id = tid;
		tids[tid].n = nt;
		pthread_create(&thread[tid], NULL, wrapper, &tids[tid]);
    }

    for (tid = 0; tid < nt; ++tid)
        pthread_join(thread[tid], NULL);

    // mutex.signal()
    return max;
}

ConcurrentHashMap count_words(string arch){
  //TODO
}

ConcurrentHashMap count_words(list<string> archs){
  //TODO
}

ConcurrentHashMap count_words(unsigned int n, list<string> args){
  //TODO
}

pair<string, unsigned int> maximum(unsigned int p_archivos, unsigned int p_maximos, list<string> archs){
  //TODO
}
