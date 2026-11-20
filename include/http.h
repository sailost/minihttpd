#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>
#include <sys/types.h>

typedef struct {
    char metodo[16];
    char ruta[512];
    char version[16];
    char host[256];
    char conexion[32];
    char user_agent[256];
} SolicitudHTTP;

typedef struct {
    int archivo;
    off_t tamano;
    off_t posicion;
    char cabecera[512];
    int largo_cabecera;
    size_t cabecera_enviada;
} RespuestaHTTP;

int analizar_solicitud(const char *datos, size_t longitud, SolicitudHTTP *solicitud);

const char *mensaje_estado(int codigo);

int generar_encabezado(char *buffer, size_t capacidad, int codigo,
                       const char *tipo, size_t tamano, int persistente);

int preparar_respuesta(RespuestaHTTP *respuesta, int codigo,
                      const char *ruta, int persistente);

#endif