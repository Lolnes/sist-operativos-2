# Simulador de Memoria (FIFO)

Este programa es una herramienta sencilla que simula cómo una computadora administra su memoria RAM. Muestra cómo se crean procesos, cómo se llenan los espacios de memoria y qué pasa cuando la memoria se acaba (usa un método llamado FIFO para liberar espacio).

## ¿Qué necesito para usarlo?

Necesitas tener instalado un compilador de C (como **GCC**).
* **En Linux/Mac:** Generalmente ya viene instalado o se instala fácil desde la terminal.

## Instrucciones de Uso

Sigue estos 3 pasos simples en tu terminal:

### 1. Descargar
Guarda el archivo de código como `simulador_paginacion.c` en una carpeta.

### 2. Compilar (Crear el programa)
Escribe el siguiente comando en la terminal para convertir el código en un programa ejecutable:

gcc simulador_paginacion.c -o simulador

### 3. Ejecutar
Inicia el programa con este comando:

./simulador

---

## ¿Cómo interactuar con el programa?

Al iniciar, el programa te pedirá dos números. Aquí tienes un ejemplo recomendado para empezar:

1.  **Tamaño de memoria física (en MB):** Escribe `16` y pulsa Enter.
   

2.  **Tamaño de página (en KB):** Escribe `4` y pulsa Enter.
   

**¡Listo!** El programa empezará a funcionar solo. Verás texto apareciendo que indica que se están creando procesos y moviendo memoria.

Para detenerlo antes de que termine, puedes presionar las teclas `Ctrl + C`.
