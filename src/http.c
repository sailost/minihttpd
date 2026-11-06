
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "http.h"

static int guardar_valor(char *destino, size_t capacidad, const char *valor) {
    size_t longitud = strlen(valor);

    if (longitud >= capacidad) {
        return -1;
    }

    memcpy(destino, valor, longitud + 1);
    return 0;
}

int analizar_solicitud(const char *datos, size_t longitud, SolicitudHTTP *solicitud) {
    if (datos == NULL || solicitud == NULL || longitud > 8192) {
        return -1;
    }

    if (memchr(datos, '\0', longitud) != NULL) {
        return -1;
    }

    char texto[8193];
    memcpy(texto, datos, longitud);
    texto[longitud] = '\0';

    char *fin = strstr(texto, "\r\n\r\n");

    if (fin == NULL) {
        return 0;
    }

    char *salto = strstr(texto, "\r\n");

    if (salto == NULL) {
        return -1;
    }

    *salto = '\0';
    memset(solicitud, 0, sizeof(SolicitudHTTP));

    char extra;

    if (sscanf(texto, "%15s %511s %15s %c",
               solicitud->metodo, solicitud->ruta,
               solicitud->version, &extra) != 3) {
        return -1;
    }

    if (strcmp(solicitud->version, "HTTP/1.1") != 0 ||
        solicitud->ruta[0] != '/') {
        return -1;
    }

    if (strcmp(solicitud->metodo, "GET") != 0) {
        return -2;
    }

    char *linea = salto + 2;

    while (linea < fin) {
        char *siguiente = strstr(linea, "\r\n");

        if (siguiente == NULL || siguiente > fin) {
            return -1;
        }

        *siguiente = '\0';

        char *separador = strchr(linea, ':');

        if (separador == NULL || separador == linea) {
            return -1;
        }

        *separador = '\0';
        char *valor = separador + 1;

        while (*valor == ' ' || *valor == '\t') {
            valor++;
        }

        if (strcasecmp(linea, "Host") == 0) {
            if (guardar_valor(solicitud->host, sizeof(solicitud->host), valor) == -1)
                return -1;
        } else if (strcasecmp(linea, "Connection") == 0) {
            if (guardar_valor(solicitud->conexion, sizeof(solicitud->conexion), valor) == -1)
                return -1;
        } else if (strcasecmp(linea, "User-Agent") == 0) {
            if (guardar_valor(solicitud->user_agent, sizeof(solicitud->user_agent), valor) == -1)
                return -1;
        }

        linea = siguiente + 2;
    }

    if (solicitud->host[0] == '\0') {
        return -1;
    }

    return 1;
}
