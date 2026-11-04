
#include <stdio.h>
#include <unistd.h>
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

    printf("Servidor escuchando en el puerto 8080\n");
    fflush(stdout);

    int cliente = aceptar_cliente(servidor);

    if (cliente == -1) {
        perror("Error al aceptar cliente");
        close(servidor);
        return 1;
    }

    printf("Cliente conectado correctamente\n");

    close(cliente);
    close(servidor);

    return 0;
}

