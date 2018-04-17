#include <iostream>
#include <pthread.h>
#include "ConcurrentHashMap.hpp"
#include <list>
#include <string>

using namespace std;

int main(){
    list<string> archivos;
    archivos.push_back("corpus");
    // No hay otro archivo para probar jaja
    archivos.push_back("corpus");
    ConcurrentHashMap h(count_words(archivos));

    pair<string, unsigned int> maximo = h.maximum(4);
    cout << "Maximo: " << maximo.first << " " << maximo.second << endl;
    int palabras = h.cantWords;
    cout << "Cant. palabras: " << palabras << endl;

    return 0;
}
