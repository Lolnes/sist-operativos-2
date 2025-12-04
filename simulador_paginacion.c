
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#define MIN_PROC_MB 1
#define MAX_PROC_MB 20
#define MAX_PROCESSES 1000

/* ============================
   Estructuras de datos
   ============================ */

typedef struct Page {
    int pid;
    int page_number;
    int in_ram;
    int frame_index;
    int in_swap;
    int swap_index;
} Page;

typedef struct Process {
    int pid;
    size_t size_bytes;
    int num_pages;
    Page *pages;
    int alive;
} Process;

typedef struct Frame {
    int occupied;
    Page *page;
} Frame;

typedef struct SwapSlot {
    int occupied;
    Page *page;
} SwapSlot;

/* ============================
   Variables globales
   ============================ */

static Process processes[MAX_PROCESSES];
static int process_count = 0;

static int alive_indices[MAX_PROCESSES];
static int alive_count = 0;

static int next_pid = 1;

/* RAM */
static Frame *ram_frames = NULL;
static int ram_frame_count = 0;
static int free_ram_frames = 0;

/* SWAP */
static SwapSlot *swap_slots = NULL;
static int swap_slot_count = 0;
static int free_swap_slots = 0;

/* FIFO */
static int *fifo_queue = NULL;
static int fifo_head = 0;
static int fifo_size = 0;
static int fifo_capacity = 0;

/* Parámetros */
static size_t page_size_bytes = 0;
static size_t physical_mem_bytes = 0;
static size_t virtual_mem_bytes = 0;

/* Control de simulación */
static int simulation_running = 1;

/* Estadísticas */
static long total_page_faults = 0;
static long total_fifo_replacements = 0;
static long total_pages_loaded_from_swap = 0;
static long total_pages_evicted_to_swap = 0;

/* ============================
   Prototipos
   ============================ */
void init_memory(int physical_mem_mb, int page_size_kb);
void enqueue_frame_fifo(int frame_index);
int dequeue_frame_fifo(void);
void create_process(void);
void kill_random_process(time_t elapsed);
void simulate_random_access(time_t elapsed);
int get_free_ram_frame(void);
int get_free_swap_slot(void);
void end_simulation(const char *reason);
void print_statistics(void);

/* ============================
   Utils
   ============================ */

static inline size_t mb_to_bytes(int mb) {
    return (size_t)mb * 1024ULL * 1024ULL;
}

/* ============================
             MAIN
   ============================ */

int main(void) {
    int physical_mem_mb;
    int page_size_kb;

    srand(time(NULL));

    printf("=== Simulador de paginacion (FIFO) ===\n");
    printf("Ingrese tamano de memoria fisica (en MB): ");
    if (scanf("%d", &physical_mem_mb) != 1) {
    fprintf(stderr, "Error: entrada invalida para memoria fisica.\n");
    return 1;
}

    printf("Ingrese tamano de pagina (en KB): ");
    if (scanf("%d", &page_size_kb) != 1) {
    fprintf(stderr, "Error: entrada invalida para tamano de pagina.\n");
    return 1;
}

    init_memory(physical_mem_mb, page_size_kb);

    printf("\nConfiguracion inicial:\n");
    printf("  Memoria fisica: %d MB (%.2f MB efectivos)\n",
           physical_mem_mb, (double)physical_mem_bytes / (1024.0 * 1024.0));
    printf("  Tamano de pagina: %d KB\n", page_size_kb);
    printf("  Memoria virtual total: %.2f MB\n",
           (double)virtual_mem_bytes / (1024.0 * 1024.0));
    printf("  Frames en RAM: %d\n", ram_frame_count);
    printf("  Slots en SWAP: %d\n", swap_slot_count);
    printf("  Politica de reemplazo: FIFO\n\n");

    printf("Comenzando simulacion...\n");

    time_t start = time(NULL);
    time_t last_create = start;
    time_t last_kill = start;
    time_t last_access = start;

    while (simulation_running) {
        time_t now = time(NULL);
        time_t elapsed = now - start;

        /* Crear proceso cada 2 s */
        if (now - last_create >= 2) {
            create_process();
            last_create = now;
        }

        /* Desde t >= 30 */
        if (elapsed >= 30) {
            if (now - last_kill >= 5) {
                kill_random_process(elapsed);
                last_kill = now;
            }
            if (now - last_access >= 5) {
                simulate_random_access(elapsed);
                last_access = now;
            }
        }

        /* Fin global por memoria */
        if (simulation_running &&
            free_ram_frames == 0 &&
            free_swap_slots == 0) {
            end_simulation("No queda memoria disponible en RAM ni en SWAP.");
        }

        sleep(1);
    }

    /* Liberar memoria y mostrar estadisticas */
    print_statistics();

    for (int i = 0; i < process_count; i++)
        free(processes[i].pages);

    free(ram_frames);
    free(swap_slots);
    free(fifo_queue);

    printf("Simulacion terminada.\n");
    return 0;
}

