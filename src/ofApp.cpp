#include "ofApp.h"


void ofApp::setup(){
    ofEnableDepthTest();
    ofEnableAntiAliasing();
    glEnable(GL_NORMALIZE);  // ✅ Normalizar normales después de transformaciones (FIX: iluminación incorrecta)
    
    // Crear primera figura por defecto
    auto initialShape = std::make_unique<Shape3D>();
    initialShape->setup(ShapeType::Box);
    shapes.push_back(std::move(initialShape));
    activeShapeIndex = 0;

    // Token manager - handles multiple tokens
    tokenManager.setup();

    // herramientas para rotación (mantener en tools vector)
    tools.emplace_back(std::make_unique<RotationTool>());       // index 0
    tools.emplace_back(std::make_unique<AxisSelectorTool>());   // index 1

    // mapear tokens a herramientas específicas (F, G, H, B, J, E)
    tokenTools['F'] = std::make_unique<ScaleTool>();
    tokenTools['G'] = std::make_unique<TranslateTool>();
    tokenTools['H'] = std::make_unique<SpawnTool>();
    tokenTools['B'] = std::make_unique<ColorTool>();  // Token B = triángulo isósceles 80-80-20
    tokenTools['J'] = std::make_unique<SelectionTool>();  // Token J = selección de objetos
    tokenTools['D'] = std::make_unique<UtilityTool>();  // Token D = utilidades (DELETE, UNDO, RESET)

    // ✅ Cargar fuentes para texto de botones (solución híbrida ofTrueTypeFont + bold)
    // Intentar cargar fuente del sistema primero, fallback a Verdana
    if (!AxisSelectorTool::buttonFont.load("Arial.ttf", 20)) {
        if (!AxisSelectorTool::buttonFont.load("Verdana.ttf", 20)) {
            // Fallback: usar fuente predeterminada de openFrameworks
            AxisSelectorTool::buttonFont.load(OF_TTF_SANS, 20);
        }
    }
    
    // Cargar la misma fuente para todos los tools (compartida)
    ScaleTool::buttonFont = AxisSelectorTool::buttonFont;
    TranslateTool::buttonFont = AxisSelectorTool::buttonFont;
    SpawnTool::buttonFont = AxisSelectorTool::buttonFont;
    UtilityTool::buttonFont = AxisSelectorTool::buttonFont;
    
    ofLogNotice("ofApp") << "Button fonts loaded successfully for all tools";

    // ✅ TooltipManagers se inicializan bajo demanda cuando se detectan tokens
    ofLogNotice("ofApp") << "Tooltip system ready (multiple tooltips supported)";

    // ✅ Inicializar sistema de feedback 3D para transformaciones
    transformFeedback.setup();
    transformFeedback.setFontSize(18);
    transformFeedback.setGlowIntensity(0.3f);
    ofLogNotice("ofApp") << "Transform feedback 3D initialized";

    // Setup lighting, materials, and background
    setupLights();
    setupGradientBackground();

    cam.setDistance(TUI3D::Camera::DEFAULT_RADIUS);
    // desactivar control con mouse (si quieres que token A lo controle)
    cam.disableMouseInput();
    
    // ✅ Ajustar planos de recorte para grilla grande (10000 unidades)
    cam.setNearClip(1.0f);       // Ver objetos desde 1 unidad de distancia
    cam.setFarClip(50000.0f);    // Ver objetos hasta 50000 unidades (5x la grilla)

    // inicializar variables esféricas respecto al pivote = centro de la primera figura
    {
        glm::vec3 pivot = shapes[activeShapeIndex]->getPosition();
        glm::vec3 camPos = cam.getPosition();
        glm::vec3 rel = camPos - pivot;
        cameraState.radius = glm::length(rel);
        if (cameraState.radius < 1.0f) cameraState.radius = 1.0f;
        cameraState.azimuth = atan2(rel.z, rel.x);
        cameraState.elevation = atan2(rel.y, sqrt(rel.x*rel.x + rel.z*rel.z));
        cam.lookAt(pivot);
    }
}

// ========== LIGHTING SETUP (6-POINT PRODUCT VISUALIZATION) ==========
void ofApp::setupLights() {
    // ✅ Luz ambiental global (MANTENER valor actual)
    ofSetGlobalAmbientColor(ofColor(90, 90, 90));

    // ========== 1. KEY LIGHT (Superior - tono frío) ==========
    keyLight.setup();
    keyLight.setPointLight();
    keyLight.setPosition(0, 1200, 1000);  // Frontal-superior-centro
    keyLight.setAttenuation(1.0f, 0.0f, 0.0f);
    keyLight.setDiffuseColor(ofColor(50, 50, 50));  // Azul frío brillante (superior)
    keyLight.setSpecularColor(ofColor(255, 255, 255));

    // ========== 2. FILL LIGHT (Frontal - tono cálido) ==========
    fillLight.setup();
    fillLight.setPointLight();
    fillLight.setPosition(0, 800, 1200);  // Frontal-baja
    fillLight.setAttenuation(1.0f, 0.0f, 0.0f);
    fillLight.setDiffuseColor(ofColor(180, 180, 180));  // Naranja cálido suave (frontal)
    fillLight.setSpecularColor(ofColor(100, 100, 100));

    // ========== 3. RIM LIGHT (Contorno - NUEVA) ==========
    rimLight.setup();
    rimLight.setPointLight();
    rimLight.setPosition(-600, 1200, -800);  // Trasera-superior-izq
    rimLight.setAttenuation(1.0f, 0.0f, 0.0f);
    rimLight.setDiffuseColor(ofColor(200, 210, 220));  // Blanco frío
    rimLight.setSpecularColor(ofColor(150, 150, 150));

    // ========== 4. SIDE LEFT LIGHT (Relleno Lateral Izq. - NUEVA) ==========
    sideLeftLight.setup();
    sideLeftLight.setPointLight();
    sideLeftLight.setPosition(-1000, 0, 300);  // Lateral izquierdo-frontal
    sideLeftLight.setAttenuation(1.0f, 0.0f, 0.0f);
    sideLeftLight.setDiffuseColor(ofColor(140, 140, 145));  // Muy suave
    sideLeftLight.setSpecularColor(ofColor(50, 50, 50));

    // ========== 5. SIDE RIGHT LIGHT (Relleno Lateral Der. - NUEVA) ==========
    sideRightLight.setup();
    sideRightLight.setPointLight();
    sideRightLight.setPosition(1000, 0, 300);  // Lateral derecho-frontal (simétrico)
    sideRightLight.setAttenuation(1.0f, 0.0f, 0.0f);
    sideRightLight.setDiffuseColor(ofColor(140, 140, 145));  // Idéntico a sideLeft
    sideRightLight.setSpecularColor(ofColor(50, 50, 50));

    // ========== 6. BOTTOM LIGHT (Relleno Inferior - NUEVA) ==========
    bottomLight.setup();
    bottomLight.setPointLight();
    bottomLight.setPosition(0, -400, 0);  // Debajo del objeto
    bottomLight.setAttenuation(1.0f, 0.0f, 0.0f);
    bottomLight.setDiffuseColor(ofColor(150, 150, 150));  // Neutro suave
    bottomLight.setSpecularColor(ofColor(0, 0, 0));  // SIN brillos

    ofLogNotice("ofApp") << "6-point lighting system initialized (product visualization - frontal emphasis)";
}

// ========== MATERIALS SETUP ==========

// ========== GRADIENT BACKGROUND ==========
void ofApp::setupGradientBackground() {
    backgroundGradient.setMode(OF_PRIMITIVE_TRIANGLE_STRIP);

    ofColor topColor(252, 250, 248);      // Blanco cálido (más rojo, menos azul)
    ofColor bottomColor(238, 235, 230);   // Gris cálido claro (más rojo, menos azul)

    // Create fullscreen quad with vertical gradient
    backgroundGradient.addVertex(ofVec3f(0, 0, 0));
    backgroundGradient.addColor(bottomColor);

    backgroundGradient.addVertex(ofVec3f(ofGetWidth(), 0, 0));
    backgroundGradient.addColor(bottomColor);

    backgroundGradient.addVertex(ofVec3f(0, ofGetHeight(), 0));
    backgroundGradient.addColor(topColor);

    backgroundGradient.addVertex(ofVec3f(ofGetWidth(), ofGetHeight(), 0));
    backgroundGradient.addColor(topColor);

    ofLogNotice("ofApp") << "Warm gradient background created";
}

// ========== MATERIAL SELECTOR ==========
ofMaterial& ofApp::getMaterialForShape(size_t index) {
    // Alternate materials: 0=plastic, 1=metal, 2=glass, 3=plastic, ...
    int materialIndex = index % 3;
    switch (materialIndex) {
        case 0: return matPlastic;
        case 1: return matMetal;
        case 2: return matGlass;
        default: return matPlastic;
    }
}

