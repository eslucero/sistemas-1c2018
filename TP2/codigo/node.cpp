#include "node.h"
#include "picosha2.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <cstdlib>
#include <queue>
#include <atomic>
#include <mpi.h>
#include <map>
#include <algorithm>

using namespace std;

int total_nodes, mpi_rank;
Block *last_block_in_chain;
map<string, Block> node_blocks;
int* nodos_mezclados;
MPI_Request* envios;
pthread_mutex_t mutex_nodo_nuevo;

//Cuando me llega una cadena adelantada, y tengo que pedir los nodos que me faltan
//Si nos separan más de VALIDATION_BLOCKS bloques de distancia entre las cadenas, se descarta por seguridad
bool verificar_y_migrar_cadena(const Block *rBlock, const MPI_Status *status){

  //TODO: Enviar mensaje TAG_CHAIN_HASH

  Block *blockchain = new Block[VALIDATION_BLOCKS];

  //TODO: Recibir mensaje TAG_CHAIN_RESPONSE

  //TODO: Verificar que los bloques recibidos
  //sean válidos y se puedan acoplar a la cadena
    //delete []blockchain;
    //return true;

  delete []blockchain;
  return false;
}

//Verifica que el bloque tenga que ser incluido en la cadena, y lo agrega si corresponde
bool validate_block_for_chain(const Block *rBlock, const MPI_Status *status){
  if(valid_new_block(rBlock)){

    //Agrego el bloque al diccionario, aunque no
    //necesariamente eso lo agrega a la cadena
    node_blocks[string(rBlock->block_hash)] = *rBlock;

    //TODO: Si el índice del bloque recibido es 1
    //y mí último bloque actual tiene índice 0,
    //entonces lo agrego como nuevo último.
    if (rBlock->index == 1 && last_block_in_chain->index == 0){
        memcpy(last_block_in_chain, rBlock, sizeof(Block));
        printf("[%d] Agregado a la lista bloque con index %d enviado por %d \n", mpi_rank, rBlock->index, status->MPI_SOURCE);
        return true;
    }

    //TODO: Si el índice del bloque recibido es
    //el siguiente a mí último bloque actual,
    //y el bloque anterior apuntado por el recibido es mí último actual,
    //entonces lo agrego como nuevo último.
    if (rBlock->index == last_block_in_chain->index + 1){
    	if (string(rBlock->previous_block_hash) == string(last_block_in_chain->block_hash)){
    		memcpy(last_block_in_chain, rBlock, sizeof(Block));
    		printf("[%d] Agregado a la lista bloque con index %d enviado por %d \n", mpi_rank, rBlock->index, status->MPI_SOURCE);
        	return true;
    	}else{
    		//TODO: Si el índice del bloque recibido es
    		//el siguiente a mí último bloque actual,
    		//pero el bloque anterior apuntado por el recibido no es mí último actual,
    		//entonces hay una blockchain más larga que la mía.
    		printf("[%d] Perdí la carrera por uno (%d) contra %d \n", mpi_rank, rBlock->index, status->MPI_SOURCE);
    		bool res = verificar_y_migrar_cadena(rBlock, status);
    		return res;
    	}
    }

    //TODO: Si el índice del bloque recibido es igual al índice de mi último bloque actual,
    //entonces hay dos posibles forks de la blockchain pero mantengo la mía
    if (rBlock->index == last_block_in_chain->index){
    	printf("[%d] Conflicto suave: Conflicto de branch (%d) contra %d \n", mpi_rank, rBlock->index, status->MPI_SOURCE);
    	return false;
    }

    //TODO: Si el índice del bloque recibido es anterior al índice de mi último bloque actual,
    //entonces lo descarto porque asumo que mi cadena es la que está quedando preservada.
    if (rBlock->index < last_block_in_chain->index){
    	printf("[%d] Conflicto suave: Descarto el bloque (%d vs %d) contra %d \n", mpi_rank, rBlock->index, last_block_in_chain->index, status->MPI_SOURCE);
    	return false;
    }

    //TODO: Si el índice del bloque recibido está más de una posición adelantada a mi último bloque actual,
    //entonces me conviene abandonar mi blockchain actual
    if (rBlock->index > last_block_in_chain->index + 1){
    	printf("[%d] Perdí la carrera por varios contra %d \n", mpi_rank, status->MPI_SOURCE);
    	bool res = verificar_y_migrar_cadena(rBlock,status);
    	return res;
    }
  }

  printf("[%d] Error duro: Descarto el bloque recibido de %d porque no es válido \n", mpi_rank, status->MPI_SOURCE);
  return false;
}