/* ============================
   Inicializacion de memoria
   ============================ */

void init_memory(int physical_mem_mb, int page_size_kb) {
    page_size_bytes = (size_t)page_size_kb * 1024ULL;
    physical_mem_bytes = mb_to_bytes(physical_mem_mb);

    double factor = 1.5 + ((double)rand() / RAND_MAX) * 3.0;
    virtual_mem_bytes = (size_t)(physical_mem_bytes * factor);

    ram_frame_count = physical_mem_bytes / page_size_bytes;
    int total_pages = virtual_mem_bytes / page_size_bytes;

    swap_slot_count = total_pages - ram_frame_count;

    ram_frames = calloc(ram_frame_count, sizeof(Frame));
    swap_slots = calloc(swap_slot_count, sizeof(SwapSlot));

    fifo_capacity = ram_frame_count;
    fifo_queue = malloc(sizeof(int) * fifo_capacity);

    free_ram_frames = ram_frame_count;
    free_swap_slots = swap_slot_count;
}

/* ============================
   FIFO queue operations
   ============================ */

void enqueue_frame_fifo(int frame_index) {
    int pos = (fifo_head + fifo_size) % fifo_capacity;
    fifo_queue[pos] = frame_index;
    fifo_size++;
}

int dequeue_frame_fifo(void) {
    while (fifo_size > 0) {
        int frame_index = fifo_queue[fifo_head];
        fifo_head = (fifo_head + 1) % fifo_capacity;
        fifo_size--;

        if (ram_frames[frame_index].occupied)
            return frame_index;
    }
    return -1;
}

/* ============================
   Utilidades RAM / SWAP
   ============================ */

int get_free_ram_frame(void) {
    for (int i = 0; i < ram_frame_count; i++)
        if (!ram_frames[i].occupied)
            return i;
    return -1;
}

int get_free_swap_slot(void) {
    for (int i = 0; i < swap_slot_count; i++)
        if (!swap_slots[i].occupied)
            return i;
    return -1;
}

/* ============================
   Crear proceso
   ============================ */

void create_process(void) {
    if (!simulation_running) return;

    if (process_count >= MAX_PROCESSES)
        return;

    int proc_mb = MIN_PROC_MB + rand() % (MAX_PROC_MB - MIN_PROC_MB + 1);
    size_t size_bytes = mb_to_bytes(proc_mb);
    int num_pages = (size_bytes + page_size_bytes - 1) / page_size_bytes;

    Process *p = &processes[process_count];
    p->pid = next_pid++;
    p->size_bytes = size_bytes;
    p->num_pages = num_pages;
    p->alive = 1;

    p->pages = calloc(num_pages, sizeof(Page));

    printf("[CREAR] Proceso PID=%d, tamano=%d MB, paginas=%d\n",
           p->pid, proc_mb, num_pages);

    int in_ram = 0, in_swap = 0;

    for (int i = 0; i < num_pages; i++) {
        Page *pg = &p->pages[i];
        pg->pid = p->pid;
        pg->page_number = i;
        pg->frame_index = -1;
        pg->swap_index = -1;

        int f = get_free_ram_frame();
        if (f >= 0) {
            ram_frames[f].occupied = 1;
            ram_frames[f].page = pg;
            pg->in_ram = 1;
            pg->frame_index = f;
            enqueue_frame_fifo(f);
            free_ram_frames--;
            in_ram++;
        } else {
            int s = get_free_swap_slot();
            if (s < 0) {
                end_simulation("Memoria insuficiente al crear proceso.");
                return;
            }
            swap_slots[s].occupied = 1;
            swap_slots[s].page = pg;
            pg->in_swap = 1;
            pg->swap_index = s;
            free_swap_slots--;
            in_swap++;
        }
    }

    printf("        Paginas en RAM: %d, Paginas en SWAP: %d\n", in_ram, in_swap);

    alive_indices[alive_count++] = process_count;
    process_count++;
}

/* ============================
   Finalizar proceso
   ============================ */

void kill_random_process(time_t elapsed) {
    if (!simulation_running) return;

    if (alive_count == 0) return;

    int pos = rand() % alive_count;
    int idx = alive_indices[pos];
    Process *p = &processes[idx];

    if (!p->alive) return;

    printf("[t=%lds] [FIN] Finalizando proceso PID=%d, paginas=%d\n",
           (long)elapsed, p->pid, p->num_pages);

    for (int i = 0; i < p->num_pages; i++) {
        Page *pg = &p->pages[i];
        if (pg->in_ram) {
            ram_frames[pg->frame_index].occupied = 0;
            ram_frames[pg->frame_index].page = NULL;
            free_ram_frames++;
        }
        if (pg->in_swap) {
            swap_slots[pg->swap_index].occupied = 0;
            swap_slots[pg->swap_index].page = NULL;
            free_swap_slots++;
        }
    }

    p->alive = 0;
    free(p->pages);
    p->pages = NULL;

    alive_indices[pos] = alive_indices[alive_count - 1];
    alive_count--;

    printf("        Proceso PID=%d finalizado.\n", p->pid);
}