// ========== FAKE GROUND SHADOW ==========
void ofApp::drawGroundShadow(Shape3D* shape) {
    ofPushMatrix();
    ofPushStyle();

    glm::vec3 pos = shape->getPosition();
    glm::vec3 scl = shape->getScale();

    // Shadow projection matrix (flatten Y onto ground plane Y=0)
    float shadowHeight = pos.y;
    if (shadowHeight <= 0) {
        ofPopStyle();
        ofPopMatrix();
        return;  // No shadow if object is at or below ground
    }

    // Simple planar shadow (project onto Y=0.1 plane to avoid Z-fighting with grid)
    ofTranslate(pos.x, 0.1, pos.z);

    // Shadow intensity based on height (higher = lighter shadow)
    float alpha = ofMap(shadowHeight, 0, 500, 80, 10, true);
    ofSetColor(0, 0, 0, alpha);

    // Draw shadow as ellipse (approximate projection)
    float maxScale = std::max(std::max(scl.x, scl.y), scl.z);
    float shadowRadius = maxScale * 60.0f;

    ofFill();
    ofDrawEllipse(0, 0, shadowRadius * 1.2f, shadowRadius * 0.8f);

    ofPopStyle();
    ofPopMatrix();
}

void ofApp::update(){
    // Update token manager - handles clustering and multi-token detection
    // Pass UI state: when tooltips are visible, tokens lock immediately (skip 175ms delay)
    bool uiActive = isAnyTooltipActive();
    tokenManager.update(uiActive);

    // Get all tokens (locked and unlocked) - needed for initialization
    auto activeTokens = tokenManager.getAllTokens();

    // ✅ Detectar nuevos tokens LOCKED para mostrar tooltips
    std::set<char> currentActiveTokens;
    for (auto* token : activeTokens) {
        if (token->isLocked()) {
            currentActiveTokens.insert(token->getId());
        }
    }

    // 🔍 DEBUG: Log para ver qué tokens están activos
    ofLogNotice("DEBUG") << "═══ FRAME UPDATE ═══";
    ofLogNotice("DEBUG") << "Current active tokens: " << currentActiveTokens.size();
    for (char id : currentActiveTokens) {
        ofLogNotice("DEBUG") << "  - Token " << id << " is active";
    }
    ofLogNotice("DEBUG") << "Previous active tokens: " << previousActiveTokens.size();
    for (char id : previousActiveTokens) {
        ofLogNotice("DEBUG") << "  - Token " << id << " was active";
    }

    // Process each token (both locked and unlocked - needed for tool initialization)
    for (auto* token : activeTokens) {
        auto current = token->getPoints();
        auto locked = token->getLockedPoints();
        char id = token->getId();

        // ✅ Mostrar tooltip si es un token nuevo Y LOCKED
        if (token->isLocked() && previousActiveTokens.find(id) == previousActiveTokens.end()) {
            // Token nuevo detectado - crear y posicionar TooltipManager
            Tool* tool = nullptr;
            TooltipInfo info;

            if (id == 'A') {
                // Cámara orbital (Token A)
                info = TooltipInfo(
                    "Cámara",
                    "Desplaza el token por la pantalla para mover la camara y gire para hacer zoom",
                    ofColor(100, 150, 255) // Azul claro
                );
            } else if (id == 'C') {
                tool = dynamic_cast<RotationTool*>(tools[0].get());
            } else if (tokenTools.find(id) != tokenTools.end()) {
                tool = tokenTools[id].get();
            }

            if (tool) {
                info = tool->getTooltipInfo();
            }

            // Crear TooltipManager para este token
            if (!info.title.empty()) {
                tooltipManagers[id].setup();

                // Calcular posición horizontal según índice (cuántos tooltips ya hay)
                int index = 0;
                for (const auto& pair : tooltipManagers) {
                    if (pair.first < id) index++;  // Contar tooltips que vienen antes alfabéticamente
                }
                float xPos = 20 + (index * 420);  // 420px spacing (400px width + 20px gap)
                tooltipManagers[id].setPosition(xPos, 20);

                tooltipManagers[id].show(info);
                ofLogNotice("ofApp") << "Showing tooltip for token " << id << " at x=" << xPos << ": " << info.title;

                // ✅ Track insertion order (temporal, not alphabetical)
                if (std::find(tooltipInsertionOrder.begin(), tooltipInsertionOrder.end(), id) == tooltipInsertionOrder.end()) {
                    tooltipInsertionOrder.push_back(id);
                }

                // ✅ Reordenar todos los tooltips después de agregar uno nuevo
                reflowTooltips();
            }
        }

        // ✅ DETECTAR tokens que REAPARECEN (estaban siendo removidos pero volvieron)
        for (char newToken : currentActiveTokens) {
            if (tokenManager.isTokenRemoving(newToken)) {
                // Token que estaba desapareciendo ha VUELTO a aparecer
                // Cancelar el proceso de remoción
                tokenManager.clearTokenRemoving(newToken);

                // Restaurar tooltip si existe
                if (tooltipManagers.find(newToken) != tooltipManagers.end()) {
                    Tool* tool = nullptr;
                    TooltipInfo info;

                    if (newToken == 'C') {
                        tool = dynamic_cast<RotationTool*>(tools[0].get());
                    } else if (tokenTools.find(newToken) != tokenTools.end()) {
                        tool = tokenTools[newToken].get();
                    }

                    if (tool) {
                        info = tool->getTooltipInfo();
                    }

                    if (!info.title.empty()) {
                        tooltipManagers[newToken].show(info);
                        ofLogNotice("ofApp") << "Token " << newToken << " volvió - restaurando tooltip";
                    }
                }

                // ✅ CRÍTICO: Restaurar menú visualmente
                auto itTool = tokenTools.find(newToken);
                if (itTool != tokenTools.end() &&
                    (itTool->second->menuState == Tool::MENU_HIDING_DELAYED ||
                     itTool->second->menuState == Tool::MENU_FADING_OUT)) {
                    itTool->second->showMenu();
                    ofLogNotice("ofApp") << "Token " << newToken << " volvió - restaurando menú";
                }

                // Caso especial: AxisSelectorTool (token 'C')
                if (newToken == 'C') {
                    auto* axisTool = dynamic_cast<AxisSelectorTool*>(tools[1].get());
                    if (axisTool && axisTool->hasLockedMenu()) {
                        axisTool->showMenu();
                        ofLogNotice("ofApp") << "Token C volvió - restaurando menú AxisSelector";
                    }
                }
            }
        }

        // punteros a rotation tools (en tools vector)
        auto* rotTool = dynamic_cast<RotationTool*>(tools[0].get());
        auto* axisTool = dynamic_cast<AxisSelectorTool*>(tools[1].get());

        // ------------ ROTACIÓN (token C) ------------
        if (id == 'C') {
            // crear menú fijo a partir del snapshot
            if (token->isLocked() && axisTool && !axisTool->hasLockedMenu()) {
                saveSnapshot();  // Guardar snapshot antes de empezar rotación
                axisTool->setLockedPoints(locked);
            }

            // actualizar selección del eje con puntos actuales
            if (axisTool) axisTool->update(current);

            // rotator toma current points + eje seleccionado
            if (rotTool && !shapes.empty()) {
                glm::vec3 right = cam.getXAxis();
                glm::vec3 up = cam.getYAxis();
                glm::vec3 forward = -cam.getZAxis(); // -Z mira hacia adelante en cámara
                rotTool->setCameraBasis(right, up, forward);

                RotationAxis sel = axisTool ? axisTool->currentAxis : RotationAxis::None;
                rotTool->update(current, sel);
                
                // ✅ Aplicar rotación incremental en espacio GLOBAL usando cuaterniones
                // Obtener cuaternión actual del objeto
                glm::quat currentRot = shapes[activeShapeIndex]->getRotation();
                
                // Aplicar rotación incremental
                glm::quat newRot = rotTool->applyRotation(currentRot, sel);
                
                // Actualizar cuaternión del objeto
                shapes[activeShapeIndex]->setRotation(newRot);
            }
        }

        // ------------ ESCALA (token F) ------------
        if (id == 'F' && !shapes.empty()) {
            auto itF = tokenTools.find('F');
            if (itF != tokenTools.end()) {
                ScaleTool* sTool = dynamic_cast<ScaleTool*>(itF->second.get());
                if (sTool) {
                    if (token->isLocked() && !sTool->locked) {
                        saveSnapshot();  // Guardar snapshot antes de empezar escala
                        if (scaleInteractionMode == ScaleInteractionMode::Knob) {
                            sTool->setLockedPointsWithIDs(token->getLockedPointsWithIDs(),
                                                          shapes[activeShapeIndex]->getScale());
                        } else {
                            sTool->setLockedPoints(locked, shapes[activeShapeIndex]->getScale());
                        }
                    }
                    if (scaleInteractionMode == ScaleInteractionMode::Knob) {
                        sTool->updateWithIDs(token->getPointsWithIDs());
                    } else {
                        sTool->update(current);
                    }
                    shapes[activeShapeIndex]->setScale(sTool->getScale());
                    lastScale = shapes[activeShapeIndex]->getScale();
                }
            }
        }

        // ------------ TRASLACIÓN (token G) ------------
        if (id == 'G' && !shapes.empty()) {
            auto itG = tokenTools.find('G');
            if (itG != tokenTools.end()) {
                TranslateTool* tTool = dynamic_cast<TranslateTool*>(itG->second.get());
                if (tTool) {
                    // Actualizar base de cámara para gestos dependientes de la vista
                    glm::vec3 right = cam.getXAxis();
                    glm::vec3 up = cam.getYAxis();
                    glm::vec3 forward = -cam.getZAxis(); // -Z mira hacia adelante en cámara
                    tTool->setCameraBasis(right, up, forward);

                    if (token->isLocked() && !tTool->locked) {
                        saveSnapshot();  // Guardar snapshot antes de empezar traslación
                        tTool->setLockedPoints(locked, shapes[activeShapeIndex]->getPosition());
                    }
                    tTool->update(current);
                    shapes[activeShapeIndex]->setPosition(tTool->getPosition());
                    lastPosition = shapes[activeShapeIndex]->getPosition();
                }
            }
        }

        // ------------ SPAWN (token H) ------------
        if (id == 'H') {
            auto itH = tokenTools.find('H');
            if (itH != tokenTools.end()) {
                SpawnTool* spawnTool = dynamic_cast<SpawnTool*>(itH->second.get());
                if (spawnTool) {
                    if (token->isLocked() && !spawnTool->lockedMenu) {
                        spawnTool->setLockedPoints(locked);
                    }
                    // Solo actualizar selección - no crear figura aquí
                    spawnTool->update(current);
                }
            }
        }

        // ------------ COLOR (token B - triángulo isósceles 80-80-20) ------------
        if (id == 'B' && !shapes.empty()) {
            auto itB = tokenTools.find('B');
            if (itB != tokenTools.end()) {
                ColorTool* cTool = dynamic_cast<ColorTool*>(itB->second.get());
                if (cTool) {
                    if (token->isLocked() && !cTool->locked) {
                        saveSnapshot();  // Guardar snapshot antes de empezar cambio de color
                        // Usar método con Session IDs para tracking robusto
                        auto lockedWithIDs = token->getLockedPointsWithIDs();
                        // ✅ Obtener color del objeto activo (no global)
                        ofColor currentObjColor = shapes[activeShapeIndex]->getColor();
                        cTool->setLockedPointsWithIDs(lockedWithIDs, currentObjColor, activeShapeIndex);
                    }
                    // Actualizar con Session IDs para mantener tracking del vértice de 20°
                    auto currentWithIDs = token->getPointsWithIDs();
                    cTool->updateWithIDs(currentWithIDs);
                    // ✅ Aplicar color SOLO al objeto activo
                    shapes[activeShapeIndex]->setColor(cTool->getColor());
                }
            }
        }
        
        // ------------ SELECCIÓN (token J) ------------
        if (id == 'J' && !shapes.empty()) {
            auto itJ = tokenTools.find('J');
            if (itJ != tokenTools.end()) {
                SelectionTool* selTool = dynamic_cast<SelectionTool*>(itJ->second.get());
                if (selTool) {
                    selTool->update(current);
                    
                    // Realizar raycast cuando el token está presente
                    if (!current.empty()) {
                        // Calcular centro del token
                        ofVec2f center(0, 0);
                        for (auto& p : current) {
                            center += p;
                        }
                        center /= (float)current.size();
                        
                        // Ejecutar raycast 3D
                        int hit = selTool->performRaycast(center, cam, shapes);
                        if (hit >= 0 && hit < shapes.size()) {
                            activeShapeIndex = hit;  // Cambiar objeto activo
                            ofLogNotice("ofApp") << "Shape selected: #" << hit;
                        }
                    }
                }
            }
        }
        
        // ------------ UTILIDADES (token D) ------------
        if (id == 'D') {
            auto itD = tokenTools.find('D');
            if (itD != tokenTools.end()) {
                UtilityTool* uTool = dynamic_cast<UtilityTool*>(itD->second.get());
                if (uTool) {
                    if (token->isLocked() && !uTool->lockedMenu) {
                        uTool->setLockedPoints(locked);
                    }
                    // Actualizar selección de acción
                    uTool->update(current);
                }
            }
        }
        
        // ------------ Cámara (token A) ------------
        if (id == 'A') {
            updateCameraWithToken(current, token->isLocked());
        }
    }

    // ✅ PRIMERO: Detectar tokens RETIRADOS y marcarlos ANTES de los checks de herramientas
    // Esto previene la condición de carrera donde los checks de herramientas ejecutan
    // antes de que markTokenRemoving() haya sido llamado
    for (char prevToken : previousActiveTokens) {
        if (currentActiveTokens.find(prevToken) == currentActiveTokens.end()) {
            // Token retirado - iniciar hide de su tooltip específico (delay de 2.0s antes de fade-out)
            if (tooltipManagers.find(prevToken) != tooltipManagers.end()) {
                tooltipManagers[prevToken].hide();
                tokenManager.markTokenRemoving(prevToken);  // Mantener token "activo" durante fadeout
                ofLogNotice("ofApp") << "Token " << prevToken << " retirado - ocultando tooltip en 2.0s";

                // ✅ SINCRONIZACIÓN: Ocultar el menú de la herramienta al mismo tiempo que el tooltip
                // Esto garantiza que tooltip y menú desaparezcan simultáneamente
                auto itTool = tokenTools.find(prevToken);
                if (itTool != tokenTools.end()) {
                    itTool->second->hideMenu();
                    ofLogNotice("ofApp") << "Token " << prevToken << " - ocultando menú de herramienta en 2.0s";
                }

                // Caso especial: AxisSelectorTool (token 'C') no está en tokenTools
                if (prevToken == 'C') {
                    auto* axisTool = dynamic_cast<AxisSelectorTool*>(tools[1].get());
                    if (axisTool) {
                        axisTool->hideMenu();
                        ofLogNotice("ofApp") << "Token C - ocultando menú AxisSelector en 2.0s";
                    }
                }

                // ========== EJECUTAR ACCIÓN INMEDIATAMENTE AL RETIRAR TOKEN ==========

                // Token H: SpawnTool - Crear figura si hay selección
                if (prevToken == 'H') {
                    auto itH = tokenTools.find('H');
                    if (itH != tokenTools.end()) {
                        SpawnTool* spawnTool = dynamic_cast<SpawnTool*>(itH->second.get());
                        if (spawnTool && spawnTool->hasSelection()) {
                            saveSnapshot();

                            // Mapear SpawnShape a ShapeType
                            ShapeType newType = ShapeType::Box;
                            switch (spawnTool->getSelectedShape()) {
                                case SpawnShape::Box: newType = ShapeType::Box; break;
                                case SpawnShape::Cylinder: newType = ShapeType::Cylinder; break;
                                case SpawnShape::Cone: newType = ShapeType::Cone; break;
                                case SpawnShape::Sphere: newType = ShapeType::Sphere; break;
                            }

                            // Crear nueva figura
                            auto newShape = std::make_unique<Shape3D>();
                            newShape->setup(newType);
                            glm::vec3 validPos = findNearestValidPosition(newType);
                            newShape->setPosition(validPos);
                            shapes.push_back(std::move(newShape));
                            activeShapeIndex = shapes.size() - 1;

                            ofLogNotice("ofApp") << "Shape spawned immediately on token removal";
                            spawnTool->clearSelection();
                        }
                    }
                }

                // Token D: UtilityTool - Ejecutar acción si hay selección
                if (prevToken == 'D') {
                    auto itD = tokenTools.find('D');
                    if (itD != tokenTools.end()) {
                        UtilityTool* uTool = dynamic_cast<UtilityTool*>(itD->second.get());
                        if (uTool && uTool->hasSelection()) {
                            switch (uTool->getSelectedAction()) {
                                case UtilityAction::Delete:
                                    if (!shapes.empty() && activeShapeIndex < shapes.size()) {
                                        saveSnapshot();
                                        shapes.erase(shapes.begin() + activeShapeIndex);
                                        if (activeShapeIndex >= shapes.size() && !shapes.empty()) {
                                            activeShapeIndex = shapes.size() - 1;
                                        }
                                        ofLogNotice("ofApp") << "Shape deleted immediately on token removal";
                                    }
                                    break;
                                case UtilityAction::Undo:
                                    performUndo();
                                    ofLogNotice("ofApp") << "Undo executed immediately on token removal";
                                    break;
                                case UtilityAction::Reset:
                                    saveSnapshot();
                                    reset();
                                    clearUndoHistory();
                                    ofLogNotice("ofApp") << "Reset executed immediately on token removal";
                                    break;
                                default:
                                    break;
                            }
                            uTool->clearSelection();
                        }
                    }
                }

                // reflowTooltips() will be called AFTER cleanup (after tooltip fully removed)
            }
        }
    }

    // Reset tools that are no longer active
    // Ahora estos checks ven correctamente los tokens marcados como "removing"
    auto* axisTool = dynamic_cast<AxisSelectorTool*>(tools[1].get());

    if (!tokenManager.isTokenActive('C') && axisTool && axisTool->hasLockedMenu()) {
        axisTool->clearLocked();
    }

    if (!tokenManager.isTokenActive('F')) {
        auto itF = tokenTools.find('F');
        if (itF != tokenTools.end()) {
            ScaleTool* sTool = dynamic_cast<ScaleTool*>(itF->second.get());
            if (sTool && sTool->locked) sTool->reset();
        }
    }

    if (!tokenManager.isTokenActive('G')) {
        auto itG = tokenTools.find('G');
        if (itG != tokenTools.end()) {
            TranslateTool* tTool = dynamic_cast<TranslateTool*>(itG->second.get());
            if (tTool && tTool->locked) tTool->reset();
        }
    }

    if (!tokenManager.isTokenActive('B')) {
        auto itB = tokenTools.find('B');
        if (itB != tokenTools.end()) {
            ColorTool* cTool = dynamic_cast<ColorTool*>(itB->second.get());
            if (cTool && cTool->locked) cTool->reset();
        }
    }

    if (!tokenManager.isTokenActive('H')) {
        auto itH = tokenTools.find('H');
        if (itH != tokenTools.end()) {
            SpawnTool* spawnTool = dynamic_cast<SpawnTool*>(itH->second.get());
            if (spawnTool && spawnTool->menuActive) {
                // La acción ya fue ejecutada al retirar el token
                // Solo hacer cleanup aquí
                spawnTool->clearSelection();  // Por seguridad (ya se hizo en retiro)
                spawnTool->reset();
            }
        }
    }

    if (!tokenManager.isTokenActive('D')) {
        auto itD = tokenTools.find('D');
        if (itD != tokenTools.end()) {
            UtilityTool* uTool = dynamic_cast<UtilityTool*>(itD->second.get());
            if (uTool && uTool->menuActive) {
                // La acción ya fue ejecutada al retirar el token
                // Solo hacer cleanup aquí
                uTool->clearSelection();  // Por seguridad (ya se hizo en retiro)
                uTool->reset();
            }
        }
    }

    // ✅ Actualizar todos los TooltipManagers activos (gestiona fade-in/out y delays)
    for (auto& pair : tooltipManagers) {
        pair.second.update();
    }

    // ✅ Actualizar fade de menús de TODAS las herramientas (incondicional)
    // Esto asegura que los menús en HIDING_DELAYED/FADING_OUT sigan animándose
    // incluso después de que el token sea retirado
    for (auto& pair : tokenTools) {
        pair.second->updateMenuFade();
    }

    // También actualizar AxisSelectorTool (token 'C') que no está en tokenTools
    auto* axisToolFade = dynamic_cast<AxisSelectorTool*>(tools[1].get());
    if (axisToolFade) {
        axisToolFade->updateMenuFade();
    }

    // ✅ Eliminar tooltips que han terminado de hacer fade-out completamente
    std::vector<char> tokensToRemove;
    for (auto& pair : tooltipManagers) {
        // Si el tooltip está HIDDEN Y el token ya no está activo, removerlo del mapa
        if (pair.second.getState() == TooltipManager::HIDDEN &&
            currentActiveTokens.find(pair.first) == currentActiveTokens.end()) {
            tokensToRemove.push_back(pair.first);
        }
    }

    // Eliminar tooltips completamente ocultos
    for (char id : tokensToRemove) {
        tooltipManagers.erase(id);

        // ✅ También eliminar del vector de orden temporal
        auto it = std::find(tooltipInsertionOrder.begin(), tooltipInsertionOrder.end(), id);
        if (it != tooltipInsertionOrder.end()) {
            tooltipInsertionOrder.erase(it);
        }

        // ✅ Limpiar estado de "removing" en TokenManager (token ya no está "activo")
        tokenManager.clearTokenRemoving(id);

        ofLogNotice("ofApp") << "Tooltip " << id << " removed from manager (fade-out complete)";
    }

    // Si se eliminó algún tooltip, reordenar los restantes
    if (!tokensToRemove.empty()) {
        reflowTooltips();
    }

    // ✅ Actualizar feedback 3D de transformaciones (solo si hay objeto activo)
    if (!shapes.empty() && activeShapeIndex < shapes.size()) {
        // Obtener herramientas activas según tokens
        RotationTool* rotTool = nullptr;
        RotationAxis rotAxis = RotationAxis::None;
        if (tokenManager.isTokenActive('C')) {
            if (!tools.empty()) {
                rotTool = dynamic_cast<RotationTool*>(tools[0].get());
                if (tools.size() > 1) {
                    auto* axisTool = dynamic_cast<AxisSelectorTool*>(tools[1].get());
                    if (axisTool) rotAxis = axisTool->currentAxis;
                }
            }
        }

        ScaleTool* scaleTool = nullptr;
        if (tokenManager.isTokenActive('F')) {
            auto itF = tokenTools.find('F');
            if (itF != tokenTools.end()) {
                scaleTool = dynamic_cast<ScaleTool*>(itF->second.get());
            }
        }

        TranslateTool* transTool = nullptr;
        if (tokenManager.isTokenActive('G')) {
            auto itG = tokenTools.find('G');
            if (itG != tokenTools.end()) {
                transTool = dynamic_cast<TranslateTool*>(itG->second.get());
            }
        }

        transformFeedback.update(rotTool, rotAxis, scaleTool, transTool,
                                 shapes[activeShapeIndex].get(), cam);
    }

    // ✅ Actualizar set de tokens activos previos para próximo frame
    previousActiveTokens = currentActiveTokens;
}

