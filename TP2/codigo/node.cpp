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
pthread_mutex_t mutex_nodo_nuevo;
atomic_bool fin_cadena;

//Cuando me llega una cadena adelantada, y tengo que pedir los nodos que me faltan
//Si nos separan más de VALIDATION_BLOCKS bloques de distancia entre las cadenas, se descarta por seguridad
bool verificar_y_migrar_cadena(const Block *rBlock, const MPI_Status *status){

  //TODO: Enviar mensaje TAG_CHAIN_HASH

  // ¿Por qué reservan nueva memoria, en lugar de hacer Block blockchain[VALIDATION_BLOCKS]?
  // ¿Capaz no entra en el stack?
  // ¿Tendremos que hacer lo mismo con la respuesta (Block *res = new Block[VALIDATION_BLOCKS])?
  // (En ese caso, cuidado de no liberar la memoria antes de que se haya copiado al buffer del otro)
  Block *blockchain = new Block[VALIDATION_BLOCKS];

  printf("[%u] Pido lista a %d a partir de %u \n", mpi_rank, status->MPI_SOURCE, rBlock->index);
  const char *buffer = rBlock->block_hash;
  MPI_Send(buffer, HASH_SIZE, MPI_CHAR, status->MPI_SOURCE, TAG_CHAIN_HASH, MPI_COMM_WORLD);

  //TODO: Recibir mensaje TAG_CHAIN_RESPONSE
  MPI_Status stat;
  MPI_Recv(blockchain, VALIDATION_BLOCKS, *MPI_BLOCK, status->MPI_SOURCE, TAG_CHAIN_RESPONSE, MPI_COMM_WORLD, &stat);
  // Veo cuántos elementos me enviaron
  int count;
  MPI_Get_count(&stat, *MPI_BLOCK, &count);
  printf("[%u] Recibí %d bloques de %d \n", mpi_rank, count, stat.MPI_SOURCE);
  // count siempre debería ser mayor o igual a 1.
  if (count <= 0){
  	delete []blockchain;
  	return false;
  }

  //TODO: Verificar que los bloques recibidos
  //sean válidos y se puedan acoplar a la cadena
  // Primer caso: el primer bloque corresponde al que recibí (rBlock)
  if (blockchain[0].index != rBlock->index || string(blockchain[0].block_hash) != string(rBlock->block_hash)){
  	printf("[%u] El primer bloque no coincide con el enviado por %d \n", mpi_rank, stat.MPI_SOURCE);
  	delete []blockchain;
  	return false;
  }
  // Segundo caso: el hash del bloque recibido es igual al calculado por la función block_to_hash
  string str_hash;
  block_to_hash(&blockchain[0], str_hash);
  if (string(blockchain[0].block_hash) != str_hash){
  	printf("[%u] No coincide el hash calculado con el escrito por %d \n", mpi_rank, stat.MPI_SOURCE);
  	delete []blockchain;
  	return false;  	
  }
  // Tercer caso: el previous_block_hash efectivamente contiene el hash del anterior
  for (int i = 0; i < count - 1; i++){
  	if (string(blockchain[i].previous_block_hash) != string(blockchain[i + 1].block_hash)){
		printf("[%u] Hash anteriores no coincidentes en cadena enviada por %d \n", mpi_rank, stat.MPI_SOURCE); 
		delete []blockchain;
  		return false;  
  	}
  	// Cuarto caso: índices consecutivos
  	if (blockchain[i].index != blockchain[i + 1].index + 1){
		printf("[%u] Índices no consecutivos en cadena enviada por %d \n", mpi_rank, stat.MPI_SOURCE); 
		delete []blockchain;
  		return false;  
  	}
  }

  // Si ninguno de los bloques estaba en mi cadena (o nos separan más de VALIDATION_BLOCKS) y el último no tiene índice 1, descarto la cadena
  int primero = -1;
  for (int i = 0; i < count; i++){
  	// Siempre comienzo desde el final de mi cadena
  	map<string, Block>::iterator it = node_blocks.find(string(last_block_in_chain->block_hash));
  	string anterior;
  	for (int j = 0; j < VALIDATION_BLOCKS; j++){
  		if (it->first == string(blockchain[i].block_hash)){
  			primero = i;
  			break;
  		}
  		anterior = string((it->second).previous_block_hash);
  		it = node_blocks.find(anterior);
  	}
  	// Si ya encontré uno, dejo de buscar
  	if (primero >= 0)
  		break;
  }
  if (primero < 0){
  	// No tengo a ninguno; me fijo si el índice del último es 1
  	if (blockchain[count - 1].index > 1){
 		printf("[%u] Cadena insegura enviada por %d \n", mpi_rank, stat.MPI_SOURCE); 
		delete []blockchain;
  		return false;   		
  	} else{
  		// El último tiene índice 1
  		primero = count;
  	}
  }
  // Le resto uno porque primero apunta al que ya tengo; si no tenía ninguno, vale count
  primero--;
  // Agrego todos los nodos que me faltan
  // El primero de la lista ya estaba definido (y capaz otro también); lo estoy definiendo dos veces!
  // No lo tomo como error porque asumo que se sobreescribe
  for (int i = primero; i >= 0; i--)
  	node_blocks[string(blockchain[i].block_hash)] = blockchain[i];

  *last_block_in_chain = blockchain[0];

  delete []blockchain;
  return true;
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
        printf("[%u] Agregado a la lista bloque con index %u enviado por %d \n", mpi_rank, rBlock->index, status->MPI_SOURCE);
        return true;
    }

    //TODO: Si el índice del bloque recibido es
    //el siguiente a mí último bloque actual,
    //y el bloque anterior apuntado por el recibido es mí último actual,
    //entonces lo agrego como nuevo último.
    if (rBlock->index == last_block_in_chain->index + 1){
    	if (string(rBlock->previous_block_hash) == string(last_block_in_chain->block_hash)){
    		memcpy(last_block_in_chain, rBlock, sizeof(Block));
    		printf("[%u] Agregado a la lista bloque con index %u enviado por %d \n", mpi_rank, rBlock->index, status->MPI_SOURCE);
        	return true;
    	}else{
    		//TODO: Si el índice del bloque recibido es
    		//el siguiente a mí último bloque actual,
    		//pero el bloque anterior apuntado por el recibido no es mí último actual,
    		//entonces hay una blockchain más larga que la mía.
    		printf("[%u] Perdí la carrera por uno (%u) contra %d \n", mpi_rank, rBlock->index, status->MPI_SOURCE);
    		bool res = verificar_y_migrar_cadena(rBlock, status);
    		return res;
    	}
    }

    //TODO: Si el índice del bloque recibido es igual al índice de mi último bloque actual,
    //entonces hay dos posibles forks de la blockchain pero mantengo la mía
    if (rBlock->index == last_block_in_chain->index){
    	printf("[%u] Conflicto suave: Conflicto de branch (%u) contra %d \n", mpi_rank, rBlock->index, status->MPI_SOURCE);
    	return false;
    }

    //TODO: Si el índice del bloque recibido es anterior al índice de mi último bloque actual,
    //entonces lo descarto porque asumo que mi cadena es la que está quedando preservada.
    if (rBlock->index < last_block_in_chain->index){
    	printf("[%u] Conflicto suave: Descarto el bloque (%u vs %u) contra %d \n", mpi_rank, rBlock->index, last_block_in_chain->index, status->MPI_SOURCE);
    	return false;
    }

    //TODO: Si el índice del bloque recibido está más de una posición adelantada a mi último bloque actual,
    //entonces me conviene abandonar mi blockchain actual
    if (rBlock->index > last_block_in_chain->index + 1){
    	printf("[%u] Perdí la carrera por varios contra %d \n", mpi_rank, status->MPI_SOURCE);
    	bool res = verificar_y_migrar_cadena(rBlock,status);
    	return res;
    }
  }

  printf("[%u] Error duro: Descarto el bloque recibido de %d porque no es válido \n", mpi_rank, status->MPI_SOURCE);
  return false;
}


