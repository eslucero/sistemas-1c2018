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
		// Lo usamos para llamar a funciones dentro de clases con pthread_create
		static void *wrapper(void *context);

		// Semáforo para cada una de las posiciones de la tabla,
		// así dos procesos no modifican la misma entrada de la tabla simultáneamente (en addAndInc)
		sem_t semaforo[26];
		// Semáforo utilizado para que maximum y addAndInc no corran concurrentemente
		sem_t lock_max;
		// Mutex (implementado a mano) utilizado en maximum, para sincronizar el uso del recurso compartido "max"
		atomic_bool lock;
		// Mutex que se encarga en addAndInc de sincronizar los distintos procesos para implementar el protocolo SRMW (Single Reader Multiple Writers)
		pthread_mutex_t lock_add;

		// Variable utilizada en addAndInc para contar la cantidad de procesos que la están ejecutando concurrentemente
		int escritores;
		// Variable que alberga la palabra con mayor cantidad de repeticiones luego de ejecutar maximum
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
	    // Cada thread de maximum va a ejecutar una instancia de ella
	    void *buscar_maximo(unsigned int id, unsigned int nt);
};

// Mapeo el string a una clave de hash
int ConcurrentHashMap::hash_key(string key){
	int res = key.at(0) - 97;
	return res;
}

// Todos los semaforos arrancan en 1, el mutex también (por defecto)
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
	// Necesito acceso exclusivo a la variable escritores, la cual cuenta cuántos procesos están ejecutando addAndInc
	// Es necesario para implementar Single Reader Multiple Writers (al revés que la teórica, donde es Multiple Readers Single Writer)
	// Esto es así porque queremos que más de un proceso pueda acceder a la tabla; el locking únicamente ocurre a nivel del array
	// Es decir, queremos que dos threads puedan agregar "perro" y "gato" concurrentemente
	// Pero no queremos agregar elementos si se está ejecutando maximum (por pedido del enunciado)
	pthread_mutex_lock(&lock_add);
	escritores++;
	// Si soy el primero, no dejo a nadie hacer maximum o espero a que termine
	if (escritores == 1)
		sem_wait(&lock_max);
	// Notar que el lock se mantiene mientras hago el wait, entonces ningún otro thread ejecutando addAndInc va a entrar
	pthread_mutex_unlock(&lock_add);

	// Si llegué acá, significa que ya no se está ejecutando maximum y puse su semáforo en 0
	// Entonces dicha función no se va a poder ejecutar hasta que todos terminemos
	int k = hash_key(key);
	// Obtengo acceso exclusivo de la lista a modificar
	sem_wait(&semaforo[k]);
	
	// Busco si ya está la clave, en cuyo caso aumento la cant. de repeticiones de la misma
	bool no_esta = true;
	for (auto it = tabla[k]->CrearIt(); it.HaySiguiente(); it.Avanzar()){
		auto& t = it.Siguiente();
		if (t.first == key){
			t.second++;
			no_esta = false;
		}
	}
	// Si no está, creo un par nuevo y la agrego a la lista correspondiente.
	if (no_esta){
		pair<string, unsigned int> palabra(key, 1);
		tabla[k]->push_front(palabra);
	}
	// Incremento la cantidad total de palabras (solo sirve para testear, no lo piden)
	cantWords++; // Operación atómica

	// Terminé de modificar la lista, dejo que otro siga
	sem_post(&semaforo[k]);

	// Necesito acceso exclusivo a la variable escritores, para decrementarla y ver si yo soy el último en ejecutar addAndInc
	pthread_mutex_lock(&lock_add);
	escritores--;
	// Si ya todos los threads escribieron, libero a maximum
	if (escritores == 0)
		sem_post(&lock_max);
	pthread_mutex_unlock(&lock_add);
}

// Nada nuevo. Permite concurrencia con cualquier función de la clase.
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

// Función ejecutada por cada thread de maximum. Se encarga de procesar filas buscando el máximo de cada una
// Qué filas va a procesar cada thread está determinado de antemano por su id y la cantidad total de threads
// Podríamos haber puesto una cola con 26 elementos, y a medida que uno termina, va desencolando
// Pero esto puede ser más lento, porque todos los threads tienen que acceder a un recurso en común (menos concurrencia)
// Más rápido simplemente incrementar una variable y distribuir las filas a procesar uniformemente
void *ConcurrentHashMap::buscar_maximo(unsigned int id, unsigned int nt){
	// Cada thread procesa distintas filas dependiendo de su id
	// Por ej., si hay 4 threads, el 1 va a procesar la 1era, 5ta, 9na, 13va, 17va, 21va, 25va.
	// El 2 va a procesar la 2da, 6ta, 10va, 14va, 18va, 22va, 26va.
	// El 3 va a procesar la 3ra, 7ma, 11va, 15va, 19va, 23va.
	// El 4 va a procesar la 4ta, 8va, 12va, 16va, 20va, 24va.
	for (unsigned int i = id; i < 26; i += nt){
		pair<string, unsigned int> max_fila("", 0);
		for (auto it = tabla[i]->CrearIt(); it.HaySiguiente(); it.Avanzar()){
			auto t = it.Siguiente();
			if (t.second > max_fila.second)
				max_fila = t;
		}
		// Recurso compartido: variable global con el par máximo de toda la tabla
		// Hago un TTAS Spinlock
		// Como es una comparación y asignación, mejor hacer busy waiting antes que llamar a un semáforo
		while(true){
			while(lock.load());
			if (!lock.exchange(true))
				break;
		}
		// Tengo acceso exclusivo
		if (max_fila.second > max.second || (max_fila.second == max.second && max_fila.first < max.first))
			max = max_fila;
		// unlock
		lock.store(false);
	}

	return NULL;
}