// ========== REORDENAMIENTO DINÁMICO DE TOOLTIPS ==========
void ofApp::reflowTooltips() {
    // ✅ Usar orden temporal (tooltipInsertionOrder) en lugar de orden alfabético
    std::vector<char> activeTooltipIds;

    // Iterar en el orden en que fueron colocados (temporal)
    for (char id : tooltipInsertionOrder) {
        // Solo incluir si el tooltip sigue activo (visible/apareciendo/delay)
        if (tooltipManagers.find(id) != tooltipManagers.end() && tooltipManagers[id].isActive()) {
            activeTooltipIds.push_back(id);
        }
    }

    // Recalcular posiciones para TODOS los tooltips activos (sin gaps)
    for (size_t i = 0; i < activeTooltipIds.size(); i++) {
        char id = activeTooltipIds[i];
        float xPos = 20 + (i * 420);  // 420px spacing (400px width + 20px gap)
        tooltipManagers[id].setPosition(xPos, 20);
        ofLogNotice("ofApp") << "Tooltip " << id << " repositioned to x=" << xPos << " (index " << i << ", temporal order)";
    }
}

// ========== UI STATE QUERY ==========
bool ofApp::isAnyTooltipActive() const {
    for (const auto& pair : tooltipManagers) {
        if (pair.second.isActive()) {
            return true;
        }
    }
    return false;
}

