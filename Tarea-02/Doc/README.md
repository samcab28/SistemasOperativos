# Proyecto 02 - Sistemas operativos

Estudiantes:
 - Barrantes Jimenez Anthony
 - Cabrera Tabash Samir 

Link video: 



src/
├── main.c
├── patterns/
│   ├── factory.c/h          # Factory para productos
│   ├── chain_handler.c/h    # Chain of Responsibility mejorado
│   └── strategy.c/h         # Strategy para algoritmos de scheduling
├── core/
│   ├── product.c/h          # Producto con más información
│   ├── station.c/h          # Estación como hilo/proceso
│   ├── scheduler.c/h        # Scheduler con hilos
│   └── queue.c/h           # Cola thread-safe (ya tienes)
├── metrics/
│   ├── metrics.c/h         # Sistema de métricas completo
│   └── logger.c/h          # Para logging de eventos
└── utils/
    ├── time_utils.c/h      # Utilidades de tiempo
    └── sync_utils.c/h      # Utilidades de sincronización