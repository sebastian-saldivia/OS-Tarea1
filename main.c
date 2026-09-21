#define _DEFAULT_SOURCE // para usar strsep
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> 

#define MAX_LINE 256
#define MAX_ACTIVIDADES 10000 // criterio de exigencia segun rubrica

typedef struct {
    char id[32];
    char nombre[100];
    int tiempo_ms;
    char dependencias[20][32];
    int num_dependencias;
} Actividad;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Error de uso. Forma correcta: %s <archivo_plan.txt> <K>\n", argv[0]); 
        return EXIT_FAILURE;  // retorna codigo de error si ponen mas o menos argumentos
    }

    char *archivo_plan = argv[1]; //se guarda el nombre del archivo de entrada
    int limite_k = atoi(argv[2]); //se guarda el valor de K
    
    // para que no de error por no usar la variable k
    (void)limite_k; 

    srand(time(NULL)); 

    FILE *archivo = fopen(archivo_plan, "r"); // se abre el archivo de entrada en modo lectura el r quiere decir read
    if (archivo == NULL) {
        printf("Error: No se pudo abrir el archivo %s\n", archivo_plan);//mensaje de error por archivo no encontrado
        return EXIT_FAILURE;
    }

    Actividad actividades[MAX_ACTIVIDADES];
    int total_actividades = 0;// contador de actividades cargadas
    char linea[MAX_LINE];// buffer para leer cada linea del archivo

    printf("Leyendo el archivo y modelando el DAG...\n");
    printf("------------------------------------------------\n");

    while (fgets(linea, sizeof(linea), archivo) != NULL) { // se lee cada linea del archivo
        linea[strcspn(linea, "\n")] = 0; // se elimina el salto de linea al final de la linea leida
        if (strlen(linea) == 0) continue;// se ignoran lineas vacias 

        char *resto = linea;// se guarda la linea completa en la variable resto para poder usar strsep
        char *id_str     = strsep(&resto, ":");// se separa la linea por el primer ":" y se guarda en id_str
        char *nombre_str = strsep(&resto, ":");// se sigue separando la linea por el segundo ":" y se guarda en nombre_str
        char *tiempo_str = strsep(&resto, ":");//lo mismo para el tiempo
        char *deps_str   = resto; // lo que queda son las dependencias

        if (id_str == NULL || nombre_str == NULL) continue;// se ignoran lineas que no tengan id o nombre

        strcpy(actividades[total_actividades].id, id_str);// se copia el id a la estructura de actividades
        strcpy(actividades[total_actividades].nombre, nombre_str);// lo mismo con el nombre

        if (tiempo_str != NULL && strlen(tiempo_str) > 0 && atoi(tiempo_str) > 0) {// se valida que el tiempo sea un numero positivo
            actividades[total_actividades].tiempo_ms = atoi(tiempo_str);// se guarda el tiempo como se hizo con el id y nombre
        } else {
            actividades[total_actividades].tiempo_ms = (rand() % 4901) + 100; //tiempo aleatorio entre 100 y 5000 ms al no asignar uno
        }

        actividades[total_actividades].num_dependencias = 0;// se inicializa el contador de dependencias en 0
        if (deps_str != NULL) {
            char *dep_resto = deps_str;// se guarda el resto de la linea en dep_resto para poder usar strsep
            char *dep = strsep(&dep_resto, " ,"); // se separa el resto de la linea por espacios o comas y se guarda en dep
            while (dep != NULL) {// se sigue mientras hayan dependemcias
                if (strlen(dep) > 0) {
                    strcpy(actividades[total_actividades].dependencias[actividades[total_actividades].num_dependencias], dep);
                    actividades[total_actividades].num_dependencias++;// se copia la dependencia a la estructura actividades
                }
                dep = strsep(&dep_resto, " ,"); // se sigue separando el resto de la linea por espacios o comas
            }
        }

        printf("ID: %-2s | Tarea: %-18s | Tiempo: %4d ms | Dependencias: %d\n",//tabla de informacion de las actividades cargadas
               actividades[total_actividades].id,
               actividades[total_actividades].nombre,
               actividades[total_actividades].tiempo_ms,
               actividades[total_actividades].num_dependencias);

        total_actividades++; 
    }

    fclose(archivo);// se cierra el archivo de entrada
    printf("------------------------------------------------\n");
    printf("Total de actividades cargadas exitosamente: %d\n\n", total_actividades);

    return EXIT_SUCCESS;
}