//Envia el bloque minado a todos los nodos
void broadcast_block(const Block *block){
  const unsigned char* buf = (const unsigned char*) block;
  MPI_Status stat;

  for(int i = 0; i < total_nodes - 1; i++){ // No contiene al rank del nodo mismo
      if (nodos_mezclados[i] != mpi_rank){
      	// Espero a que haya finalizado el envío anterior.
      	// Si el request es NULL, devuelve error pero sigue de largo.
      	MPI_Wait(&envios[i], &stat);
        MPI_Isend(&buf, 1, *MPI_BLOCK, nodos_mezclados[i], TAG_NEW_BLOCK, MPI_COMM_WORLD, &envios[i]);
    }
  }
}

//Proof of work
//TODO: Advertencia: puede tener condiciones de carrera
void* proof_of_work(void *ptr){
    string hash_hex_str;
    Block block;
    unsigned int mined_blocks = 0;
    while(true){

      block = *last_block_in_chain;

      //Preparar nuevo bloque
      block.index += 1;
      block.node_owner_number = mpi_rank;
      block.difficulty = DEFAULT_DIFFICULTY;
      memcpy(block.previous_block_hash, block.block_hash, HASH_SIZE);

      //Agregar un nonce al azar al bloque para intentar resolver el problema
      gen_random_nonce(block.nonce);

      //Hashear el contenido (con el nuevo nonce)
      block_to_hash(&block, hash_hex_str);

      //Contar la cantidad de ceros iniciales (con el nuevo nonce)
      if(solves_problem(hash_hex_str)){

      	  // SECCION CRITICA
      	  pthread_mutex_lock(&mutex_nodo_nuevo);
      	  //Verifico que no haya cambiado mientras calculaba
          if (last_block_in_chain->index < block.index){
            mined_blocks += 1;
            *last_block_in_chain = block;
            strcpy(last_block_in_chain->block_hash, hash_hex_str.c_str());
            last_block_in_chain->created_at = static_cast<unsigned long int> (time(NULL));
            node_blocks[hash_hex_str] = *last_block_in_chain;
            printf("[%d] Agregué un producido con index %d \n", mpi_rank, last_block_in_chain->index);

            //TODO: Mientras comunico, no responder mensajes de nuevos nodos
            broadcast_block(last_block_in_chain);
          }
          pthread_mutex_unlock(&mutex_nodo_nuevo);
          // FIN SECCION CRITICA
      }

    }

    return NULL;
}