//Envia el bloque minado a todos los nodos
void broadcast_block(const Block *block){
  for(int i = 0; i < total_nodes - 1; i++){ // No contiene al rank del nodo mismo
      if (nodos_mezclados[i] != mpi_rank){
      	// "It is unsafe to modify the application buffer (your variable space) until you know for a fact the requested non-blocking operation was actually performed by the library.
      	// El problema con esto es que el buffer es *block, que apunta a last_block_in_chain
      	// Después de haber ejecutado todos los MPI_Isend, puede ser que modifique last_block_in_chain!
      	//MPI_Wait(&envios[i], &stat);
        //MPI_Isend(&buf, 1, *MPI_BLOCK, nodos_mezclados[i], TAG_NEW_BLOCK, MPI_COMM_WORLD, &envios[i]);

        // Lo hago bloqueante hasta que se nos ocurra cómo hacer.
        MPI_Send(block, 1, *MPI_BLOCK, nodos_mezclados[i], TAG_NEW_BLOCK, MPI_COMM_WORLD);
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
   	  // Verifico si ya alcancé el máximo de bloques
      if (last_block_in_chain->index >= BLOCKS_TO_MINE){
      	printf("[%u] Ya alcancé el final de la lista \n", mpi_rank);
        fin_cadena.store(true);
        //char buf = '\0'; // Para poner algo
        //MPI_Send(&buf, 1, MPI_CHAR, mpi_rank, TAG_END_CHAIN, MPI_COMM_WORLD);
        pthread_exit(NULL);
      }

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
            printf("[%u] Agregué un producido con index %u \n", mpi_rank, last_block_in_chain->index);

            //TODO: Mientras comunico, no responder mensajes de nuevos nodos
            broadcast_block(last_block_in_chain);
          }
          pthread_mutex_unlock(&mutex_nodo_nuevo);
          // FIN SECCION CRITICA
      }

    }
    pthread_exit(NULL);
}


