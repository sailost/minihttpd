
#include <sys/socket.h>
#include <netinet/in.h>
#include <stddef.h>
#include <fcntl.h>

int crear_socket(void) {
    return socket(AF_INET, SOCK_STREAM, 0);
}

int enlazar_socket(int servidor) {
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
