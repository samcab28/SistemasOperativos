// Tarea 01 Sistemas Operativos 
// Estudiantes Anthonhy Barrantes y Samir Cabrera


#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#define __NR_my_syscall 548  // número que usaste en syscall_64.tbl

int main() {
    long res = syscall(__NR_my_syscall);
    printf("Resultado del syscall: %ld\n", res);
    return 0;
}