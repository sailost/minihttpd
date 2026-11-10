
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "files.h"

int abrir_archivo(const char *ruta, int *archivo, off_t *tamano) {
    char raiz[PATH_MAX];
    char ruta_completa[PATH_MAX];
    char ruta_real[PATH_MAX];

    if (ruta == NULL || archivo == NULL || tamano == NULL || ruta[0] != '/') {
        return 400;
    }

    if (strstr(ruta, "..") != NULL) {
        return 403;
    }

    if (realpath("www", raiz) == NULL) {
        return 500;
    }

    const char *nombre = strcmp(ruta, "/") == 0 ? "/index.html" : ruta;

    int longitud = snprintf(ruta_completa, sizeof(ruta_completa), "%s%s", raiz, nombre);

    if (longitud < 0 || (size_t)longitud >= sizeof(ruta_completa)) {
        return 400;
    }

    if (realpath(ruta_completa, ruta_real) == NULL) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return 404;
        }
        if (errno == EACCES) {
            return 403;
        }
        return 500;
    }

    size_t largo = strlen(raiz);

    if (strncmp(ruta_real, raiz, largo) != 0 || ruta_real[largo] != '/') {
        return 403;
    }

    int fd = open(ruta_real, O_RDONLY);
    if (fd == -1) {
        return errno == EACCES ? 403 : 500;
    }
    struct stat info;
    if (fstat(fd, &info) == -1) {
        close(fd);
        return 500;
    }
    if (!S_ISREG(info.st_mode)) {
        close(fd);
        return 403;
    }
    *archivo = fd;
    *tamano = info.st_size;

    return 200;
}