// Simplemente llama a la función buscar_máximo con la instancia de clase pasada por parámetro
// Además, se le pasa el id del thread y la cantidad total de threads.
void *ConcurrentHashMap::wrapper(void* context){
	struct args_struct *args = (struct args_struct*) context;
	ConcurrentHashMap* clase = (ConcurrentHashMap*) args->c;
	return clase->buscar_maximo(args->t_id, args->n);
}

pair<string, unsigned int> ConcurrentHashMap::maximum(unsigned int nt){
	// Me fijo que no pueda correr o no esté corriendo addAndInc
	// Utilizado para implementar SRMW
	// maximum puede correr en un solo thread a la vez
	// Lo cual no es un problema porque de por sí utiliza varios threads, no hay razón por la cual más de un thread llame a maximum
	// Y si lo hace, que espere
	sem_wait(&lock_max);

	pthread_t thread[nt];
    unsigned int tid;
    //A cada thread le paso su tid y la cant. de threads, para saber qué filas procesar
    args_struct tids[nt];
    // Inicializo el "mutex"
    lock.store(false);

    // Recordar que es necesario pasarle la instancia de la clase por parámetro porque pthread_create no admite clases por ser una librería de C
    for (tid = 0; tid < nt; ++tid) {
		tids[tid].c = this;
		tids[tid].t_id = tid;
		tids[tid].n = nt;
		pthread_create(&thread[tid], NULL, wrapper, &tids[tid]);
    }

    // Espero a que terminen todos los threads
    for (tid = 0; tid < nt; ++tid)
        pthread_join(thread[tid], NULL);

    pair<string, unsigned int> max_ = max;
    // Terminé de ejecutar maximum. Si addAndInc estaba esperando (u otro thread queriendo ejecutar maximum), se despierta.
    sem_post(&lock_max);
    return max_;
}

// Funciones choreadas de Stackoverflow para dividir un string según un delimitador (en nuestro caso, el espacio en blanco)
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

// Toma una instancia de ConcurrentHashMap y le carga cada palabra del archivo pasado por parámetro.
// Generalmente es ejecutada por varios threads a la vez (sobre archivos distintos, pero una misma instancia de clase).
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

// count_words no concurrente.
// Simplemente abre el archivo y lo procesa línea por línea, agregando cada palabra al ConcurrentHashMap.
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

// Método que ejecuta cada thread del punto 3. Recibe por parámetro el archivo a procesar y la instancia de la clase.
// Hay 1 thread por archivo.
void* count_words_3(void * args){
    args_count_words* acw = (args_count_words*)args;
    ConcurrentHashMap* h = acw->c;

    cargar_archivo(acw->path, h);
    return 0;
}

// count_words del punto 3. Voy sacando cada elemento de la lista y se la paso a algún thread.
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

// Método que ejecuta cada thread del count_words del punto 4.
// Pueden haber menos threads que archivos, entonces todos los threads tienen un recurso compartido: la lista de archivos
// Dicha lista se utiliza como cola, y a medida que cada thread termina de procesar un archivo, saca un elemento de la cola.
// Utilizo un mutex para obtener acceso exclusivo a la cola.
// Si ya no hay más elementos, terminé y muere el thread.
// Por parámetro, recibe: instancia de la clase, puntero al mutex en común, puntero a la lista en común.
void* count_words_4(void * args){
    args_count_words* acw = (args_count_words*)args;
    ConcurrentHashMap* h = acw->c;
    pthread_mutex_t* mutex = acw->mutex;
    list<string>* archs = acw->archivos;

    string fichero;
    while(true){
	    // Obtengo acceso exclusivo a lista de archivos
	    pthread_mutex_lock(mutex);
	    // Si no está vacía, aún hay elementos por procesar
	    if (!archs->empty()){
	    	// Desencolo
	    	fichero = archs->front();
	    	archs->pop_front();
	    }else{
	    	// Si está vacía, limpio la variable fichero como indicador de que no hay más elementos
	    	fichero.clear();
	    }
	    pthread_mutex_unlock(mutex);
	    if (fichero.empty()) // Terminé
	    	return NULL;

	    cargar_archivo(fichero, h);
	}
}

// count_words del punto 4. Inicializo un mutex en común para pasarselo a todos los threads.
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
	// Versión no concurrente:
	// Creo p_archivos instancias de hashmaps, cada thread va a recibir una instancia diferente.
	// El resto del código es exactamente igual al count_words de arriba.
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

    // Auno todas las instancias en uno solo
    // Para los amantes de los bucles añidados
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
    cout << accum;

	return h.maximum(p_maximos);
}

pair<string, unsigned int> maximum_c(unsigned int p_archivos, unsigned int p_maximos, list<string> archs){
	struct timespec start, end;
	double accum;
	clock_gettime(CLOCK_REALTIME, &start);
	ConcurrentHashMap h(count_words(p_archivos, archs));
	clock_gettime(CLOCK_REALTIME, &end);
 	accum = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / BILLION;
 	cout << accum;

	return h.maximum(p_maximos);
}