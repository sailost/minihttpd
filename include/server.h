
#ifndef SERVER_H
#define SERVER_H

int crear_socket(void);
int enlazar_socket(int servidor);
int escuchar_socket(int servidor);
int aceptar_cliente(int servidor);
int configurar_no_bloqueante(int fd);

#endif
