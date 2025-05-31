# Compilación
- Se debe tener instalado `cmake`
- Ejecutar `cmake . && make`

# Monitoreo
## MacOS
- Para obtener el PID usar `htop`y buscar con `F3` el ejecutable `./server`
- Ejecutar `lldb -p <PID>`
  - Este comando hace attach de un nuevo hilo al proceso que recibe por parametro y lo bloquea para debug
  - Para dejarlo correr ejecutar `process continue`, aunque esto bloquea poder ejecutar los comandos de debug como `thread list`
  - Para frenar el proceso con el debugger devuelta usar `process interrupt`
- Dentro de `lldb` ejecutar `thread list`
  - Aca se podrá ver el `TID`, como MacOS no brinda una API desde C para obtener el `TID`, no se puede ver la correspondencia del `TID` desde el ejecutable para identificar a que conexión corresponde
- Salir de `lldb` con `quit`

## Linux
TBD