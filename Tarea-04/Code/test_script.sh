#!/bin/bash

# Script de pruebas automáticas para el sistema de archivos

PROGRAM="./bin/filesystem"
BOLD='\033[1m'
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BOLD}=== Pruebas del Sistema de Archivos ===${NC}\n"

# Verificar que el programa existe
if [ ! -f "$PROGRAM" ]; then
    echo -e "${RED}Error: No se encuentra el ejecutable. Ejecuta 'make' primero.${NC}"
    exit 1
fi

# Contador de pruebas
PASSED=0
FAILED=0

# Función para ejecutar prueba
run_test() {
    local test_name=$1
    local commands=$2
    
    echo -e "${BOLD}Prueba: ${test_name}${NC}"
    echo "$commands" | $PROGRAM > /dev/null 2>&1
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✓ PASÓ${NC}\n"
        ((PASSED++))
    else
        echo -e "${RED}✗ FALLÓ${NC}\n"
        ((FAILED++))
    fi
}

# Prueba 1: Crear archivo
echo -e "${BOLD}Ejecutando Prueba 1: Crear archivo${NC}"
OUTPUT=$(echo -e "CREATE archivo1.txt 1000\nLIST\nexit" | $PROGRAM)
if echo "$OUTPUT" | grep -q "archivo1.txt"; then
    echo -e "${GREEN}✓ PASÓ${NC}\n"
    ((PASSED++))
else
    echo -e "${RED}✗ FALLÓ${NC}\n"
    ((FAILED++))
fi

# Prueba 2: Escribir y leer datos
echo -e "${BOLD}Ejecutando Prueba 2: Escribir y leer datos${NC}"
OUTPUT=$(echo -e "CREATE archivo2.txt 100\nWRITE archivo2.txt 0 \"Hola, mundo\"\nREAD archivo2.txt 0 14\nexit" | $PROGRAM)
if echo "$OUTPUT" | grep -q "Hola, mundo"; then
    echo -e "${GREEN}✓ PASÓ${NC}\n"
    ((PASSED++))
else
    echo -e "${RED}✗ FALLÓ${NC}\n"
    ((FAILED++))
fi

# Prueba 3: Eliminar archivo
echo -e "${BOLD}Ejecutando Prueba 3: Eliminar archivo${NC}"
OUTPUT=$(echo -e "CREATE archivo3.txt 500\nDELETE archivo3.txt\nLIST\nexit" | $PROGRAM)
if echo "$OUTPUT" | grep -q "no hay archivos"; then
    echo -e "${GREEN}✓ PASÓ${NC}\n"
    ((PASSED++))
else
    echo -e "${RED}✗ FALLÓ${NC}\n"
    ((FAILED++))
fi

# Prueba 4: Archivo duplicado (debe fallar)
echo -e "${BOLD}Ejecutando Prueba 4: Crear archivo duplicado${NC}"
OUTPUT=$(echo -e "CREATE archivo4.txt 100\nCREATE archivo4.txt 100\nexit" | $PROGRAM)
if echo "$OUTPUT" | grep -q "ya existe"; then
    echo -e "${GREEN}✓ PASÓ${NC}\n"
    ((PASSED++))
else
    echo -e "${RED}✗ FALLÓ${NC}\n"
    ((FAILED++))
fi

# Prueba 5: Leer archivo inexistente
echo -e "${BOLD}Ejecutando Prueba 5: Leer archivo inexistente${NC}"
OUTPUT=$(echo -e "READ noexiste.txt 0 10\nexit" | $PROGRAM)
if echo "$OUTPUT" | grep -q "no encontrado"; then
    echo -e "${GREEN}✓ PASÓ${NC}\n"
    ((PASSED++))
else
    echo -e "${RED}✗ FALLÓ${NC}\n"
    ((FAILED++))
fi

# Prueba 6: Múltiples archivos
echo -e "${BOLD}Ejecutando Prueba 6: Múltiples archivos${NC}"
OUTPUT=$(echo -e "CREATE test1.txt 100\nCREATE test2.txt 200\nCREATE test3.txt 300\nLIST\nexit" | $PROGRAM)
COUNT=$(echo "$OUTPUT" | grep -E -c "^test[123]\.txt")
if [ "$COUNT" -eq 3 ]; then
    echo -e "${GREEN}✓ PASÓ${NC}\n"
    ((PASSED++))
else
    echo -e "${RED}✗ FALLÓ${NC}\n"
    ((FAILED++))
fi


# Prueba 7: Escribir con offset
echo -e "${BOLD}Ejecutando Prueba 7: Escribir con offset${NC}"
OUTPUT=$(echo -e "CREATE offset.txt 100\nWRITE offset.txt 10 \"Test\"\nREAD offset.txt 10 10\nexit" | $PROGRAM)
if echo "$OUTPUT" | grep -q "Test"; then
    echo -e "${GREEN}✓ PASÓ${NC}\n"
    ((PASSED++))
else
    echo -e "${RED}✗ FALLÓ${NC}\n"
    ((FAILED++))
fi

# Prueba 8: Tamaño inválido
echo -e "${BOLD}Ejecutando Prueba 8: Crear archivo con tamaño inválido${NC}"
OUTPUT=$(echo -e "CREATE invalid.txt 0\nexit" | $PROGRAM)
if echo "$OUTPUT" | grep -q "Error"; then
    echo -e "${GREEN}✓ PASÓ${NC}\n"
    ((PASSED++))
else
    echo -e "${RED}✗ FALLÓ${NC}\n"
    ((FAILED++))
fi

# Resumen
echo -e "\n${BOLD}=== Resumen de Pruebas ===${NC}"
echo -e "Pruebas pasadas: ${GREEN}${PASSED}${NC}"
echo -e "Pruebas fallidas: ${RED}${FAILED}${NC}"
TOTAL=$((PASSED + FAILED))
echo -e "Total: ${TOTAL}"

if [ $FAILED -eq 0 ]; then
    echo -e "\n${GREEN}${BOLD}¡Todas las pruebas pasaron exitosamente!${NC}"
    exit 0
else
    echo -e "\n${RED}${BOLD}Algunas pruebas fallaron.${NC}"
    exit 1
fi