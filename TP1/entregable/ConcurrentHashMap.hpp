#include <string>
#include <list>

class ConcurretHashMap{
    ConcurrentHashMap();
    ~ConcurrentHashMap();

    void addAndInc(std::string key);
    bool member(std::string key);
    std::pair<std::string, unsigned int> maximum(unsigned int nt);
};

ConcurrentHashMap::ConcurrentHashMap(){
  //TODO
}

ConcurrentHashMap::~ConcurrentHashMap(){
  //TODO
}

void ConcurrentHashMap::addAndInc(std::string key){
  //TODO
}

bool ConcurrentHashMap::member(std::string key){
  //TODO
}

std::pair<std::string, unsigned int> ConcurrentHashMap::maximum(unsigned int nt){
  //TODO
}

ConcurrentHashMap count_words(std::string arch){
  //TODO
}

ConcurrentHashMap count_words(std::list<std::string> archs){
  //TODO
}

ConcurrentHashMap count_words(unsigned int n, std::list<std::string> args){
  //TODO
}

std::pair<std::string, unsigned int> maximum(unsigned int p_archivos, unsigned int p_maximos, std::list<std::string> archs){
  //TODO
}
