#include <string>
#include <list>
#include <vector>
#include <iterator>
#include <fstream>
#include <sstream>
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
		sem_t lock_max;
		atomic_bool lock;
		pthread_mutex_t lock_add;

		int escritores;
		pair<string, unsigned int> max;
	public:
		// La hago publica porque los tests acceden directamente a ella
		Lista<pair<string, unsigned int> >* tabla[26];
		// Para testear
		atomic_int cantWords;

	    ConcurrentHashMap();
            // Constructor por move, no por copia
            // El de copia no anda ya que los iteradores no permiten que otro sea const
            ConcurrentHashMap(ConcurrentHashMap&& otro);
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
	pthread_mutex_init(&lock_add, NULL);
	sem_init(&lock_max, 0, 1);
	max = make_pair("", 0);
	escritores = 0;
        cantWords = 0;
}

// Constructor por move
// otro queda inutilizado
// No es concurrente
ConcurrentHashMap::ConcurrentHashMap(ConcurrentHashMap&& otro){
	for (int i = 0; i < 26; i++){
		tabla[i] = otro.tabla[i];
                otro.tabla[i] = NULL;
		sem_init(&semaforo[i], 0, 1);
	}
        
	pthread_mutex_init(&lock_add, NULL);
	sem_init(&lock_max, 0, 1);
	max = otro.max;
	escritores = 0;
        cantWords.store(otro.cantWords.load());
}

ConcurrentHashMap::~ConcurrentHashMap(){
	for (int i = 0; i < 26; i++){
  		delete tabla[i];
  		sem_destroy(&semaforo[i]);
	}
	pthread_mutex_destroy(&lock_add);
	sem_destroy(&lock_max);
}

void ConcurrentHashMap::addAndInc(string key){
	pthread_mutex_lock(&lock_add);
	escritores++;
	// Si soy el primero, no dejo a nadie hacer maximum o espero a que termine
	if (escritores == 1)
		sem_wait(&lock_max);
	pthread_mutex_unlock(&lock_add);

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

	pthread_mutex_lock(&lock_add);
	escritores--;
	// Si ya todos los threads escribieron, libero a maximum
	if (escritores == 0)
		sem_post(&lock_max);
	pthread_mutex_unlock(&lock_add);
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
	// Me fijo que no pueda correr o no esté corriendo addAndInc
	// maximum puede correr en un solo thread a la vez (tiene sentido)
	sem_wait(&lock_max);

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

    sem_post(&lock_max);
    return max;
}

template<typename Out>
void split(const string &s, char delim, Out result) {
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        *(result++) = item;
    }
}

vector<string> split(const string &s, char delim) {
    vector<string> elems;
    split(s, delim, back_inserter(elems));
    return elems;
}

ConcurrentHashMap count_words(string arch){
	ConcurrentHashMap h;
	string linea;
	ifstream archivo(arch);
	if (archivo.is_open()){
		while(getline(archivo, linea)){
			vector<string> palabras = split(linea, ' ');
			// Me fijo que no esté vacío para asegurarme de que el iterador sea válido
			if (!palabras.empty()){
				for (vector<string>::iterator it = palabras.begin(); it != palabras.end(); it++)
					h.addAndInc(*it);
			}
		}
	}else {
		perror("Error al abrir el archivo: ");
	}

	return h;
}

// Parametros a pasarle a count_words_c
struct args_count_words{
    string path;
    ConcurrentHashMap* c;
};

void* count_words_c(void * args){
        args_count_words * acw = (args_count_words*)args;

        //cout<<"Archivo: "<<acw->path<<'\n'<<std::endl;
        ConcurrentHashMap * h = acw->c;

	string linea;
	ifstream archivo(acw->path);
	if (archivo.is_open()){
		while(getline(archivo, linea)){
			vector<string> palabras = split(linea, ' ');
			// Me fijo que no esté vacío para asegurarme de que el iterador sea válido
			if (!palabras.empty()){
				for (vector<string>::iterator it = palabras.begin(); it != palabras.end(); it++)
					h->addAndInc(*it);
			}
		}
	}else {
		perror("Error al abrir el archivo: ");
                //cout<<"Error en: "<<acw->path<<'\n'<<std::endl;
	}

        return 0;
}

ConcurrentHashMap count_words(list<string> archs){
    ConcurrentHashMap c;
    int nt = archs.size();
    pthread_t thread[nt];
    args_count_words args[nt];

    for(int i = 0; i < nt;i++){
        args[i].path = archs.front();
        archs.pop_front();
        args[i].c = &c;
    }

    for(int i = 0;i < nt;i++)
        pthread_create(&thread[i], NULL, count_words_c, (void*)&args[i]);

    for(int i = 0;i < nt;i++)
        pthread_join(thread[i], NULL);

    return c;
}
/*
ConcurrentHashMap count_words(unsigned int n, list<string> args){
  //TODO
}

pair<string, unsigned int> maximum(unsigned int p_archivos, unsigned int p_maximos, list<string> archs){
  //TODO
}
*/
