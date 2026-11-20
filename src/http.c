
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "http.h"
#include "files.h"
#include "mime.h"

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

        char *final = valor + strlen(valor);

        while (final > valor &&
               (final[-1] == ' ' || final[-1] == '\t')) {
            *--final = '\0';
        }

        if (strcasecmp(linea, "Host") == 0) {
            if (guardar_valor(solicitud->host, sizeof(solicitud->host), valor) == -1)
                return -1;
        } else if (strcasecmp(linea, "Connection") == 0) {
            if (guardar_valor(solicitud->conexion, sizeof(solicitud->conexion), valor) == -1)
                return -1;

            char *parte = strtok(solicitud->conexion, ", \t");
            while (parte != NULL && strcasecmp(parte, "close") != 0)
                parte = strtok(NULL, ", \t");
            if (parte != NULL)
                memcpy(solicitud->conexion, "close", 6);
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

const char *mensaje_estado(int codigo) {
    switch (codigo) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        default: return "Internal Server Error";
    }
}

int generar_encabezado(char *buffer, size_t capacidad, int codigo,
                       const char *tipo, size_t tamano, int persistente) {
    if (buffer == NULL || tipo == NULL || capacidad == 0) {
        return -1;
    }

    int longitud = snprintf(buffer, capacidad,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n"
        "\r\n",
        codigo, mensaje_estado(codigo), tipo, tamano,
        persistente ? "keep-alive" : "close");

    if (longitud < 0 || (size_t)longitud >= capacidad) {
        return -1;
    }

    return longitud;
}

int preparar_respuesta(RespuestaHTTP *respuesta, int codigo,
                      const char *ruta, int persistente) {
    if (respuesta == NULL) {
        return -1;
    }

    respuesta->archivo = -1;
    respuesta->tamano = 0;
    respuesta->posicion = 0;
    respuesta->cabecera_enviada = 0;

    const char *tipo = "text/plain";

    if (codigo == 200) {
        codigo = abrir_archivo(ruta, &respuesta->archivo, &respuesta->tamano);

        if (codigo == 200) {
            tipo = obtener_mime(strcmp(ruta, "/") == 0 ? "index.html" : ruta);
        }
    }

    respuesta->largo_cabecera = generar_encabezado(
        respuesta->cabecera, sizeof(respuesta->cabecera),
        codigo, tipo, (size_t)respuesta->tamano, persistente
    );

    if (respuesta->largo_cabecera == -1 && respuesta->archivo != -1) {
        close(respuesta->archivo);
        respuesta->archivo = -1;
    }

    return respuesta->largo_cabecera;
}