int node(){
  //Tomar valor de mpi_rank y de nodos totales
  MPI_Comm_size(MPI_COMM_WORLD, &total_nodes);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);

  nodos_mezclados = new int[total_nodes - 1];
  fin_cadena.store(false);
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

  while(true){
      int flag;
      MPI_Status stat;
      //TODO: Recibir mensajes de otros nodos
      // Idea: primero veo qué mensaje voy a recibir
      //MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &stat);
      do{
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &stat);
      }
      while(!flag && !fin_cadena.load());

      if(fin_cadena.load())
          break;

      if (stat.MPI_TAG == TAG_NEW_BLOCK){
        //TODO: Si es un mensaje de nuevo bloque, llamar a la función
        // validate_block_for_chain con el bloque recibido y el estado de MPI

      	// Por las dudas hago que el buffer sea del mismo tipo de dato
      	Block buffer;

        // Nota: count (segundo parámetro) es la cantidad de elementos de ese datatype
        MPI_Recv(&buffer, 1, *MPI_BLOCK, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &stat);
        pthread_mutex_lock(&mutex_nodo_nuevo);

        // Hay que validar el bloque
        Block * b = &buffer;
        if (validate_block_for_chain(b, &stat)){
            // Hay que agregar este bloque a la cadena
            // La función validate_block_for_chain ya hace los cambios necesarios.
            // Si devuelve false, no habría que hacer nada.
            // Si devuelve true, deberíamos ver si ya aceptamos una cadena de largo BLOCKS_TO_MINE y terminar la ejecución.
            // Esta verificación también debería estar cuando minamos, probablemente.
            if (last_block_in_chain->index >= BLOCKS_TO_MINE){
            	printf("[%u] Me llegó una lista completa \n", mpi_rank);
                fin_cadena.store(true);
            	pthread_mutex_unlock(&mutex_nodo_nuevo);
            	break;
            }
        }

        pthread_mutex_unlock(&mutex_nodo_nuevo);
      }
      else if (stat.MPI_TAG == TAG_CHAIN_HASH){
        //TODO: Si es un mensaje de pedido de cadena,
        //responderlo enviando los bloques correspondientes

      	char buffer[HASH_SIZE];
      	MPI_Recv(buffer, HASH_SIZE, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &stat);
      	printf("[%u] Recibí pedido de lista de %d \n", mpi_rank, stat.MPI_SOURCE);
        // Esto se puede hacer en C++11
        string hash_buscado(buffer);
        
        // La respuesta es un arreglo con la maxima cantidad de bloques que se pasan
        // De ser necesario, el que recibe puede verificar cuantos elementos se recibieron
        Block res[VALIDATION_BLOCKS];

        // El hash siempre va a estar porque es el nodo que broadcasteamos
        map<string, Block>::iterator actual = node_blocks.find(hash_buscado);
        if (actual == end(node_blocks))
        	printf("[%u] El bloque pedido por %d no lo tengo \n", mpi_rank, stat.MPI_SOURCE);
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

        // "It is unsafe to modify the application buffer (your variable space) until you know for a fact the requested non-blocking operation was actually performed by the library.
        // res puede ser modificada por otro proceso que me pida bloques
        // Además, si sale del scope (el else if), teóricamente se borra y puede tener cualquier basura
        // Lo hago bloqueante
        //MPI_Request req;
        //MPI_Isend(&res, count, *MPI_BLOCK, stat.MPI_SOURCE, TAG_CHAIN_RESPONSE, MPI_COMM_WORLD, &req);
        //MPI_Request_free(&req);
        printf("[%u] Envío %d bloques a %d \n", mpi_rank, count, stat.MPI_SOURCE);

        // Envío la lista al que me lo pidió
        MPI_Send(&res, count, *MPI_BLOCK, stat.MPI_SOURCE, TAG_CHAIN_RESPONSE, MPI_COMM_WORLD);
      }
  }
  pthread_join(thread_minero, NULL);
  pthread_mutex_destroy(&mutex_nodo_nuevo);
 
  printf("[%u] Terminando \n", mpi_rank);

  delete last_block_in_chain;
  delete[] nodos_mezclados;
  return 0;
}

