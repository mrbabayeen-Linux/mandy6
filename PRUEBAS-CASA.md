# Notas de pruebas (casa)

Este árbol es una bifurcación privada de la continuación de Leonida con
**tres commits locales** encima de la punta del proyecto. Aquí pongo qué se
estuvo probando, con qué máquina, qué sirvió y qué queda por delante — es
para que alguien con **otra GPU** pueda correr estas pruebas y sumar una
medición distinta.

## Qué hay en este árbol (y qué NO)

- El código del port (cargador + capas de traducción) tal como lo publica
  el proyecto, más los cambios locales del final:
  - `cd9aa6b49` / `99bc01cf5`: integración del catálogo gráfico nativo
    (los PRs #1592 del proyecto): la fabrica `fspx_gfx/index.json` para que
    el slot 57 resuelva por identidad de programa en vez de compilar cada
    tubería desde cero.
  - `307868b8a`: los PRs #1851 (copia de imagen del anfitrión) más parches
    de casa: disfraz de placa por DXGI (`LEONIDA_ADAPTER_NAME` /
    `LEONIDA_ADAPTER_MEM_GB`), tope de lista de modos de pantalla
    (`LEONIDA_DISPLAY_MAX=WxH`) y llave de sonido en el envoltorio.
- **NO hay nada del juego**: ni archivos, ni ejecutable, ni shaders
  traducidos, ni cachés. Cada quien los construye de SU propia copia con
  `make_game.py` (ver abajo). La biblioteca de shaders traducidos queda
  siempre fuera del repositorio.

## Dónde se probó (se estuvo haciendo pruebas activas: ~27 corridas)

- Máquina: Ryzen 5 5600GT + **GTX 1660 SUPER (Turing, 6 GB)**, 32 GB RAM.
- Sistema: Nobara (GNU/Linux) + **Wine del sistema** (11.21) — el
  cargador `gta6rt` corre sobre Wine con Vulkan directo (SPIR-V). Nota
  importante medida 4 veces: **Proton NO pasa el relevo del splash** en
  la punta actual (muere a los ~7-9 s siempre); el vino del sistema sí.
- Estado al corte: récord de la casa **t≈385 s de vida / ~11.500 cuadros
  con mando DualSense**; splash, película, selector de perfil, páginas de
  primer arranque y carga de historia pasan. La pared actual (en
  NVIDIA/Turing) es la **compilación en vivo de tuberías gráficas**: el
  programa más pesado de cada lote compila 9-13 s en el driver y el
  tiempo serial acumulado agota la paciencia del vigilante del título.
  El catálogo resuelve QUÉ código es cada tubería, no el tiempo del
  compilador del driver — una GPU más nueva con compilador más rápido
  puede pasar mucho más lejos.

## Cómo reproducir

1. Paquete Xbox Series 1.0.2.1 propio + `game_scarlett_master.exe`
   (revisar suma de verificación; el ejecutable va DENTRO de la carpeta
   del paquete para el cargador).
2. Biblioteca de shaders y activos (horas):

   ```sh
   python3 -I make_game.py --game <paquete> --out <biblioteca> \
     --work <trabajo> --stages shaders,assets --wave64-emulate on --jobs 3
   ```

   `--wave64-emulate on` es correcto en placas NVIDIA (subgrupos de 32).
3. Etapa `pipelines` opcional (valida, y deja caliente lo precompilado);
   el precompilado útil para la corrida se hace después con el propio
   cargador (`--precompile-pipelines`).
4. Cargador: etapa `runtime` (clang-cl + kit Microsoft vía `xwin`; en
   Linux se compila apuntando al kit descargado).
5. Lanzar con el **vino del sistema** (no Proton) y la biblioteca apuntada.

## Llaves (variables) que sirvieron en esta máquina

Todas van prefijadas `LEONIDA_`; las doy con su valor:

- `D3D12=vulkan`, `SKIP_INT3=1` + trazado activado — base de toda corrida.
- Wine del sistema; `WINEDLLOVERRIDES="mscoree,mshtml=,winedbg=d"` (el
  depurador fuera, si no cuelga el cierre).
- `COMPUTE_WARMUP_LIVE_MAX_BYTES=131072` — el calentamiento grande
  sostenía el candado de envío y el ojo del título mataba; con esta
  llave la película corre a 30 fps.
- `SLOT31_ASYNC=1` — el remapeo de páginas sale del camino del liberto
  y el releaser respira (la pared de las vallas).
- `TITLE_MEMORY_MB=8192` (+ `TITLE_WINDOWS_MB=2048`, `GEOMETRY_MB=2048`)
  — el título veía la RAM del anfitrión y reservaba como PC de 32 GB;
  con 8 GB reserva como una consola chica real y baja mucho el pico.
- `TITLE_WATCHDOG=0` + `TITLE_WATCHDOG_KICK=2215c35,71b6338` — trampa y
  latido de la puerta de muerte del título (medidos contra el ejecutable
  1.0.2.1; si tu paquete es otro, las direcciones pueden cambiar).
- `DISPLAY_MAX=1280x720` — achica swapchain/present (achica también el
  pico de copias de texturas a 4K); los objetivos internos de render los
  fija el perfil del título, no la pantalla.
- `ARENA_MB=4095`, `STARTUP_CACHE_CHECKPOINTS=1`, caché de disco de
  shaders de NVIDIA a 20 G, y pre-carga de los .rpf livianos antes de
  largar (aunque el arranque ya deja de morder disco temprano).
- Skip list de cómputo: los programas pesados (≥2 MiB de SPIR-V, unos
  800 en el plan del cargador) se pueden dejar afuera con la lista
  generada por el propio cargador (`--list`). OJO: si saltás los
  programas que el título usa para el arranque del mundo, la pared cambia
  de forma — probar primero SIN saltos en una GPU nueva, y saltar solo lo
  que de verdad colgó en TU placa.
- Mando: DualSense funciona con prefijo de Wine caliente (conectar ANTES
  de largar); sin mando, `KEYBOARD_PAD=always` lo emula con teclado.

## Qué medir en una GPU distinta

- GPU + driver + sistema, y si es NVIDIA/AMD/Intel el camino SPIR-V del
  driver (AMD/RADV es más rápido de compilar: la corrida del proyecto ya
  pintó el mundos en una RX 7900 XTX/Linux).
- El tiempo de vida y el último bloque del trazo: la pared de esta casa
  es compilación serial; en otra placa puede aparecer antes otra cosa —
  eso mismo es el dato útil.
- Con la biblioteca del catálogo (`fspx_gfx/index.json`) el arranque
  acierta de catálogo y las fallas de identidad de tuberías bajan a cero.

## Aviso

Este árbol sigue las reglas del proyecto (nada del juego, nada de
descifrado, nada en línea). Es un árbol de PRUEBAS de una persona; no
reemplaza la punta del proyecto — para trabajar de verdad, leer el
proyecto de origen y sus reglas.
