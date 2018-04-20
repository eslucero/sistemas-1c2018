#include <string>
#include <list>
#include <vector>
#include <iterator>
#include <fstream>
#include <sstream>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <semaphore.h>
#include <atomic>
#include "ListaAtomica.hpp"

#define BILLION 1E9

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
		Lista<pair<string, unsigned int> >* tabla[26];
		atomic_int cantWords;

	    ConcurrentHashMap();
		ConcurrentHashMap(ConcurrentHashMap&& otro);
	    ~ConcurrentHashMap();

	    void addAndInc(string key);
	    bool member(string key);
	    pair<string, unsigned int> maximum(unsigned int nt);
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

// Función ejecutada por cada thread de maximum
void *ConcurrentHashMap::buscar_maximo(unsigned int id, unsigned int nt){
	// Cada thread procesa distintas filas dependiendo de su id
	for (unsigned int i = id; i < 26; i += nt){
		pair<string, unsigned int> max_fila("", 0);
		for (auto it = tabla[i]->CrearIt(); it.HaySiguiente(); it.Avanzar()){
			auto t = it.Siguiente();
			if (t.second > max_fila.second)
				max_fila = t;
		}
		while(true){
			while(lock.load());
			if (!lock.exchange(true))
				break;
		}

		if (max_fila.second > max.second || (max_fila.second == max.second && max_fila.first < max.first))
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
	sem_wait(&lock_max);

	pthread_t thread[nt];
    unsigned int tid;
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

    pair<string, unsigned int> max_ = max;
    sem_post(&lock_max);
    return max_;
}

// Funcion utilizada para dividir un string
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

void cargar_archivo(string arch, ConcurrentHashMap* h){
	string linea;
	ifstream archivo(arch);
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
	}	
	archivo.close();
}

ConcurrentHashMap count_words(string arch){
	ConcurrentHashMap h;
	cargar_archivo(arch, &h);
	return h;
}

// Parametros a pasarle a count_words_3/4
struct args_count_words{
	string path;
	list<string>* archivos;
	pthread_mutex_t* mutex;
	ConcurrentHashMap* c;
};


void* count_words_3(void * args){
    args_count_words* acw = (args_count_words*)args;
    ConcurrentHashMap* h = acw->c;

    cargar_archivo(acw->path, h);
    return 0;
}

ConcurrentHashMap count_words(list<string> archs){
    ConcurrentHashMap h;
    int nt = archs.size();
    pthread_t thread[nt];
    args_count_words args[nt];

    for(int i = 0; i < nt; i++){
        args[i].path = archs.front();
        archs.pop_front();
        args[i].c = &h;
    }

    for(int i = 0; i < nt; i++)
        pthread_create(&thread[i], NULL, count_words_3, (void*)&args[i]);

    for(int i = 0; i < nt; i++)
        pthread_join(thread[i], NULL);

    return h;
}

void* count_words_4(void * args){
    args_count_words* acw = (args_count_words*)args;
    ConcurrentHashMap* h = acw->c;
    pthread_mutex_t* mutex = acw->mutex;
    list<string>* archs = acw->archivos;

    string fichero;
    while(true){
	    pthread_mutex_lock(mutex);
	    if (!archs->empty()){
	    	fichero = archs->front();
	    	archs->pop_front();
	    }else{
	    	fichero.clear();
	    }
	    pthread_mutex_unlock(mutex);
	    if (fichero.empty()) // Terminé
	    	return NULL;

	    cargar_archivo(fichero, h);
	}
}

ConcurrentHashMap count_words(unsigned int n, list<string> archs){
    ConcurrentHashMap h;
    pthread_t thread[n];
    args_count_words args[n];
    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);

    for(int i = 0; i < n; i++){
        args[i].archivos = &archs;
        args[i].c = &h;
        args[i].mutex = &lock;
    }

    for(int i = 0; i < n; i++)
        pthread_create(&thread[i], NULL, count_words_4, (void*)&args[i]);

    for(int i = 0; i < n; i++)
        pthread_join(thread[i], NULL);

    pthread_mutex_destroy(&lock);
    return h;	
}

pair<string, unsigned int> maximum(unsigned int p_archivos, unsigned int p_maximos, list<string> archs){
	struct timespec start, end;
	double accum;
	clock_gettime(CLOCK_REALTIME, &start);

	ConcurrentHashMap hashmaps[p_archivos];
	pthread_t thread[p_archivos];
	args_count_words args[p_archivos];
    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);

    for(int i = 0; i < p_archivos; i++){
        args[i].archivos = &archs;
        args[i].c = &hashmaps[i];
        args[i].mutex = &lock;
    }

    for(int i = 0; i < p_archivos; i++)
        pthread_create(&thread[i], NULL, count_words_4, (void*)&args[i]);

    for(int i = 0; i < p_archivos; i++)
        pthread_join(thread[i], NULL);

    ConcurrentHashMap h;
    for (int j = 0; j < p_archivos; j++){
    	for (int i = 0; i < 26; i++){
			for (auto it = hashmaps[j].tabla[i]->CrearIt(); it.HaySiguiente(); it.Avanzar()){
				auto t = it.Siguiente();
				for (int k = 0; k < t.second; k++)
					h.addAndInc(t.first);
			}
    	}
    }

    clock_gettime(CLOCK_REALTIME, &end);
    accum = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / BILLION;
    cout << accum << endl;

	return h.maximum(p_maximos);
}

pair<string, unsigned int> maximum_c(unsigned int p_archivos, unsigned int p_maximos, list<string> archs){
	struct timespec start, end;
	double accum;
	clock_gettime(CLOCK_REALTIME, &start);

	ConcurrentHashMap h(count_words(p_archivos, archs));

	clock_gettime(CLOCK_REALTIME, &end);
 	accum = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / BILLION;
 	cout << accum << endl;

	return h.maximum(p_maximos);
}