void ofApp::draw(){
    // ========== GRADIENT BACKGROUND (2D) ==========
    ofDisableDepthTest();
    ofDisableLighting();

    ofPushMatrix();
    ofPushStyle();
    backgroundGradient.draw();
    ofPopStyle();
    ofPopMatrix();

    // dibujar tokens y HUD 2D
    tokenManager.draw();

    // texto e info siempre debe ir sin luces/material
    ofDisableLighting();
    ofDisableDepthTest();
    // Color será establecido por cada elemento de UI según necesite

    // Get active tokens for rendering (needed later in draw)
    auto activeTokens = tokenManager.getActiveTokens();

    // --- 3D ---
    ofEnableDepthTest();
    cam.begin();

        // ========== GRILLA DE REFERENCIA 3D (solo líneas) ==========
        ofPushStyle();
        
        // GRILLA MANUAL CON LÍNEAS (clara referencia espacial)
        ofDisableLighting();
        ofSetColor(200, 200, 205, 80);  // Gris muy claro, semi-transparente
        ofSetLineWidth(1);
        
        float gridSize = 10000.0f;   // Aumentado de 2000 a 10000 (5x más grande)
        float gridStep = 200.0f;     // Aumentado de 100 a 200 (líneas más espaciadas)
        
        // Líneas paralelas al eje X (van en dirección X)
        for (float z = -gridSize/2; z <= gridSize/2; z += gridStep) {
            ofDrawLine(-gridSize/2, 0, z, gridSize/2, 0, z);
        }
        
        // Líneas paralelas al eje Z (van en dirección Z)
        for (float x = -gridSize/2; x <= gridSize/2; x += gridStep) {
            ofDrawLine(x, 0, -gridSize/2, x, 0, gridSize/2);
        }
        
        ofPopStyle();

        // 3D axis gizmo (X=rojo, Y=verde, Z=azul)
        drawAxisGizmo();

        // ========== ENABLE LIGHTING SYSTEM (6-POINT) ==========
        ofEnableLighting();  // Activar sistema de lighting de OpenGL

        // Activar TODAS las luces del sistema de 6 puntos
        keyLight.enable();
        fillLight.enable();
        rimLight.enable();
        sideLeftLight.enable();
        sideRightLight.enable();
        bottomLight.enable();

        // ========== DRAW FAKE SHADOWS FIRST (under objects) ==========
        ofEnableBlendMode(OF_BLENDMODE_ALPHA);
        for (size_t i = 0; i < shapes.size(); i++) {
            drawGroundShadow(shapes[i].get());
        }
        ofDisableBlendMode();

        // ========== DRAW ALL SHAPES (SIMPLIFIED - NO MATERIALS) ==========
        for (size_t i = 0; i < shapes.size(); i++) {
            // Determinar color según estado
            ofColor objectColor;
            if (i == activeShapeIndex && tokenManager.isTokenActive('J')) {
                // ✅ Objeto seleccionado con Token J → AZUL BRILLANTE
                objectColor = ofColor(0, 150, 255);
            } else {
                // ✅ Cualquier objeto → SU COLOR INDIVIDUAL
                objectColor = shapes[i]->getColor();
            }

            // ✅ Aplicar color directo (sin materiales)
            ofSetColor(objectColor);
            shapes[i]->draw();
        }

        // ========== DISABLE LIGHTS ==========
        // Desactivar TODAS las luces del sistema de 6 puntos
        keyLight.disable();
        fillLight.disable();
        rimLight.disable();
        sideLeftLight.disable();
        sideRightLight.disable();
        bottomLight.disable();

        ofDisableLighting();  // Desactivar sistema de lighting

        // -------- GIZMOS DE TRANSFORMACIÓN --------
        // Dibujar gizmos según herramienta activa (solo para objeto activo)
        if (!shapes.empty() && activeShapeIndex < shapes.size()) {
            // Lighting ya está desactivado para gizmos 2D
            
            // Gizmo de Rotación (Token C)
            if (tokenManager.isTokenActive('C')) {
                auto* axisTool = dynamic_cast<AxisSelectorTool*>(tools[1].get());
                if (axisTool) {
                    shapes[activeShapeIndex]->drawRotationGizmo(axisTool->currentAxis);
                }
            }
            
            // Gizmo de Traslación (Token G)
            if (tokenManager.isTokenActive('G')) {
                auto itG = tokenTools.find('G');
                if (itG != tokenTools.end()) {
                    TranslateTool* tTool = dynamic_cast<TranslateTool*>(itG->second.get());
                    if (tTool) {
                        shapes[activeShapeIndex]->drawTranslationGizmo(tTool->currentAxis);
                    }
                }
            }
            
            // Gizmo de Escala (Token F)
            if (tokenManager.isTokenActive('F')) {
                auto itF = tokenTools.find('F');
                if (itF != tokenTools.end()) {
                    ScaleTool* sTool = dynamic_cast<ScaleTool*>(itF->second.get());
                    if (sTool) {
                        shapes[activeShapeIndex]->drawScaleGizmo(sTool->currentAxis);
                    }
                }
            }

            // ✅ FEEDBACK 3D: Texto flotante con valores de transformación
            ofDisableLighting();
            transformFeedback.draw(cam);
        }

        // -------- FEEDBACK VISUAL PARA SELECCIÓN --------
        // Mostrar wireframe + anillos cuando Token J está activo
        if (tokenManager.isTokenActive('J') && !shapes.empty() && activeShapeIndex < shapes.size()) {
            ofDisableLighting();  // ✅ Desactivar lighting para colores correctos
            
            // 1. WIREFRAME azul brillante alrededor del objeto seleccionado
            ofPushStyle();
            ofNoFill();
            ofSetColor(0, 150, 255);  // Azul brillante (mismo que figura)
            ofSetLineWidth(3);
            shapes[activeShapeIndex]->drawWireframe();
            ofPopStyle();
            
            // 2. ANILLOS dobles en el suelo (plano XZ) - usando polilínea manual
            ofPushStyle();
            ofPushMatrix();
            glm::vec3 pos = shapes[activeShapeIndex]->getPosition();
            glm::vec3 scl = shapes[activeShapeIndex]->getScale();
            
            ofTranslate(pos);
            ofRotateXDeg(90);  // Rotar al plano horizontal
            
            // Radio basado en la escala máxima del objeto
            float maxScale = std::max(std::max(scl.x, scl.y), scl.z);
            float ringRadius = maxScale * 60.0f;
            
            // Generar anillo exterior con polilínea
            ofPolyline ring1;
            for (int i = 0; i <= 64; i++) {
                float angle = (i / 64.0f) * TWO_PI;
                float x = cos(angle) * ringRadius;
                float y = sin(angle) * ringRadius;
                ring1.addVertex(x, y, 0);
            }
            
            // Generar anillo interior con polilínea
            ofPolyline ring2;
            for (int i = 0; i <= 64; i++) {
                float angle = (i / 64.0f) * TWO_PI;
                float x = cos(angle) * ringRadius * 0.8f;
                float y = sin(angle) * ringRadius * 0.8f;
                ring2.addVertex(x, y, 0);
            }
            
            // Dibujar anillos
            ofSetColor(0, 150, 255, 150);  // Azul semi-transparente
            ofSetLineWidth(4);
            ring1.draw();
            
            ofSetColor(0, 150, 255, 80);  // Azul tenue
            ofSetLineWidth(2);
            ring2.draw();
            
            ofPopMatrix();
            ofPopStyle();
        }

    cam.end();
    ofDisableDepthTest();
    
    // --- 2D UI info según token activo ---
    ofDisableLighting();   // 🔑 evita que se aplique luz al texto
    
    ofSetColor(255);

    // ✅ Mover info de herramientas a BOTTOM-LEFT para no solapar con tooltips
    int y = ofGetHeight() - 150;  // Empezar desde abajo, reservar 150px para debug

    // ✅ Habilitar alpha blending para transparencia correcta de menús
    ofEnableBlendMode(OF_BLENDMODE_ALPHA);

    // Draw info for each active tool
    if (tokenManager.isTokenActive('C')) {
        RotationTool* rT = dynamic_cast<RotationTool*>(tools[0].get());
        AxisSelectorTool* aT = dynamic_cast<AxisSelectorTool*>(tools[1].get());
        if (aT) aT->drawMenu();
        if (rT) {
            ofDrawBitmapString("Rot X: " + ofToString(rT->rotationX,2), 20, y+=15);
            ofDrawBitmapString("Rot Y: " + ofToString(rT->rotationY,2), 20, y+=15);
            ofDrawBitmapString("Rot Z: " + ofToString(rT->rotationZ,2), 20, y+=15);
        }
    }
    
    if (tokenManager.isTokenActive('F')) {
        auto itF = tokenTools.find('F');
        if (itF != tokenTools.end()){
            ScaleTool* sTool = dynamic_cast<ScaleTool*>(itF->second.get());
            if (sTool) {
                // Draw scale menu and bar interface
                sTool->drawMenu();
                sTool->drawScaleBar();
                
                // Draw scale info in HUD
                auto scaleModeLabel = [](ScaleInteractionMode mode) {
                    switch (mode) {
                        case ScaleInteractionMode::Gesture: return string("Gestos");
                        case ScaleInteractionMode::Knob: return string("Perilla");
                        case ScaleInteractionMode::Menu:
                        default: return string("Botones");
                    }
                };
                string modeLabel = scaleModeLabel(scaleInteractionMode);
                string axisName = sTool->getAxisName();
                ofDrawBitmapString("Escala [" + axisName + "] (" + modeLabel + "): " + ofToString(sTool->getScale().x, 2), 20, y+=15);
            }
        }
    }
    
    if (tokenManager.isTokenActive('G')) {
        auto itG = tokenTools.find('G');
        if (itG != tokenTools.end()) {
            TranslateTool* tTool = dynamic_cast<TranslateTool*>(itG->second.get());
            if (tTool) {
                // Draw translate menu
                tTool->drawMenu();
                
                // Draw info in HUD
                auto translateModeLabel = [](TranslateInteractionMode mode) {
                    switch (mode) {
                        case TranslateInteractionMode::Gesture: return string("Gestos");
                        case TranslateInteractionMode::Knob: return string("Perilla");
                        case TranslateInteractionMode::Menu:
                        default: return string("Botones");
                    }
                };
                string axisName = "None";
                string modeLabel = translateModeLabel(translateInteractionMode);
                bool gestureMode = translateInteractionMode == TranslateInteractionMode::Gesture;
                if (tTool->currentAxis == TranslateAxis::X) {
                    axisName = gestureMode ? "Vista Der" : "X";
                } else if (tTool->currentAxis == TranslateAxis::Y) {
                    axisName = gestureMode ? "Vista Arriba" : "Y";
                } else if (tTool->currentAxis == TranslateAxis::Z) {
                    axisName = gestureMode ? "Vista Prof" : "Z";
                }
                
                ofDrawBitmapString("Translate [" + axisName + "] (" + modeLabel + ")", 20, y+=15);
                ofDrawBitmapString("Pos: " + ofToString(lastPosition.x,1) + ", " + 
                                  ofToString(lastPosition.y,1) + ", " + 
                                  ofToString(lastPosition.z,1), 20, y+=15);
            }
        }
    }
    
    if (tokenManager.isTokenActive('H')) {
        auto itH = tokenTools.find('H');
        if (itH != tokenTools.end()) {
            SpawnTool* spawnTool = dynamic_cast<SpawnTool*>(itH->second.get());
            if (spawnTool) {
                // Draw spawn menu
                spawnTool->drawMenu();
                
                // Draw info in HUD
                string shapeName = "Box";
                switch (spawnTool->getSelectedShape()) {
                    case SpawnShape::Box: shapeName = "Box"; break;
                    case SpawnShape::Cylinder: shapeName = "Cylinder"; break;
                    case SpawnShape::Cone: shapeName = "Cone"; break;
                    case SpawnShape::Sphere: shapeName = "Sphere"; break;
                }
                ofDrawBitmapString("Spawn: " + shapeName, 20, y+=15);
                ofDrawBitmapString("Shapes: " + ofToString(shapes.size()), 20, y+=15);
            }
        }
    }
    
    if (tokenManager.isTokenActive('B')) {
        auto itB = tokenTools.find('B');
        if (itB != tokenTools.end()) {
            ColorTool* cTool = dynamic_cast<ColorTool*>(itB->second.get());
            if (cTool) {
                // Orden de dibujado: 1) Rueda, 2) Línea indicadora, 3) Círculo central
                // Esto hace que la línea esté SOBRE la rueda
                cTool->drawColorWheel();
                
                // Buscar token B en la lista de tokens activos para dibujar línea indicadora
                for (auto* token : activeTokens) {
                    if (token->getId() == 'B') {
                        // Usar método con Session IDs para dibujo consistente
                        auto pointsWithIDs = token->getPointsWithIDs();
                        cTool->drawAngleIndicatorWithIDs(pointsWithIDs);
                        break;
                    }
                }
                
                // Dibujar círculo central al final (más adelante)
                cTool->drawColorIndicator();
                
                // Dibujar info en HUD - mostrar color del objeto activo
                if (!shapes.empty() && activeShapeIndex < shapes.size()) {
                    ofColor activeColor = shapes[activeShapeIndex]->getColor();
                    ofDrawBitmapString("Color H: " + ofToString(activeColor.getHue()), 20, y+=15);
                }
            }
        }
    }
    
    if (tokenManager.isTokenActive('J')) {
        auto itJ = tokenTools.find('J');
        if (itJ != tokenTools.end()) {
            SelectionTool* selTool = dynamic_cast<SelectionTool*>(itJ->second.get());
            if (selTool) {
                // Mostrar objeto activo
                ofDrawBitmapString("Objeto activo: #" + ofToString(activeShapeIndex), 20, y+=15);
                ofDrawBitmapString("Total objetos: " + ofToString(shapes.size()), 20, y+=15);
            }
        }
    }
    
    if (tokenManager.isTokenActive('D')) {
        auto itD = tokenTools.find('D');
        if (itD != tokenTools.end()) {
            UtilityTool* uTool = dynamic_cast<UtilityTool*>(itD->second.get());
            if (uTool) {
                // Dibujar menú radial con 3 círculos
                uTool->drawMenu();

                // Dibujar info en HUD
                string actionName = "None";
                switch (uTool->getSelectedAction()) {
                    case UtilityAction::Delete: actionName = "Delete"; break;
                    case UtilityAction::Undo: actionName = "Undo"; break;
                    case UtilityAction::Reset: actionName = "Reset"; break;
                    default: break;
                }
                ofDrawBitmapString("Utility: " + actionName, 20, y+=15);
                ofDrawBitmapString("Undo History: " + ofToString(undoHistory.size()), 20, y+=15);
            }
        }
    }

    // ✅ Deshabilitar blending después de dibujar menús
    ofDisableBlendMode();

    // --- Dibujar cubo de navegación AL FINAL (para que no afecte otros elementos 2D) ---
    // Esto previene colisiones de viewport con la rueda de color y otros elementos
    drawNavigationCube();

    // ✅ Dibujar todos los tooltips en la capa superior (última operación de draw)
    // Asegurar contexto 2D limpio para que el texto sea visible
    ofDisableDepthTest();   // Desactivar depth test (texto 2D no necesita z-buffer)
    ofDisableLighting();    // Desactivar luces (texto 2D con color directo)
    ofViewport();           // Restaurar viewport completo de pantalla
    ofSetupScreen();        // Configurar matriz de proyección 2D ortográfica

    // Dibujar cada tooltip activo (múltiples tooltips simultáneos)
    for (auto& pair : tooltipManagers) {
        pair.second.draw();
    }
}

