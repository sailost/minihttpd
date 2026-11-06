
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include "server.h"

int main(void) {
    int servidor = crear_socket();

    if (servidor == -1) {
        perror("Error al crear el socket");
        return 1;
    }

    if (enlazar_socket(servidor) == -1) {
        perror("Error al enlazar el socket");
        close(servidor);
        return 1;
    }

    if (escuchar_socket(servidor) == -1) {
        perror("Error al escuchar conexiones");
        close(servidor);
        return 1;
    }

    if (configurar_no_bloqueante(servidor) == -1) {
        perror("Error al configurar socket");
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
    evento.data.fd = servidor;

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
            int fd = eventos[i].data.fd;

            if (fd == servidor) {
                int cliente = aceptar_cliente(servidor);

                if (cliente == -1) {
                    perror("Error al aceptar cliente");
                    continue;
                }

                if (configurar_no_bloqueante(cliente) == -1) {
                    perror("Error al configurar cliente");
                    close(cliente);
                    continue;
                }

                struct epoll_event nuevo = {0};
                nuevo.events = EPOLLIN | EPOLLRDHUP;
                nuevo.data.fd = cliente;

                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cliente, &nuevo) == -1) {
                    perror("Error al registrar cliente");
                    close(cliente);
                    continue;
                }

                printf("Cliente conectado correctamente\n");
            } else {
                char buffer[2048];
                ssize_t bytes = recv(fd, buffer, sizeof(buffer), 0);

                if (bytes > 0) {
                    printf("Recibidos %zd bytes\n", bytes);
                } else if (bytes == 0) {
                    close(fd);
                } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    perror("Error al recibir datos");
                    close(fd);
                } else if (eventos[i].events & (EPOLLHUP | EPOLLRDHUP)) {
                    close(fd);
                }
            }
        }
    }

    close(epoll_fd);
    close(servidor);

    return 1;
}
