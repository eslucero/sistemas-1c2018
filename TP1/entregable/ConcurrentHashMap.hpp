#include <string>
#include <list>
#include <pthread.h>
#include <stdio.h>
#include <atomic>
#include "ListaAtomica.hpp"

using namespace std;

class ConcurrentHashMap{
	private:
		int hash_key(string key);

		// Acá probablemente tengamos que definir un array[26] de semaforos (inicializados en 1)
		// No entiendo por qué especifican que addAndInc y maximum no pueden ser concurrentes,
		// si nunca hay necesidad de ejecutarlos simultáneamente.
	public:
		// La hago publica porque los tests acceden directamente a ella
		Lista<pair<string, unsigned int> >* tabla[26];

	    ConcurrentHashMap();
	    ~ConcurrentHashMap();

	    void addAndInc(string key);
	    bool member(string key);
	    pair<string, unsigned int> maximum(unsigned int nt);
};

int ConcurrentHashMap::hash_key(string key){
	int res = key.at(0) - 97;
	return res;
}

ConcurrentHashMap::ConcurrentHashMap(){
	for (int i = 0; i < 26; i++)
		tabla[i] = new Lista<pair<string, unsigned int> >();
}

ConcurrentHashMap::~ConcurrentHashMap(){
	for (int i = 0; i < 26; i++)
  		delete tabla[i];
}

void ConcurrentHashMap::addAndInc(string key){
  //TODO
}

bool ConcurrentHashMap::member(string key){
  //TODO
}

pair<string, unsigned int> ConcurrentHashMap::maximum(unsigned int nt){
  //TODO
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
