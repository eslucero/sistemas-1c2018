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
    archivos.push_back("corpus");
    archivos.push_back("corpus");
    archivos.push_back("corpus");
    archivos.push_back("corpus");
    archivos.push_back("corpus");
    archivos.push_back("corpus");
    ConcurrentHashMap h(count_words(archivos));
    ConcurrentHashMap h_2(count_words(3, archivos));

    pair<string, unsigned int> maximo = h.maximum(4);
    pair<string, unsigned int> maximo_2 = h_2.maximum(4);
    pair<string, unsigned int> maximo_3 = maximum(4, 6, archivos);
    cout << "Maximo: " << maximo.first << " " << maximo.second << endl;
    cout << "Maximo 2: " << maximo_2.first << " " << maximo_2.second << endl;
    cout << "Maximo 3: " << maximo_3.first << " " << maximo_3.second << endl;
    int palabras = h.cantWords;
    int palabras_2 = h_2.cantWords;
    cout << "Cant. palabras: " << palabras << endl;
    cout << "Cant. palabras 2: " << palabras_2 << endl;

    return 0;
}
