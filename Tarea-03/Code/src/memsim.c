#include "memsim.h"
#include "util.h"
#include "varmap.h"
#include "block.h"

/* ---------- Inicialización ---------- */

void init_memsim(MemSim *ms, FitStrategy s, size_t pool_size) {
    ms->pool = (uint8_t*)xmalloc(pool_size); // usa calloc internamente
    ms->pool_size = pool_size;
    ms->strat = s;
    ms->vars = NULL;
    ms->blocks = new_block(0, pool_size, 1);
}

/* ---------- Expansión de memoria (usa realloc) ---------- */

void expand_pool(MemSim *ms, size_t extra_bytes) {
    uint8_t *newpool = realloc(ms->pool, ms->pool_size + extra_bytes);
    if (!newpool) die("realloc falló al expandir pool");

    size_t old_size = ms->pool_size;
    ms->pool = newpool;
    ms->pool_size += extra_bytes;

    // Buscar el último bloque en la lista
    BlockHeader *last = ms->blocks;
    while (last && last->next) last = last->next;

    // Si el último bloque es libre, lo agrandamos
    if (last && last->free) {
        last->size += extra_bytes;
    } else {
        // Si no, creamos un nuevo bloque libre al final
        BlockHeader *b = new_block(old_size, extra_bytes, 1);
        insert_after(last, b);
    }

    printf("[INFO] Pool expandido a %zu bytes (+%zu)\n", ms->pool_size, extra_bytes);
}

/* ---------- Búsqueda de hueco según estrategia ---------- */

static BlockHeader* pick_block(MemSim *ms, size_t need) {
    BlockHeader *best = NULL, *worst = NULL;
    for (BlockHeader *b = ms->blocks; b; b = b->next) {
        if (!b->free || b->size < need) continue;
        if (ms->strat == FIRST_FIT) return b;
        if (ms->strat == BEST_FIT) {
            if (!best || b->size < best->size) best = b;
        } else if (ms->strat == WORST_FIT) {
            if (!worst || b->size > worst->size) worst = b;
        }
    }
    return (ms->strat == BEST_FIT) ? best : worst;
}

/* ---------- Split de hueco ---------- */

static BlockHeader* alloc_from_block(MemSim *ms, BlockHeader *hole, size_t need) {
    if (!hole || !hole->free || hole->size < need) return NULL;

    if (hole->size == need) {
        hole->free = 0;
        return hole;
    }

    BlockHeader *allocb = new_block(hole->offset, need, 0);
    hole->offset += need;
    hole->size   -= need;

    if (hole->prev) {
        insert_after(hole->prev, allocb);
    } else {
        allocb->next = hole;
        hole->prev = allocb;
        ms->blocks = allocb;
    }
    return allocb;
}

/* ---------- Utilidad: rellenar bloque con nombre ---------- */

static void fill_with_name(uint8_t *p, size_t n, const char *name) {
    size_t L = strlen(name);
    if (L == 0) { memset(p, 0, n); return; }
    for (size_t i = 0; i < n; i++) p[i] = (uint8_t)name[i % L];
}

/* ---------- API: ALLOC / REALLOC / FREE ---------- */

static BlockHeader* do_alloc(MemSim *ms, const char *var, size_t size) {
    BlockHeader *hole = pick_block(ms, size);

    if (!hole) {
        // Si no hay espacio, expandimos el pool usando realloc
        size_t extra = size * 2; // crece dinámicamente (puedes ajustar el factor)
        printf("[WARN] Sin espacio: expandiendo pool en +%zu bytes...\n", extra);
        expand_pool(ms, extra);

        // volver a intentar
        hole = pick_block(ms, size);
        if (!hole) return NULL;
    }

    BlockHeader *b = alloc_from_block(ms, hole, size);
    if (!b) return NULL;

    fill_with_name(ms->pool + b->offset, b->size, var);
    set_var(ms, var, b);
    return b;
}

static BlockHeader* do_realloc(MemSim *ms, const char *var, size_t newsize) {
    VarMap *v = find_var(ms->vars, var);
    if (!v || !v->block) return NULL;
    BlockHeader *b = v->block;

    if (b->size == newsize) {
        fill_with_name(ms->pool + b->offset, b->size, var);
        return b;
    }

    if (b->size > newsize) {
        size_t shrink = b->size - newsize;
        b->size = newsize;
        BlockHeader *freeb = new_block(b->offset + b->size, shrink, 1);
        insert_after(b, freeb);
        try_coalesce(ms, freeb);
        fill_with_name(ms->pool + b->offset, b->size, var);
        return b;
    }

    size_t grow = newsize - b->size;
    if (b->next && b->next->free && b->next->size >= grow &&
        b->offset + b->size == b->next->offset) {
        b->next->offset += grow;
        b->next->size   -= grow;
        b->size = newsize;
        if (b->next->size == 0) remove_block(ms, b->next);
        fill_with_name(ms->pool + b->offset, b->size, var);
        return b;
    }

    // si no hay espacio contiguo → intentar expandir el pool
    printf("[WARN] REALLOC no encontró espacio contiguo; expandiendo pool...\n");
    expand_pool(ms, grow * 2);

    BlockHeader *nb = do_alloc(ms, var, newsize);
    if (!nb) return NULL;

    v->block = nb;
    b->free = 1;
    try_coalesce(ms, b);
    return nb;
}

