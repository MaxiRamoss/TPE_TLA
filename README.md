[![✗](https://github.com/MaxiRamoss/TPE_TLA/actions/workflows/ci.yaml/badge.svg?branch=development)](https://github.com/MaxiRamoss/TPE_TLA/actions/workflows/ci.yaml)

# Vial

Vial es un lenguaje de dominio específico para describir y simular redes viales: intersecciones, calles, semáforos con fases y políticas adaptativas, rutas, flujos de vehículos y métricas. Este repositorio contiene su compilador, desarrollado con Flex y Bison sobre el proyecto base [Flex-Bison-Compiler](https://github.com/Alpha-Theta-Gamma-Mu/Flex-Bison-Compiler), para el Trabajo Práctico Especial de Autómatas, Teoría de Lenguajes y Compiladores (ITBA).

| Nombres y Apellidos      | Grupo |
| :----------------------- | :---: |
| Máximo Ramos             | G-139 |
| Nicolás Rodriguez Falcon | G-139 |
| Juan Cruz Procaccini     | G-139 |

* [Requisitos](#requisitos)
* [Compilar, correr y testear](#compilar-correr-y-testear)
* [Estado del Stage II](#estado-del-stage-ii)
* [El lenguaje](#el-lenguaje)
* [Tests](#tests)
* [Especificación](#especificación)

## Requisitos

Con Docker alcanza con:

* [Docker](https://www.docker.com/) con Docker Compose.

Sin Docker, hace falta un entorno Linux (los scripts usan GNU coreutils) con:

* GCC y Make.
* CMake 3.31 o superior.
* Bison 3.8 o superior (el Bison 2.3 que trae macOS no sirve).
* Flex 2.6 o superior.

## Compilar, correr y testear

### Con Docker Compose

Levantar el contenedor efímero de desarrollo, que monta el repositorio en su directorio de trabajo:

```bash
docker compose run --rm compiler
```

Dentro del contenedor, compilar desde cero (Bison, Flex, CMake y Make), compilar un programa Vial y correr los tests:

```bash
src/main/bash/build.sh
src/main/bash/run.sh src/test/c/accept/25-full-scenario.itba
src/main/bash/test.sh
```

Para salir del contenedor y liberar los recursos del cluster:

```bash
exit
docker compose down
```

También se puede ejecutar un comando puntual sin abrir una shell interactiva:

```bash
docker compose run --rm compiler bash -c "src/main/bash/build.sh && src/main/bash/test.sh"
```

### Sin Docker

Con las herramientas instaladas, los mismos scripts funcionan directamente desde la raíz del repositorio:

```bash
src/main/bash/build.sh
src/main/bash/run.sh src/test/c/accept/25-full-scenario.itba
src/main/bash/test.sh
```

### Configuración

El compilador lee las siguientes variables de entorno (Docker Compose también las toma de un archivo `.env`, ver `compose.yaml`):

| Nombre                | Default | Descripción                                                                                                                                   |
| :-------------------- | :-----: | :-------------------------------------------------------------------------------------------------------------------------------------------- |
| `ENVIRONMENT`         | `Local` | El nombre del entorno activo: `Local`, `Development` o `Production`.                                                                          |
| `LOG_IGNORED_LEXEMES` | `true`  | Si es `true`, registra en nivel `DEBUGGING` los lexemas ignorados por Flex (espacios y comentarios).                                          |
| `LOGGING_LEVEL`       | `ALL`   | El nivel mínimo que se muestra por consola. De menor a mayor: `ALL`, `DEBUGGING`, `INFORMATION`, `WARNING`, `ERROR` y `CRITICAL`.            |

El código de salida es `0` si el programa se acepta y distinto de `0` si se rechaza. Los errores léxicos y sintácticos se reportan por `stderr` con la línea donde ocurren; por ejemplo: `Syntax error at line 5: unexpected }, expecting phase or apply`.

## Estado del Stage II

El frontend está completo:

* **Análisis léxico** (Flex): palabras reservadas, unidades, identificadores con letras acentuadas y ñ, enteros, strings con interpolación `{ … }` y escapes, comentarios `//` y `/* … */`, y operadores `≤ ≥ ≠` además de `<= >= !=`.
* **Análisis sintáctico** (Bison): la gramática completa del lenguaje, sin conflictos S/R ni R/R.
* **AST**: un nodo por construcción, con destructores recursivos; el build compila con AddressSanitizer y no hay fugas de memoria.
* **Mensajes de error**: cada error léxico o sintáctico indica la línea y el token esperado.

El backend todavía no genera salida: si el programa es válido, la compilación termina con código `0` sin producir nada. El análisis semántico y la generación de código corresponden al Stage III.

## El lenguaje

Un programa Vial empieza con un encabezado `simulation` seguido de declaraciones, sin terminadores de sentencia. Las magnitudes llevan unidad (`m km s min h km/h m/s`), y los identificadores admiten letras acentuadas y ñ.

| Construcción       | Sintaxis                                                                                                                          |
| :----------------- | :-------------------------------------------------------------------------------------------------------------------------------- |
| Simulación         | `simulation Centro { duration 30 min }`                                                                                           |
| Constantes         | `duration VerdeBase = 30 s` (tipos: `integer boolean string duration distance speed`)                                             |
| Intersección       | `intersection CórdobaCallao at 370 m east of CórdobaPueyrredón label "Córdoba y Callao"` (o `at (0 m, 0 m)`)                      |
| Calle              | `road Callao from CallaoRivadavia to CórdobaCallao length 260 m limit 40 km/h label "Av. Callao"` (`length`, `limit` y `label` son opcionales) |
| Arreglos e iteración | `for i in 1 .. 4 { intersection Rivadavia[i] at 100 m east of Rivadavia[i - 1] label "Rivadavia {i}" }`                          |
| Semáforo           | `light Sem1 at CórdobaCallao { phase green for CórdobaOeste during VerdeBase  apply Adaptativa(CórdobaOeste, 15) }`                |
| Policy             | `policy Adaptativa(road principal, integer umbral) { if queue(principal) > umbral { extend principal by 10 s } else { keep } }`    |
| Log                | `log "Extendida {principal} a los {elapsed}"` (la interpolación vale en cualquier string, labels incluidos)                        |
| Ruta               | `route Corredor = CórdobaPueyrredón -> CórdobaCallao -> CórdobaMontevideo`                                                        |
| Flujo              | `flow HoraPico along Corredor { spawn 2 every 10 s from 0 min to 15 min }` (la ventana `from … to …` es opcional)                 |
| Métrica            | `metric ViajePromedio = average(trip.travel_time) on arrival`                                                                     |
| Expresiones        | `+ - * /`, `= != < <= > >=`, `and or not`, paréntesis, acceso a miembros (`trip.travel_time`) y llamadas (`queue(principal)`)     |
| Comentarios        | `// hasta el fin de línea` y `/* de bloque */`                                                                                    |

Como ejemplo completo, ver [`src/test/c/accept/25-full-scenario.itba`](src/test/c/accept/25-full-scenario.itba): un corredor sobre Córdoba con una transversal por Callao, un semáforo adaptativo, dos flujos y tres métricas.

## Tests

Cada test es un programa Vial en un archivo `NN-nombre.itba` dentro de `src/test/c/`:

| Carpeta          | Contenido                                                                                   | `test.sh`    |
| :--------------- | :------------------------------------------------------------------------------------------ | :----------- |
| `accept/`        | Programas que el compilador debe aceptar (código de salida `0`).                            | Los ejecuta. |
| `reject/`        | Programas que el compilador debe rechazar por un error léxico o sintáctico.                 | Los ejecuta. |
| `ignore/reject/` | Programas con un único error **semántico**, que el compilador todavía no detecta.           | Solo los lista. |

Los casos de `ignore/` son sintácticamente válidos, así que hoy el compilador los acepta. Cada uno lleva en su primera línea un comentario que explica el defecto (una calle hacia una intersección inexistente, una fase sobre una calle que no entra a la intersección, unidades incompatibles, etc.). Cuando el Stage III incorpore el análisis semántico, pasan a `reject/` y deben rechazarse.

`test.sh` termina con código de salida `0` solo si todos los tests de `accept/` y `reject/` dan el resultado esperado. La integración continua (`.github/workflows/ci.yaml`) corre `build.sh` y `test.sh` en cada push.

## Especificación

[`doc/Especificacion_Vial.pdf`](doc/Especificacion_Vial.pdf) es el documento de diseño del lenguaje entregado en el Stage I. La sintaxis implementada en este Stage II cambió según la devolución de la cátedra: unidades SI en lugar de ticks y celdas, posiciones obligatorias en metros, arreglos por declaraciones indexadas dentro de `for`, sin terminadores `;`, interpolación `{ … }` en strings, métricas definidas por el usuario y palabras reservadas en minúscula. Ante cualquier diferencia, manda la sintaxis que aceptan los tests de `src/test/c/accept/`.