// ========== 3D AXIS GIZMO (X, Y, Z) ==========

void ofApp::drawAxisGizmo() {
    ofPushStyle();
    
    // Desactivar lighting para colores puros
    ofDisableLighting();
    
    float axisLength = 200.0f;  // Longitud visible de los ejes
    float lineWidth = 3.0f;
    float textOffset = 10.0f;
    
    // -------- EJE X (ROJO) --------
    ofSetColor(255, 0, 0);  // Rojo
    ofSetLineWidth(lineWidth);
    ofDrawLine(glm::vec3(0, 0, 0), glm::vec3(axisLength, 0, 0));
    
    // Etiqueta "X"
    ofPushMatrix();
    ofTranslate(axisLength + textOffset, 0, 0);
    ofRotateYDeg(90);  // Rotar para que el texto siempre sea legible
    ofDrawBitmapString("X", 0, 0);
    ofPopMatrix();
    
    // -------- EJE Y (VERDE) --------
    ofSetColor(0, 255, 0);  // Verde
    ofSetLineWidth(lineWidth);
    ofDrawLine(glm::vec3(0, 0, 0), glm::vec3(0, axisLength, 0));
    
    // Etiqueta "Y"
    ofPushMatrix();
    ofTranslate(0, axisLength + textOffset, 0);
    ofRotateXDeg(-90);  // Rotar para que el texto siempre sea legible
    ofDrawBitmapString("Y", 0, 0);
    ofPopMatrix();
    
    // -------- EJE Z (AZUL) --------
    ofSetColor(0, 0, 255);  // Azul
    ofSetLineWidth(lineWidth);
    ofDrawLine(glm::vec3(0, 0, 0), glm::vec3(0, 0, axisLength));
    
    // Etiqueta "Z"
    ofPushMatrix();
    ofTranslate(0, 0, axisLength + textOffset);
    ofDrawBitmapString("Z", 0, 0);
    ofPopMatrix();
    
    ofPopStyle();
}



