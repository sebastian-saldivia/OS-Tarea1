**Tarea 1 - Sistemas Operativos: Planificador de Procesos (La Ramada)**


## 1. Compilación y Ejecución
Para compilar el código fuente, simplemente utiliza el Makefile incluido ejecutando en la terminal:
`make`

Para correr la simulación, ejecuta el programa pasándole como argumentos el archivo de texto y el límite de concurrencia (K):
`./planificador plan.txt 2`

## 2. Decisiones de Diseño y Soluciones

Para cumplir con el 100% de las restricciones de la rúbrica, se tomaron las siguientes decisiones de diseño a nivel de sistema:

**Soporte de sobrecarga (+10.000 tareas):** Para soportar la carga masiva sin provocar un *Stack Overflow* (Violación de segmento), el arreglo principal de actividades se definió de manera global utilizando `static`. Esto mueve los datos al Segmento BSS de la memoria, permitiendo escalar el máximo a 15.000 para cumplir holgadamente con el piso técnico que pide la rúbrica.


**Control de concurrencia sin Busy-Waiting:** El respeto por el límite `K` se implementó usando `wait()`. Con esto se garantiza que el proceso padre se bloquee y no consuma ciclos de CPU inútilmente hasta que un hijo libere un cupo.


**Tuberías y Mensajes:** La comunicación se logra creando un `pipe()` antes de cada `fork()`. Se aislaron correctamente los extremos (el padre cierra la escritura y el hijo cierra la lectura) para transmitir los mensajes sin colisiones.


**Aislamiento de errores (Cascada):** Las tareas tienen un porcentaje de fallo simulado. Cuando el padre detecta con las macros `WIFEXITED` y `WEXITSTATUS` que un hijo falló, cancela en cascada únicamente la rama de dependencias directas, permitiendo que el resto de las ramas independientes sigan ejecutándose.


**Plan Seremi (Manejo de señales):** Se intercepta la señal de `Ctrl + C` (SIGINT). Para evitar el problema de herencia donde múltiples procesos capturan la señal, se protegió a los hijos usando `signal(SIGINT, SIG_IGN)`. De esta forma, solo el proceso padre atiende a la Seremi y se encarga de usar `kill()` para limpiar y cerrar los procesos en curso de forma ordenada.
