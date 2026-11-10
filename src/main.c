
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/epoll.h>
#include "server.h"
#include "http.h"
#include "files.h"
#include "mime.h"

typedef struct {
    int fd;
    int archivo;
    char datos[8192];
    size_t usados;
    char cabecera[512];
    int largo_cabecera;
    size_t cabecera_enviada;
    off_t tamano;
    off_t posicion;
    int persistente;
} Cliente;

static void cerrar_cliente(int epoll_fd, Cliente *cliente) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, cliente->fd, NULL);

    if (cliente->archivo != -1) {
        close(cliente->archivo);
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

static int preparar_respuesta(Cliente *cliente, int codigo, const char *ruta) {
    cliente->archivo = -1;
    cliente->tamano = 0;
    cliente->posicion = 0;
    cliente->cabecera_enviada = 0;

    const char *tipo = "text/plain";

    if (codigo == 200) {
        codigo = abrir_archivo(ruta, &cliente->archivo, &cliente->tamano);

        if (codigo == 200) {
            tipo = obtener_mime(strcmp(ruta, "/") == 0 ? "index.html" : ruta);
        }
    }

    cliente->largo_cabecera = generar_encabezado(
        cliente->cabecera, sizeof(cliente->cabecera), codigo,
        tipo, (size_t)cliente->tamano, cliente->persistente
    );

    return cliente->largo_cabecera;
}

static int enviar_respuesta(Cliente *cliente) {
    while (cliente->cabecera_enviada < (size_t)cliente->largo_cabecera) {
        ssize_t n = send(
            cliente->fd,
            cliente->cabecera + cliente->cabecera_enviada,
            (size_t)cliente->largo_cabecera - cliente->cabecera_enviada,
            MSG_NOSIGNAL
        );

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return 0;
        }

        if (n <= 0) {
            return -1;
        }

        cliente->cabecera_enviada += (size_t)n;
    }

    while (cliente->archivo != -1 && cliente->posicion < cliente->tamano) {
        ssize_t n = sendfile(
            cliente->fd, cliente->archivo,
            &cliente->posicion,
            (size_t)(cliente->tamano - cliente->posicion)
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

int main(void) {
    signal(SIGPIPE, SIG_IGN);

    int servidor = crear_socket();

    if (servidor == -1) {
        perror("Error al crear socket");
        return 1;
    }

    if (enlazar_socket(servidor) == -1) {
        perror("Error al enlazar socket");
        close(servidor);
        return 1;
    }

    if (escuchar_socket(servidor) == -1 ||
        configurar_no_bloqueante(servidor) == -1) {
        perror("Error al preparar socket");
        close(servidor);
        return 1;
    }

    int epoll_fd = epoll_create1(0);

    if (epoll_fd == -1) {
        perror("Error al crear epoll");
        close(servidor);
        return 1;
    }

    struct epoll_event evento = {0};
    struct epoll_event eventos[10];

    evento.events = EPOLLIN;
    evento.data.ptr = NULL;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, servidor, &evento) == -1) {
        perror("Error al registrar socket");
        close(epoll_fd);
        close(servidor);
        return 1;
    }

    printf("Servidor escuchando en el puerto 8080\n");
    fflush(stdout);

    while (1) {
        int listos = epoll_wait(epoll_fd, eventos, 10, -1);

        if (listos == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("Error en epoll_wait");
            break;
        }

        for (int i = 0; i < listos; i++) {
            if (eventos[i].data.ptr == NULL) {
                int fd = aceptar_cliente(servidor);

                if (fd == -1) {
                    continue;
                }

                if (configurar_no_bloqueante(fd) == -1) {
                    close(fd);
                    continue;
                }

                Cliente *cliente = calloc(1, sizeof(Cliente));

                if (cliente == NULL) {
                    close(fd);
                    continue;
                }

                cliente->fd = fd;
                cliente->archivo = -1;

                struct epoll_event nuevo = {0};
                nuevo.events = EPOLLIN | EPOLLRDHUP;
                nuevo.data.ptr = cliente;

                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &nuevo) == -1) {
                    close(fd);
                    free(cliente);
                    continue;
                }

                continue;
            }

            Cliente *cliente = eventos[i].data.ptr;

            if (eventos[i].events & (EPOLLERR | EPOLLHUP)) {
                cerrar_cliente(epoll_fd, cliente);
                continue;
            }

            if (eventos[i].events & EPOLLIN) {
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
                    continue;
                }

                cliente->usados += (size_t)n;

                SolicitudHTTP solicitud;
                int resultado = analizar_solicitud(
                    cliente->datos, cliente->usados, &solicitud
                );

                if (resultado == 0 && cliente->usados < sizeof(cliente->datos)) {
                    continue;
                }

                int codigo = resultado == 1 ? 200 : resultado == -2 ? 405 : 400;

                cliente->persistente = resultado == 1 &&
                    strcasecmp(solicitud.conexion, "close") != 0;

                const char *ruta = resultado == 1 ? solicitud.ruta : "/";

                if (preparar_respuesta(cliente, codigo, ruta) == -1 ||
                    cambiar_evento(epoll_fd, cliente, EPOLLOUT) == -1) {
                    cerrar_cliente(epoll_fd, cliente);
                    continue;
                }

                cliente->usados = 0;
            } else if (eventos[i].events & EPOLLOUT) {
                int resultado = enviar_respuesta(cliente);

                if (resultado == 0) {
                    continue;
                }

                if (resultado == -1 || !cliente->persistente) {
                    cerrar_cliente(epoll_fd, cliente);
                    continue;
                }

                if (cliente->archivo != -1) {
                    close(cliente->archivo);
                    cliente->archivo = -1;
                }

                if (cambiar_evento(epoll_fd, cliente, EPOLLIN) == -1) {
                    cerrar_cliente(epoll_fd, cliente);
                }
            }
        }
    }

    close(epoll_fd);
    close(servidor);

    return 1;
}