/* ============================
   Simular acceso virtual
   ============================ */

void simulate_random_access(time_t elapsed) {
    if (!simulation_running) return;
    if (alive_count == 0) return;

    int pos = rand() % alive_count;
    int idx = alive_indices[pos];
    Process *p = &processes[idx];

    if (!p->alive) return;

    size_t addr = rand() % p->size_bytes;
    int page_num = addr / page_size_bytes;

    Page *pg = &p->pages[page_num];

    printf("[t=%lds] [ACCESO] PID=%d, dir_virtual=%zu (pagina=%d)\n",
           (long)elapsed, p->pid, addr, page_num);

    if (pg->in_ram) {
        printf("          -> Pagina ya en RAM (frame=%d)\n", pg->frame_index);
        return;
    }

    /* PAGE FAULT */
    total_page_faults++;

    printf("          -> PAGE FAULT\n");

    /* Si no está en swap, ponerla */
    if (!pg->in_swap) {
        int s = get_free_swap_slot();
        if (s < 0) {
            end_simulation("No hay espacio en SWAP para page fault.");
            return;
        }
        swap_slots[s].occupied = 1;
        swap_slots[s].page = pg;
        pg->in_swap = 1;
        pg->swap_index = s;
        free_swap_slots--;
    }

    /* Intentar cargar a RAM */
    int f = get_free_ram_frame();

    if (f < 0) {
        /* FIFO replacement */
        int victim_frame = dequeue_frame_fifo();
        if (victim_frame < 0) {
            end_simulation("No se encontro frame victima FIFO.");
            return;
        }

        Page *victim = ram_frames[victim_frame].page;

        int s = get_free_swap_slot();
        if (s < 0) {
            end_simulation("No hay swap para victima FIFO.");
            return;
        }

        /* Mover victima a SWAP */
        swap_slots[s].occupied = 1;
        swap_slots[s].page = victim;
        victim->in_ram = 0;
        victim->in_swap = 1;
        victim->swap_index = s;
        free_swap_slots--;

        ram_frames[victim_frame].occupied = 0;
        ram_frames[victim_frame].page = NULL;
        free_ram_frames++;

        total_fifo_replacements++;
        total_pages_evicted_to_swap++;

        printf("          -> FIFO: victima PID=%d pag=%d frame=%d\n",
               victim->pid, victim->page_number, victim_frame);

        f = victim_frame;
    }

    /* Traer pagina desde SWAP */
    int old_s = pg->swap_index;
    if (old_s >= 0) {
        swap_slots[old_s].occupied = 0;
        swap_slots[old_s].page = NULL;
        free_swap_slots++;
    }

    ram_frames[f].occupied = 1;
    ram_frames[f].page = pg;
    pg->in_ram = 1;
    pg->frame_index = f;
    pg->in_swap = 0;
    pg->swap_index = -1;
    free_ram_frames--;

    enqueue_frame_fifo(f);
    total_pages_loaded_from_swap++;

    printf("          -> Pagina cargada a frame=%d\n", f);
}

/* ============================
   Fin de simulación único
   ============================ */

void end_simulation(const char *reason) {
    if (!simulation_running) return; // Evitar duplicados

    simulation_running = 0;

    printf("\n*** FIN DE SIMULACION ***\n");
    if (reason) printf("Motivo: %s\n\n", reason);
}

/* ============================
   Estadísticas finales
   ============================ */

void print_statistics(void) {
    printf("\n===== ESTADISTICAS DE LA SIMULACION =====\n");

    printf("\n--- Page Faults ---\n");
    printf("Total page faults: %ld\n", total_page_faults);
    printf("Reemplazos FIFO: %ld\n", total_fifo_replacements);
    printf("Paginas cargadas desde SWAP: %ld\n", total_pages_loaded_from_swap);
    printf("Paginas enviadas a SWAP por FIFO: %ld\n", total_pages_evicted_to_swap);

    printf("\n--- Memoria RAM ---\n");
    printf("Frames totales: %d\n", ram_frame_count);
    printf("Frames libres: %d\n", free_ram_frames);
    printf("Uso: %.2f%%\n",
           100.0 * (ram_frame_count - free_ram_frames) / ram_frame_count);

    printf("\n--- Memoria SWAP ---\n");
    printf("Slots totales: %d\n", swap_slot_count);
    printf("Slots libres: %d\n", free_swap_slots);
    printf("Uso: %.2f%%\n",
           100.0 * (swap_slot_count - free_swap_slots) / swap_slot_count);

    printf("\n--- Procesos ---\n");
    printf("Procesos creados: %d\n", process_count);
    printf("Procesos vivos: %d\n", alive_count);
    printf("Procesos finalizados: %d\n", process_count - alive_count);

    printf("=========================================\n\n");
}
