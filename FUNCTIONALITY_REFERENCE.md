# Referencia rápida de funcionalidades (TUI3D)
Resumen de comportamiento para consultas futuras de IA.

## Núcleo y ciclo de vida
- `src/main.cpp`: arranca `ofApp`.
- `src/ofApp.cpp`: `setup()` inicializa figura por defecto, cámara orbital, luces 6 puntos, fondo gradiente, `TransformFeedback3D`, `TokenManager` y herramientas (`RotationTool`, `AxisSelectorTool` + `tokenTools` F/G/H/B/J/E). Carga fuentes compartidas para menús.
- `update()`: actualiza `TokenManager`, muestra/oculta tooltips por token, enruta puntos TUIO a cada herramienta según ID, aplica transformaciones a la `Shape3D` activa, crea/elimina objetos (spawn/delete), undo/reset, y mantiene feedback HUD. Gestiona fades de menús y tooltips, reflow de tooltips y estado de tokens retirados.
- `draw()`: pinta fondo, puntos TUIO y tokens, escena 3D con grilla, gizmos y luces, sombras planas, objetos con color individual y selección, feedback 3D flotante, menús UI de herramientas, HUD de texto, cubo de navegación y tooltips.
- Otros helpers en `ofApp.cpp`: `setupLights()`, `setupGradientBackground()`, `drawAxisGizmo()`, `updateCameraWithToken()` (Token A), `reset()`, snapshot/undo (`saveSnapshot()`, `performUndo()`, `clearUndoHistory()`), colisiones y posicionamiento inteligente (`getBoundingBox()`, `checkCollision()`, `findNearestValidPosition()`), `drawNavigationCube()`.

## Datos y escena
- `src/Shape3D.(hpp|cpp)`: primitivas Box/Cylinder/Cone/Sphere con alta resolución; transformaciones por posición/escala/cuaternión; gizmos de rotación/traslación/escala adaptativos; wireframe para selección; color por objeto; reset de transform.
- `src/TransformFeedback3D.(hpp|cpp)`: HUD 3D con textos flotantes (rotación/traslación/escala) con billboard y glow; calcula posiciones cercanas al gizmo según eje activo; animación fade (in/visible/out).
- `src/Config.hpp`: constantes de cámara orbital, TUIO, UI (menús, grilla, tolerancias de clasificación, etc.).

## Entrada TUIO y tokens físicos
- `src/TokenManager.(hpp|cpp)`: conecta TUIO (`ofxTuioUdpReceiver` en puerto 3333), mantiene `allTouches` con sessionID y asignaciones persistentes a tokens, clustering de puntos libres (`MultiToken` thresholds), detección de nuevos tokens (mín. 3 puntos) hasta `MAX_TOKENS`. Mantiene `tokensBeingRemoved` para permitir fade de UI. Convierte coordenadas normalizadas a pantalla, reindexa asignaciones tras eliminar tokens.
- `src/Token.(hpp|cpp)`: snapshot “locked” de puntos para clasificar geometría; clasificación adaptativa por lados/ángulos (A..L) con tolerancias en `Config`; soporta puntos con Session IDs; dibuja puntos y etiquetas de ID.

## Herramientas y menús (token → acción)
- Definiciones en `src/Tools.(hpp|cpp)`. Todas heredan de `Tool` (estado de menú con fade in/out, delay y alpha). Menús se posicionan con `clampMenuPosition` para que siempre queden en pantalla. Tooltips en `getTooltipInfo()`.
- **Token A: Cámara orbital** (`ofApp::updateCameraWithToken`): token mueve cámara en azimut/elevación relativo al centro; zoom con rotación del token; suavizado, límites de radio y elevación, detección de saltos.
- **Token C: Rotación** (`RotationTool` + `AxisSelectorTool`):
  - `AxisSelectorTool`: bloquea puntos iniciales, menú radial X/Y/Z con fade; selección por tocar botones.
  - `RotationTool`: usa 2 puntos para ángulo, acumula delta por eje y aplica cuaternión global (`applyRotation`) al objeto activo. HUD mantiene rotationX/Y/Z para compatibilidad.
