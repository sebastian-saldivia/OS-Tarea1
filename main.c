#define _DEFAULT_SOURCE // para usar strsep
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> 
// librerias para el manejo de procesos y la concurrencia
#include <unistd.h>     // Para fork(), usleep() y getpid()
#include <sys/types.h>  // Tipos de datos del sistema (pid_t)
#include <sys/wait.h>   // Para la funcion wait() que evita el busy-waiting

#define MAX_LINE 256
#define MAX_ACTIVIDADES 10000 // criterio de exigencia segun rubrica

typedef struct {
    char id[32];
    char nombre[100];
    int tiempo_ms;
    char dependencias[20][32];
    int num_dependencias;
    // variables para el manejo de procesos y la concurrencia
    pid_t pid;       // Guarda el ID del proceso hijo
    int estado;      // 0: pendiente, 1: corriendo, 2: terminada
} Actividad;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Error de uso. Forma correcta: %s <archivo_plan.txt> <K>\n", argv[0]); 
        return EXIT_FAILURE;  // retorna codigo de error si ponen mas o menos argumentos
    }

    char *archivo_plan = argv[1]; //se guarda el nombre del archivo de entrada
    int limite_k = atoi(argv[2]); //se guarda el valor de K

    srand(time(NULL)); // inicializamos la semilla para generar tiempos aleatorios

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
        
        // se inicializan las variables de control de procesos y concurrencia
        actividades[total_actividades].estado = 0; // las tareas nacen "pendiente" porque aun no se han lanzado
        actividades[total_actividades].pid = 0;    // Aún no tiene proceso asociado es decir no hay un hijo que la ejecute
        
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
        total_actividades++; 
    }

    fclose(archivo);// se cierra el archivo de entrada
    printf("Total de actividades cargadas exitosamente: %d\n\n", total_actividades);

    // motor de planificación de actividades concurrentes con límite K
    
    printf("PLANIFICADOR DIECIOCHERO CARGANDO (K = %d) ---\n\n", limite_k);
    
    int actividades_terminadas = 0;
    int procesos_corriendo = 0;

    // bucle que se ejecuta hasta que todas las actividades hayan terminado
    while (actividades_terminadas < total_actividades) {
        int se_lanzo_proceso = 0;

        // se ejecutan nuevas actividades siempre que no se supere el limite k de la concurrencia 
        while (procesos_corriendo < limite_k) {
            int indice_lista = -1;

            // revisando si hay actividades pendientes es decir en estado 0 para ejecutar
            for (int i = 0; i < total_actividades; i++) {
                if (actividades[i].estado == 0) {
                    int dependencias_ok = 1;
                    
                    //se revisa si todas las depencias ya terminaron es decir que esten en estado 2
                    for (int j = 0; j < actividades[i].num_dependencias; j++) {
                        for (int k = 0; k < total_actividades; k++) {
                            if (strcmp(actividades[i].dependencias[j], actividades[k].id) == 0) {
                                if (actividades[k].estado != 2) { 
                                    dependencias_ok = 0; // error por una dependencia que aun no termina
                                }
                                break;
                            }
                        }
                        if (dependencias_ok == 0) break;// si se encuentra una dependencia que no ha terminado, se rompe el bucle
                    }

                    if (dependencias_ok == 1) {
                        indice_lista = i;
                        break; // se encuentra una tarea lista para ejecutar por lo que se dej a de buscar
                    }
                }
            }

            if (indice_lista != -1) {
                // creacion de un proceso hijo para ejecutar la actividad seleccionada
                pid_t pid_hijo = fork();

                if (pid_hijo < 0) {// error al crear el proceso hijo ya que fork() devuelve un valor negativo si falla
                    printf("Error crítico: falló la clonación (fork).\n");
                    return EXIT_FAILURE;
                } else if (pid_hijo == 0) {// el proceso hijo entra en este bloque de código
                    printf("[+] Iniciando: %s (PID: %d, Duracion: %d ms)\n", actividades[indice_lista].nombre, getpid(), actividades[indice_lista].tiempo_ms);
                    
                    usleep(actividades[indice_lista].tiempo_ms * 1000);// el proceso hijo se duerme por el tiempo de la actividad en milisegundos (usleep() recibe microsegundos)
                    // el proceso hijo termina su ejecución y devuelve un codigo de salida exitoso
                    exit(EXIT_SUCCESS); 
                } else {// el proceso padre entra en este bloque de código
                    actividades[indice_lista].pid = pid_hijo; // se guarda el pid del proceso hijo en la estructura de actividades
                    actividades[indice_lista].estado = 1;     // se marca la actividad como corriendo
                    procesos_corriendo++;                     // aumenta el contador de procesos corriendo
                    se_lanzo_proceso = 1;
                }
            } else {
                // no hay más actividades pendientes que puedan ejecutarse en este momento
                break;
            }
        }

        // esperar sin Busy-Waiting para cumplir con la rubrica
        // si aun hay procesos corriendo el padre se va a dormir hasta que uno de los hijos termine su ejecución y le envie la señal SIGCHLD
        if (procesos_corriendo > 0) {// si hay procesos corriendo, el padre espera a que uno de ellos termine
            int estado_salida;
            pid_t pid_terminado = wait(&estado_salida); // El Padre se pausa aqui hasta recibir la señal de un hijo.

            if (pid_terminado > 0) {
                //buscamos la actividad correspondiente al pid del proceso hijo que terminó y actualizamos su estado
                for (int i = 0; i < total_actividades; i++) {
                    if (actividades[i].pid == pid_terminado) {
                        actividades[i].estado = 2; // Marcamos como terminada
                        procesos_corriendo--;
                        actividades_terminadas++;
                        printf("[-] Terminado: %s (PID: %d)\n", actividades[i].nombre, pid_terminado);
                        break;
                    }
                }
            }
        } else if (se_lanzo_proceso == 0 && actividades_terminadas < total_actividades) {// si no se lanzo ningun proceso y aun hay actividades pendientes, significa que hay un deadlock
            printf("Error: Deadlock detectado.\n");
            break;
        }
    }

    printf("\n--- SIMULACION EXITOSA: LA RAMADA ESTA LISTA ---\n");
    return EXIT_SUCCESS;
}

//Hay un error en el codigo que se debe revisar respecto servir la mesa que se ejecuto en el primer
//grupo junto con prender el carbon pero este de servir mesa depende de armas choripan se debe revisar