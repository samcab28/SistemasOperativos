#include "memsim.h"
#include "util.h"

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <estrategia:first|best|worst> <tam_pool_bytes> <archivo_entrada>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FitStrategy strat = parse_strategy(argv[1]);
    errno = 0;
    char *endp = NULL;
    unsigned long long ps = strtoull(argv[2], &endp, 10);
    if (errno || endp == argv[2] || *endp != '\0' || ps == 0)
        die("tam_pool_bytes inválido");

    MemSim ms;
    init_memsim(&ms, strat, (size_t)ps);
    run_script(&ms, argv[3]);
    cleanup_memsim(&ms);
    return 0;
}
