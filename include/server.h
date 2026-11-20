
#ifndef SERVER_H
#define SERVER_H

typedef struct Cliente Cliente;

int crear_socket(void);
int enlazar_socket(int servidor);
int escuchar_socket(int servidor);
int aceptar_cliente(int servidor);
int configurar_no_bloqueante(int fd);

int registrar_cliente(int servidor, int epoll_fd);
void atender_cliente(int epoll_fd, Cliente *cliente, unsigned int eventos);

#endif