void ofApp::updateCameraWithToken(const std::vector<ofVec2f>& current, bool isLocked) {
    // Orbit camera control using Token A - RELATIVE movement only
    // Uses spherical coordinates (radius, azimuth, elevation) for smooth orbital movement
    // Zoom is controlled by rotating the token itself
    
    if (isLocked && !current.empty()) {
        // --- Centro del token ---
        ofVec2f c(0,0);
        for (auto &p: current) c += p;
        c /= (float)current.size();

        // --- Token acaba de aparecer ---
        if (!cameraState.tokenActive) {
            // Solo guardar posición de referencia - NO mover la cámara
            cameraState.lastTokenPos = c;
            cameraState.prevAngle = 0.0f;
            cameraState.hasInitialAngle = false;
            cameraState.tokenActive = true;
            ofLogNotice("Camera") << "Token A detected - reference position set (no camera movement)";
            return;  // Salir sin aplicar ningún cambio a la cámara
        }

        // --- Token ya estaba activo - aplicar movimiento RELATIVO ---
        ofVec2f delta = c - cameraState.lastTokenPos;
        float deltaDistance = delta.length();

        // Detectar SALTOS (reposicionamiento del token)
        if (deltaDistance > TUI3D::Camera::JUMP_THRESHOLD) {
            // Es un SALTO - NO aplicar cambios a la cámara
            // Solo actualizar referencia para próximo frame
            ofLogNotice("Camera") << "Token jump detected (" << deltaDistance << "px) - updating reference only";
            cameraState.lastTokenPos = c;
            cameraState.hasInitialAngle = false;  // Resetear zoom reference también
            return;
        }

        // -------- ORBITA DE LA CÁMARA (solo movimiento continuo) --------
        cameraState.azimuth  += -delta.x * TUI3D::Camera::SENSITIVITY;
        cameraState.elevation += -delta.y * TUI3D::Camera::SENSITIVITY;

        // limitar elevación a ±89° para evitar gimbal lock
        float maxElev = glm::radians(TUI3D::Camera::MAX_ELEVATION_DEG);
        cameraState.elevation = ofClamp(cameraState.elevation, -maxElev, maxElev);

        // coordenadas esféricas → cartesianas
        float cosEl = cos(cameraState.elevation);
        glm::vec3 relNew;
        relNew.x = cameraState.radius * cosEl * cos(cameraState.azimuth);
        relNew.y = cameraState.radius * sin(cameraState.elevation);
        relNew.z = cameraState.radius * cosEl * sin(cameraState.azimuth);

        glm::vec3 pivot = shapes.empty() ? glm::vec3(0) : shapes[activeShapeIndex]->getPosition();
        glm::vec3 targetCamPos = pivot + relNew;

        // suavizado para movimiento fluido
        glm::vec3 smoothPos = cam.getPosition() * (1.0f - TUI3D::Camera::SMOOTHING) 
                            + targetCamPos * TUI3D::Camera::SMOOTHING;
        cam.setPosition(smoothPos);
        cam.lookAt(pivot);

        // -------- ZOOM CON ROTACIÓN DEL TOKEN --------
        // Solo aplicar si hay suficientes puntos
        if (current.size() >= 2) {
            glm::vec2 p0 = current[0];
            glm::vec2 p1 = current[1];
            glm::vec2 dir = glm::normalize(p1 - p0);
            float angle = atan2(dir.y, dir.x);

            if (!cameraState.hasInitialAngle) {
                // Inicializar referencia de ángulo
                cameraState.prevAngle = angle;
                cameraState.hasInitialAngle = true;
            } else {
                // Calcular delta angular
                float deltaAngle = angle - cameraState.prevAngle;

                // normalizar ángulo en [-PI, PI]
                if (deltaAngle > PI) deltaAngle -= TWO_PI;
                if (deltaAngle < -PI) deltaAngle += TWO_PI;

                // ajustar zoom basado en rotación del token
                float targetRadius = cameraState.radius - deltaAngle * TUI3D::Camera::ZOOM_SPEED;
                targetRadius = ofClamp(targetRadius, TUI3D::Camera::MIN_RADIUS, TUI3D::Camera::MAX_RADIUS);

                // suavizado del zoom
                cameraState.radius = cameraState.radius * (1.0f - TUI3D::Camera::SMOOTHING) 
                                   + targetRadius * TUI3D::Camera::SMOOTHING;

                cameraState.prevAngle = angle;
            }
        }

        // Actualizar posición de referencia para próximo frame
        cameraState.lastTokenPos = c;
    } else {
        // Token removido - reset estado
        cameraState.tokenActive = false;
        cameraState.hasInitialAngle = false;
    }
}

