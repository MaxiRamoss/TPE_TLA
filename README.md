[![✗](https://github.com/MaxiRamoss/TPE_TLA/actions/workflows/ci.yaml/badge.svg?branch=development)](https://github.com/MaxiRamoss/TPE_TLA/actions/workflows/ci.yaml)

# Vial

Vial es un lenguaje de dominio específico para describir y simular redes viales: intersecciones, calles, semáforos con fases y políticas adaptativas, rutas, flujos de vehículos y métricas. Este repositorio contiene su compilador, que traduce un programa Vial a un HTML autocontenido con la simulación de la red. Está desarrollado con Flex y Bison sobre el proyecto base [Flex-Bison-Compiler](https://github.com/Alpha-Theta-Gamma-Mu/Flex-Bison-Compiler), para el Trabajo Práctico Especial de Autómatas, Teoría de Lenguajes y Compiladores (ITBA).

| Nombres y Apellidos      | Grupo |
| :----------------------- | :---: |
| Máximo Ramos             | G-139 |
| Nicolás Rodriguez Falcon | G-139 |
| Juan Cruz Procaccini     | G-139 |

* [Requisitos](#requisitos)
* [Compilar, correr y testear](#compilar-correr-y-testear)
* [Estado del Stage III](#estado-del-stage-iii)
* [El lenguaje](#el-lenguaje)
* [Validaciones semánticas](#validaciones-semánticas)
* [Salida generada](#salida-generada)
* [Arquitectura](#arquitectura)
* [Tests](#tests)
* [Documentación](#documentación)

## Requisitos

Con Docker alcanza con:

* [Docker](https://www.docker.com/) con Docker Compose.

Sin Docker, hace falta un entorno Linux (los scripts usan GNU coreutils) con:

* GCC y Make.
* CMake 3.31 o superior.
* Bison 3.8 o superior (el Bison 2.3 que trae macOS no sirve).
* Flex 2.6 o superior.

Para ver la salida alcanza con cualquier browser moderno.

**Dependencias externas: ninguna.** El compilador no usa librerías de C fuera de la biblioteca estándar, y el HTML generado no usa librerías de JavaScript ni recursos de una CDN: funciona sin conexión.

## Compilar, correr y testear

### Con Docker Compose

Levantar el contenedor efímero de desarrollo, que monta el repositorio en su directorio de trabajo:

```bash
docker compose run --rm compiler
```

Dentro del contenedor, compilar desde cero (Bison, Flex, CMake y Make), compilar un programa Vial y correr los tests:

```bash
src/main/bash/build.sh
src/main/bash/run.sh src/test/c/accept/25-full-scenario.itba > salida.html
src/main/bash/test.sh
```

`run.sh` recibe el programa y escribe el HTML por `stdout`. Como el contenedor monta el repositorio, `salida.html` también queda en la raíz del repositorio de la máquina anfitriona. Se abre directamente en cualquier browser moderno (Chrome, Firefox, Safari o Edge), sin conexión ni servidor.

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

La compilación nativa requiere Linux, GCC con soporte de AddressSanitizer, las herramientas GNU que usa `build.sh` y Bison 3.8 o superior. En macOS, y en general fuera de Linux, hay que usar Docker Compose.

Con las herramientas instaladas, los mismos scripts funcionan directamente desde la raíz del repositorio:

```bash
src/main/bash/build.sh
src/main/bash/run.sh src/test/c/accept/25-full-scenario.itba > salida.html
src/main/bash/test.sh
```

### Configuración

El compilador lee las siguientes variables de entorno (Docker Compose también las toma de un archivo `.env`, ver `compose.yaml`):

| Nombre                | Default | Descripción                                                                                                                                   |
| :-------------------- | :-----: | :-------------------------------------------------------------------------------------------------------------------------------------------- |
| `LOG_IGNORED_LEXEMES` | `true`  | Si es `true`, registra en nivel `DEBUGGING` los lexemas ignorados por Flex (espacios y comentarios).                                          |
| `LOGGING_LEVEL`       | `ALL` / `INFORMATION` | El nivel mínimo que se muestra por consola. De menor a mayor: `ALL`, `DEBUGGING`, `INFORMATION`, `WARNING`, `ERROR` y `CRITICAL`. Vale `ALL` con Docker Compose e `INFORMATION` al ejecutar el binario directamente. |
| `VIAL_RUNTIME_PATH`   | `src/main/runtime` | El directorio de `viewer.html` y `runtime.js`, relativo al directorio de trabajo. Los scripts corren desde la raíz del repositorio, así que el default sirve sin configurar nada. |

Los logs, de cualquier nivel, van a `stderr`. `stdout` es solo para el HTML generado, así que `> salida.html` nunca captura un log.

El código de salida es `0` si el programa se acepta, y en ese caso el HTML sale por `stdout`. Si se rechaza, el código es distinto de `0` y no se emite nada por `stdout`. Cada error se reporta por `stderr` con la línea donde ocurre; por ejemplo:

```text
Syntax error at line 5: unexpected }, expecting phase or apply
Semantic error at line 7: road "X" references road "AB" where an intersection is expected
```

## Estado del Stage III

El compilador está completo: acepta un programa Vial válido y emite su simulación como un HTML autocontenido.

**Frontend.** El análisis léxico (Flex, en modo push) reconoce palabras reservadas, unidades, identificadores con letras acentuadas y ñ, enteros, strings con interpolación `{ … }` y escapes, comentarios y los operadores `≤ ≥ ≠` además de `<= >= !=`. La gramática de Bison no tiene conflictos S/R ni R/R. El AST guarda la línea de cada nodo, y cada error léxico o sintáctico indica la línea y el token esperado.

**Análisis semántico.** Recorre las declaraciones una sola vez, en orden, con una tabla de símbolos con scopes (global, `for` y `policy`). Desenrolla cada `for` (loop unrolling), pliega las constantes con aritmética entera exacta, chequea tipos y unidades, y valida la red vial, las rutas, los flujos y los semáforos. Reporta cada error con su línea y, si no hay errores, arma el modelo resuelto del programa.

**Generación de código.** A partir del modelo, emite por `stdout` un HTML con los datos del programa en JSON y con las policies y las métricas traducidas a JavaScript, insertados en el runtime fijo.

**Runtime.** El motor de simulación y el visor están escritos en JavaScript sin dependencias y versionados en `src/main/runtime/`. El generador los inserta sin cambios en cada HTML.

El build compila con AddressSanitizer y no hay fugas de memoria, tampoco en los programas rechazados.

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

## Validaciones semánticas

Cada error semántico se reporta con el formato `Semantic error at line N: <mensaje>`. El compilador reporta cada error del programa, no solo el primero. Una expresión con un error no produce más errores en cascada, y un mensaje idéntico en la misma línea (por ejemplo, el que se repite en cada iteración de un `for`) se reporta una sola vez.

Cada símbolo se declara antes de usarse, y no hay shadowing. Las reglas que se validan son:

* Identificador duplicado, incluidos los que genera el desenrollado de un `for` y los nombres reservados de los builtins.
* Referencia a un nombre no declarado o declarado más adelante, o a un índice inexistente de un arreglo.
* Referencia a un símbolo de otra clase (una calle donde se espera una intersección, por ejemplo), un nombre usado como escalar y como arreglo, o un arreglo con elementos de distinta clase.
* Tipos o unidades incompatibles en una operación, en el valor de una constante o en un campo de una declaración.
* División por cero o desborde de entero al plegar constantes, al convertir unidades o al calcular posiciones y longitudes.
* Builtin fuera de su contexto: `queue`, `occupancy` y `elapsed` solo valen en policies y métricas, y `trip` solo en métricas `on arrival`.
* Magnitud no positiva (`length` explícito, `during`, `limit`, `spawn`, `every` y la duración de la simulación), o calle con el mismo origen y destino.
* Dos intersecciones en la misma posición.
* Tramo de ruta sin calle, o con más de una calle en ese sentido.
* Ventana de flujo con `from` mayor o igual que `to`.
* Fase que habilita una calle que no entra a la intersección de su semáforo.
* Calle que entra a una intersección con semáforo y no aparece en ninguna fase.
* Más de un semáforo en la misma intersección.
* `apply` con una cantidad de argumentos o un tipo de argumento que no coincide con los parámetros de la policy.
* Llamada a una función desconocida, policy usada como expresión, o agregador fuera de una métrica.
* Miembro desconocido: el único es `trip.travel_time`.
* Agregador desconocido, o con una cantidad o un tipo de argumentos incorrectos.
* Evento desconocido: los únicos son `tick` y `arrival`.

No son errores: una policy que no se aplica, un parámetro sin usar, una intersección aislada y un `extend` sobre una calle que la fase vigente no habilita (no tiene efecto). Una ventana de flujo que termina después de la simulación se trunca.

## Salida generada

El HTML generado es un único archivo con cuatro partes:

| Parte         | Origen   | Contenido                                                                                                                            |
| :------------ | :------- | :----------------------------------------------------------------------------------------------------------------------------------- |
| `vial-config` | Generada | Un JSON con las entidades del programa ya resueltas, en orden de declaración. Las magnitudes van en m, s y m/h.                       |
| `VIAL_LOGIC`  | Generada | Las policies como funciones de JavaScript y la expresión de cada métrica. Las constantes y las variables de `for` ya van plegadas. |
| `runtime.js`  | Fija     | El motor de simulación, en funciones sin DOM, y el visor.                                                                            |
| `viewer.html` | Fija     | El markup y el CSS del visor, con los marcadores donde el generador inserta las otras tres partes.                                  |

Las partes fijas son las de [`src/main/runtime/`](src/main/runtime/) y son iguales para cualquier programa. El generador nunca emite código de interfaz: solo los datos del programa y su lógica.

La simulación avanza en ticks de 1 s y divide cada calle en celdas de 7,5 m. La red es localmente unidimensional: cada calle es una secuencia de celdas y las intersecciones son puntos de transferencia entre calles. La simulación es determinística: no hay azar, y en los conflictos gana el orden de declaración (las calles al cruzar y los flujos al ingresar).

El visor muestra el mapa con los vehículos en movimiento y detenidos, la fase de cada semáforo, las métricas del programa, la ocupación de cada calle, los flujos y una traza con los cambios de fase, las llegadas y los `log` de las policies. Los botones **Pausar**, **Paso** y **Reinicio** controlan la ejecución, y el selector de velocidad es un multiplicador de tiempo: ×1, ×2, ×5, ×10 (el valor inicial), ×30 o ×60 segundos simulados por segundo real.

El botón **Exportar JSON** baja `<id>-metricas.json` con el valor de cada métrica en el tiempo simulado hasta ese momento (`elapsed`, en s). Para `accept/25`, al terminar la simulación:

```json
{
  "simulation": { "id": "Centro", "label": null, "duration": 1800 },
  "elapsed": 1800,
  "metrics": [
    { "id": "Completados", "label": null, "aggregator": "count", "event": "arrival", "value": 397, "unit": null },
    { "id": "ViajePromedio", "label": null, "aggregator": "average", "event": "arrival", "value": 66.57682619647355, "unit": "s" },
    { "id": "PicoCórdobaOeste", "label": null, "aggregator": "maximum", "event": "tick", "value": 24, "unit": null }
  ]
}
```

Cada `value` va en la unidad de `unit`: `s`, `m` o `km/h` (una velocidad puede tener decimales), y `unit` es `null` para `count` y para los enteros. `average` da un número real. Si no hubo eventos, `count` y `sum` valen `0`, y `average`, `minimum` y `maximum` valen `null`.

## Arquitectura

```text
stdin → Flex → Bison → AST → análisis semántico → modelo → generador → HTML por stdout
```

| Carpeta                                  | Contenido                                                                                                                  |
| :--------------------------------------- | :------------------------------------------------------------------------------------------------------------------------- |
| `src/main/c/frontend/`                   | El lexer (Flex, en modo push), la gramática (Bison) y el AST.                                                              |
| `src/main/c/backend/semantic-analysis/`  | La tabla de símbolos con sus scopes, el desenrollado de los `for`, el plegado de constantes, el chequeo de tipos de las policies y las métricas, y las validaciones de la red. |
| `src/main/c/backend/model/`              | El modelo resuelto: las entidades del programa con sus valores ya plegados y en unidades base, listo para el generador.    |
| `src/main/c/backend/code-generation/`    | El generador: traduce el modelo a `vial-config` y `VIAL_LOGIC`, y los inserta en el runtime.                               |
| `src/main/runtime/`                      | El runtime fijo: `runtime.js` (motor y visor) y `viewer.html` (markup y CSS). No lo genera el compilador: el generador solo lo inserta. |

## Tests

Cada test es un programa Vial en un archivo `NN-nombre.itba` dentro de `src/test/c/`:

| Carpeta   | Contenido                                                                                                                       |
| :-------- | :------------------------------------------------------------------------------------------------------------------------------ |
| `accept/` | 30 programas válidos, que el compilador acepta (código de salida `0`) y para los que emite HTML.                                 |
| `reject/` | 40 programas con un único defecto, que el compilador rechaza: del 01 al 15, léxicos y sintácticos, y del 16 al 40, semánticos. Los semánticos (16 a 40) llevan en la primera línea un comentario que explica su defecto. |

`test.sh` ejecuta cada archivo de `accept/` y de `reject/`, mira solo el código de salida y termina con `0` únicamente si cada test da el resultado esperado. La carpeta `ignore/` ya no existe, pero `test.sh` sigue imprimiendo la sección de tests ignorados, vacía, porque el script es el del proyecto base.

La integración continua (`.github/workflows/ci.yaml`) corre `build.sh` y `test.sh` en cada push.

## Documentación

* [`doc/Informe_Vial.pdf`](doc/Informe_Vial.pdf) es el informe final del trabajo práctico.
* [`doc/Especificacion_Vial.pdf`](doc/Especificacion_Vial.pdf) es el documento de diseño del lenguaje entregado en el Stage I. La sintaxis implementada cambió según la devolución de la cátedra: unidades SI en lugar de ticks y celdas, posiciones obligatorias en metros, arreglos por declaraciones indexadas dentro de `for`, sin terminadores `;`, interpolación `{ … }` en strings, métricas definidas por el usuario y palabras reservadas en minúscula. Ante cualquier diferencia, manda la sintaxis que aceptan los tests de `src/test/c/accept/`.
