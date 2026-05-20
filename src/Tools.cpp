#include "Tools.hpp"
#include "Config.hpp"
#include "Shape3D.hpp"  // Necesario para SelectionTool::performRaycast
#include <limits>

// ✅ Definiciones de variables estáticas para fuentes de botones
ofTrueTypeFont AxisSelectorTool::buttonFont;
ofTrueTypeFont ScaleTool::buttonFont;
ofTrueTypeFont TranslateTool::buttonFont;
ofTrueTypeFont SpawnTool::buttonFont;
ofTrueTypeFont UtilityTool::buttonFont;

// ---------------- UTILS ----------------
std::vector<std::vector<ofVec2f>> clusterTouches(const std::vector<ofVec2f>& pts, float threshold) {
    std::vector<std::vector<ofVec2f>> clusters;
    for (const auto& p : pts) {
        bool found = false;
        for (auto& group : clusters) {
            for (const auto& gp : group) {
                if (p.distance(gp) < threshold) {
                    group.push_back(p);
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        if (!found) clusters.push_back({p});
    }
    return clusters;
}

// Función universal para ajustar posición de menús (responsividad)
ofVec2f clampMenuPosition(ofVec2f center, float maxRadius) {
    // Obtener dimensiones de pantalla
    float screenW = ofGetWidth();
    float screenH = ofGetHeight();
    
    // Aplicar margen de seguridad (radio + padding extra)
    float margin = maxRadius + 20.0f;
    
    // Limitar posición para que el menú completo sea visible
    center.x = ofClamp(center.x, margin, screenW - margin);
    center.y = ofClamp(center.y, margin, screenH - margin);
    
    return center;
}

namespace {
float normalizeAngleRadians(float ang) {
    while (ang > PI) ang -= TWO_PI;
    while (ang < -PI) ang += TWO_PI;
    return ang;
}

ofVec2f centroidOfPoints(const vector<ofVec2f>& pts) {
    if (pts.empty()) return ofVec2f(0, 0);
    ofVec2f c(0, 0);
    for (auto& p : pts) c += p;
    c /= static_cast<float>(pts.size());
    return c;
}

float principalAxisAngle(const vector<ofVec2f>& pts, float fallbackAngle) {
    if (pts.size() < 2) return fallbackAngle;
    ofVec2f c = centroidOfPoints(pts);

    float cov_xx = 0.0f;
    float cov_xy = 0.0f;
    float cov_yy = 0.0f;
    for (auto& p : pts) {
        ofVec2f d = p - c;
        cov_xx += d.x * d.x;
        cov_xy += d.x * d.y;
        cov_yy += d.y * d.y;
    }
    cov_xx /= pts.size();
    cov_xy /= pts.size();
    cov_yy /= pts.size();

    float variance = cov_xx + cov_yy;
    if (variance < 1e-3f) return fallbackAngle;

    return 0.5f * atan2(2.0f * cov_xy, cov_xx - cov_yy);
}

float stabilizedPrincipalAxisAngle(const vector<ofVec2f>& pts, float fallbackAngle) {
    float current = principalAxisAngle(pts, fallbackAngle);
    float alt = normalizeAngleRadians(current + PI);
    float diff = normalizeAngleRadians(current - fallbackAngle);
    float altDiff = normalizeAngleRadians(alt - fallbackAngle);
    if (std::abs(altDiff) < std::abs(diff)) {
        current = alt;
    }
    return current;
}
} // namespace

// ---------------- ROTATION TOOL ----------------
void RotationTool::update(const vector<ofVec2f>& pts, RotationAxis currentAxis) {
    active = (pts.size() >= 2);
    
    // Si NO hay eje seleccionado O faltan puntos, resetear deltaRotation
    if (currentAxis == RotationAxis::None || !active) {
        deltaRotation = 0.0f;
        lastAxis = RotationAxis::None;
        return;
    }

    ofVec2f p1 = pts[0];
    ofVec2f p2 = pts[1];
    ofVec2f delta = p2 - p1;
    float newAngle = atan2(delta.y, delta.x) * RAD_TO_DEG;

    // Detectar cambio de eje: resetear referencia
    if (currentAxis != lastAxis) {
        lastAngle = newAngle;
        lastAxis = currentAxis;
        deltaRotation = 0.0f;
        return;
    }

    // Calcular cambio incremental
    float d = newAngle - lastAngle;
    if (d > 180)  d -= 360;
    if (d < -180) d += 360;

    // Acumular cambio para este frame
    deltaRotation = d;
    
    // ⚠️ DEPRECADO: Mantener rotationX/Y/Z solo para HUD
    switch (currentAxis) {
        case RotationAxis::X: rotationX += d; break;
        case RotationAxis::Y: rotationY += d; break;
        case RotationAxis::Z: rotationZ += d; break;
        default: break;
    }
    
    lastAngle = newAngle;
}

// ✅ Aplicar rotación incremental en espacio GLOBAL
glm::quat RotationTool::applyRotation(const glm::quat& currentRotation, RotationAxis axis) {
    if (axis == RotationAxis::None || std::abs(deltaRotation) < 0.01f) {
        return currentRotation;  // Sin cambio
    }
    
    // Determinar eje global (restringido a X/Y/Z del mundo)
    glm::vec3 globalAxis;
    switch (axis) {
        case RotationAxis::X: globalAxis = glm::vec3(1, 0, 0); break;
        case RotationAxis::Y: globalAxis = glm::vec3(0, 1, 0); break;
        case RotationAxis::Z: globalAxis = glm::vec3(0, 0, 1); break;
        default: return currentRotation;
    }

    // Crear cuaternión de rotación incremental alrededor del eje global
    float viewSign = 1.0f;
    auto screenAngle = [&](const glm::vec3& v, float& angleOut) -> bool {
        float sx = glm::dot(v, basisRight);
        float sy = -glm::dot(v, basisUp); // pantalla: +y hacia abajo
        float len = std::sqrt(sx * sx + sy * sy);
        if (len < 0.0001f) return false;
        angleOut = std::atan2(sy, sx);
        return true;
    };

    glm::vec3 refCandidates[2];
    if (axis == RotationAxis::X) {
        refCandidates[0] = glm::vec3(0, 1, 0);
        refCandidates[1] = glm::vec3(0, 0, 1);
    } else if (axis == RotationAxis::Y) {
        refCandidates[0] = glm::vec3(0, 0, 1);
        refCandidates[1] = glm::vec3(1, 0, 0);
    } else {
        refCandidates[0] = glm::vec3(1, 0, 0);
        refCandidates[1] = glm::vec3(0, 1, 0);
    }

    float angle0 = 0.0f;
    float angle1 = 0.0f;
    glm::vec3 ref = refCandidates[0];
    if (!screenAngle(refCandidates[0], angle0) && screenAngle(refCandidates[1], angle0)) {
        ref = refCandidates[1];
    }

    if (screenAngle(ref, angle0)) {
        float smallStep = glm::radians(1.0f);
        glm::quat stepQuat = glm::angleAxis(smallStep, globalAxis);
        glm::vec3 refRot = glm::mat3_cast(stepQuat) * ref;
        if (screenAngle(refRot, angle1)) {
            float delta = angle1 - angle0;
            if (delta > PI) delta -= TWO_PI;
            if (delta < -PI) delta += TWO_PI;
            if (std::abs(delta) > 0.0001f) {
                viewSign = (delta >= 0.0f) ? 1.0f : -1.0f;
            }
        }
    }

    float angleRad = glm::radians(deltaRotation * viewSign);
    glm::quat deltaQuat = glm::angleAxis(angleRad, globalAxis);
    
    // ✅ CORRECCIÓN FINAL: Multiplicar a la IZQUIERDA = rotación en espacio GLOBAL
    // Con GLM: deltaQuat * currentRotation significa:
    //   - Primero aplicar currentRotation (orientación actual del objeto)
    //   - Luego aplicar deltaQuat en el sistema de coordenadas GLOBAL
    // Esto mantiene los ejes X/Y/Z siempre alineados con los ejes globales (rojo/verde/azul)
    glm::quat newRotation = deltaQuat * currentRotation;
    
    // Normalizar para evitar acumulación de errores
    return glm::normalize(newRotation);
}

void RotationTool::setCameraBasis(const glm::vec3& right, const glm::vec3& up, const glm::vec3& forward) {
    auto safeNormalize = [](const glm::vec3& v, const glm::vec3& fallback) {
        float len = glm::length(v);
        if (len <= 0.0001f) return fallback;
        return v / len;
    };
    basisRight = safeNormalize(right, glm::vec3(1, 0, 0));
    basisUp = safeNormalize(up, glm::vec3(0, 1, 0));
    basisForward = safeNormalize(forward, glm::vec3(0, 0, 1));
}

void RotationTool::draw(int x, int y) {
    ofDrawBitmapString("Rotator: " + string(active ? "SI" : "NO"), x, y);
}

// ---------------- AXIS SELECTOR TOOL ----------------
void AxisSelectorTool::update(const vector<ofVec2f>& pts, RotationAxis) {
    updateSelection(pts);
    updateMenuFade();  // ✅ Actualizar animación de fade
}
void AxisSelectorTool::draw(int x, int y) {
    string axisName = "Ninguno";
    if      (currentAxis == RotationAxis::X) axisName = "X";
    else if (currentAxis == RotationAxis::Y) axisName = "Y";
    else if (currentAxis == RotationAxis::Z) axisName = "Z";
    ofDrawBitmapString("Eje seleccionado: " + axisName, x, y);
}

void AxisSelectorTool::setLockedPoints(const vector<ofVec2f>& lockedPts) {
    if (lockedPts.size() < 3) return;
    ofVec2f c(0,0);
    for (auto &p: lockedPts) c += p;
    c /= (float)lockedPts.size();

    // Calculate average distance between points to scale button size
    float sum = 0; int count = 0;
    for (size_t i = 0; i < lockedPts.size(); ++i) {
        for (size_t j = i+1; j < lockedPts.size(); ++j) {
            sum += lockedPts[i].distance(lockedPts[j]);
            ++count;
        }
    }
    float avgDist = (count>0) ? (sum / count) : 100.0f;
    buttonRadius = ofClamp(avgDist * TUI3D::AxisSelector::RADIUS_SCALE_FACTOR, 
                           TUI3D::AxisSelector::MIN_BUTTON_RADIUS, 
                           TUI3D::AxisSelector::MAX_BUTTON_RADIUS);
    buttonSpacing = avgDist * TUI3D::AxisSelector::SPACING_FACTOR;
    
    // ✅ Aplicar responsividad: calcular radio máximo y ajustar posición
    float maxRadius = buttonSpacing + buttonRadius;
    fixedMenuCenter = clampMenuPosition(c, maxRadius);

    lockedMenu = true;
    menuActive = true;
    currentAxis = RotationAxis::None;

    // ✅ Iniciar fade-in del menú
    showMenu();
}
void AxisSelectorTool::clearLocked() {
    lockedMenu = false;
    menuActive = false;
    currentAxis = RotationAxis::None;

    // ✅ Iniciar fade-out del menú
    hideMenu();
}
bool AxisSelectorTool::hasLockedMenu() const { return lockedMenu; }
ofVec2f AxisSelectorTool::buttonPos(const string& axis) const {
    if (axis == "X") return fixedMenuCenter + ofVec2f(-buttonSpacing, 0);
    if (axis == "Y") return fixedMenuCenter + ofVec2f(0, -buttonSpacing);
    if (axis == "Z") return fixedMenuCenter + ofVec2f(buttonSpacing, 0);
    return fixedMenuCenter;
}
bool AxisSelectorTool::insideCircle(const ofVec2f& p, const ofVec2f& c) const { return p.distance(c) < buttonRadius; }
void AxisSelectorTool::drawButton(const string& label, const ofColor& color) {
    ofVec2f pos = buttonPos(label);

    // ✅ Aplicar alpha del fade a todos los elementos
    int fillAlpha = 60 * menuAlpha;  // MÁS transparente (era 120)
    int borderAlpha = 255 * menuAlpha;
    int selectionAlpha = 180 * menuAlpha;  // NUEVO: Alpha específico para círculo de selección

    // Dibujar círculo relleno con color del eje
    ofFill();
    ofSetColor(color.r, color.g, color.b, fillAlpha);  // Semi-transparente con fade
    ofDrawCircle(pos, buttonRadius);

    // Dibujar borde para mejor contraste
    ofNoFill();
    ofSetLineWidth(3);
    ofSetColor(color.r, color.g, color.b, borderAlpha);
    ofDrawCircle(pos, buttonRadius);

    // ✅ Texto con efecto bold híbrido: ofTrueTypeFont + múltiples dibujos
    ofSetColor(0, 0, 0, borderAlpha);  // Negro con fade
    ofPushMatrix();
    ofTranslate(pos);  // Mover al punto correcto
    // ✅ Efecto bold: dibujar múltiples veces con pequeños offsets
    for(int dx = 0; dx <= 1; dx++) {
        for(int dy = 0; dy <= 1; dy++) {
            buttonFont.drawString(label, -8 + dx, 8 + dy);
        }
    }
    ofPopMatrix();

    // Highlight si está seleccionado (semi-transparente pero visible)
    if ((label == "X" && currentAxis == RotationAxis::X) ||
        (label == "Y" && currentAxis == RotationAxis::Y) ||
        (label == "Z" && currentAxis == RotationAxis::Z)) {
        ofNoFill();
        ofSetLineWidth(5);
        ofSetColor(255, 230, 0, selectionAlpha);  // Usa selectionAlpha (180 vs 120)
        ofDrawCircle(pos, buttonRadius + 12);
    }
}
void AxisSelectorTool::updateSelection(const vector<ofVec2f>& currentPts) {
    if (!lockedMenu) return;
    if (currentPts.empty()) { currentAxis = RotationAxis::None; return; }
    for (auto &pt : currentPts) {
        if (insideCircle(pt, buttonPos("X"))) { currentAxis = RotationAxis::X; return; }
        if (insideCircle(pt, buttonPos("Y"))) { currentAxis = RotationAxis::Y; return; }
        if (insideCircle(pt, buttonPos("Z"))) { currentAxis = RotationAxis::Z; return; }
    }
    currentAxis = RotationAxis::None;
}
void AxisSelectorTool::drawMenu() {
    // ✅ Solo dibujar si hay algo visible (menuAlpha > 0)
    if (menuAlpha <= 0.0f) return;
    if (!menuActive || !lockedMenu) return;

    drawButton("X", ofColor::red);
    drawButton("Y", ofColor::green);
    drawButton("Z", ofColor::blue);
}

// ---------------- SCALE TOOL ----------------
void ScaleTool::setLockedPoints(const vector<ofVec2f>& lockedPts, const glm::vec3& currentScale) {
    if (lockedPts.empty()) return;
    
    // Guardar escala inicial del objeto
    initialScale = currentScale;
    
    // Calcular centro inicial del token (posición de referencia)
    ofVec2f center(0, 0);
    for (auto& p : lockedPts) {
        center += p;
    }
    center /= (float)lockedPts.size();
    initialCenter = center;
    menuLastCenter = center;
    
    // ✅ Aplicar responsividad: radio máximo = botón más lejano + barra
    float maxRadiusMenu = 0.0f;
    if (interactionMode == ScaleInteractionMode::Knob) {
        maxRadiusMenu = radialDistance + buttonRadius;
    } else {
        maxRadiusMenu = buttonSpacing * 1.5f + buttonHeight / 2.0f;
    }
    float maxRadiusBar = (interactionMode == ScaleInteractionMode::Knob)
                             ? (radialDistance + 90.0f + 15.0f)
                             : (buttonSpacing + 90.0f + 15.0f);
    float maxRadius = std::max(maxRadiusMenu, maxRadiusBar);
    fixedMenuCenter = clampMenuPosition(center, maxRadius);
    
    scaleVec = initialScale;
    perAxisFactor = glm::vec3(1.0f);  // Resetear factores acumulados
    menuScaleAccum = 0.0f;
    locked = true;
    gestureAxisChosen = false;
    gestureStart = center;
    knobHasOrientation = false;
    knobLastOrientation = 0.0f;
    knobLastCenter = center;
    knobRefSessionId = -1;
    knobLastRefPos = center;
    lockedPointsWithIDs.clear();

    if (interactionMode == ScaleInteractionMode::Menu ||
        interactionMode == ScaleInteractionMode::Knob) {
        lockedMenu = true;
        menuActive = true;
        currentAxis = ScaleAxis::Uniform; // Default: escala uniforme
        showMenu();
    } else {
        lockedMenu = false;
        menuActive = false;
        currentAxis = ScaleAxis::None;
        menuState = MENU_HIDDEN;
        menuAlpha = 0.0f;
    }

    ofLogNotice("ScaleTool") << "Scale interface activated ("
                             << (interactionMode == ScaleInteractionMode::Gesture ? "gesture" :
                                 interactionMode == ScaleInteractionMode::Knob ? "knob" : "menu")
                             << ")";
    ofLogNotice("ScaleTool") << "  Initial center: " << initialCenter;
    ofLogNotice("ScaleTool") << "  Initial scale: " << initialScale.x;
}

void ScaleTool::setLockedPointsWithIDs(const vector<TouchPointWithID>& lockedPts, const glm::vec3& currentScale) {
    if (lockedPts.empty()) return;

    vector<ofVec2f> coords;
    coords.reserve(lockedPts.size());
    for (const auto& pt : lockedPts) coords.push_back(pt.pos);

    lockedPointsWithIDs = lockedPts;
    setLockedPoints(coords, currentScale);

    knobRefSessionId = -1;
    if (!lockedPts.empty()) {
        knobRefSessionId = lockedPts.front().sessionID;
        for (const auto& pt : lockedPts) {
            if (pt.sessionID < knobRefSessionId) {
                knobRefSessionId = pt.sessionID;
            }
        }
        for (const auto& pt : lockedPts) {
            if (pt.sessionID == knobRefSessionId) {
                knobLastRefPos = pt.pos;
                break;
            }
        }
    }
}

void ScaleTool::reset() {
    locked = false;
    lockedMenu = false;
    menuActive = false;
    initialCenter = ofVec2f(0, 0);
    menuLastCenter = ofVec2f(0, 0);
    scaleVec = glm::vec3(1.0f);
    perAxisFactor = glm::vec3(1.0f);
    menuScaleAccum = 0.0f;
    initialScale = glm::vec3(1.0f);
    currentAxis = ScaleAxis::Uniform;
    gestureAxisChosen = false;
    gestureStart = ofVec2f(0, 0);
    knobHasOrientation = false;
    knobLastOrientation = 0.0f;
    knobLastCenter = ofVec2f(0, 0);
    knobRefSessionId = -1;
    knobLastRefPos = ofVec2f(0, 0);
    lockedPointsWithIDs.clear();
    menuState = MENU_HIDDEN;
    menuAlpha = 0.0f;

    // ✅ Iniciar fade-out del menú
    hideMenu();
}

void ScaleTool::setInteractionMode(ScaleInteractionMode mode) {
    if (interactionMode == mode) return;
    interactionMode = mode;

    currentAxis = (interactionMode == ScaleInteractionMode::Menu ||
                   interactionMode == ScaleInteractionMode::Knob)
                      ? ScaleAxis::Uniform
                      : ScaleAxis::None;
    gestureAxisChosen = false;
    gestureStart = initialCenter;
    menuLastCenter = initialCenter;
    perAxisFactor = glm::vec3(1.0f);
    menuScaleAccum = 0.0f;
    knobHasOrientation = false;
    knobLastOrientation = 0.0f;
    knobLastCenter = initialCenter;
    knobRefSessionId = -1;
    knobLastRefPos = initialCenter;

    if ((interactionMode == ScaleInteractionMode::Menu ||
         interactionMode == ScaleInteractionMode::Knob) && locked) {
        lockedMenu = true;
        menuActive = true;
        showMenu();
    } else {
        lockedMenu = false;
        menuActive = false;
        menuState = MENU_HIDDEN;
        menuAlpha = 0.0f;
    }

    ofLogNotice("ScaleTool") << "Interaction mode set to " 
        << (interactionMode == ScaleInteractionMode::Gesture ? "gesture" :
            interactionMode == ScaleInteractionMode::Knob ? "knob" : "menu");
}

void ScaleTool::update(const vector<ofVec2f>& pts, RotationAxis) {
    // ✅ Actualizar animación de fade
    updateMenuFade();

    if (!locked || pts.empty()) return;

    auto applyDeadzone = [&](float value) {
        float absVal = std::abs(value);
        if (absVal <= motionDeadzone) return 0.0f;
        float trimmed = absVal - motionDeadzone;
        return (value >= 0 ? trimmed : -trimmed);
    };

    if (interactionMode == ScaleInteractionMode::Menu) {
        ScaleAxis previousAxis = currentAxis;
        // Actualizar selección de eje
        updateSelection(pts);
        
        // Calcular centro actual del token
        ofVec2f currentCenter(0, 0);
        for (auto& p : pts) {
            currentCenter += p;
        }
        currentCenter /= (float)pts.size();

        float centerJump = currentCenter.distance(menuLastCenter);
        if (centerJump > TUI3D::ScaleTool::JUMP_THRESHOLD) {
            menuLastCenter = currentCenter;
            menuScaleAccum = 0.0f;
            return;
        }

        if (previousAxis != currentAxis) {
            menuLastCenter = currentCenter;
            menuScaleAccum = 0.0f;
        }
        
        // Si no hay eje seleccionado, no escalar pero refrescar referencia
        if (currentAxis == ScaleAxis::None) { 
            menuLastCenter = currentCenter; 
            return; 
        }
        
        // Desplazamiento incremental desde el último frame (sin perder pasos lentos)
        float deltaX = currentCenter.x - menuLastCenter.x;
        float deltaY = currentCenter.y - menuLastCenter.y;
        menuLastCenter = currentCenter;

        auto clampFactor = [](float f) { return std::max(0.1f, std::min(f, 5.0f)); };
        auto resetIfFlipped = [&](float accum, float delta) {
            if (accum != 0.0f && delta != 0.0f && ((accum > 0) != (delta > 0))) {
                return delta;  // Re-ancorar al cambiar de sentido para evitar "deshacer" el movimiento
            }
            return accum + delta;
        };

        auto consumeAccum = [&](float& accum) -> float {
            float absAccum = std::abs(accum);
            if (absAccum <= motionDeadzone) return 0.0f;
            float applied = (accum > 0) ? (accum - motionDeadzone) : (accum + motionDeadzone);
            accum = 0.0f;
            return applied;
        };
        
        // Actualizar factor acumulado según el eje seleccionado
        switch (currentAxis) {
            case ScaleAxis::X: {
                menuScaleAccum = resetIfFlipped(menuScaleAccum, deltaX);
                float applied = consumeAccum(menuScaleAccum);
                perAxisFactor.x = clampFactor(perAxisFactor.x + (applied / sensitivity));
                break;
            }
            case ScaleAxis::Y: {
                float axisDelta = deltaX; // Gesto horizontal también para eje Y
                menuScaleAccum = resetIfFlipped(menuScaleAccum, axisDelta);
                float applied = consumeAccum(menuScaleAccum);
                perAxisFactor.y = clampFactor(perAxisFactor.y + (applied / sensitivity));
                break;
            }
            case ScaleAxis::Z: {
                float axisDelta = (deltaX + deltaY) * 0.70710678f;
                menuScaleAccum = resetIfFlipped(menuScaleAccum, axisDelta);
                float applied = consumeAccum(menuScaleAccum);
                perAxisFactor.z = clampFactor(perAxisFactor.z + (applied / sensitivity));
                break;
            }
            case ScaleAxis::Uniform: {
                menuScaleAccum = resetIfFlipped(menuScaleAccum, deltaX);
                float applied = consumeAccum(menuScaleAccum);
                float base = (perAxisFactor.x + perAxisFactor.y + perAxisFactor.z) / 3.0f;
                float newFactor = clampFactor(base + (applied / sensitivity));
                perAxisFactor = glm::vec3(newFactor);  // Todos los ejes iguales
                break;
            }
            default:
                break;
        }
    } else if (interactionMode == ScaleInteractionMode::Gesture) {
        // Modo gestos: dirección dominante define el eje (cambia en vivo)
        ofVec2f currentCenter(0, 0);
        for (auto& p : pts) currentCenter += p;
        currentCenter /= (float)pts.size();

        ofVec2f delta = currentCenter - gestureStart;
        float len = delta.length();

        if (len >= gestureThreshold || currentAxis != ScaleAxis::None) {
            float absX = std::abs(delta.x);
            float absY = std::abs(delta.y);

            ScaleAxis chosenAxis = currentAxis;
            if (len >= gestureThreshold) {
                if (absX > absY * dominantAxisRatio) {
                    chosenAxis = ScaleAxis::X;
                } else if (absY > absX * dominantAxisRatio) {
                    chosenAxis = ScaleAxis::Y;
                } else {
                    chosenAxis = ScaleAxis::Z; // Diagonal = eje Z
                }
            }

            if (chosenAxis != currentAxis) {
                currentAxis = chosenAxis;
                gestureAxisChosen = true;
                gestureStart = currentCenter; // Re-ancorar para evitar saltos
                delta.set(0, 0);
                ofLogNotice("ScaleTool") << "Gesture axis set to: "
                                         << (currentAxis == ScaleAxis::X ? "X" :
                                             currentAxis == ScaleAxis::Y ? "Y" : "Z");
            }
        } else {
            return; // Aún no hay movimiento suficiente
        }

        float deltaX = applyDeadzone(currentCenter.x - gestureStart.x);
        float deltaY = applyDeadzone(currentCenter.y - gestureStart.y);

        float factor = 1.0f;
        switch (currentAxis) {
            case ScaleAxis::X:
                factor = 1.0f + (deltaX / sensitivity);
                perAxisFactor.x = std::max(0.1f, std::min(factor, 5.0f));
                break;
            case ScaleAxis::Y:
                factor = 1.0f + (deltaX / sensitivity); // Gesto horizontal para eje Y
                perAxisFactor.y = std::max(0.1f, std::min(factor, 5.0f));
                break;
            case ScaleAxis::Z: {
                float diag = (deltaX + deltaY) * 0.70710678f;
                factor = 1.0f + (diag / sensitivity);
                perAxisFactor.z = std::max(0.1f, std::min(factor, 5.0f));
                break;
            }
            default:
                break;
        }
    } else {
        ScaleAxis previousAxis = currentAxis;
        updateSelection(pts);

        ofVec2f currentCenter = centroidOfPoints(pts);
        float fallback = knobHasOrientation ? knobLastOrientation : 0.0f;
        float currentOrientation = stabilizedPrincipalAxisAngle(pts, fallback);

        if (!knobHasOrientation) {
            knobHasOrientation = true;
            knobLastOrientation = currentOrientation;
            knobLastCenter = currentCenter;
            return;
        }

        float centerJump = currentCenter.distance(knobLastCenter);
        if (centerJump > TUI3D::ScaleTool::JUMP_THRESHOLD) {
            knobLastCenter = currentCenter;
            knobLastOrientation = currentOrientation;
            return;
        }

        if (previousAxis != currentAxis) {
            knobLastOrientation = currentOrientation;
        }

        if (currentAxis == ScaleAxis::None) {
            knobLastOrientation = currentOrientation;
            knobLastCenter = currentCenter;
            return;
        }

        float delta = normalizeAngleRadians(currentOrientation - knobLastOrientation);
        knobLastOrientation = currentOrientation;
        knobLastCenter = currentCenter;

        float deltaDeg = delta * RAD_TO_DEG;
        if (std::abs(deltaDeg) < TUI3D::ScaleTool::KNOB_DEADZONE_DEG) {
            return;
        }
        if (std::abs(deltaDeg) > TUI3D::ScaleTool::KNOB_MAX_DELTA_DEG) {
            return;
        }
        float deltaFactor = deltaDeg / TUI3D::ScaleTool::KNOB_SENSITIVITY_DEG;

        auto clampFactor = [](float f) { return std::max(0.1f, std::min(f, 5.0f)); };
        switch (currentAxis) {
            case ScaleAxis::X:
                perAxisFactor.x = clampFactor(perAxisFactor.x + deltaFactor);
                break;
            case ScaleAxis::Y:
                perAxisFactor.y = clampFactor(perAxisFactor.y + deltaFactor);
                break;
            case ScaleAxis::Z:
                perAxisFactor.z = clampFactor(perAxisFactor.z + deltaFactor);
                break;
            case ScaleAxis::Uniform: {
                float base = (perAxisFactor.x + perAxisFactor.y + perAxisFactor.z) / 3.0f;
                float newFactor = clampFactor(base + deltaFactor);
                perAxisFactor = glm::vec3(newFactor);
                break;
            }
            default:
                break;
        }
    }

    // Aplicar factores acumulados a escala inicial
    scaleVec = initialScale * perAxisFactor;
}

void ScaleTool::updateWithIDs(const vector<TouchPointWithID>& pts) {
    vector<ofVec2f> coords;
    coords.reserve(pts.size());
    for (const auto& pt : pts) coords.push_back(pt.pos);

    if (interactionMode != ScaleInteractionMode::Knob) {
        update(coords);
        return;
    }

    // ✅ Actualizar animación de fade
    updateMenuFade();

    if (!locked || pts.empty()) return;

    ScaleAxis previousAxis = currentAxis;
    updateSelection(coords);

    ofVec2f currentCenter = centroidOfPoints(coords);

    ofVec2f refPos = knobLastRefPos;
    bool found = false;
    if (knobRefSessionId != -1) {
        for (const auto& pt : pts) {
            if (pt.sessionID == knobRefSessionId) {
                refPos = pt.pos;
                found = true;
                break;
            }
        }
    }
    if (!found && !pts.empty()) {
        float bestDist = std::numeric_limits<float>::max();
        for (const auto& pt : pts) {
            float d = pt.pos.distance(knobLastRefPos);
            if (d < bestDist) {
                bestDist = d;
                refPos = pt.pos;
            }
        }
    }

    float currentOrientation = atan2(refPos.y - currentCenter.y, refPos.x - currentCenter.x);
    if (!knobHasOrientation) {
        knobHasOrientation = true;
        knobLastOrientation = currentOrientation;
        knobLastCenter = currentCenter;
        knobLastRefPos = refPos;
        return;
    }

    float centerJump = currentCenter.distance(knobLastCenter);
    if (centerJump > TUI3D::ScaleTool::JUMP_THRESHOLD) {
        knobLastCenter = currentCenter;
        knobLastRefPos = refPos;
        knobLastOrientation = currentOrientation;
        knobHasOrientation = true;
        return;
    }

    if (previousAxis != currentAxis) {
        knobLastCenter = currentCenter;
        knobLastRefPos = refPos;
        knobLastOrientation = currentOrientation;
        return;
    }

    if (currentAxis == ScaleAxis::None) {
        knobLastCenter = currentCenter;
        knobLastRefPos = refPos;
        return;
    }

    float delta = normalizeAngleRadians(currentOrientation - knobLastOrientation);
    knobLastOrientation = currentOrientation;
    knobLastCenter = currentCenter;
    knobLastRefPos = refPos;

    float deltaDeg = delta * RAD_TO_DEG;
    if (std::abs(deltaDeg) < TUI3D::ScaleTool::KNOB_DEADZONE_DEG) {
        return;
    }
    if (std::abs(deltaDeg) > TUI3D::ScaleTool::KNOB_MAX_DELTA_DEG) {
        return;
    }
    float deltaFactor = deltaDeg / TUI3D::ScaleTool::KNOB_SENSITIVITY_DEG;

    auto clampFactor = [](float f) { return std::max(0.1f, std::min(f, 5.0f)); };
    switch (currentAxis) {
        case ScaleAxis::X:
            perAxisFactor.x = clampFactor(perAxisFactor.x + deltaFactor);
            break;
        case ScaleAxis::Y:
            perAxisFactor.y = clampFactor(perAxisFactor.y + deltaFactor);
            break;
        case ScaleAxis::Z:
            perAxisFactor.z = clampFactor(perAxisFactor.z + deltaFactor);
            break;
        case ScaleAxis::Uniform: {
            float base = (perAxisFactor.x + perAxisFactor.y + perAxisFactor.z) / 3.0f;
            float newFactor = clampFactor(base + deltaFactor);
            perAxisFactor = glm::vec3(newFactor);
            break;
        }
        default:
            break;
    }

    scaleVec = initialScale * perAxisFactor;
}
void ScaleTool::updateSelection(const vector<ofVec2f>& currentPts) {
    if (!lockedMenu) return;
    if (currentPts.empty()) { 
        currentAxis = ScaleAxis::None;  // Sin puntos = sin selección
        return; 
    }
    
    // Detectar toque en botones
    for (auto &pt : currentPts) {
        if (insideCircle(pt, buttonPos("X"))) { 
            currentAxis = ScaleAxis::X; 
            return; 
        }
        if (insideCircle(pt, buttonPos("Y"))) { 
            currentAxis = ScaleAxis::Y; 
            return; 
        }
        if (insideCircle(pt, buttonPos("Z"))) { 
            currentAxis = ScaleAxis::Z; 
            return; 
        }
        if (insideCircle(pt, buttonPos("ALL"))) { 
            currentAxis = ScaleAxis::Uniform; 
            return; 
        }
    }
    
    // Si llegamos aquí, ningún punto está encima de un botón
    // → Deseleccionar todo
    currentAxis = ScaleAxis::None;
}

ofVec2f ScaleTool::buttonPos(const string& axis) const {
    if (interactionMode == ScaleInteractionMode::Knob) {
        if (axis == "X") return fixedMenuCenter + ofVec2f(radialDistance, 0);
        if (axis == "Y") return fixedMenuCenter + ofVec2f(0, -radialDistance);
        if (axis == "Z") return fixedMenuCenter + ofVec2f(-radialDistance, 0);
        if (axis == "ALL") return fixedMenuCenter + ofVec2f(0, radialDistance);
        return fixedMenuCenter;
    }
    // Disposición vertical: X arriba, luego Y, Z, ALL hacia abajo
    if (axis == "X") return fixedMenuCenter + ofVec2f(0, -buttonSpacing * 1.5f);
    if (axis == "Y") return fixedMenuCenter + ofVec2f(0, -buttonSpacing * 0.5f);
    if (axis == "Z") return fixedMenuCenter + ofVec2f(0, buttonSpacing * 0.5f);
    if (axis == "ALL") return fixedMenuCenter + ofVec2f(0, buttonSpacing * 1.5f);
    return fixedMenuCenter;
}

bool ScaleTool::insideCircle(const ofVec2f& p, const ofVec2f& c) const { 
    if (interactionMode == ScaleInteractionMode::Knob) {
        return p.distance(c) <= buttonRadius;
    }
    float halfWidth = buttonWidth / 2.0f;
    float halfHeight = buttonHeight / 2.0f;
    return (p.x >= c.x - halfWidth && p.x <= c.x + halfWidth &&
            p.y >= c.y - halfHeight && p.y <= c.y + halfHeight);
}

void ScaleTool::drawButton(const string& label, const ofColor& color) {
    ofVec2f pos = buttonPos(label);

    // ✅ Aplicar alpha del fade a todos los elementos
    int fillAlpha = 60 * menuAlpha;  // MÁS transparente (era 120)
    int borderAlpha = 255 * menuAlpha;
    int selectionAlpha = 180 * menuAlpha;  // NUEVO: Alpha específico para rectángulo de selección

    if (interactionMode == ScaleInteractionMode::Knob) {
        // Dibujar círculo relleno con color del eje
        ofFill();
        ofSetColor(color.r, color.g, color.b, fillAlpha);
        ofDrawCircle(pos, buttonRadius);

        // Dibujar borde para mejor contraste
        ofNoFill();
        ofSetLineWidth(5);
        ofSetColor(color.r, color.g, color.b, borderAlpha);
        ofDrawCircle(pos, buttonRadius);
    } else {
        float x = pos.x - buttonWidth / 2.0f;
        float y = pos.y - buttonHeight / 2.0f;
        ofFill();
        ofSetColor(color.r, color.g, color.b, fillAlpha);
        ofDrawRectRounded(x, y, buttonWidth, buttonHeight, cornerRadius);

        ofNoFill();
        ofSetLineWidth(5);
        ofSetColor(color.r, color.g, color.b, borderAlpha);
        ofDrawRectRounded(x, y, buttonWidth, buttonHeight, cornerRadius);
    }

    // ✅ Texto con efecto bold híbrido: ofTrueTypeFont + múltiples dibujos
    ofSetColor(0, 0, 0, borderAlpha);  // Negro con fade
    ofPushMatrix();
    ofTranslate(pos);  // Mover al punto correcto
    // ✅ Efecto bold: dibujar múltiples veces con pequeños offsets
    for(int dx = 0; dx <= 1; dx++) {
        for(int dy = 0; dy <= 1; dy++) {
            buttonFont.drawString(label, -12 + dx, 8 + dy);
        }
    }
    ofPopMatrix();

    // Highlight si está seleccionado
    bool isSelected = false;
    if ((label == "X" && currentAxis == ScaleAxis::X) ||
        (label == "Y" && currentAxis == ScaleAxis::Y) ||
        (label == "Z" && currentAxis == ScaleAxis::Z) ||
        (label == "ALL" && currentAxis == ScaleAxis::Uniform)) {
        isSelected = true;
    }

    if (isSelected) {
        ofNoFill();
        ofSetLineWidth(8);
        ofSetColor(255, 230, 0, selectionAlpha);
        if (interactionMode == ScaleInteractionMode::Knob) {
            ofDrawCircle(pos, buttonRadius + 12);
        } else {
            float x = pos.x - buttonWidth / 2.0f;
            float y = pos.y - buttonHeight / 2.0f;
            float expandX = x - 12;
            float expandY = y - 12;
            ofDrawRectRounded(expandX, expandY, buttonWidth + 24, buttonHeight + 24, cornerRadius + 8);
        }
    }
}

void ScaleTool::drawMenu() {
    // ✅ Solo dibujar si hay algo visible (menuAlpha > 0)
    if (menuAlpha <= 0.0f) return;
    if (!menuActive || !lockedMenu) return;

    drawButton("X", ofColor::red);
    drawButton("Y", ofColor::green);
    drawButton("Z", ofColor::blue);
    drawButton("ALL", ofColor(200, 200, 200));
}

void ScaleTool::drawScaleBar() {
    // ✅ Solo dibujar si hay algo visible (menuAlpha > 0)
    if (menuAlpha <= 0.0f) return;
    if (!menuActive || !lockedMenu) return;

    int barAlpha = 255 * menuAlpha;  // Alpha para elementos de la barra

    ofVec2f barPos = fixedMenuCenter + ofVec2f(0,
        (interactionMode == ScaleInteractionMode::Knob ? radialDistance : buttonSpacing) + 90);
    float barWidth = 300;     // Aumentado de 200 a 300
    float barHeight = 15;     // Aumentado de 10 a 15

    // Fondo de barra
    ofFill();
    ofSetColor(80, 80, 80, barAlpha);
    ofDrawRectangle(barPos.x - barWidth/2, barPos.y, barWidth, barHeight);

    // Marcador central (1.0x)
    ofSetColor(150, 150, 150, barAlpha);
    ofDrawRectangle(barPos.x - 3, barPos.y - 8, 6, barHeight + 16);  // Más grande

    // Calcular posición del indicador
    float factor = getCurrentFactor();
    float normalizedFactor = ofMap(factor, 0.1, 5.0, 0, 1, true);
    float indicatorX = ofMap(normalizedFactor, 0, 1, -barWidth/2, barWidth/2);

    // Indicador de posición actual (más grande)
    ofSetColor(255, 200, 0, barAlpha);
    ofDrawCircle(barPos.x + indicatorX, barPos.y + barHeight/2, 12);  // Aumentado de 8 a 12

    // Texto con valor (más grande usando ofDrawBitmapStringHighlight)
    string axisName = getAxisName();
    string valueText = axisName + ": " + ofToString(factor, 2) + "x";

    ofSetColor(0, 0, 0, barAlpha);  // Fondo negro con fade
    ofDrawRectangle(barPos.x - 60, barPos.y + barHeight + 15, 120, 20);
    ofSetColor(255, 255, 255, barAlpha);  // Texto blanco con fade
    ofDrawBitmapString(valueText, barPos.x - 50, barPos.y + barHeight + 30);
}

float ScaleTool::getCurrentFactor() const {
    switch (currentAxis) {
        case ScaleAxis::X:
            return scaleVec.x / initialScale.x;
        case ScaleAxis::Y:
            return scaleVec.y / initialScale.y;
        case ScaleAxis::Z:
            return scaleVec.z / initialScale.z;
        case ScaleAxis::Uniform:
        default:
            return scaleVec.x / initialScale.x; // Asume escala uniforme
    }
}

string ScaleTool::getAxisName() const {
    switch (currentAxis) {
        case ScaleAxis::X: return "X";
        case ScaleAxis::Y: return "Y";
        case ScaleAxis::Z: return "Z";
        case ScaleAxis::Uniform: return "ALL";
        default: return "NONE";
    }
}

void ScaleTool::draw(int x, int y) { 
    ofDrawBitmapString("Scale: " + ofToString(scaleVec.x, 2), x, y); 
}

// ---------------- TRANSLATE TOOL ----------------
void TranslateTool::setLockedPoints(const vector<ofVec2f>& lockedPts, const glm::vec3& currentPos) {
    if (lockedPts.size() < 3) return;
    
    // Calcular centro del token
    ofVec2f c(0,0);
    for (auto &p : lockedPts) c += p;
    c /= (float)lockedPts.size();
    initialCenter = c;
    menuLastCenter = c;
    menuAxisAccum = 0.0f;
    initialPos = currentPos;
    perAxisDelta = glm::vec3(0.0f);  // Resetear deltas acumulados
    position = currentPos;
    
    // ✅ Aplicar responsividad: calcular radio máximo del menú
    float maxRadius = 0.0f;
    if (interactionMode == TranslateInteractionMode::Knob) {
        maxRadius = radialDistance + buttonRadius;
    } else {
        float maxRadiusH = buttonSpacingH + std::max(buttonWidthH / 2.0f, buttonHeightH / 2.0f);
        float maxRadiusV = buttonSpacingV + std::max(buttonWidthV / 2.0f, buttonHeightV / 2.0f);
        maxRadius = std::max(maxRadiusH, maxRadiusV);
    }
    fixedMenuCenter = clampMenuPosition(c, maxRadius);
    
    locked = true;
    currentAxis = TranslateAxis::None;
    gestureAxisChosen = false;
    gestureStart = c;
    knobHasOrientation = false;
    knobLastOrientation = 0.0f;
    knobLastCenter = c;

    if (interactionMode == TranslateInteractionMode::Menu ||
        interactionMode == TranslateInteractionMode::Knob) {
        lockedMenu = true;
        menuActive = true;
        showMenu();
    } else {
        lockedMenu = false;
        menuActive = false;
        menuState = MENU_HIDDEN;
        menuAlpha = 0.0f;
    }

    ofLogNotice("TranslateTool") << "Translate token locked ("
        << (interactionMode == TranslateInteractionMode::Gesture ? "gesture mode" :
            interactionMode == TranslateInteractionMode::Knob ? "knob mode" : "menu mode")
        << ")";
}

void TranslateTool::setCameraBasis(const glm::vec3& right, const glm::vec3& up, const glm::vec3& forward) {
    // Normalizar para evitar escalado inesperado al combinar con sensibilidad
    basisRight = glm::normalize(right);
    basisUp = glm::normalize(up);
    basisForward = glm::normalize(forward);
}

void TranslateTool::setInteractionMode(TranslateInteractionMode mode) {
    if (interactionMode == mode) return;
    interactionMode = mode;

    // Reset axis selection and fade states when switching
    currentAxis = TranslateAxis::None;
    gestureAxisChosen = false;
    perAxisDelta = glm::vec3(0.0f);
    gestureStart = initialCenter;
    menuLastCenter = initialCenter;
    menuAxisAccum = 0.0f;
    knobHasOrientation = false;
    knobLastOrientation = 0.0f;
    knobLastCenter = initialCenter;

    if ((interactionMode == TranslateInteractionMode::Menu ||
         interactionMode == TranslateInteractionMode::Knob) && locked) {
        lockedMenu = true;
        menuActive = true;
        showMenu();
    } else {
        lockedMenu = false;
        menuActive = false;
        menuState = MENU_HIDDEN;
        menuAlpha = 0.0f;
    }

    ofLogNotice("TranslateTool") << "Interaction mode set to " 
        << (interactionMode == TranslateInteractionMode::Gesture ? "gesture" :
            interactionMode == TranslateInteractionMode::Knob ? "knob" : "menu");
}

void TranslateTool::reset() {
    locked = false;
    lockedMenu = false;
    menuActive = false;
    initialCenter = ofVec2f(0,0);
    menuLastCenter = ofVec2f(0,0);
    menuAxisAccum = 0.0f;
    initialPos = glm::vec3(0.0f);
    perAxisDelta = glm::vec3(0.0f);
    position = glm::vec3(0.0f);
    currentAxis = TranslateAxis::None;
    gestureAxisChosen = false;
    gestureStart = ofVec2f(0, 0);
    knobHasOrientation = false;
    knobLastOrientation = 0.0f;
    knobLastCenter = ofVec2f(0, 0);
    menuState = MENU_HIDDEN;
    menuAlpha = 0.0f;

    // ✅ Iniciar fade-out del menú
    hideMenu();
}

void TranslateTool::update(const vector<ofVec2f>& pts, RotationAxis) {
    // ✅ Actualizar animación de fade
    updateMenuFade();

    if (!locked || pts.empty()) return;

    auto applyDeadzone = [&](float value) {
        float absVal = std::abs(value);
        if (absVal <= motionDeadzone) return 0.0f;
        float trimmed = absVal - motionDeadzone;
        return (value >= 0 ? trimmed : -trimmed);
    };

    // Selección de eje según modo de interacción
    if (interactionMode == TranslateInteractionMode::Menu) {
        // Actualizar selección de eje por botones
        TranslateAxis previousAxis = currentAxis;
        updateSelection(pts);
        
        // Calcular centro actual del token
        ofVec2f currentCenter(0,0);
        for (auto &p: pts) currentCenter += p;
        currentCenter /= (float)pts.size();

        float centerJump = currentCenter.distance(menuLastCenter);
        if (centerJump > TUI3D::TranslateTool::JUMP_THRESHOLD) {
            menuLastCenter = currentCenter;
            menuAxisAccum = 0.0f;
            return;
        }

        // Re-ancorar acumulador al cambiar de eje o al entrar sin selección
        if (previousAxis != currentAxis) {
            menuLastCenter = currentCenter;
            menuAxisAccum = 0.0f;
        }
        
        // Si no hay eje seleccionado, mantener posición actual pero refrescar referencia
        if (currentAxis == TranslateAxis::None) { 
            menuLastCenter = currentCenter; 
            menuAxisAccum = 0.0f;
            return; 
        }
        
        // ✅ Calcular desplazamiento incremental desde el último frame (sin perder pasos lentos)
        float deltaX = currentCenter.x - menuLastCenter.x;
        float deltaY = currentCenter.y - menuLastCenter.y;
        menuLastCenter = currentCenter;

        auto resetIfFlipped = [&](float accum, float delta) {
            if (accum != 0.0f && delta != 0.0f && ((accum > 0) != (delta > 0))) {
                return delta;  // Re-ancorar al cambiar de sentido para evitar "deshacer" el movimiento
            }
            return accum + delta;
        };

        auto consumeAccum = [&](float& accum) -> float {
            float absAccum = std::abs(accum);
            if (absAccum <= motionDeadzone) return 0.0f;
            float applied = (accum > 0) ? (accum - motionDeadzone) : (accum + motionDeadzone);
            accum = 0.0f;
            return applied;
        };

        auto axisToScreenDir = [&](TranslateAxis axisSel) -> ofVec2f {
            glm::vec3 axisWorld;
            switch (axisSel) {
                case TranslateAxis::X: axisWorld = glm::vec3(1, 0, 0); break;
                case TranslateAxis::Y: axisWorld = glm::vec3(0, 1, 0); break;
                case TranslateAxis::Z: axisWorld = glm::vec3(0, 0, 1); break;
                default: return ofVec2f(0, 0);
            }
            float sx = glm::dot(axisWorld, basisRight);
            float sy = -glm::dot(axisWorld, basisUp); // pantalla: +y hacia abajo
            float len = std::sqrt(sx * sx + sy * sy);
            if (len < 0.0001f) {
                float sign = (glm::dot(axisWorld, basisForward) >= 0.0f) ? 1.0f : -1.0f;
                return ofVec2f(sign, 0.0f);
            }
            return ofVec2f(sx / len, sy / len);
        };

        ofVec2f screenDir = axisToScreenDir(currentAxis);
        float axisDelta = deltaX * screenDir.x + deltaY * screenDir.y;

        // Actualizar delta acumulado según eje seleccionado
        switch (currentAxis) {
            case TranslateAxis::X: {
                menuAxisAccum = resetIfFlipped(menuAxisAccum, axisDelta);
                float applied = consumeAccum(menuAxisAccum);
                perAxisDelta.x += applied / sensitivity;
                break;
            }
            case TranslateAxis::Y: {
                menuAxisAccum = resetIfFlipped(menuAxisAccum, axisDelta);
                float applied = consumeAccum(menuAxisAccum);
                perAxisDelta.y += applied / sensitivity;
                break;
            }
            case TranslateAxis::Z: {
                menuAxisAccum = resetIfFlipped(menuAxisAccum, axisDelta);
                float applied = consumeAccum(menuAxisAccum);
                perAxisDelta.z += applied / sensitivity;
                break;
            }
            default:
                break;
        }
    } else if (interactionMode == TranslateInteractionMode::Gesture) {
        // Modo de gestos: el primer movimiento define el eje
        ofVec2f currentCenter(0,0);
        for (auto &p: pts) currentCenter += p;
        currentCenter /= (float)pts.size();

        ofVec2f delta = currentCenter - gestureStart;
        float deltaLen = delta.length();

        // Permitir seleccionar o CAMBIAR de eje en cualquier momento según la dirección dominante
        if (deltaLen >= gestureThreshold || currentAxis != TranslateAxis::None) {
            float absX = std::abs(delta.x);
            float absY = std::abs(delta.y);

            TranslateAxis chosenAxis = currentAxis;
            if (deltaLen >= gestureThreshold) {
                if (absX > absY * dominantAxisRatio) {
                    chosenAxis = TranslateAxis::X;
                } else if (absY > absX * dominantAxisRatio) {
                    chosenAxis = TranslateAxis::Y;
                } else {
                    chosenAxis = TranslateAxis::Z;  // Movimiento diagonal
                }
            }

            if (chosenAxis != currentAxis) {
                currentAxis = chosenAxis;
                gestureAxisChosen = true;
                gestureStart = currentCenter;  // Re-ancorar para evitar saltos al cambiar de eje
                delta.set(0, 0);               // Delta arranca en 0 para el nuevo eje
                ofLogNotice("TranslateTool") << "Gesture axis set to: "
                                             << (currentAxis == TranslateAxis::X ? "X" :
                                                 currentAxis == TranslateAxis::Y ? "Y" : "Z");
            }
        } else {
            return;  // Aún no hay movimiento suficiente para elegir eje
        }

        // Calcular delta respecto al punto donde se fijó el eje actual
        float deltaX = applyDeadzone(currentCenter.x - gestureStart.x);
        float deltaY = applyDeadzone(currentCenter.y - gestureStart.y);

        switch (currentAxis) {
            case TranslateAxis::X:
                perAxisDelta.x = deltaX / sensitivity;
                break;
            case TranslateAxis::Y:
                perAxisDelta.y = -deltaY / sensitivity;
                break;
            case TranslateAxis::Z: {
                // Usar proyección en la diagonal para un control consistente
                float diag = (deltaX + deltaY) * 0.70710678f; // ~1/sqrt(2)
                perAxisDelta.z = diag / sensitivity;
                break;
            }
            default:
                break;
        }
    } else {
        TranslateAxis previousAxis = currentAxis;
        updateSelection(pts);

        ofVec2f currentCenter = centroidOfPoints(pts);
        float fallback = knobHasOrientation ? knobLastOrientation : 0.0f;
        float currentOrientation = stabilizedPrincipalAxisAngle(pts, fallback);

        if (!knobHasOrientation) {
            knobHasOrientation = true;
            knobLastOrientation = currentOrientation;
            knobLastCenter = currentCenter;
            return;
        }

        float centerJump = currentCenter.distance(knobLastCenter);
        if (centerJump > TUI3D::TranslateTool::JUMP_THRESHOLD) {
            knobLastCenter = currentCenter;
            knobLastOrientation = currentOrientation;
            return;
        }

        if (previousAxis != currentAxis) {
            knobLastOrientation = currentOrientation;
        }

        if (currentAxis == TranslateAxis::None) {
            knobLastOrientation = currentOrientation;
            knobLastCenter = currentCenter;
            return;
        }

        float delta = normalizeAngleRadians(currentOrientation - knobLastOrientation);
        knobLastOrientation = currentOrientation;
        knobLastCenter = currentCenter;

        float deltaDeg = delta * RAD_TO_DEG;
        if (std::abs(deltaDeg) < TUI3D::TranslateTool::KNOB_DEADZONE_DEG) {
            return;
        }
        if (std::abs(deltaDeg) > TUI3D::TranslateTool::KNOB_MAX_DELTA_DEG) {
            return;
        }
        float deltaMove = deltaDeg / TUI3D::TranslateTool::KNOB_SENSITIVITY_DEG;

        switch (currentAxis) {
            case TranslateAxis::X:
                perAxisDelta.x += deltaMove;
                break;
            case TranslateAxis::Y:
                perAxisDelta.y += deltaMove;
                break;
            case TranslateAxis::Z:
                perAxisDelta.z += deltaMove;
                break;
            default:
                break;
        }
    }

    // Aplicar deltas acumulados a posición inicial
    if (interactionMode == TranslateInteractionMode::Gesture) {
        // Proyectar movimientos sobre la base de cámara (dependen de la vista)
        glm::vec3 worldOffset =
            basisRight * perAxisDelta.x +
            basisUp * perAxisDelta.y +
            basisForward * perAxisDelta.z;
        position = initialPos + worldOffset;
    } else {
        // Modo menú: ejes globales del mundo
        position = initialPos + perAxisDelta;
    }
}

void TranslateTool::draw(int x, int y) {
    string axisName = "None";
    if (currentAxis == TranslateAxis::X) axisName = "X";
    else if (currentAxis == TranslateAxis::Y) axisName = "Y";
    else if (currentAxis == TranslateAxis::Z) axisName = "Z";
    
    ofDrawBitmapString("Translate [" + axisName + "]: " + 
                       ofToString(position.x,1) + ", " + 
                       ofToString(position.y,1) + ", " + 
                       ofToString(position.z,1), x, y);
}

void TranslateTool::drawMenu() {
    // ✅ Solo dibujar si hay algo visible (menuAlpha > 0)
    if (menuAlpha <= 0.0f) return;
    if (!menuActive || !lockedMenu) return;
    if (interactionMode == TranslateInteractionMode::Gesture) return;

    drawButton("X", ofColor::red);
    drawButton("Y", ofColor::green);
    drawButton("Z", ofColor::blue);
}

void TranslateTool::updateSelection(const vector<ofVec2f>& currentPts) {
    if (!lockedMenu) return;
    if (currentPts.empty()) { 
        currentAxis = TranslateAxis::None; 
        return; 
    }
    
    // Detectar qué eje está siendo tocado (usando detección de rectángulo)
    for (auto &pt : currentPts) {
        if (insideCircle(pt, buttonPos("X"))) { 
            currentAxis = TranslateAxis::X; 
            return; 
        }
        if (insideCircle(pt, buttonPos("Y"))) { 
            currentAxis = TranslateAxis::Y; 
            return; 
        }
        if (insideCircle(pt, buttonPos("Z"))) { 
            currentAxis = TranslateAxis::Z; 
            return; 
        }
    }
    
    // Si no tocó ningún botón, deseleccionar
    currentAxis = TranslateAxis::None;
}

ofVec2f TranslateTool::buttonPos(const string& axis) const {
    if (interactionMode == TranslateInteractionMode::Knob) {
        if (axis == "X") return fixedMenuCenter + ofVec2f(radialDistance, 0);
        if (axis == "Y") return fixedMenuCenter + ofVec2f(0, -radialDistance);
        if (axis == "Z") return fixedMenuCenter + ofVec2f(-radialDistance, 0);
        return fixedMenuCenter;
    }
    // X e Z: rectángulos horizontales (izquierda y derecha)
    // Y: rectángulo vertical (arriba)
    if (axis == "X") return fixedMenuCenter + ofVec2f(-buttonSpacingH, 0);
    if (axis == "Y") return fixedMenuCenter + ofVec2f(0, -buttonSpacingV);
    if (axis == "Z") return fixedMenuCenter + ofVec2f(buttonSpacingH, 0);
    return fixedMenuCenter;
}

bool TranslateTool::insideCircle(const ofVec2f& p, const ofVec2f& c) const { 
    if (interactionMode == TranslateInteractionMode::Knob) {
        return p.distance(c) <= buttonRadius;
    }
    // Detección de toque en rectángulo redondeado
    float halfWidth, halfHeight;
    if (c.y < fixedMenuCenter.y) {
        halfWidth = buttonWidthV / 2.0f;
        halfHeight = buttonHeightV / 2.0f;
    } else {
        halfWidth = buttonWidthH / 2.0f;
        halfHeight = buttonHeightH / 2.0f;
    }
    return (p.x >= c.x - halfWidth && p.x <= c.x + halfWidth &&
            p.y >= c.y - halfHeight && p.y <= c.y + halfHeight);
}

void TranslateTool::drawButton(const string& label, const ofColor& color) {
    ofVec2f pos = buttonPos(label);
    
    // ✅ Aplicar alpha del fade a todos los elementos
    int fillAlpha = 60 * menuAlpha;  // MÁS transparente (era 120)
    int borderAlpha = 255 * menuAlpha;
    int selectionAlpha = 180 * menuAlpha;  // NUEVO: Alpha específico para rectángulo de selección

    float w = buttonWidthH;
    float h = buttonHeightH;
    if (label == "Y") {
        w = buttonWidthV;
        h = buttonHeightV;
    }

    if (interactionMode == TranslateInteractionMode::Knob) {
        ofFill();
        ofSetColor(color.r, color.g, color.b, fillAlpha);
        ofDrawCircle(pos, buttonRadius);

        ofNoFill();
        ofSetLineWidth(4);
        ofSetColor(color.r, color.g, color.b, borderAlpha);
        ofDrawCircle(pos, buttonRadius);
    } else {
        float x = pos.x - w / 2.0f;
        float y = pos.y - h / 2.0f;
        ofFill();
        ofSetColor(color.r, color.g, color.b, fillAlpha);
        ofDrawRectRounded(x, y, w, h, cornerRadius);

        ofNoFill();
        ofSetLineWidth(4);
        ofSetColor(color.r, color.g, color.b, borderAlpha);
        ofDrawRectRounded(x, y, w, h, cornerRadius);
    }

    // ✅ Texto con efecto bold híbrido: ofTrueTypeFont + múltiples dibujos
    ofSetColor(0, 0, 0, borderAlpha);  // Negro con fade
    ofPushMatrix();
    ofTranslate(pos);  // Mover al punto correcto
    // ✅ Efecto bold: dibujar múltiples veces con pequeños offsets
    for(int dx = 0; dx <= 1; dx++) {
        for(int dy = 0; dy <= 1; dy++) {
            buttonFont.drawString(label, -8 + dx, 8 + dy);
        }
    }
    ofPopMatrix();

    // Highlight si está seleccionado (semi-transparente pero visible)
    if ((label == "X" && currentAxis == TranslateAxis::X) ||
        (label == "Y" && currentAxis == TranslateAxis::Y) ||
        (label == "Z" && currentAxis == TranslateAxis::Z)) {
        ofNoFill();
        ofSetLineWidth(7);
        ofSetColor(255, 230, 0, selectionAlpha);  // Usa selectionAlpha (180 vs 120)
        if (interactionMode == TranslateInteractionMode::Knob) {
            ofDrawCircle(pos, buttonRadius + 12);
        } else {
            float x = pos.x - w / 2.0f;
            float y = pos.y - h / 2.0f;
            float expandX = x - 12;
            float expandY = y - 12;
            ofDrawRectRounded(expandX, expandY, w + 24, h + 24, cornerRadius + 8);
        }
    }
}

// ---------------- SPAWN TOOL ----------------
void SpawnTool::setLockedPoints(const vector<ofVec2f>& lockedPts) {
    if (lockedPts.empty()) return;
    
    // Calcular centro del token
    ofVec2f center(0, 0);
    for (auto& p : lockedPts) {
        center += p;
    }
    center /= (float)lockedPts.size();
    
    // ✅ Aplicar responsividad: radio máximo = radialDistance + buttonRadius
    float maxRadius = radialDistance + buttonRadius;
    fixedMenuCenter = clampMenuPosition(center, maxRadius);

    lockedMenu = true;
    menuActive = true;
    selectedShape = SpawnShape::Box;
    selectionMade = false;

    // ✅ Iniciar fade-in del menú
    showMenu();

    ofLogNotice("SpawnTool") << "Spawn menu activated (radial layout)";
}

void SpawnTool::reset() {
    lockedMenu = false;
    menuActive = false;
    selectedShape = SpawnShape::Box;
    selectionMade = false;

    // ✅ Iniciar fade-out del menú
    hideMenu();
}

void SpawnTool::update(const vector<ofVec2f>& pts, RotationAxis) {
    // ✅ Actualizar animación de fade
    updateMenuFade();

    updateSelection(pts);
}

void SpawnTool::updateSelection(const vector<ofVec2f>& currentPts) {
    if (!lockedMenu || currentPts.empty()) return;
    
    bool touchedAny = false;  // Flag para detectar si tocó algún círculo
    
    // Detectar selección de tipo de figura en círculos radiales
    for (auto& pt : currentPts) {
        if (insideCircle(pt, buttonPos("BOX"))) {
            selectedShape = SpawnShape::Box;
            selectionMade = true;
            touchedAny = true;
            return;
        }
        if (insideCircle(pt, buttonPos("CYL"))) {
            selectedShape = SpawnShape::Cylinder;
            selectionMade = true;
            touchedAny = true;
            return;
        }
        if (insideCircle(pt, buttonPos("CONE"))) {
            selectedShape = SpawnShape::Cone;
            selectionMade = true;
            touchedAny = true;
            return;
        }
        if (insideCircle(pt, buttonPos("SPH"))) {
            selectedShape = SpawnShape::Sphere;
            selectionMade = true;
            touchedAny = true;
            return;
        }
    }
    
    // Si no tocó ningún círculo, deseleccionar
    if (!touchedAny) {
        selectionMade = false;
    }
}

ofVec2f SpawnTool::buttonPos(const string& label) const {
    // Layout radial en arco semicircular superior
    // Ángulos: 150°, 110°, 70°, 30° (de izquierda a derecha)
    // Espaciamiento uniforme de 40° entre círculos
    // IMPORTANTE: Negar Y porque en openFrameworks +Y va hacia ABAJO
    if (label == "BOX") {
        float angle = 150 * DEG_TO_RAD;  // 150°
        return fixedMenuCenter + ofVec2f(cos(angle) * radialDistance, -sin(angle) * radialDistance);
    }
    if (label == "CYL") {
        float angle = 110 * DEG_TO_RAD;  // 110°
        return fixedMenuCenter + ofVec2f(cos(angle) * radialDistance, -sin(angle) * radialDistance);
    }
    if (label == "CONE") {
        float angle = 70 * DEG_TO_RAD;  // 70°
        return fixedMenuCenter + ofVec2f(cos(angle) * radialDistance, -sin(angle) * radialDistance);
    }
    if (label == "SPH") {
        float angle = 30 * DEG_TO_RAD;  // 30°
        return fixedMenuCenter + ofVec2f(cos(angle) * radialDistance, -sin(angle) * radialDistance);
    }
    return fixedMenuCenter;
}

bool SpawnTool::insideCircle(const ofVec2f& p, const ofVec2f& c) const {
    return p.distance(c) < buttonRadius;
}

void SpawnTool::drawButton(const string& label, const ofColor& color) {
    ofVec2f pos = buttonPos(label);

    // ✅ Aplicar alpha del fade a todos los elementos
    int fillAlpha = 60 * menuAlpha;  // MÁS transparente (era 120)
    int borderAlpha = 255 * menuAlpha;
    int selectionAlpha = 180 * menuAlpha;  // NUEVO: Alpha específico para círculo de selección

    // Dibujar círculo relleno con color
    ofFill();
    ofSetColor(color.r, color.g, color.b, fillAlpha);  // Semi-transparente con fade
    ofDrawCircle(pos, buttonRadius);

    // Dibujar borde para mejor contraste
    ofNoFill();
    ofSetLineWidth(3);
    ofSetColor(color.r, color.g, color.b, borderAlpha);
    ofDrawCircle(pos, buttonRadius);

    // ✅ Dibujar icono 3D de la figura geométrica
    SpawnShape shape = SpawnShape::Box;
    if (label == "BOX") shape = SpawnShape::Box;
    else if (label == "CYL") shape = SpawnShape::Cylinder;
    else if (label == "CONE") shape = SpawnShape::Cone;
    else if (label == "SPH") shape = SpawnShape::Sphere;

    drawShapeIcon(shape, pos, 40.0f);  // Tamaño del icono 3D

    // Highlight si está seleccionado
    bool isSelected = false;
    if ((label == "BOX" && selectedShape == SpawnShape::Box) ||
        (label == "CYL" && selectedShape == SpawnShape::Cylinder) ||
        (label == "CONE" && selectedShape == SpawnShape::Cone) ||
        (label == "SPH" && selectedShape == SpawnShape::Sphere)) {
        isSelected = true;
    }

    if (isSelected && selectionMade) {
        ofNoFill();
        ofSetLineWidth(5);
        ofSetColor(255, 230, 0, selectionAlpha);  // Usa selectionAlpha (180 vs 120)
        ofDrawCircle(pos, buttonRadius + 12);
    }
}

void SpawnTool::drawShapeIcon(SpawnShape shape, const ofVec2f& pos, float size) const {
    ofPushMatrix();
    ofPushStyle();

    // ========== SETUP ILUMINACIÓN LOCAL PARA ICONOS ==========
    ofEnableLighting();

    // Crear 3 luces locales temporales (simplificado)
    ofLight iconKeyLight, iconFillLight, iconRimLight;

    // 1. KEY LIGHT (Principal - frontal-superior-derecha)
    iconKeyLight.setup();
    iconKeyLight.setPointLight();
    iconKeyLight.setPosition(pos.x + 200, pos.y - 300, 400);
    iconKeyLight.setDiffuseColor(ofColor(200, 200, 200));
    iconKeyLight.setSpecularColor(ofColor(0, 0, 0));  // Mate
    iconKeyLight.setAttenuation(1.0f, 0.0f, 0.0f);

    // 2. FILL LIGHT (Relleno - frontal-inferior-izquierda)
    iconFillLight.setup();
    iconFillLight.setPointLight();
    iconFillLight.setPosition(pos.x - 150, pos.y, 200);
    iconFillLight.setDiffuseColor(ofColor(100, 100, 100));
    iconFillLight.setSpecularColor(ofColor(0, 0, 0));  // Mate
    iconFillLight.setAttenuation(1.0f, 0.0f, 0.0f);

    // 3. RIM LIGHT (Contorno - trasera-superior)
    iconRimLight.setup();
    iconRimLight.setPointLight();
    iconRimLight.setPosition(pos.x - 100, pos.y - 250, -150);
    iconRimLight.setDiffuseColor(ofColor(120, 120, 120));
    iconRimLight.setSpecularColor(ofColor(0, 0, 0));  // Mate
    iconRimLight.setAttenuation(1.0f, 0.0f, 0.0f);

    // Activar las 3 luces
    iconKeyLight.enable();
    iconFillLight.enable();
    iconRimLight.enable();

    // ========== MATERIAL MATE PARA ICONOS ==========
    ofMaterial iconMaterial;
    iconMaterial.setDiffuseColor(ofColor(180, 180, 180));  // Gris medio
    iconMaterial.setAmbientColor(ofColor(60, 60, 60));     // Ambiente oscuro
    iconMaterial.setSpecularColor(ofColor(0, 0, 0));       // SIN brillos
    iconMaterial.setShininess(0);                           // Completamente mate
    iconMaterial.begin();

    // ========== RENDERIZAR ICONO 3D ==========
    ofEnableDepthTest();

    // Mover al centro del botón
    ofTranslate(pos.x, pos.y, 0);

    // Aplicar rotación isométrica para mejor percepción 3D
    ofRotateDeg(30, 1, 0, 0);   // Rotación en X
    ofRotateDeg(-25, 0, 1, 0);  // Rotación en Y

    switch(shape) {
        case SpawnShape::Box: {
            ofBoxPrimitive iconBox;
            iconBox.set(size);
            iconBox.setResolution(8);  // Genera normales suaves (mismo que Shape3D)
            iconBox.draw();
            break;
        }

        case SpawnShape::Cylinder: {
            ofRotateDeg(90, 1, 0, 0);  // Cilindro horizontal
            ofCylinderPrimitive iconCylinder;
            iconCylinder.set(size * 0.4f, size, 32, 8);  // radius, height, radiusSegments, heightSegments
            iconCylinder.draw();
            break;
        }

        case SpawnShape::Cone: {
            ofRotateDeg(180, 1, 0, 0);  // Cono apuntando hacia arriba
            ofConePrimitive iconCone;
            iconCone.set(size * 0.5f, size, 32, 8, 4);  // radius, height, radiusSegments, heightSegments, capSegments
            iconCone.draw();
            break;
        }

        case SpawnShape::Sphere: {
            ofSpherePrimitive iconSphere;
            iconSphere.set(size * 0.5f, 64);  // radius, resolution
            iconSphere.draw();
            break;
        }
    }

    // ========== CLEANUP ==========
    iconMaterial.end();

    // Desactivar las 3 luces locales
    iconKeyLight.disable();
    iconFillLight.disable();
    iconRimLight.disable();

    ofDisableLighting();
    ofDisableDepthTest();

    ofPopStyle();
    ofPopMatrix();
}

void SpawnTool::drawMenu() {
    // ✅ Solo dibujar si hay algo visible (menuAlpha > 0)
    if (menuAlpha <= 0.0f) return;
    if (!menuActive || !lockedMenu) return;

    // Círculos radiales en arco superior
    drawButton("BOX", ofColor(150, 150, 255));
    drawButton("CYL", ofColor(150, 255, 150));
    drawButton("CONE", ofColor(255, 200, 100));
    drawButton("SPH", ofColor(255, 150, 255));
}

void SpawnTool::draw(int x, int y) {
    string shapeName = "Box";
    switch (selectedShape) {
        case SpawnShape::Box: shapeName = "Box"; break;
        case SpawnShape::Cylinder: shapeName = "Cylinder"; break;
        case SpawnShape::Cone: shapeName = "Cone"; break;
        case SpawnShape::Sphere: shapeName = "Sphere"; break;
    }
    ofDrawBitmapString("SpawnTool: " + shapeName + (selectionMade ? " *" : ""), x, y);
}

// ---------------- UTILITY TOOL ----------------
void UtilityTool::setLockedPoints(const vector<ofVec2f>& lockedPts) {
    if (lockedPts.empty()) return;
    
    // Calcular centro del token
    ofVec2f center(0, 0);
    for (auto& p : lockedPts) {
        center += p;
    }
    center /= (float)lockedPts.size();
    
    // ✅ Aplicar responsividad: radio máximo = radialDistance + buttonRadius
    float maxRadius = radialDistance + buttonRadius;
    fixedMenuCenter = clampMenuPosition(center, maxRadius);

    lockedMenu = true;
    menuActive = true;
    selectedAction = UtilityAction::None;

    // ✅ Iniciar fade-in del menú
    showMenu();

    ofLogNotice("UtilityTool") << "Utility menu activated (radial layout)";
}

void UtilityTool::reset() {
    lockedMenu = false;
    menuActive = false;
    selectedAction = UtilityAction::None;

    // ✅ Iniciar fade-out del menú
    hideMenu();
}

void UtilityTool::update(const vector<ofVec2f>& pts, RotationAxis) {
    // ✅ Actualizar animación de fade
    updateMenuFade();

    updateSelection(pts);
}

void UtilityTool::updateSelection(const vector<ofVec2f>& currentPts) {
    if (!lockedMenu || currentPts.empty()) {
        selectedAction = UtilityAction::None;
        return;
    }
    
    // Detectar selección en círculos radiales
    for (auto& pt : currentPts) {
        if (insideCircle(pt, buttonPos("Eliminar"))) {
            selectedAction = UtilityAction::Delete;
            return;
        }
        if (insideCircle(pt, buttonPos("Deshacer"))) {
            selectedAction = UtilityAction::Undo;
            return;
        }
        if (insideCircle(pt, buttonPos("Resetear"))) {
            selectedAction = UtilityAction::Reset;
            return;
        }
    }
    
    // Si no tocó ningún círculo, deseleccionar
    selectedAction = UtilityAction::None;
}

ofVec2f UtilityTool::buttonPos(const string& label) const {
    // Layout radial en arco semicircular superior (como SpawnTool)
    // Eliminar: 140° (izquierda del arco)
    // Deshacer: 90° (centro del arco)
    // Resetear: 40° (derecha del arco)
    // Espaciamiento uniforme de 50° entre círculos
    if (label == "Eliminar") {
        float angle = 140 * DEG_TO_RAD;
        return fixedMenuCenter + ofVec2f(cos(angle) * radialDistance, -sin(angle) * radialDistance);
    }
    if (label == "Deshacer") {
        float angle = 90 * DEG_TO_RAD;
        return fixedMenuCenter + ofVec2f(cos(angle) * radialDistance, -sin(angle) * radialDistance);
    }
    if (label == "Resetear") {
        float angle = 40 * DEG_TO_RAD;
        return fixedMenuCenter + ofVec2f(cos(angle) * radialDistance, -sin(angle) * radialDistance);
    }
    return fixedMenuCenter;
}

bool UtilityTool::insideCircle(const ofVec2f& p, const ofVec2f& c) const {
    return p.distance(c) < buttonRadius;
}

void UtilityTool::drawButton(const string& label, const ofColor& color) {
    ofVec2f pos = buttonPos(label);

    // ✅ Aplicar alpha del fade a todos los elementos
    int fillAlpha = 60 * menuAlpha;  // MÁS transparente (era 120)
    int borderAlpha = 255 * menuAlpha;
    int selectionAlpha = 180 * menuAlpha;  // NUEVO: Alpha específico para círculo de selección

    // Dibujar círculo relleno con color
    ofFill();
    ofSetColor(color.r, color.g, color.b, fillAlpha);  // Semi-transparente con fade
    ofDrawCircle(pos, buttonRadius);

    // Dibujar borde para mejor contraste
    ofNoFill();
    ofSetLineWidth(3);
    ofSetColor(color.r, color.g, color.b, borderAlpha);
    ofDrawCircle(pos, buttonRadius);

    // ✅ Texto con efecto bold híbrido: ofTrueTypeFont + múltiples dibujos
    ofSetColor(0, 0, 0, borderAlpha);  // Negro con fade
    ofPushMatrix();
    ofTranslate(pos);  // Mover al punto correcto

    // ✅ CENTRADO DINÁMICO: Calcular ancho real del texto
    ofRectangle bbox = buttonFont.getStringBoundingBox(label, 0, 0);
    float textWidth = bbox.width;
    float textHeight = bbox.height;
    float offsetX = -textWidth / 2.0f;  // Centrar horizontalmente
    float offsetY = textHeight / 2.0f;  // Centrar verticalmente (baseline)

    // ✅ Efecto bold: dibujar múltiples veces con pequeños offsets
    for(int dx = 0; dx <= 1; dx++) {
        for(int dy = 0; dy <= 1; dy++) {
            buttonFont.drawString(label, offsetX + dx, offsetY + dy);
        }
    }
    ofPopMatrix();

    // Highlight si está seleccionado
    bool isSelected = false;
    if ((label == "Eliminar" && selectedAction == UtilityAction::Delete) ||
        (label == "Deshacer" && selectedAction == UtilityAction::Undo) ||
        (label == "Resetear" && selectedAction == UtilityAction::Reset)) {
        isSelected = true;
    }

    if (isSelected) {
        ofNoFill();
        ofSetLineWidth(5);
        ofSetColor(255, 230, 0, selectionAlpha);  // Usa selectionAlpha (180 vs 120)
        ofDrawCircle(pos, buttonRadius + 12);
    }
}

void UtilityTool::drawMenu() {
    // ✅ Solo dibujar si hay algo visible (menuAlpha > 0)
    if (menuAlpha <= 0.0f) return;
    if (!menuActive || !lockedMenu) return;

    // Círculos radiales
    drawButton("Eliminar", ofColor(255, 100, 100));    // Rojo
    drawButton("Deshacer", ofColor(255, 230, 100));   // Amarillo
    drawButton("Resetear", ofColor(100, 150, 255));  // Azul
}

void UtilityTool::draw(int x, int y) {
    string actionName = "None";
    switch (selectedAction) {
        case UtilityAction::Delete: actionName = "Delete"; break;
        case UtilityAction::Undo: actionName = "Undo"; break;
        case UtilityAction::Reset: actionName = "Reset"; break;
        default: break;
    }
    ofDrawBitmapString("UtilityTool: " + actionName, x, y);
}

// ---------------- SELECTION TOOL ----------------
void SelectionTool::update(const vector<ofVec2f>& pts, RotationAxis) {
    if (pts.empty()) return;
    
    // Calcular centro del token
    ofVec2f center(0, 0);
    for (auto& p : pts) {
        center += p;
    }
    center /= (float)pts.size();
    
    lastTokenCenter = center;
    hasPerformedSelection = true;
}

void SelectionTool::draw(int x, int y) {
    if (hasSelection()) {
        ofDrawBitmapString("Objeto seleccionado: #" + ofToString(selectedIndex), x, y);
    } else {
        ofDrawBitmapString("SelectionTool: Ningún objeto", x, y);
    }
}

int SelectionTool::performRaycast(const ofVec2f& screenPos, 
                                   const ofEasyCam& cam,
                                   const vector<std::unique_ptr<Shape3D>>& shapes) {
    if (shapes.empty()) return -1;
    
    // Convertir coordenadas de pantalla a mundo 3D
    // nearPoint: punto en el near plane
    // farPoint: punto en el far plane
    glm::vec3 nearPoint = cam.screenToWorld(glm::vec3(screenPos.x, screenPos.y, 0));
    glm::vec3 farPoint = cam.screenToWorld(glm::vec3(screenPos.x, screenPos.y, 1));
    
    // Dirección del rayo
    glm::vec3 rayDir = glm::normalize(farPoint - nearPoint);
    glm::vec3 rayOrigin = nearPoint;
    
    // Variables para tracking de la figura más cercana
    int closestIndex = -1;
    float closestDist = std::numeric_limits<float>::max();
    
    // Iterar sobre todas las figuras
    for (size_t i = 0; i < shapes.size(); i++) {
        glm::vec3 shapePos = shapes[i]->getPosition();
        glm::vec3 shapeScale = shapes[i]->getScale();
        
        // Aproximación simple: bounding sphere (más robusto que AABB)
        // Radio basado en la escala máxima de la figura
        float maxScale = std::max(std::max(shapeScale.x, shapeScale.y), shapeScale.z);
        float boundingRadius = maxScale * 50.0f;  // Factor ajustable según tamaño de tus primitivas
        
        // Test de intersección rayo-esfera
        glm::vec3 oc = rayOrigin - shapePos;
        float a = glm::dot(rayDir, rayDir);
        float b = 2.0f * glm::dot(oc, rayDir);
        float c = glm::dot(oc, oc) - boundingRadius * boundingRadius;
        float discriminant = b * b - 4 * a * c;
        
        if (discriminant >= 0) {
            // Intersección detectada
            float t = (-b - sqrt(discriminant)) / (2.0f * a);
            if (t > 0 && t < closestDist) {
                closestDist = t;
                closestIndex = i;
            }
        }
    }
    
    // Actualizar índice seleccionado
    selectedIndex = closestIndex;
    
    return closestIndex;
}

// ---------------- COLOR TOOL ----------------
void ColorTool::setLockedPoints(const vector<ofVec2f>& lockedPts, const ofColor& startColor) {
    if (lockedPts.size() < 3) return;  // Necesitamos al menos 3 puntos para triángulo
    (void)startColor; // comportamiento: siempre arranca en rojo

    // Guardar snapshot de puntos
    lockedPoints = lockedPts;
    
    // Calcular centroide
    ofVec2f center = centroid(lockedPts);
    
    // ✅ Aplicar responsividad: radio máximo = wheelRadius (200px)
    initialCenter = clampMenuPosition(center, wheelRadius);
    
    // Calcular orientación inicial usando el eje principal del triángulo
    initialOrientation = principalAxisAngle(lockedPts, 0.0f);
    lastOrientation = initialOrientation;

    // Arrancar en morado (hue ~191 = indicador apuntando arriba) por defecto,
    // o en el color del objeto si viene coloreado
    const float INITIAL_PURPLE_HUE = 191.25f;
    bool hasColor = startColor.getSaturation() > 10;
    if (hasColor) {
        currentHue = startColor.getHue();
        currentColor = startColor;
    } else {
        currentHue = INITIAL_PURPLE_HUE;
        currentColor.setHsb((int)INITIAL_PURPLE_HUE, 255, 200);
    }
    lastOrientation = initialOrientation;  // Evita salto de hue en el primer frame
    
    locked = true;
    
    ofLogNotice("ColorTool") << "Token B locked - using principal axis for orientation";
    ofLogNotice("ColorTool") << "Initial orientation: " << (initialOrientation * RAD_TO_DEG) << "°";
    ofLogNotice("ColorTool") << "Starting hue at " << currentHue;
}

void ColorTool::reset() {
    locked = false;
    initialCenter = ofVec2f(0,0);
    initialOrientation = 0.0f;
    lastOrientation = 0.0f;
    currentHue = 0.0f;
    currentShapeIndex = -1;  // Clear shape tracking
    lockedPoints.clear();
}

void ColorTool::update(const vector<ofVec2f>& pts, RotationAxis) {
    if (!locked || pts.size() < 3) return;  // Necesitamos 3 puntos para triángulo
    
    // Calcular centroide actual
    ofVec2f currentCenter = centroid(pts);

    // Calcular orientación actual usando eje principal de los puntos
    float currentOrientation = principalAxisAngle(pts, lastOrientation);
    // Prevenir flips por ambigüedad de signo del eigenvector
    auto normalize = [](float ang){
        if (ang > PI) ang -= TWO_PI;
        if (ang < -PI) ang += TWO_PI;
        return ang;
    };
    float altOrientation = normalize(currentOrientation + PI); // vector opuesto
    float diff = normalize(currentOrientation - lastOrientation);
    float altDiff = normalize(altOrientation - lastOrientation);
    if (std::abs(altDiff) < std::abs(diff)) {
        currentOrientation = altOrientation;
    }

    // --- Detectar SALTOS (reposicionamiento del token) ---
    float centerDelta = currentCenter.distance(initialCenter);
    if (centerDelta > jumpThreshold) {
        ofLogNotice("ColorTool") << "Token jump detected (" << centerDelta << "px) - updating reference only";
        lastOrientation = currentOrientation;
        initialCenter = currentCenter;  // Actualizar referencia de centro
        return;
    }
    
    // --- Movimiento continuo: aplicar cambio relativo ---
    float deltaOrientation = currentOrientation - lastOrientation;
    
    // Normalizar ángulo en [-PI, PI]
    if (deltaOrientation > PI) deltaOrientation -= TWO_PI;
    if (deltaOrientation < -PI) deltaOrientation += TWO_PI;
    
    // Mapear cambio angular a cambio de hue (360° = 255 hue)
    float deltaHue = deltaOrientation * RAD_TO_DEG * (255.0f / 360.0f);
    currentHue += deltaHue;
    
    // Mantener hue en rango 0-255
    while (currentHue < 0) currentHue += 255;
    while (currentHue >= 255) currentHue -= 255;
    
    // Actualizar color
    currentColor.setHsb((int)currentHue, 255, 200);
    
    // Actualizar última orientación válida
    lastOrientation = currentOrientation;
}

void ColorTool::draw(int x, int y) { 
    ofDrawBitmapString("Color tool: H" + ofToString(currentColor.getHue()), x, y); 
}

void ColorTool::drawColorWheel() {
    if (!locked) return;
    
    float innerRadius = 100.0f;  // ✅ Aumentado de 60 a 100 (espacio para token)
    float outerRadius = wheelRadius;  // Radio exterior (200px)
    int segments = 360;
    
    ofPushStyle();
    ofFill();
    
    for (int i = 0; i < segments; i++) {
        float angle1 = ofMap(i, 0, segments, 0, TWO_PI);
        float angle2 = ofMap(i + 1, 0, segments, 0, TWO_PI);
        
        float hue = ofMap(i, 0, segments, 0, 255);
        ofColor segmentColor;
        segmentColor.setHsb(hue, 255, 200);
        
        ofSetColor(segmentColor);
        
        // Dibujar quad (anillo) desde innerRadius a outerRadius
        ofBeginShape();
        ofVertex(initialCenter.x + cos(angle1) * innerRadius,
                 initialCenter.y + sin(angle1) * innerRadius);
        ofVertex(initialCenter.x + cos(angle1) * outerRadius,
                 initialCenter.y + sin(angle1) * outerRadius);
        ofVertex(initialCenter.x + cos(angle2) * outerRadius,
                 initialCenter.y + sin(angle2) * outerRadius);
        ofVertex(initialCenter.x + cos(angle2) * innerRadius,
                 initialCenter.y + sin(angle2) * innerRadius);
        ofEndShape(true);
    }
    
    ofPopStyle();
}

void ColorTool::drawColorIndicator() {
    if (!locked) return;
    
    ofPushStyle();
    ofFill();
    
    // Borde blanco
    ofSetColor(255);
    ofDrawCircle(initialCenter, indicatorRadius + 3);
    
    // Círculo con color actual
    ofSetColor(currentColor);
    ofDrawCircle(initialCenter, indicatorRadius);
    
    ofPopStyle();
}

void ColorTool::drawAngleIndicator(const vector<ofVec2f>& pts) {
    if (!locked) return;
    (void)pts; // indicador ya no depende de puntos individuales
    
    ofPushStyle();
    
    // Usar hue actual como ángulo visual (giro acumulado del token)
    float angle = ofDegToRad(ofMap(currentHue, 0, 255, 0, 360));
    
    // Dibujar línea desde innerRadius hasta outerRadius (apuntando al vértice de 20°)
    float innerRadius = 100.0f;
    float outerRadius = wheelRadius;
    float midRadius = (innerRadius + outerRadius) / 2.0f;
    
    ofSetColor(255);
    ofSetLineWidth(4);  // Más grueso para indicar orientación
    
    ofVec2f innerPoint = initialCenter + ofVec2f(
        cos(angle) * innerRadius,
        sin(angle) * innerRadius
    );
    
    ofVec2f outerPoint = initialCenter + ofVec2f(
        cos(angle) * outerRadius,
        sin(angle) * outerRadius
    );
    
    ofDrawLine(innerPoint, outerPoint);
    
    // Círculo en mitad del anillo
    ofVec2f midPoint = initialCenter + ofVec2f(
        cos(angle) * midRadius,
        sin(angle) * midRadius
    );
    ofFill();
    ofSetColor(255, 255, 0);  // Amarillo para destacar
    ofDrawCircle(midPoint, 9);
        
    ofPopStyle();
}

ofVec2f ColorTool::centroid(const vector<ofVec2f>& pts) const {
    if (pts.empty()) return ofVec2f(0,0);
    ofVec2f c(0,0);
    for (auto &p: pts) c += p;
    c /= (float)pts.size();
    return c;
}

float ColorTool::principalAxisAngle(const vector<ofVec2f>& pts, float fallbackAngle) const {
    if (pts.size() < 2) return fallbackAngle;
    ofVec2f c = centroid(pts);

    float cov_xx = 0.0f, cov_xy = 0.0f, cov_yy = 0.0f;
    for (auto& p : pts) {
        ofVec2f d = p - c;
        cov_xx += d.x * d.x;
        cov_xy += d.x * d.y;
        cov_yy += d.y * d.y;
    }
    cov_xx /= pts.size();
    cov_xy /= pts.size();
    cov_yy /= pts.size();

    float variance = cov_xx + cov_yy;
    if (variance < 1e-3f) {
        return fallbackAngle;  // nube casi circular: usa orientación previa
    }

    // PCA 2D: ángulo del eigenvector principal
    float angle = 0.5f * atan2(2.0f * cov_xy, cov_xx - cov_yy);
    return angle;
}

// ---------------- COLOR TOOL WITH SESSION IDs ----------------
void ColorTool::setLockedPointsWithIDs(const vector<TouchPointWithID>& lockedPts, const ofColor& startColor, int shapeIndex) {
    if (lockedPts.size() < 3) return;

    // Detectar si es una figura nueva respecto a la última editada
    bool newShape = (currentShapeIndex == -1 || currentShapeIndex != shapeIndex);
    // Guardar el índice del objeto actual
    currentShapeIndex = shapeIndex;

    // Guardar snapshot de puntos CON Session IDs
    lockedPointsWithID = lockedPts;

    // Extraer solo coordenadas para cálculos geométricos
    vector<ofVec2f> coords;
    for (auto& pt : lockedPts) {
        coords.push_back(pt.pos);
    }

    // Calcular centroide
    ofVec2f center = centroid(coords);

    // ✅ Aplicar responsividad: radio máximo = wheelRadius (200px)
    initialCenter = clampMenuPosition(center, wheelRadius);

    // Calcular orientación inicial del token (eje principal)
    initialOrientation = principalAxisAngle(coords, 0.0f);

    // Elegir color inicial:
    // - Si es figura nueva con color por defecto (baja saturación): morado (indicador arriba)
    // - Si es figura nueva pero ya coloreada: usar su color
    // - Si es la misma figura: usar su color actual
    const float INITIAL_PURPLE_HUE = 191.25f; // ~270° → apunta hacia arriba en pantalla
    bool hasColor = startColor.getSaturation() > 10;
    if (newShape && !hasColor) {
        currentHue = INITIAL_PURPLE_HUE;
        currentColor.setHsb((int)INITIAL_PURPLE_HUE, 255, 200);
    } else {
        currentHue = startColor.getHue();
        currentColor = startColor;
    }

    lastOrientation = initialOrientation;  // Evita salto de hue en el primer frame

    locked = true;

    ofLogNotice("ColorTool") << "Token B locked with Session ID tracking";
    ofLogNotice("ColorTool") << "  Initial orientation: " << (initialOrientation * RAD_TO_DEG) << "°";
    ofLogNotice("ColorTool") << "  Shape index: " << shapeIndex;
    ofLogNotice("ColorTool") << "  Starting hue at " << currentHue;
}

void ColorTool::updateWithIDs(const vector<TouchPointWithID>& pts) {
    if (!locked || pts.size() < 3) return;
    
    // Extraer coordenadas para calcular centroide
    vector<ofVec2f> coords;
    for (auto& pt : pts) {
        coords.push_back(pt.pos);
    }
    
    ofVec2f currentCenter = centroid(coords);
    
    // Calcular orientación actual usando eje principal de los puntos
    float currentOrientation = principalAxisAngle(coords, lastOrientation);
    auto normalize = [](float ang){
        if (ang > PI) ang -= TWO_PI;
        if (ang < -PI) ang += TWO_PI;
        return ang;
    };
    float altOrientation = normalize(currentOrientation + PI);
    float diff = normalize(currentOrientation - lastOrientation);
    float altDiff = normalize(altOrientation - lastOrientation);
    if (std::abs(altDiff) < std::abs(diff)) {
        currentOrientation = altOrientation;
    }

    // Detectar SALTOS (reposicionamiento del token)
    float centerDelta = currentCenter.distance(initialCenter);
    if (centerDelta > jumpThreshold) {
        ofLogNotice("ColorTool") << "Token jump detected (" << centerDelta << "px) - updating reference only";
        lastOrientation = currentOrientation;
        initialCenter = currentCenter;
        return;
    }
    
    // Calcular cambio de orientación desde el último frame (ya estabilizado)
    float deltaOrientation = currentOrientation - lastOrientation;

    // Normalizar ángulo en [-PI, PI] para manejar wrapping
    if (deltaOrientation > PI) deltaOrientation -= TWO_PI;
    if (deltaOrientation < -PI) deltaOrientation += TWO_PI;

    // Mapear cambio angular a cambio de hue
    float deltaHue = deltaOrientation * RAD_TO_DEG * (255.0f / 360.0f);
    currentHue += deltaHue;  // Acumular cambio de hue

    // Mantener hue en rango [0, 255)
    while (currentHue < 0) currentHue += 255;
    while (currentHue >= 255) currentHue -= 255;
    
    // Actualizar color
    currentColor.setHsb((int)currentHue, 255, 200);
    
    // Actualizar última orientación válida
    lastOrientation = currentOrientation;
}

void ColorTool::drawAngleIndicatorWithIDs(const vector<TouchPointWithID>& pts) {
    if (!locked || pts.size() < 3) return;
    (void)pts; // indicador usa hue acumulado, no la punta del triángulo
    
    ofPushStyle();
    
    // Usar hue actual como ángulo visual
    float angle = ofDegToRad(ofMap(currentHue, 0, 255, 0, 360));
    
    // Dibujar línea indicadora
    float innerRadius = 100.0f;
    float outerRadius = wheelRadius;
    float midRadius = (innerRadius + outerRadius) / 2.0f;
    
    ofSetColor(255);
    ofSetLineWidth(4);
    
    // ✅ Usar initialCenter (FIJO) para que la línea no se mueva con el token
    // Solo ROTA según el ángulo del token
    ofVec2f innerPoint = initialCenter + ofVec2f(
        cos(angle) * innerRadius,
        sin(angle) * innerRadius
    );
    
    ofVec2f outerPoint = initialCenter + ofVec2f(
        cos(angle) * outerRadius,
        sin(angle) * outerRadius
    );
    
    ofDrawLine(innerPoint, outerPoint);

    // Círculo en mitad del anillo
    ofVec2f midPoint = initialCenter + ofVec2f(
        cos(angle) * midRadius,
        sin(angle) * midRadius
    );
    ofFill();
    ofSetColor(255, 255, 0);
    ofDrawCircle(midPoint, 9);

    ofPopStyle();
}

// ============================================================
// IMPLEMENTACIONES DE TOOLTIPS PARA TODAS LAS HERRAMIENTAS
// ============================================================

TooltipInfo RotationTool::getTooltipInfo() const {
    return TooltipInfo(
        "Rotación",
        "Coloque el token sobre los circulos de los eje y gire el token para rotar el objero",
        ofColor(255, 150, 0) // Naranja
    );
}

TooltipInfo AxisSelectorTool::getTooltipInfo() const {
    return TooltipInfo(
        "Selector de Eje",
        "Selecciona el eje de rotación X, Y o Z tocando los botones de colores.",
        ofColor(255, 255, 0) // Amarillo
    );
}

TooltipInfo ScaleTool::getTooltipInfo() const {
    string desc;
    if (interactionMode == ScaleInteractionMode::Gesture) {
        desc = "Modo gestos: domina una dirección para escalar X (horizontal), Y (vertical) o Z (diagonal). Cambia de dirección sin retirar el token.";
    } else if (interactionMode == ScaleInteractionMode::Knob) {
        desc = "Modo perilla: selecciona un eje del menú y gira el token para escalar de forma incremental.";
    } else {
        desc = "Modo botones: coloca el token sobre el eje y arrastra (desde el botón) para escalar el objeto. Uniforme por defecto.";
    }
    return TooltipInfo(
        "Escala",
        desc,
        ofColor(200, 100, 255) // Morado
    );
}

TooltipInfo TranslateTool::getTooltipInfo() const {
    string desc;
    if (interactionMode == TranslateInteractionMode::Gesture) {
        desc = "Modo gestos: movimiento según la vista (derecha=X, arriba=Y, diagonal=Z/profundidad). Puedes cambiar de eje sin retirar el token.";
    } else if (interactionMode == TranslateInteractionMode::Knob) {
        desc = "Modo perilla: selecciona un eje del menú y gira el token para trasladar de forma incremental.";
    } else {
        desc = "Modo botones: toca un eje (X/Y/Z) y arrastra desde ese botón para trasladar el objeto con margen anti-ruido.";
    }
    return TooltipInfo(
        "Traslación",
        desc,
        ofColor(0, 200, 255) // Cyan
    );
}

TooltipInfo SpawnTool::getTooltipInfo() const {
    return TooltipInfo(
        "Crear Objeto",
        "Selecciona una forma geométrica del menú y confirma colocando el token para crear un nuevo objeto.",
        ofColor(100, 255, 100) // Verde claro
    );
}

TooltipInfo ColorTool::getTooltipInfo() const {
    return TooltipInfo(
        "Color",
        "Gira el token sobre la rueda cromática para cambiar el color del objeto seleccionado.",
        ofColor(255, 100, 180) // Rosa
    );
}

TooltipInfo SelectionTool::getTooltipInfo() const {
    return TooltipInfo(
        "Selección",
        "Coloca el token sobre un objeto en la escena para seleccionarlo y poder editarlo.",
        ofColor(0, 150, 255) // Azul brillante
    );
}

TooltipInfo UtilityTool::getTooltipInfo() const {
    return TooltipInfo(
        "Utilidades",
        "Elige eliminar el objeto activo, deshacer la última acción o reiniciar la escena completa.",
        ofColor(255, 255, 255) // Blanco
    );
}
