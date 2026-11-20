
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include "server.h"
#include "http.h"

struct Cliente {
    int fd;
    char datos[8192];
    size_t usados;
    RespuestaHTTP respuesta;
    int persistente;
};

int crear_socket(void) {
    return socket(AF_INET, SOCK_STREAM, 0);
}

int enlazar_socket(int servidor) {
    int opcion = 1;

    if (setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR,
                   &opcion, sizeof(opcion)) == -1) {
        return -1;
    }

    struct sockaddr_in direccion = {0};

    direccion.sin_family = AF_INET;
    direccion.sin_port = htons(8080);
    direccion.sin_addr.s_addr = INADDR_ANY;

    return bind(servidor, (struct sockaddr *)&direccion, sizeof(direccion));
}

int escuchar_socket(int servidor) {
    return listen(servidor, 10);
}

int aceptar_cliente(int servidor) {
    return accept(servidor, NULL, NULL);
}

int configurar_no_bloqueante(int fd) {
    int banderas = fcntl(fd, F_GETFL, 0);

    if (banderas == -1) {
        return -1;
    }

    return fcntl(fd, F_SETFL, banderas | O_NONBLOCK);
}

static void cerrar_cliente(int epoll_fd, Cliente *cliente) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cliente->fd, NULL);

    if (cliente->respuesta.archivo != -1) {
        close(cliente->respuesta.archivo);
    }

    close(cliente->fd);
    free(cliente);
}

static int cambiar_evento(int epoll_fd, Cliente *cliente, unsigned int eventos) {
    struct epoll_event evento = {0};

    evento.events = eventos | EPOLLRDHUP;
    evento.data.ptr = cliente;

    return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, cliente->fd, &evento);
}

static int enviar_respuesta(Cliente *cliente) {
    RespuestaHTTP *respuesta = &cliente->respuesta;

    while (respuesta->cabecera_enviada < (size_t)respuesta->largo_cabecera) {
        ssize_t n = send(
            cliente->fd,
            respuesta->cabecera + respuesta->cabecera_enviada,
            (size_t)respuesta->largo_cabecera - respuesta->cabecera_enviada,
            MSG_NOSIGNAL
        );

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return 0;
        }

        if (n <= 0) {
            return -1;
        }

        respuesta->cabecera_enviada += (size_t)n;
    }

    while (respuesta->archivo != -1 && respuesta->posicion < respuesta->tamano) {
        ssize_t n = sendfile(
            cliente->fd,
            respuesta->archivo,
            &respuesta->posicion,
            (size_t)(respuesta->tamano - respuesta->posicion)
        );

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return 0;
        }

        if (n <= 0) {
            return -1;
        }
    }

    return 1;
}

int registrar_cliente(int servidor, int epoll_fd) {
    int fd = aceptar_cliente(servidor);

    if (fd == -1) {
        return -1;
    }

    if (configurar_no_bloqueante(fd) == -1) {
        close(fd);
        return -1;
    }

    Cliente *cliente = calloc(1, sizeof(Cliente));

    if (cliente == NULL) {
        close(fd);
        return -1;
    }

    cliente->fd = fd;
    cliente->respuesta.archivo = -1;

    struct epoll_event evento = {0};
    evento.events = EPOLLIN | EPOLLRDHUP;
    evento.data.ptr = cliente;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &evento) == -1) {
        close(fd);
        free(cliente);
        return -1;
    }

    return 0;
}

void atender_cliente(int epoll_fd, Cliente *cliente, unsigned int eventos) {
    if (eventos & (EPOLLERR | EPOLLHUP)) {
        cerrar_cliente(epoll_fd, cliente);
        return;
    }

    if (eventos & EPOLLIN) {
        ssize_t n = recv(
            cliente->fd,
            cliente->datos + cliente->usados,
            sizeof(cliente->datos) - cliente->usados,
            0
        );

        if (n <= 0) {
            if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
                cerrar_cliente(epoll_fd, cliente);
            }
            return;
        }

        cliente->usados += (size_t)n;

        SolicitudHTTP solicitud;
        int resultado = analizar_solicitud(
            cliente->datos, cliente->usados, &solicitud
        );

        if (resultado == 0 && cliente->usados < sizeof(cliente->datos)) {
            return;
        }

        int codigo = resultado == 1 ? 200 : resultado == -2 ? 405 : 400;

        cliente->persistente = resultado == 1 &&
            strcasecmp(solicitud.conexion, "close") != 0;

        const char *ruta = resultado == 1 ? solicitud.ruta : "/";

        if (preparar_respuesta(&cliente->respuesta, codigo, ruta,
                              cliente->persistente) == -1 ||
            cambiar_evento(epoll_fd, cliente, EPOLLOUT) == -1) {
            cerrar_cliente(epoll_fd, cliente);
            return;
        }

        cliente->usados = 0;
    } else if (eventos & EPOLLOUT) {
        int resultado = enviar_respuesta(cliente);

        if (resultado == 0) {
            return;
        }

        if (resultado == -1 || !cliente->persistente) {
            cerrar_cliente(epoll_fd, cliente);
            return;
        }

        if (cliente->respuesta.archivo != -1) {
            close(cliente->respuesta.archivo);
            cliente->respuesta.archivo = -1;
        }

        if (cambiar_evento(epoll_fd, cliente, EPOLLIN) == -1) {
            cerrar_cliente(epoll_fd, cliente);
        }
    }
}

