#!/bin/bash

# Limpiar compilación anterior
make clean

# Compilar
make
if [ $? -ne 0 ]; then
  echo "❌ Error en la compilación"
  exit 1
fi

echo "✅ Compilación exitosa. Ejecutando simulador..."
echo "----------------------------------------------"

# Ejecutar
./simulador
