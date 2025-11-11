#include "util.h"

void die(const char *msg) { // Mensaje de error y salida    
    fprintf(stderr, "Error: %s\n", msg);
    exit(EXIT_FAILURE);
}

void *xmalloc(size_t n) { // calloc con verificación de errores
    void *p = calloc(1, n); // calloc inicializa a 0
    if (!p) die("calloc falló");
    return p;
}

// Realiza una copia de cadena. Usa malloc internamente
char *xstrdup(const char *s) { // strdup con verificación de errores
    char *r = strdup(s);
    if (!r) die("strdup falló");
    return r;
}
FitStrategy parse_strategy(const char *s) { // Parsear estrategia desde cadena
    if (strcmp(s, "first")==0) return FIRST_FIT;
    if (strcmp(s, "best")==0)  return BEST_FIT;
    if (strcmp(s, "worst")==0) return WORST_FIT;
    die("Estrategia inválida (usa: first | best | worst)");
    return FIRST_FIT;
}
