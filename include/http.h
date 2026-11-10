
#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

typedef struct {
    char metodo[16];
    char ruta[512];
    char version[16];
    char host[256];
    char conexion[32];
    char user_agent[256];
} SolicitudHTTP;

int analizar_solicitud(const char *datos, size_t longitud, SolicitudHTTP *solicitud);

const char *mensaje_estado(int codigo);

int generar_encabezado(char *buffer, size_t capacidad, int codigo,
                      const char *tipo, size_t tamano, int persistente);

#endif
