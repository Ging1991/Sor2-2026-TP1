#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned char first_byte;
    unsigned char start_chs[3];
    unsigned char partition_type;
    unsigned char end_chs[3];
    char starting_cluster[4];
    char file_size[4];
} __attribute((packed)) PartitionTable;

typedef struct {
    unsigned char jmp[3];
    char oem[8];
    unsigned short sector_size;
    unsigned char sector_cluster;
    unsigned short reserved_sectors;
    unsigned char number_of_fats;
    unsigned short root_dir_entries;
    unsigned short sector_volumen;
    unsigned char descriptor;
    unsigned short fat_size_sectors;
    unsigned short sector_track;
    unsigned short headers;
    unsigned int sector_hidden;
    unsigned int sector_partition;
    unsigned char physical_device;
    unsigned char current_header;
    unsigned char firm;
    unsigned int volume_id;
    char volume_label[11];
    char fs_type[8];
    char boot_code[448];
    unsigned short boot_sector_signature;
} __attribute((packed)) Fat12BootSector;

typedef struct {
	unsigned char filename[8];
    unsigned char extension[3];
    unsigned char attributes;
    unsigned char reserved;
    unsigned char creation_time_seconds;
    unsigned short creation_time;
    unsigned short creation_date;
    unsigned short accessed_date;
    unsigned short cluster_highbytes_address;
    unsigned short written_time;
    unsigned short written_date;
    unsigned short cluster_lowbytes_address;
    unsigned int size_of_file;

} __attribute((packed)) Fat12Entry;

void leer(unsigned short firstCluster, unsigned short fileFirstCluster, unsigned short clusterSize, int fileSize){
    FILE *in = fopen("test.img", "rb");
    int i;
    char leer[fileSize]; // array de caracteres para almacenar los datos que contiene el archivo

    fseek(in, firstCluster + ((fileFirstCluster - 2) * clusterSize), SEEK_SET); //posicionamiento del primer cluster 
                                                                                //del archivo

    fread(leer, fileSize, 1, in);//Lee el contenido del archivo y lo almacena en el array
    for (i = 0; i < fileSize; i++)//Recorre el array para imprimirlo por pantalla
    {
        printf("%c", leer[i]);
    }
    printf("\n");

    fclose(in);
}


void print_file_info(Fat12Entry *entry, unsigned short firstCluster, unsigned short clusterSize)
{
    switch(entry->filename[0]){
        case 0x00:
            return;

        case 0xE5:
            printf("Archivo borrado: [?%.8s.%.3s]\n", entry->filename+1, entry->extension);
            return;

        case 0x05:
            printf("Archivo que comienza con 0xE5: [%c%.7s.%.3s]\n", 0xE5, entry->filename+1, entry->extension);
            break;

        default:
        switch (entry->attributes)
        {
            case 0x10:
                printf("Directorio: [%.8s.%.3s]\n\n", entry->filename, entry->extension); 
                return;

            case 0x20:                                                        
                printf("Archivo: [%.8s.%.3s] su contenido es:\n", entry->filename, entry->extension); 
                leer(firstCluster, entry->cluster_lowbytes_address, clusterSize, entry->size_of_file);
                printf("\n");
                return;
        }
    }
}

int main() {
    FILE * in = fopen("test.img", "r+b");
    int i,position_root_directory,sizeOfCluster;
    PartitionTable pt[4];
    Fat12BootSector bs;
    Fat12Entry entry;
    unsigned short firstCluster;
   
    fseek(in, 0x1BE, SEEK_SET);
    fread(pt, sizeof(PartitionTable),4,in);

    for(i=0; i<4; i++) {   
        if(pt[i].partition_type == 1) {
            printf("Encontrada particion FAT12 %d\n", i);
            break;
        }
    }  
    if(i == 4) {
       printf("No encontrado filesystem FAT12, saliendo...\n");
        return -1;
    }

    fseek(in, 0, SEEK_SET);
    fread(&bs, sizeof(Fat12BootSector),1,in);
	//{...} Leo boot sector
    
    printf("En  0x%lX, sector size %d, FAT size %d sectors, %d FATs\n\n", 
           ftell(in), bs.sector_size, bs.fat_size_sectors, bs.number_of_fats);
           
    position_root_directory = (bs.reserved_sectors - 1 + bs.fat_size_sectors * bs.number_of_fats)
                            *bs.sector_size;//Calculo para la posicion inicial del root directory

    fseek(in, position_root_directory, SEEK_CUR); // Vamos al inicio del root diretory
    
    printf("Root dir_entries %d \n", bs.root_dir_entries);  
    
    firstCluster =ftell(in) + (bs.root_dir_entries*sizeof(entry)); //Obtenemos la posicion del primer
                                                                    //byte del primer cluster de datos

    sizeOfCluster = bs.sector_size * bs.sector_cluster;//Tamaño del cluster

    for(i=0; i<bs.root_dir_entries;i++)//Recorro las entrada e imprimo el contenido
    {
        //fread(&entry,sizeof(entry),1,in);
        //print_file_info(&entry,firstCluster,sizeOfCluster);
        long pos = ftell(in);
        fread(&entry, sizeof(entry), 1, in);

        // Buscar especificamente LAPAPA.TXT
        if (entry.filename[0] == 0xE5 &&
            memcmp(entry.filename + 1, "APAPA  ", 7) == 0 &&
            memcmp(entry.extension, "TXT", 3) == 0) {

            printf("Archivo borrado encontrado: [?%.7s.%.3s]\n",
                entry.filename + 1, entry.extension);

            entry.filename[0] = 'L';   // recuperamos LAPAPA.TXT

            fseek(in, pos, SEEK_SET);
            fwrite(&entry, sizeof(entry), 1, in);
            fflush(in);

            printf("Archivo recuperado como: [%.8s.%.3s]\n\n",
                entry.filename, entry.extension);

            break;
        }
        
        print_file_info(&entry, firstCluster, sizeOfCluster);
    }
    
    printf("\nLeido Root directory, ahora en 0x%lX\n", ftell(in));

    fclose(in);
    return 0;
}