static int do_free(MemSim *ms, const char *var) {
    VarMap *v = find_var(ms->vars, var);
    if (!v || !v->block) return -1;
    BlockHeader *b = v->block;
    b->free = 1;
    try_coalesce(ms, b);
    unset_var(ms, var);
    return 0;
}

/* ---------- PRINT ---------- */

static void print_state(MemSim *ms) {
    printf("=== ESTADO DE LA MEMORIA ===\n");
    printf("Pool size: %zu bytes\n", ms->pool_size);
    printf("%-8s %-8s %-8s %-6s\n", "OFFSET", "SIZE", "END", "STATE");

    for (BlockHeader *b = ms->blocks; b; b = b->next) {
        size_t end = b->offset + b->size - 1;
        printf("%-8zu %-8zu %-8zu %-6s\n",
               b->offset, b->size, end, b->free ? "FREE" : "USED");
    }

    printf("\nVariables activas:\n");
    if (!ms->vars) {
        printf("  (ninguna)\n");
    } else {
        for (VarMap *v = ms->vars; v; v = v->next) {
            printf("  %s -> [offset=%zu, size=%zu]\n",
                   v->name,
                   v->block ? v->block->offset : 0,
                   v->block ? v->block->size : 0);
        }
    }
    printf("============================\n");
}

/* ---------- Parser de archivo ---------- */

static char *trim(char *s) {
    if (!s) return s;
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    char *e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) *e-- = '\0';
    return s;
}

static int is_blank_or_comment(const char *s) {
    if (!s || !*s) return 1;
    if (*s == '#') return 1;
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

void run_script(MemSim *ms, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror("fopen"); exit(EXIT_FAILURE); }

    char *line = NULL;
    size_t cap = 0;
    unsigned long lineno = 0;

    while (getline(&line, &cap, f) != -1) {
        lineno++;
        char *raw = trim(line);
        if (is_blank_or_comment(raw)) continue;

        char op[16] = {0}, var[128] = {0};
        size_t size = 0;
        int n = sscanf(raw, "%15s %127s %zu", op, var, &size);

        if (strcasecmp(op, "PRINT") == 0) {
            print_state(ms);
        } else if (strcasecmp(op, "ALLOC") == 0) {
            if (n != 3) { fprintf(stderr, "[L%lu] ALLOC mal formado\n", lineno); continue; }
            if (find_var(ms->vars, var)) {
                fprintf(stderr, "[L%lu] Variable '%s' ya existe\n", lineno, var);
                continue;
            }
            BlockHeader *b = do_alloc(ms, var, size);
            if (!b) fprintf(stderr, "[L%lu] ALLOC %s %zu: sin espacio/fragmentación\n", lineno, var, size);
        } else if (strcasecmp(op, "REALLOC") == 0) {
            if (n != 3) { fprintf(stderr, "[L%lu] REALLOC mal formado\n", lineno); continue; }
            if (!find_var(ms->vars, var)) {
                fprintf(stderr, "[L%lu] REALLOC de variable inexistente '%s'\n", lineno, var);
                continue;
            }
            BlockHeader *b = do_realloc(ms, var, size);
            if (!b) fprintf(stderr, "[L%lu] REALLOC %s %zu: fallo (sin espacio)\n", lineno, var, size);
        } else if (strcasecmp(op, "FREE") == 0) {
            if (n < 2) { fprintf(stderr, "[L%lu] FREE mal formado\n", lineno); continue; }
            if (do_free(ms, var) != 0)
                fprintf(stderr, "[L%lu] FREE de variable inexistente '%s'\n", lineno, var);
        } else {
            fprintf(stderr, "[L%lu] Operación desconocida: %s\n", lineno, op);
        }
    }

    free(line);
    fclose(f);
}

/* ---------- Limpieza final ---------- */

void cleanup_memsim(MemSim *ms) {
    while (ms->vars) {
        VarMap *t = ms->vars->next;
        free(ms->vars->name);
        free(ms->vars);
        ms->vars = t;
    }
    BlockHeader *b = ms->blocks;
    while (b) {
        BlockHeader *n = b->next;
        free(b);
        b = n;
    }
    free(ms->pool);
}
