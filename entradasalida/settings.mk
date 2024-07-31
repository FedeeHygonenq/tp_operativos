# Libraries
LIBS=utils commons pthread readline m

# Custom libraries' paths
STATIC_LIBPATHS=../utils

# Compiler flags
CDEBUG=-g -Wall -DDEBUG -fdiagnostics-color=always
CRELEASE=-O3 -Wall -DNDEBUG -fcommon

# Arguments when executing with start, memcheck or helgrind
ARGS=cfg/SLP1.config SLP1 cfg/MONITOR.config MONITOR cfg/TECLADO.config TECLADO cfg/MONITOR_SALVATION.config MONITOR_SALVATION cfg/TECLADO_SALVATION.config TECLADO_SALVATION cfg/IO_GEN_SLEEP.config IO_GEN_SLEEP cfg/GENERICA.config GENERICA cfg/ESPERA.config ESPERA cfg/FS.config FS


# Valgrind flags
MEMCHECK_FLAGS=--track-origins=yes
HELGRIND_FLAGS=
