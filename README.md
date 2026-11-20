# MiniHTTPd

MiniHTTPd es un servidor HTTP/1.1 desarrollado en C para Linux. Permite atender solicitudes mediante sockets TCP, procesar peticiones GET y servir archivos estaticos como HTML, CSS e imagenes. Para manejar multiples conexiones utiliza epoll junto con sockets no bloqueantes.

## Caracteristicas implementadas

- Servidor TCP con IPv4 en el puerto 8080.
- Concurrencia mediante epoll y sockets no bloqueantes.
- Conexiones persistentes mediante keep-alive.
- Procesamiento de solicitudes HTTP/1.1 con el metodo GET.
- Analisis de los encabezados Host, Connection y User-Agent.
- Acceso a archivos estaticos desde la carpeta www.
- Identificacion de tipos MIME para HTML, CSS, JavaScript, PNG y JPG.
- Respuestas HTTP con los codigos 200, 400, 403, 404, 405 y 500.
- Validaciones de seguridad contra Directory Traversal, Buffer Overflow, metodos no permitidos y solicitudes demasiado grandes.

## Estructura del proyecto

```text
minihttpd/
├── Makefile
├── README.md
├── include/
│   ├── http.h
│   ├── server.h
│   ├── mime.h
│   └── files.h
├── src/
│   ├── main.c
│   ├── server.c
│   ├── http.c
│   ├── mime.c
│   └── files.c
└── www/
    ├── index.html
    ├── style.css
    └── image.png
```

## Descripcion de los modulos

**main.c:** Inicializa el servidor y contiene el bucle principal de eventos con epoll.

**server.c:** Maneja los sockets TCP, las conexiones de los clientes, la configuracion no bloqueante y el envio de respuestas.

**http.c:** Realiza el parsing de las solicitudes HTTP y prepara las respuestas.

**files.c:** Permite acceder a los archivos de la carpeta www y valida las rutas solicitadas.

**mime.c:** Identifica el tipo MIME de los archivos segun su extension.

## Compilacion y ejecucion

El proyecto requiere Linux, GCC, Make y Git. Se puede ejecutar en Linux nativo, una maquina virtual con Linux o Windows mediante WSL Ubuntu.

En Windows con WSL, primero abrir PowerShell y ejecutar:

```powershell
wsl -d Ubuntu
```

En Linux nativo o maquina virtual, abrir directamente una terminal.

Una vez dentro de Linux, descargar el repositorio:

```bash
git clone https://github.com/sailost/minihttpd.git
cd minihttpd
```

Compilar el proyecto:

```bash
make
```

Ejecutar el servidor:

```bash
./minihttpd
```

Abrir un navegador e ingresar:

http://localhost:8080

En WSL se puede utilizar el navegador de Windows. En una maquina virtual se recomienda abrir el navegador dentro de Linux, ya que el acceso desde Windows puede requerir configuracion de red.

Para detener el servidor, presionar `Ctrl + C` en la terminal.