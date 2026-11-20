#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/epoll.h>
#include "server.h"

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    int servidor = crear_socket();

    if (servidor == -1) {
        perror("Error al crear socket");
        return 1;}

    if (enlazar_socket(servidor) == -1) {
        perror("Error al enlazar socket");
        close(servidor);
        return 1;}

    if (escuchar_socket(servidor) == -1 ||
        configurar_no_bloqueante(servidor) == -1) {
        perror("Error al preparar socket");
        close(servidor);
        return 1;}

    int epoll_fd = epoll_create1(0);

    if (epoll_fd == -1) {
        perror("Error al crear epoll");
        close(servidor);
        return 1;}

    struct epoll_event evento = {0};
    struct epoll_event eventos[10];

    evento.events = EPOLLIN;
    evento.data.ptr = NULL;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, servidor, &evento) == -1) {
        perror("Error al registrar socket");
        close(epoll_fd);
        close(servidor);
        return 1;}

    printf("Servidor escuchando en el puerto 8080\n");
    fflush(stdout);

    while (1) {
        int listos = epoll_wait(epoll_fd, eventos, 10, -1);

        if (listos == -1) {
            if (errno == EINTR) {
                continue;}

            perror("Error en epoll_wait");
            break;}

        for (int i = 0; i < listos; i++) {
            if (eventos[i].data.ptr == NULL) {
                registrar_cliente(servidor, epoll_fd);
            } else {
                Cliente *cliente = eventos[i].data.ptr;
                atender_cliente(epoll_fd, cliente, eventos[i].events);
            }}
    }

    close(epoll_fd);
    close(servidor);
    return 1;
}
