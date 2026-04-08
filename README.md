# Trabajo Práctico 1 - Sistema de Archivos FAT12

**Materia:** Sistemas Operativos y Redes II.
**Grupo:** 2.
**Integrantes:** Ezequiel Ravignani, Carlos Caballero, Cristian Yoel Garay, Juan Farias y Pablo Igei Nakagawa.

## Archivos del proyecto

- **Makefile** - Compilación automatizada.
- **read_boot.c** - Lee el sector de arranque y la tabla de particiones.
- **read_mbr.c** - Lee y muestra información del Master Boot Record.
- **read_root.c** -  Lista archivos del directorio raíz y muestra su contenido.
- **recuperar_archivo.c** - Recupera archivos borrados que sean recuperables.
- **test.img** -  Imagen de disco FAT12.

## Compilación y ejecución
**Dentro de la terminal de linux:**
- Para compilar todos los programas, utilizar el comando "make all".
 - Luego para la ejecución utilizar ./nombre_de_archivo. Ejemplo: ./read_root
- Para borrar los ejecutables de nuestros archivos utilizar el comando "make clean".
