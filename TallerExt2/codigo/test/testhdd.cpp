#include <iostream>
#include <assert.h>

#include "hdd.h"
#include "mbr.h"
#include "pentry.h"
#include "ext2fs.h"


using namespace std;

void test_file_system(Ext2FS * fs){
	cout << "=== Filesystem Superblock ===" << endl;
	cout << *(fs->superblock()) << endl;

	//Verifico que la informacion de la cantidad de bloques sea la esperada
	assert(fs->superblock()->blocks_count == 102400);
}

void test_hdd(HDD * hdd){
	unsigned char buffer[SECTOR_SIZE];
	hdd->read(0, buffer);
	MBR mbr(buffer);
	const PartitionEntry & pentry = mbr[1];
	cout << "=== Partition Data ===" << endl;
	cout << pentry << endl << endl;

	//Verifico que la LBA empiece donde se espera
	assert(pentry.start_lba() == 4096);
}

void test_block_groups(Ext2FS * fs){
	cout << "=== Block Groups Data ===" << endl;
	unsigned int block_groups = fs->block_groups();
	for(unsigned int i = 0; i < block_groups; i++)
	{
		cout << *(fs->block_group(i)) << endl;
	}
	Ext2FSBlockGroupDescriptor block_group = *(fs->block_group(1));

	//Verifico que el block group 1 tenga la información correcta
	assert(block_group.block_bitmap == 8195);
}

int main(int argc, char ** argv)
{
	HDD hdd(argv[1]);

	//Esto lo pueden comentar. Es sólo para ver si descomprimieron bien la imagen de disco
	test_hdd(&hdd);

	Ext2FS * fs = new Ext2FS(hdd, 1);

	//Esto lo pueden comentar. Es sólo para ver si el FS está bien
	test_file_system(fs);

	//Esto lo pueden comentar. Es sólo para ver si el FS está bien
	test_block_groups(fs);

	//TODO: Completar el código del último ejercicio acá

        std::cout<<"Empezando nuestro test"<<std::endl;
        fd_t f = fs->open("/grupos/gNUMERO/nota.txt", "r");
       
        if (f == -1){
            std::cout<<"ERROR: No encontro el archivo"<<std::endl;
            return 1;
        }

        /*
        std::cout<<"Paso 1 listo"<<std::endl;
        fs->seek(f, 14000);
        
        std::cout<<"Paso 2 listo"<<std::endl;
        unsigned char buf[50];
        fs->read(f, buf, 50);

        std::cout<<"Paso 3 listo"<<std::endl;
        for(int i = 0; i < 50; i++)
            std::cout<<buf[i];
        std::cout<<std::endl;
        */
        fs->close(f);

        std::cout<<"Fin test"<<std::endl;
	return 0;
}
