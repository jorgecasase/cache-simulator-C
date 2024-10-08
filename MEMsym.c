#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#define HEX_BITS 3
#define LINE_TAM 16
#define MAX_LINES 8
#define RAM_SIZE 4096
#define BIN_FILE "CONTENTS_RAM.bin"
#define CACHE_FILE "CONTENTS_CACHE.bin"
#define ACC_FILE "accesos_memoria.txt"

typedef struct {
    unsigned char ETQ;
    unsigned char Data[LINE_TAM];
} T_CACHE_LINE;

typedef struct lineas_t {
    int Numlineas;
    char **Lineas;
} lineas_t;

typedef struct{
    unsigned char texto[100];
    int n;
} texto;

void LimpiarCache(T_CACHE_LINE *cache_lines);
char *leelineaDinamicaFichero(FILE *fd);
lineas_t leer_lineas(FILE *fd);
void ParsearDireccion(unsigned int addr, int *ETQ, int*palabra, int *linea, int *bloque);
void TratarFallo(T_CACHE_LINE *tbl, char *MRAM, int ETQ, int linea, int bloque);
void VolcarCACHE(T_CACHE_LINE *tbl);

unsigned int MASKETQ = 0xf80;
unsigned int MASKPALABRA = 0x00f;
unsigned int MASKLINEA = 0x070;

int globaltime = 0;
int numfallos = 0;

int main(int argc, char **argv){

    // inicializa todas las líneas de caché y la variable para almacenar los datos leídos
    T_CACHE_LINE *cache_lines = malloc(sizeof(T_CACHE_LINE) * MAX_LINES);
    LimpiarCache(cache_lines);
    texto texto; texto.n = 0;

    // lee el fichero binario CONTENTS_RAM.bin en Simul_RAM
    unsigned char *Simul_RAM = malloc(sizeof(unsigned char) * RAM_SIZE);
    FILE *bin = fopen(BIN_FILE, "rb");
    if (bin == NULL){
        printf("Error: %s no se ha podido abrir", BIN_FILE);
        return -1;
    } 
    fread(Simul_RAM, 1, RAM_SIZE, bin);
    
    // lee fichero de texto accesos_memoria.txt
    FILE *acc = fopen(ACC_FILE, "rb");
    if (acc == NULL){
        printf("Error: %s no se ha podido abrir", ACC_FILE);
        return -1;
    }
    lineas_t acc_lines = leer_lineas(acc);


    // parsea direcciones, busca si están cargadas y si no lo están trata el fallo
    for(int i = 0; i < acc_lines.Numlineas; i++){
        globaltime++;
        unsigned int dirTemp = strtol(acc_lines.Lineas[i], NULL, 16);
        unsigned int etiqueta = 0, palabra = 0, linea = 0, bloque = 0;
        ParsearDireccion(dirTemp, &etiqueta, &palabra, &linea, &bloque);
    
        // buscamos la etiqueta de la linea en la linea correspondiente en caché por correspondencia directa
        if((int) cache_lines[linea].ETQ != etiqueta){
            printf("T: %d, Fallo de CACHE %d, ADDR %04X Label %X linea %02X palabra %02X bloque %02X\n", globaltime, ++numfallos, dirTemp, etiqueta, linea, palabra, bloque);
            globaltime += 20;
            TratarFallo(cache_lines, Simul_RAM, etiqueta, linea, bloque);    
        }

        printf("T: %d, Acierto de CACHE, ADDR %04X Label %X linea %02X palabra %02X DATO %02X\n", globaltime, dirTemp, etiqueta, linea, palabra, cache_lines[linea].Data[palabra]);
        texto.texto[texto.n++] = cache_lines[linea].Data[palabra];
        VolcarCACHE(cache_lines);
        sleep(1);
    }
    
    printf("Accesos totales: %d; fallos: %d; Tiempo medio: %.2f\n", acc_lines.Numlineas, numfallos, ((float)globaltime / (float)acc_lines.Numlineas));
    texto.texto[texto.n] = '\0';
    printf("Texto leido: %s\n", texto.texto);

    // volcar la caché en un fichero binario
    FILE *CONTENTS_CACHE = fopen(CACHE_FILE, "wb");
    for(int i = 0; i < 8; i++){
        for(int j = 0; j < 16; j++){
            fprintf(CONTENTS_CACHE, "%02X", cache_lines[i].Data[j]);
        }   
        fprintf(CONTENTS_CACHE, "\n");
    }
    
    
    fclose(bin);
    fclose(acc);
    fclose(CONTENTS_CACHE);
    return 0;
}

void VolcarCACHE(T_CACHE_LINE *tbl){
    for( int i = 0; i < 8; i++){
        printf("ETQ %02X: Data: ", tbl[i].ETQ);
        for(int j = 15; j >= 0; j--){
            printf("%02X ", tbl[i].Data[j]);
        }
        printf("\n");
    }    
}

void ParsearDireccion(unsigned int addr, int *ETQ, int*palabra, int *linea, int *bloque){
     
    *ETQ = (addr & MASKETQ) >> 7;
    *palabra = (addr & MASKPALABRA);
    *linea = (addr & MASKLINEA) >> 4;
    // se calcula bloque
    *bloque = (MAX_LINES * (*ETQ)) + (*linea);
}

void TratarFallo(T_CACHE_LINE *tbl, char *MRAM, int ETQ, int linea, int bloque){
    printf("Cargando el bloque %02X en la linea %02X\n", bloque, linea);
    int j = bloque * LINE_TAM;
    for(int i = 0; i < LINE_TAM; i++){
        tbl[linea].Data[i] = MRAM[j++];
    }
    tbl[linea].ETQ = ETQ;     
}


void LimpiarCache(T_CACHE_LINE *cache_lines) {
    for (int i = 0; i < MAX_LINES; i++){
        cache_lines[i].ETQ = 0xFF;
        for(int j = 0; j < LINE_TAM; j++){
            cache_lines[i].Data[j] = 0x23;
        }
    }
}

lineas_t leer_lineas(FILE *fd) {
    int num_lineas = 0;
    char **lineas = malloc(sizeof(char*));
    char *linea_actual = leelineaDinamicaFichero(fd);
    while (linea_actual != NULL) {
        num_lineas++;
        lineas = realloc(lineas, num_lineas * sizeof(char*));
        lineas[num_lineas - 1] = linea_actual;
        linea_actual = leelineaDinamicaFichero(fd);
    }
    lineas_t resultado = {num_lineas, lineas};
    return resultado;
}

char *leelineaDinamicaFichero(FILE *fd) {
    int tam_buffer = 16;
    char *buffer = malloc(tam_buffer);
    int pos_buffer = 0;
    char c;
    while ((c = fgetc(fd)) != EOF && c != '\n') {
        buffer[pos_buffer] = c;
        pos_buffer++;
        if (pos_buffer == tam_buffer) {
            tam_buffer *= 2;
            buffer = realloc(buffer, tam_buffer);
        }
    }
    if (pos_buffer == 0 && c == EOF) {
        free(buffer);
        return NULL;
    }
    buffer[pos_buffer] = '\0';
    buffer = realloc(buffer, pos_buffer + 1);
    return buffer;
}