- **Token F: Escala** (`ScaleTool`):
  - Menú de rectángulos redondeados (X/Y/Z/ALL) + barra de valor; selección por toque.
  - Desplazamiento horizontal desde centro del token → factor de escala (sensibilidad configurable); per-axis factors sobre `initialScale`; límites 0.1x–5x; escala uniforme o por eje.
- **Token G: Traslación** (`TranslateTool`):
  - Menú de botones rectangulares (X/Z horizontales, Y vertical); selección por toque.
  - Desplazamiento relativo al centro del botón seleccionado; sensibilidad configurable; Y invierte signo para coord. pantalla; actualiza `position` en 3D.
- **Token H: Crear objeto** (`SpawnTool`):
  - Menú radial semicircular con iconos 3D Box/Cyl/Cone/Sphere; selección táctil.
  - Al retirar token y si hubo selección, crea nueva `Shape3D` con `findNearestValidPosition` (sin colisiones), la hace activa.
- **Token B: Color** (`ColorTool`):
  - Bloquea triángulo, identifica vértice de 20° (pos o Session ID), rueda cromática grande; indicador angular fijo al centro inicial.
  - Rotar token cambia hue (0–255) con detección de saltos; aplica color solo al objeto activo.
- **Token J: Selección** (`SelectionTool`):
  - Raycast desde centro del token en pantalla → objeto más cercano por bounding sphere; actualiza `activeShapeIndex`; wireframe/anillos azules en draw.
- **Token E: Utilidades** (`UtilityTool`):
  - Menú radial (Delete/Undo/Reset). Al retirar token ejecuta acción seleccionada sobre objeto activo o escena. Reset reinicia escena y limpia historial; Undo usa snapshots (`SceneSnapshot` en ofApp).
- **Ejes/gizmos y HUD**: `Shape3D::drawRotation/Translation/ScaleGizmo` se activan según token activo; `TransformFeedback3D` muestra valores cercanos al gizmo; HUD 2D muestra info y menús (draw* en tools).

## UI y tooltips
- `src/TooltipManager.(hpp|cpp)`: gestor con estados HIDDEN/FADING_IN/VISIBLE/HIDING_DELAYED/FADING_OUT; fade configurable; fuentes TTF; dibuja tarjetas con borde redondeado. `ofApp` mantiene un mapa por token, reordena horizontalmente (`reflowTooltips`), permite múltiples tooltips simultáneos y conserva durante fades aun si el token se retiró.
- Menús de herramientas usan `menuState` y `menuAlpha` con delays compartidos en `Tool`.

## Cámara, luces y render
- Cámara orbital (`ofEasyCam` sin mouse). Clipping near/far ampliado para grilla 10k.
- Sistema de iluminación 6 puntos (key/fill/rim/side-left/side-right/bottom) con colores cálidos/fríos; sombras planas elípticas en suelo; fondo gradiente cálido.
- Grilla manual grande (líneas) y gizmo de ejes; cubo de navegación en viewport superior derecho sincronizado con orientación de cámara.

## Undo y gestión de escena
- `SceneSnapshot` (en `ofApp`): guarda por figura tipo/pos/rot (cuaternión)/escala/color y objeto activo. `saveSnapshot()` antes de mutaciones; `performUndo()` restaura último; historial limitado a `MAX_UNDO_STEPS`; `clearUndoHistory()` en reset completo.

## Notas de configuración
- Constantes ajustables en `Config.hpp`: sensibilidad de cámara (`Camera::SENSITIVITY`, `ZOOM_SPEED`, `JUMP_THRESHOLD`), tolerancias de clasificación (`TokenClassification`), límites de escala (`ScaleTool`), grilla (`Grid::SIZE`), clustering multi-token (`MultiToken`).