int node(){
  nodos_mezclados = new int[total_nodes - 1];// Movi esto aca porque tiraba error feo al correr, no se si lo sigue haciendo
  envios = new MPI_Request[total_nodes - 1];
  for (int i = 0; i < total_nodes - 1; i++) // Por las dudas
  	envios[i] = NULL;
  //Tomar valor de mpi_rank y de nodos totales
  MPI_Comm_size(MPI_COMM_WORLD, &total_nodes);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

  //La semilla de las funciones aleatorias depende del mpi_ranking
  srand(time(NULL) + mpi_rank);
  printf("[MPI] Lanzando proceso %u\n", mpi_rank);

  last_block_in_chain = new Block;

  //Inicializo el primer bloque
  last_block_in_chain->index = 0;
  last_block_in_chain->node_owner_number = mpi_rank;
  last_block_in_chain->difficulty = DEFAULT_DIFFICULTY;
  last_block_in_chain->created_at = static_cast<unsigned long int> (time(NULL));
  memset(last_block_in_chain->previous_block_hash, 0, HASH_SIZE);

  // Armo un arreglo con todos los ranks disponibles excepto el mio
  int k = 0;
  for(int i = 0; i < total_nodes; i++){
      if (i != mpi_rank){
        nodos_mezclados[k] = i;
        k++;
      }
  }
  // Los mezclamos para que cada nodo envie los mensajes en ordenes distintos
  // Se puede hacer enviando a la "derecha" de cada uno
  random_shuffle(&nodos_mezclados[0], &nodos_mezclados[total_nodes - 2]);

  pthread_t thread_minero;
  pthread_mutex_init(&mutex_nodo_nuevo, NULL);
  pthread_create(&thread_minero, NULL, proof_of_work, NULL);

  char buffer[sizeof(Block)];

  while(true){
      MPI_Status stat;
      //TODO: Recibir mensajes de otros nodos
      // Idea: me bloqueo esperando el mensaje de CUALQUIER nodo
      // Nota: count (segundo parámetro) es la cantidad de elementos de ese datatype
      // "Primitive data types are contiguous.
      // Derived data types allow you to specify non-contiguous data in a convenient manner and to treat it as though it was contiguous."
      MPI_Recv(&buffer, 1, *MPI_BLOCK, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &stat);
      if (stat.MPI_TAG == TAG_NEW_BLOCK){
        //TODO: Si es un mensaje de nuevo bloque, llamar a la función
        // validate_block_for_chain con el bloque recibido y el estado de MPI
        // Aca va una seccion critica
        pthread_mutex_lock(&mutex_nodo_nuevo);

        // Hay que validar el bloque
        Block * b = (Block*)buffer;
        if (validate_block_for_chain(b, &stat)){
            // Hay que agregar este bloque a la cadena
            // La función validate_block_for_chain ya hace los cambios necesarios.
            // Si devuelve false, no habría que hacer nada.
            // Si devuelve true, deberíamos ver si ya aceptamos una cadena de largo BLOCKS_TO_MINE y terminar la ejecución.
            // Esta verificación también debería estar cuando minamos, probablemente.

            //Block * prev = last_block_in_chain;
            //last_block_in_chain = new Block;
            //memcpy(last_block_in_chain, b, sizeof(Block));
            // Hay que setear previous_block_chain o conservamos el que se copia?
            // Conservamos el que se copia; esto también depende de si hay que migrar o no.
            //memcpy(last_block_in_chain->previous_block_hash, prev->block_hash, HASH_SIZE);
            //node_blocks[string(last_block_in_chain->block_hash)] = *last_block_in_chain;
        }

        pthread_mutex_unlock(&mutex_nodo_nuevo);
      }
      else if (stat.MPI_TAG == TAG_CHAIN_HASH){
        //TODO: Si es un mensaje de pedido de cadena,
        //responderlo enviando los bloques correspondientes

        // Esto se puede hacer en C++11
        string hash_buscado(buffer, HASH_SIZE);

        // La respuesta es un arreglo con la maxima cantidad de bloques que se pasan
        // De ser necesario, el que recibe puede verificar cuantos elementos se recibieron
        Block res[VALIDATION_BLOCKS];

        // El hash siempre va a estar porque es el nodo que broadcasteamos
        map<string, Block>::iterator actual = node_blocks.find(hash_buscado);
        int count = 0;

        // El bloque que me pidieron tiene que estar incluido en la lista; es el primero
        res[count] = actual->second;
        count++;
        // Cargo los VALIDATION_BLOCKS bloques o hasta llegar al principio de la lista
        while (count < VALIDATION_BLOCKS && (actual->second).index > 1){
          string anterior = (actual->second).previous_block_hash;
          actual = node_blocks.find(anterior);
          res[count] = actual->second;

          count++;
        }

        MPI_Request req;
        // Envío la lista al que me lo pidió
        MPI_Isend(&res, count, *MPI_BLOCK, stat.MPI_SOURCE, TAG_CHAIN_RESPONSE, MPI_COMM_WORLD, &req);
        // Request sirve para saber si el envío terminó (utilizando WAIT).
        // En un principio, no es necesario: el nodo que me pidió la lista no va a pedirmela devuelta antes de recibirla.
        MPI_Request_free(&req);
      }
  }

  // Capaz haya que poner un MPI_Waitall para esperar a que se envíen todos los mensajes

  pthread_join(thread_minero, NULL);
  pthread_mutex_destroy(&mutex_nodo_nuevo);
  
  delete last_block_in_chain;
  delete[] nodos_mezclados;
  delete[] envios;
  return 0;
}