void ofApp::keyPressed(int key) {
    if (key == 'r' || key == 'R') {
        reset();
        ofLogNotice("ofApp") << "Application reset triggered by user (key 'R')";
    }
    
    if (key == '1' || key == '2' || key == '3') {
        if (key == '1') {
            translateInteractionMode = TranslateInteractionMode::Menu;
            scaleInteractionMode = ScaleInteractionMode::Menu;
        } else if (key == '2') {
            translateInteractionMode = TranslateInteractionMode::Gesture;
            scaleInteractionMode = ScaleInteractionMode::Gesture;
        } else {
            translateInteractionMode = TranslateInteractionMode::Knob;
            scaleInteractionMode = ScaleInteractionMode::Knob;
        }

        auto itG = tokenTools.find('G');
        if (itG != tokenTools.end()) {
            if (auto* tTool = dynamic_cast<TranslateTool*>(itG->second.get())) {
                tTool->setInteractionMode(translateInteractionMode);
            }
        }
        auto itF = tokenTools.find('F');
        if (itF != tokenTools.end()) {
            if (auto* sTool = dynamic_cast<ScaleTool*>(itF->second.get())) {
                sTool->setInteractionMode(scaleInteractionMode);
            }
        }
        // Refrescar tooltips activos (G y F) para reflejar el texto del modo actual
        auto refreshTooltip = [&](char tokenId) {
            auto tm = tooltipManagers.find(tokenId);
            if (tm != tooltipManagers.end()) {
                Tool* tool = nullptr;
                if (tokenId == 'C') {
                    tool = dynamic_cast<RotationTool*>(tools[0].get());
                } else if (tokenTools.find(tokenId) != tokenTools.end()) {
                    tool = tokenTools[tokenId].get();
                }
                if (tool) {
                    tm->second.show(tool->getTooltipInfo());
                }
            }
        };
        refreshTooltip('G');
        refreshTooltip('F');

        auto translateModeLabel = [](TranslateInteractionMode mode) {
            switch (mode) {
                case TranslateInteractionMode::Gesture: return "gestos";
                case TranslateInteractionMode::Knob: return "perilla";
                case TranslateInteractionMode::Menu:
                default: return "menú";
            }
        };
        auto scaleModeLabel = [](ScaleInteractionMode mode) {
            switch (mode) {
                case ScaleInteractionMode::Gesture: return "gestos";
                case ScaleInteractionMode::Knob: return "perilla";
                case ScaleInteractionMode::Menu:
                default: return "menú";
            }
        };
        ofLogNotice("ofApp") << "Translate interaction mode -> "
                             << translateModeLabel(translateInteractionMode)
                             << " | Scale -> "
                             << scaleModeLabel(scaleInteractionMode);
    }
}

void ofApp::reset() {
    // Reset all shapes
    shapes.clear();
    auto initialShape = std::make_unique<Shape3D>();
    initialShape->setup(ShapeType::Box);
    shapes.push_back(std::move(initialShape));
    activeShapeIndex = 0;
    
    // Reset state variables
    lastScale = glm::vec3(1.0f);
    lastPosition = glm::vec3(0.0f);
    // Color ya no es global - cada objeto tiene su propio color inicializado en setup()
    
    // Reset all tools
    auto* rotTool = dynamic_cast<RotationTool*>(tools[0].get());
    if (rotTool) {
        rotTool->rotationX = 0;
        rotTool->rotationY = 0;
        rotTool->rotationZ = 0;
    }
    
    auto* axisTool = dynamic_cast<AxisSelectorTool*>(tools[1].get());
    if (axisTool) {
        axisTool->clearLocked();
    }
    
    // Reset token tools
    auto itF = tokenTools.find('F');
    if (itF != tokenTools.end()) {
        ScaleTool* sTool = dynamic_cast<ScaleTool*>(itF->second.get());
        if (sTool) sTool->reset();
    }
    
    auto itG = tokenTools.find('G');
    if (itG != tokenTools.end()) {
        TranslateTool* tTool = dynamic_cast<TranslateTool*>(itG->second.get());
        if (tTool) tTool->reset();
    }
    
    auto itB = tokenTools.find('B');
    if (itB != tokenTools.end()) {
        ColorTool* cTool = dynamic_cast<ColorTool*>(itB->second.get());
        if (cTool) cTool->reset();
    }
    
    // Reset camera state
    cameraState.lastTokenPos = ofVec2f(0, 0);
    cameraState.tokenActive = false;
    cameraState.radius = TUI3D::Camera::DEFAULT_RADIUS;
    cameraState.azimuth = 0.0f;
    cameraState.elevation = 0.0f;
    cameraState.prevAngle = 0.0f;
    cameraState.hasInitialAngle = false;
    
    // Reset camera position
    cam.setDistance(TUI3D::Camera::DEFAULT_RADIUS);
    if (!shapes.empty()) {
        glm::vec3 pivot = shapes[activeShapeIndex]->getPosition();
        cam.lookAt(pivot);
    }
    
    ofLogNotice("ofApp") << "Application state reset complete";
}

// ========== COLLISION DETECTION & SMART POSITIONING ==========

BoundingBox ofApp::getBoundingBox(const Shape3D* shape) const {
    glm::vec3 pos = shape->getPosition();
    glm::vec3 scale = shape->getScale();
    
    // Radio base según tipo de figura
    // Box: 150x150x150, otros: radio 75, altura 150
    float baseRadius = 75.0f;
    
    // Calcular tamaño de la caja delimitadora
    glm::vec3 halfSize = glm::vec3(baseRadius) * scale;
    
    return {
        pos - halfSize,  // min
        pos + halfSize   // max
    };
}

