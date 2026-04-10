#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    unsigned char jmp[3];
    char oem[8];
    unsigned short bytes_per_sector;
    unsigned char sectors_per_cluster;
    unsigned short reserved_sectors;
    unsigned char num_fats;
    unsigned short root_dir_entries;
    unsigned short total_sectors;
    unsigned char media_descriptor;
    unsigned short fat_size_sectors;
    unsigned short sectors_per_track;
    unsigned short num_heads;
    unsigned int hidden_sectors;
    unsigned int partition_sectors;
    unsigned char physical_drive;
    unsigned char boot_signature;
    unsigned int volume_id;
    char volume_label[11];
    char fs_type[8];
} __attribute__((packed)) BootSector;

typedef struct {
    unsigned char filename[8];
    unsigned char extension[3];
    unsigned char attributes;
    unsigned char reserved[10];
    unsigned short creation_time;
    unsigned short creation_date;
    unsigned short first_cluster;
    unsigned int file_size;
} __attribute__((packed)) DirEntry;

typedef struct {
    long entry_position;
    DirEntry entry;
    uint16_t *clusters; // Cadena de clusters del archivo
    int cluster_count;
} RecoverableFile;

// Función para leer la tabla FAT
uint16_t* read_fat(FILE *disk, BootSector *bs, int *error) {
    uint16_t *fat = NULL;
    size_t fat_size = bs->fat_size_sectors * bs->bytes_per_sector;
    
    fat = (uint16_t*)malloc(fat_size);
    if (!fat) {
        *error = 1;
        return NULL;
    }

    fseek(disk, bs->reserved_sectors * bs->bytes_per_sector, SEEK_SET);
    if (fread(fat, 1, fat_size, disk) != fat_size) {
        free(fat);
        *error = 1;
        return NULL;
    }

    *error = 0;
    return fat;
}

// Función para seguir la cadena de clusters
int get_cluster_chain(uint16_t *fat, uint16_t start_cluster, uint16_t **chain, int *cluster_count) {
    uint16_t current_cluster = start_cluster;
    int capacity = 10;
    int count = 0;
    
    *chain = (uint16_t*)malloc(capacity * sizeof(uint16_t));
    if (!*chain) return 1;

    while (current_cluster < 0xFF8) { // Mientras no sea el último cluster
        if (current_cluster == 0xFF7) { // Cluster defectuoso
            free(*chain);
            return 2;
        }

        if (count >= capacity) {
            capacity *= 2;
            uint16_t *temp = realloc(*chain, capacity * sizeof(uint16_t));
            if (!temp) {
                free(*chain);
                return 1;
            }
            *chain = temp;
        }

        (*chain)[count++] = current_cluster;
        current_cluster = fat[current_cluster];
    }

    *cluster_count = count;
    return 0;
}

// Función para verificar clusters
int verify_clusters(FILE *disk, BootSector *bs, uint16_t *chain, int cluster_count) {
    unsigned char *buffer = (unsigned char*)malloc(bs->bytes_per_sector * bs->sectors_per_cluster);
    if (!buffer) return 1;

    int result = 0;
    for (int i = 0; i < cluster_count; i++) {
        // Calcular posición del cluster en el disco
        long position = ((bs->reserved_sectors + bs->num_fats * bs->fat_size_sectors +
                         (bs->root_dir_entries * 32 + bs->bytes_per_sector - 1) / bs->bytes_per_sector) +
                        (chain[i] - 2) * bs->sectors_per_cluster) * bs->bytes_per_sector;
        
        fseek(disk, position, SEEK_SET);
        if (fread(buffer, 1, bs->bytes_per_sector * bs->sectors_per_cluster, disk) != 
            bs->bytes_per_sector * bs->sectors_per_cluster) {
            result = 1;
            break;
        }
    }

    free(buffer);
    return result;
}

int main() {
    FILE *disk = fopen("test.img", "rb+");
    if (!disk) {
        printf("Error al abrir la imagen de disco.\n");
        return 1;
    }

    BootSector bs;
    fread(&bs, sizeof(BootSector), 1, disk);

    // Leer la tabla FAT
    int error;
    uint16_t *fat = read_fat(disk, &bs, &error);
    if (error) {
        printf("Error al leer la FAT.\n");
        fclose(disk);
        return 1;
    }

    // Leer el directorio raíz
    long root_dir_position = (bs.reserved_sectors + bs.num_fats * bs.fat_size_sectors) * bs.bytes_per_sector;
    fseek(disk, root_dir_position, SEEK_SET);

    RecoverableFile files[100];
    int file_count = 0;

    // Escanear entradas del directorio
    for (int i = 0; i < bs.root_dir_entries; i++) {
        DirEntry entry;
        long current_position = ftell(disk);
        fread(&entry, sizeof(DirEntry), 1, disk);

        // Buscar archivos borrados (0xE5) o entradas no usadas (0x00)
        if (entry.filename[0] == 0xE5 || entry.filename[0] == 0x00) {
            if (entry.filename[0] == 0xE5 && entry.first_cluster != 0) {
                // Verificar cadena de clusters
                uint16_t *cluster_chain = NULL;
                int cluster_count = 0;
                int res = get_cluster_chain(fat, entry.first_cluster, &cluster_chain, &cluster_count);

                if (res == 0 && cluster_count > 0) {
                    // Verificar si los clusters están intactos
                    if (verify_clusters(disk, &bs, cluster_chain, cluster_count) == 0) {
                        files[file_count].entry_position = current_position;
                        memcpy(&files[file_count].entry, &entry, sizeof(DirEntry));
                        files[file_count].clusters = cluster_chain;
                        files[file_count].cluster_count = cluster_count;
                        file_count++;

                        printf("%d. Archivo recuperable: [?%.8s.%.3s] Tamaño: %u bytes, Clusters: %d\n",
                               file_count, entry.filename+1, entry.extension, entry.file_size, cluster_count);
                    } else {
                        free(cluster_chain);
                    }
                }
            }
            continue;
        }
    }

    // Menú de recuperación
    if (file_count > 0) {
        printf("\nSe encontraron %d archivos recuperables.\n", file_count);
        printf("Ingrese el número del archivo a recuperar (0 para salir): ");
        
        int option;
        scanf("%d", &option);

        if (option > 0 && option <= file_count) {
            printf("Ingrese el primer carácter del nombre original: ");
            char first_char;
            scanf(" %c", &first_char);

            // Restaurar entrada en el directorio
            files[option-1].entry.filename[0] = first_char;
            fseek(disk, files[option-1].entry_position, SEEK_SET);
            fwrite(&files[option-1].entry, sizeof(DirEntry), 1, disk);
            fflush(disk);

            printf("Archivo recuperado: [%c%.7s.%.3s]\n", first_char, 
                   files[option-1].entry.filename+1, files[option-1].entry.extension);
        }
    } else {
        printf("\nNo se encontraron archivos recuperables.\n");
    }

    // Liberar memoria
    for (int i = 0; i < file_count; i++) {
        free(files[i].clusters);
    }
    free(fat);
    fclose(disk);
    return 0;
}