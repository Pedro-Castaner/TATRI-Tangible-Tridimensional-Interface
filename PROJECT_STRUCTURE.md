# Mapa del proyecto para IA
Guía rápida de la app openFrameworks TUI3D para dar contexto a modelos de lenguaje.

## Carpetas
- `src/` código principal del app.
- `bin/data/` datos en tiempo de ejecución (vacío salvo `.gitkeep`).
- `.vscode/` configuración de workspace.
- Archivos de build: `Makefile`, `config.make`, `addons.make`, `openFrameworks-Info.plist`.

## Árbol de `src`
```
src/
├─ main.cpp              // arranque estándar de openFrameworks
├─ ofApp.h / ofApp.cpp   // ciclo de vida, cámara, luces, render y ruteo de herramientas
├─ Config.hpp            // constantes de cámara, TUIO, UI y tolerancias
├─ Shape3D.(hpp|cpp)     // mallas primitivas (box, cilindro, cono, esfera), materiales y transformaciones
├─ Token.(hpp|cpp)       // definición de tokens físicos, clasificación y helpers geométricos
├─ TokenManager.(hpp|cpp)// conexión TUIO, clustering y asignación de tokens a herramientas
├─ Tools.(hpp|cpp)       // jerarquía de herramientas (rotar, escalar, trasladar, spawn, selección, color, utilidades)
├─ TooltipManager.(hpp|cpp)// tooltips contextuales con fade
└─ TransformFeedback3D.(hpp|cpp)// HUD 3D con texto/brillo para feedback de transformaciones
```

## Flujo general
- `main.cpp` crea `ofApp`.
- `ofApp` inicializa `Shape3D` por defecto, cámara orbital, luces, y registra herramientas (`Tools`) ligadas a tokens TUIO.
- `TokenManager` recibe puntos TUIO, los clasifica en tokens y llama a la herramienta adecuada; mantiene `tokenTools` y sincroniza estados.
- Cada `Tool` implementa `update/draw` y usa `TooltipManager` para overlays de ayuda; algunas cargan fuentes compartidas.
- `TransformFeedback3D` muestra indicaciones HUD (texto y brillo) sobre el espacio 3D tras acciones de transformación.

## Notas rápidas para prompts
- Las constantes de interacción y límites están en `Config.hpp`.
- Las primitivas 3D y sus propiedades viven en `Shape3D`.
- Los tokens físicos se definen en `Token.*` y se manejan en `TokenManager.*`.
- Si necesitas sumar una nueva herramienta, hereda de `Tool` en `Tools.*` y enlázala en `ofApp::setup()` y en el ruteo de eventos TUIO.
