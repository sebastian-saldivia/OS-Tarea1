#define _DEFAULT_SOURCE // para usar strsep
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> 
// librerias para el manejo de procesos y la concurrencia
#include <unistd.h>     // Para fork(), usleep() y getpid()
#include <sys/types.h>  // Tipos de datos del sistema (pid_t)
#include <sys/wait.h>   // Para la funcion wait() que evita el busy-waiting
#include <signal.h>    // para que la SIGCHLD no mate el programa principal de golpe

#define MAX_LINE 256
#define MAX_ACTIVIDADES 120000 // para superar la exigencia de la rubrica de 100000 actividades

typedef struct {// estructura que modela cada actividad del plan de trabajo
    char id[32];
    char nombre[100];
    int tiempo_ms;
    char dependencias[20][32];
    int num_dependencias;
    // variables para el manejo de procesos y la concurrencia
    pid_t pid;       // Guarda el ID del proceso hijo
    int estado;      // 0: pendiente, 1: corriendo, 2: terminada
    //variable para manejar los pipes
    char mensajes_recibidos[2024]; // buffer para almacenar mensajes recibidos por el pipe
} Actividad;

static Actividad actividades[MAX_ACTIVIDADES];
int total_actividades = 0;
// funcion que se ejecuta cuando se recibe la señal SIGINT (Ctrl+C) para simular la llegada del seremi de salud
void llegada_seremi(int sig) {
    (void)sig; // evitamos el warning de variable sin uso
    printf("\n\n[!!!] LLEGO EL SEREMI DE SALUD (Ctrl+C interceptado) [!!!]\n");
    printf("Clausurando la ramada y cancelando todas las tareas en curso...\n");

    for (int i = 0; i < total_actividades; i++) {
        if (actividades[i].estado == 1 && actividades[i].pid > 0) { 
            printf(" - asesinando proceso hijo: %s (PID: %d)\n", actividades[i].nombre, actividades[i].pid);
            kill(actividades[i].pid, SIGKILL); // mata al proceso hijo sin piedad
        }
    }
    printf("todos los procesos clausurados\n");
    exit(EXIT_FAILURE); // cerramos el programa padre
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Error de uso. Forma correcta: %s <archivo_plan.txt> <K>\n", argv[0]); 
        return EXIT_FAILURE;  // retorna codigo de error si ponen mas o menos argumentos
    }

    // registramos la funcion llegada_seremi para que se ejecute cuando se reciba la señal SIGINT (Ctrl+C)
    signal(SIGINT, llegada_seremi);// aca se intercepta la señal

    char *archivo_plan = argv[1]; //se guarda el nombre del archivo de entrada
    int limite_k = atoi(argv[2]); //se guarda el valor de K

    srand(time(NULL)); // inicializamos la semilla para generar tiempos aleatorios


    FILE *archivo = fopen(archivo_plan, "r"); // se abre el archivo de entrada en modo lectura el r quiere decir read
    if (archivo == NULL) {
        printf("Error: No se pudo abrir el archivo %s\n", archivo_plan);//mensaje de error por archivo no encontrado
        return EXIT_FAILURE;
    }

    static Actividad actividades[MAX_ACTIVIDADES];
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
        strcpy(actividades[total_actividades].mensajes_recibidos, ""); // se inicia el buffer que funciona como un buzon de mensajes para cada actividad
        
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
                        int dependencia_cumplida = 0; // asumimos que la dependencia no se ha cumplido todavia
                        
                        for (int k = 0; k < total_actividades; k++) {
                            // limpiamos saltos de linea invisibles por si acaso
                            actividades[i].dependencias[j][strcspn(actividades[i].dependencias[j], "\r\n")] = 0;
                            
                            if (strcmp(actividades[i].dependencias[j], actividades[k].id) == 0) {
                                if (actividades[k].estado == 2) { 
                                    dependencia_cumplida = 1; // encontramos la dependencia y ya esta terminada
                                }
                                break;
                            }
                        }
                        if (dependencia_cumplida == 0) {
                            dependencias_ok = 0; // error por una dependencia que no existe o aun no termina
                            break;// si se encuentra una dependencia que no ha terminado, se rompe el bucle
                        }
                    }

                    if (dependencias_ok == 1) {
                        indice_lista = i;
                        break; // se encuentra una tarea lista para ejecutar por lo que se dej a de buscar
                    }
                }
            }

            if (indice_lista != -1) {
                // se crea una tuberia (pipe) antes del fork
                int fd[2]; // 2 espacios uno para fd[0] que es para lectura y el otro fd[1] que es para escritura
                if (pipe(fd) == -1) {
                    printf("no se puede crear el pipe\n");
                    return EXIT_FAILURE;
                }

                // creacion de un proceso hijo para ejecutar la actividad seleccionada
                pid_t pid_hijo = fork();

                if (pid_hijo < 0) {// error al crear el proceso hijo porque el fork al dar un numero negativo significa que hubo un error
                    printf("no se pudo crear el proceso hijo\n");
                    return EXIT_FAILURE;
                } else if (pid_hijo == 0) {// el proceso hijo entra en esta parte del codigo
                    // el hijo no escribe en este tubo solo va a leer por lo que el apartado de escritura se cierra
                    signal(SIGINT, SIG_IGN);// el hijo ignora la señal SiGINT para que no se clone el radar de señales
                    close(fd[1]); 
                    
                    // se lee el mensaje que nos dejo el padre en la tuberia
                    char buffer_tuberia[1024] = "";//limpiamos el buffer para evitar errores de basura en memoria
                    read(fd[0], buffer_tuberia, sizeof(buffer_tuberia)); // leemos el mensaje que nos dejo el padre en la tuberia
                    close(fd[0]); // cerramos la lectura al terminar de usarla
                    
                    printf("[+] se esta iniciando: %s (PID: %d, duracion o tiempo: %d ms)\n", actividades[indice_lista].nombre, getpid(), actividades[indice_lista].tiempo_ms);
                    
                    // si se recibieron mensajes de las dependencias se imprime para comprobar quie este funcionando el "buzon" de mensajes
                    if (strlen(buffer_tuberia) > 0) {
                        printf("mensaje recibido por el pipe: %s\n", buffer_tuberia);
                    }
                    
                    usleep(actividades[indice_lista].tiempo_ms * 1000);// el proceso hijo se duerme
                    
                    // simulacion de falla del 20% para cumplir con el aislamiento de errores que pedia la rubrica
                    int probabilidad_fallo = rand() % 100;
                    if (probabilidad_fallo < 20) {
                        exit(EXIT_FAILURE); // la tarea fallo por lo que se devuelve codigo 1 al padre
                    } else {
                        exit(EXIT_SUCCESS); // la tarea funciono de buena forma por lo que se devuelve codigo 0 al padre
                    }
                } else {// el proceso padre entra a esta parte del codigo
                    // el padre no lee solo escribe por lo que cerramos el apartado de lectura
                    close(fd[0]); 
                    
                    // metemos en la tuberia los mensajes que tenia guardados la actividad
                    write(fd[1], actividades[indice_lista].mensajes_recibidos, strlen(actividades[indice_lista].mensajes_recibidos) + 1);
                    close(fd[1]); // se cierra la escritura

                    actividades[indice_lista].pid = pid_hijo; // guardamos el pid del proceso hijo en la estructura de actividades
                    actividades[indice_lista].estado = 1;     // se cambia el estado a corriendo es decir un 1
                    procesos_corriendo++;                     
                    se_lanzo_proceso = 1;
                }
            } else {
                // no hay más actividades pendientes que puedan ejecutarse en este momento
                break;
            }
        }

        // esperar sin Busy-Waiting para cumplir con la rubrica
        // si aun hay procesos corriendo el padre se va a dormir hasta que uno de los hijos termine su ejecucion y le envie la señal SIGCHLD
        if (procesos_corriendo > 0) {// si hay procesos corriendo el padre espera a que uno de ellos termine
            int estado_salida;
            pid_t pid_terminado = wait(&estado_salida); // El Padre se pausa aqui hasta recibir la señal de un hijo

            if (pid_terminado > 0) {
                //buscamos la actividad correspondiente al pid del proceso hijo que terminó y actualizamos su estado
                for (int i = 0; i < total_actividades; i++) {
                    if (actividades[i].pid == pid_terminado) {
                        if (WIFEXITED(estado_salida)) { // Verifica si el hijo termino de la manera correcta
                            int codigo = WEXITSTATUS(estado_salida);// se guarda el codigo de salida del hijo
                            if (codigo == EXIT_SUCCESS) { // entra si el hijo termino correctamente(codigo0)
                                actividades[i].estado = 2; // estado 2 termina de manerA correcta
                                printf("[-] terminado con exito: %s (PID: %d)\n", actividades[i].nombre, pid_terminado);
                                
                                // propagacion del mensaje para las actividades que dependen de ella solo si tuvo exito
                                char mensaje_insumo[200];
                                snprintf(mensaje_insumo, sizeof(mensaje_insumo), "el insumo de %s esta listo. ", actividades[i].nombre);
                                
                                for (int d = 0; d < total_actividades; d++) {// se recorre la lista de actividades para ver cuales
                                                                               // dependen de la actividad que acaba de terminar
                                    for (int j = 0; j < actividades[d].num_dependencias; j++) {
                                        if (strcmp(actividades[d].dependencias[j], actividades[i].id) == 0) {
                                            strcat(actividades[d].mensajes_recibidos, mensaje_insumo);
                                        }
                                    }
                                }
                            } else {
                                actividades[i].estado = 3; // estado 3: fallida
                                printf("[x] error: la tarea %s (PID: %d) fallo durante su ejecucion.\n", actividades[i].nombre, pid_terminado);
                            }
                        }
                        
                        procesos_corriendo--;
                        actividades_terminadas++;
                        // se aplica un efecto domino para que se vayan canmcelanmdo en cascada las actividades que dependan de una que fallo
                        int hubo_cancelaciones;
                        do {
                            hubo_cancelaciones = 0;
                            for (int c = 0; c < total_actividades; c++) {
                                if (actividades[c].estado == 0) { // si la tarea esta pendiente
                                    for (int j = 0; j < actividades[c].num_dependencias; j++) {
                                        for (int k = 0; k < total_actividades; k++) {
                                            // revisamos si la dependencia de esta tarea corresponde a una tarea fallida o cancelada (estado 3)
                                            if (strcmp(actividades[c].dependencias[j], actividades[k].id) == 0) {
                                                if (actividades[k].estado == 3) { 
                                                    actividades[c].estado = 3; // la cancelamos
                                                    actividades_terminadas++;  // la sumamos a terminadas para que no bloquee el bucle global
                                                    hubo_cancelaciones = 1;
                                                    printf("[-] cancelada por cascada (dependencias): %s (falta insumo)\n", actividades[c].nombre);
                                                    break;
                                                }
                                            }
                                        }
                                        if (actividades[c].estado == 3) break;
                                    }
                                }
                            }
                        } while (hubo_cancelaciones); // repite por si la cancelacion afecta a otra tarea mas abajo en la cadena
                        break;
                    }
                }
            }
        } else if (se_lanzo_proceso == 0 && actividades_terminadas < total_actividades) {// si no se lanzo ningun proceso y aun hay actividades pendientes significa que hay un deadlock
            printf("las tareas se bloquearon porque falta una dependencia a esta\n");
            break;
        }
    }

    printf("\n--- SIMULACION EXITOSA: LA RAMADA ESTA LISTA ---\n");
    return EXIT_SUCCESS;
}