bool ofApp::checkCollision(const BoundingBox& a, const BoundingBox& b) const {
    // AABB (Axis-Aligned Bounding Box) collision detection
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

glm::vec3 ofApp::findNearestValidPosition(ShapeType type) {
    // Si no hay figuras, colocar en el origen
    if (shapes.empty()) {
        return glm::vec3(0, 0, 0);
    }
    
    // Radio base de la figura nueva
    float baseRadius = 75.0f;
    float spacing = 50.0f;  // Espacio mínimo entre figuras
    
    // Búsqueda en círculos concéntricos alrededor del origen
    for (float radius = 0; radius < 2000; radius += baseRadius + spacing) {
        // Número de posiciones a probar en este círculo
        // Más posiciones en círculos más grandes
        int numPositions = std::max(8, (int)(radius * 0.1f));
        
        for (int i = 0; i < numPositions; i++) {
            float angle = (TWO_PI / numPositions) * i;
            
            // Posición candidata en el plano XZ (suelo)
            glm::vec3 candidatePos(
                cos(angle) * radius,
                0,  // Mantener en el suelo
                sin(angle) * radius
            );
            
            // Crear bounding box temporal para la nueva figura
            BoundingBox newBox = {
                candidatePos - glm::vec3(baseRadius + spacing/2),
                candidatePos + glm::vec3(baseRadius + spacing/2)
            };
            
            // Verificar colisiones con todas las figuras existentes
            bool collides = false;
            for (const auto& shape : shapes) {
                BoundingBox existingBox = getBoundingBox(shape.get());
                if (checkCollision(newBox, existingBox)) {
                    collides = true;
                    break;
                }
            }
            
            // Si no colisiona, esta es nuestra posición válida
            if (!collides) {
                return candidatePos;
            }
        }
    }
    
    // Fallback: posición lejana si todo falla (muy improbable)
    return glm::vec3(0, 0, 500);
}

// ========== UNDO SYSTEM (SCENE SNAPSHOTS) ==========

SceneSnapshot SceneSnapshot::capture(const std::vector<std::unique_ptr<Shape3D>>& shapes, int activeIdx) {
    SceneSnapshot snapshot;
    
    // Capturar todas las figuras
    for (const auto& shape : shapes) {
        ShapeSnapshot shapeSnap(
            shape->getShapeType(),
            shape->getPosition(),
            shape->getRotation(),
            shape->getScale(),
            shape->getColor()
        );
        snapshot.shapes.push_back(shapeSnap);
    }
    
    snapshot.activeShapeIndex = activeIdx;
    
    ofLogNotice("UNDO") << "Snapshot captured: " << snapshot.shapes.size() << " shapes";
    return snapshot;
}

void SceneSnapshot::restore(std::vector<std::unique_ptr<Shape3D>>& shapes, int& activeIdx) const {
    // Limpiar shapes actuales
    shapes.clear();
    
    // Restaurar cada figura del snapshot
    for (const auto& shapeSnap : this->shapes) {
        auto newShape = std::make_unique<Shape3D>();
        newShape->setup(shapeSnap.type);
        newShape->setPosition(shapeSnap.position);
        newShape->setRotation(shapeSnap.rotation);
        newShape->setScale(shapeSnap.scale);
        newShape->setColor(shapeSnap.color);
        
        shapes.push_back(std::move(newShape));
    }
    
    // Restaurar índice activo
    activeIdx = this->activeShapeIndex;
    if (activeIdx >= shapes.size() && !shapes.empty()) {
        activeIdx = shapes.size() - 1;
    }
    
    ofLogNotice("UNDO") << "Snapshot restored: " << shapes.size() << " shapes";
}

void ofApp::saveSnapshot() {
    // Capturar estado actual
    SceneSnapshot snapshot = SceneSnapshot::capture(shapes, activeShapeIndex);
    
    // Agregar al historial
    undoHistory.push_back(snapshot);
    
    // Limitar tamaño del historial
    if (undoHistory.size() > MAX_UNDO_STEPS) {
        undoHistory.erase(undoHistory.begin());
    }
    
    ofLogNotice("UNDO") << "Snapshot saved. History size: " << undoHistory.size();
}

void ofApp::performUndo() {
    if (undoHistory.empty()) {
        ofLogNotice("UNDO") << "No hay acciones para deshacer";
        return;
    }
    
    // Obtener último snapshot
    SceneSnapshot lastSnapshot = undoHistory.back();
    undoHistory.pop_back();
    
    // Restaurar estado
    lastSnapshot.restore(shapes, activeShapeIndex);
    
    ofLogNotice("UNDO") << "Undo performed. History size: " << undoHistory.size();
}

void ofApp::clearUndoHistory() {
    undoHistory.clear();
    ofLogNotice("UNDO") << "History cleared";
}

// ========== NAVIGATION CUBE (FUSION 360 STYLE) ==========

void ofApp::drawNavigationCube() {
    ofPushStyle();
    ofPushMatrix();
    
    // Viewport exclusivo (esquina superior derecha)
    int size = 120;
    int margin = 20;
    int vpX = ofGetWidth() - size - margin;
    int vpY = margin;
    
    ofViewport(vpX, vpY, size, size);
    ofSetupScreen();
    
    // === DIBUJAR CUBO 3D CON ROTACIÓN ===
    ofEnableDepthTest();
    ofPushMatrix();
    
    // Centrar y rotar cubo
    ofTranslate(size/2, size/2, 0);
    ofMultMatrix(glm::toMat4(cam.getOrientationQuat()));
    ofScale(-1, 1, 1);  // ← Invertir eje X para corregir orientación izquierda/derecha
    
    // Escalar cubo
    float cubeSize = 35;
    float halfSize = cubeSize / 2;
    
    // --- DIBUJAR CUBO WIREFRAME ---
    ofNoFill();
    ofSetColor(255, 255, 255, 180);
    ofSetLineWidth(2);
    ofDrawBox(0, 0, 0, cubeSize);
    
    // --- DIBUJAR CARAS CON COLORES ---
    ofFill();
    ofEnableAlphaBlending();
    
    // +Y (SUPERIOR) - Verde
    ofSetColor(100, 255, 100, 120);
    ofPushMatrix();
    ofTranslate(0, halfSize, 0);
    ofRotateXDeg(90);
    ofDrawRectangle(-halfSize, -halfSize, cubeSize, cubeSize);
    ofPopMatrix();
    
    // -Y (INFERIOR) - Verde oscuro
    ofSetColor(80, 180, 80, 120);
    ofPushMatrix();
    ofTranslate(0, -halfSize, 0);
    ofRotateXDeg(-90);
    ofDrawRectangle(-halfSize, -halfSize, cubeSize, cubeSize);
    ofPopMatrix();
    
    // +X (DERECHA) - Rojo
    ofSetColor(255, 100, 100, 120);
    ofPushMatrix();
    ofTranslate(halfSize, 0, 0);
    ofRotateYDeg(90);
    ofDrawRectangle(-halfSize, -halfSize, cubeSize, cubeSize);
    ofPopMatrix();
    
    // -X (IZQUIERDA) - Rojo oscuro
    ofSetColor(180, 80, 80, 120);
    ofPushMatrix();
    ofTranslate(-halfSize, 0, 0);
    ofRotateYDeg(-90);
    ofDrawRectangle(-halfSize, -halfSize, cubeSize, cubeSize);
    ofPopMatrix();
    
    // +Z (FRONTAL) - Azul
    ofSetColor(100, 100, 255, 120);
    ofPushMatrix();
    ofTranslate(0, 0, halfSize);
    ofDrawRectangle(-halfSize, -halfSize, cubeSize, cubeSize);
    ofPopMatrix();
    
    // -Z (TRASERA) - Azul oscuro
    ofSetColor(80, 80, 180, 120);
    ofPushMatrix();
    ofTranslate(0, 0, -halfSize);
    ofRotateYDeg(180);
    ofDrawRectangle(-halfSize, -halfSize, cubeSize, cubeSize);
    ofPopMatrix();
    
    ofDisableAlphaBlending();
    
    ofPopMatrix();  // ← Salir del sistema 3D del cubo

    // ✅ Desactivar depth test para que elementos 2D posteriores se vean correctamente
    ofDisableDepthTest();

    // Restaurar viewport
    ofViewport();
    ofSetupScreen();
    
    // === DIBUJAR TEXTO 2D FIJO (siempre visible) ===
    ofDisableLighting();
    ofSetColor(255);
    
    // Calcular centro del viewport del cubo
    int centerX = vpX + size/2;
    int centerY = vpY + size/2;
    
    // Superior (arriba centro)
    ofDrawBitmapString("S", centerX - 3, vpY + 12);
    
    // Inferior (abajo centro)
    ofDrawBitmapString("I", centerX - 3, vpY + size - 5);
    
    // Derecha (derecha centro)
    ofDrawBitmapString("D", vpX + size - 15, centerY + 5);
    
    // Izquierda (izquierda centro)
    ofDrawBitmapString("I", vpX + 5, centerY + 5);
    
    // Frontal (ligeramente abajo del centro)
    ofDrawBitmapString("F", centerX - 3, centerY + 15);
    
    // Trasera (ligeramente arriba del centro)
    ofDrawBitmapString("T", centerX - 3, centerY - 8);
    
    ofPopMatrix();
    ofPopStyle();
}
