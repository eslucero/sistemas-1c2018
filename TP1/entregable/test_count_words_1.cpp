#include <iostream>
#include <pthread.h>
#include "ConcurrentHashMap.hpp"

using namespace std;

int main(){
    ConcurrentHashMap* h = count_words("corpus");

    pair<string, unsigned int> maximo = h->maximum(4);
    cout << "Maximo: " << maximo.first << " " << maximo.second << endl;
    int palabras = h->cantWords;
    cout << "Cant. palabras: " << palabras << endl;

    delete h;
    
	return 0;
}