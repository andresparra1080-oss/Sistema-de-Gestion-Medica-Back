# Pruebas de gestión de usuarios

El ejecutable `gestion-usuario-tests` valida la lógica de negocio y persistencia
de `GET /api/usuarios` y `POST /api/usuarios` a través de
`GestionUsuarioService` y `GestionUsuarioRepository`. Utiliza una base SQLite
temporal con roles y usuarios de prueba; no modifica `heartmed_db.sqlite3`.

## Casos cubiertos

- Listado permitido a los usuarios 1, 2, 3 y 4.
- Listado denegado a IDs inválidos y al usuario 5.
- Inclusión de cuentas inactivas en la lista, ordenada por ID.
- Creación permitida al usuario 1 y denegada a los demás usuarios.
- Validación de nombre, correo, contraseña y rol.
- Normalización de espacios y rechazo de correos duplicados sin distinguir
  mayúsculas y minúsculas.
- Persistencia del hash de contraseña, respuesta del usuario activo y registro
  de auditoría.
- Reversión de la creación si no se puede escribir la auditoría.

## Ejecución

Desde la raíz del repositorio, configure CMake, compile el objetivo de pruebas
y ejecute CTest:

```powershell
cmake -S . -B build
cmake --build build --config Release --target gestion-usuario-tests
ctest --test-dir build -C Release --output-on-failure
```

También se puede ejecutar directamente el binario de pruebas:

```powershell
.\build\Release\gestion-usuario-tests.exe
```

Cada caso muestra `[OK]` o `[FALLO]`; un caso fallido o un error de preparación
devuelve un código de salida distinto de cero.

## Alcance

Estas son pruebas automatizadas del servicio y repositorio sobre SQLite. No
levantan un servidor HTTP ni prueban el formato de las respuestas de Crow;
para las rutas se debe verificar adicionalmente el contrato documentado en el
README: `GET /api/usuarios` y `POST /api/usuarios` requieren
`Authorization: Bearer <token>`.
