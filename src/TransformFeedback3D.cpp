#include "TransformFeedback3D.hpp"

// =============== FeedbackTextState Implementation ===============

void FeedbackTextState::markChanged() {
    if (state == HIDDEN || state == FADING_OUT) {
        // Iniciar fade-in desde el principio
        state = FADING_IN;
        lastChangeTime = ofGetElapsedTimeMillis();
        alpha = 0.0f;
    } else if (state == VISIBLE) {
        // Reiniciar timer de visibilidad
        visibleStartTime = ofGetElapsedTimeMillis();
    } else if (state == FADING_IN) {
        // Ya está apareciendo, mantener en fade-in
        // No reiniciar para evitar parpadeos
    }
}

void FeedbackTextState::update() {
    uint64_t now = ofGetElapsedTimeMillis();
    float elapsed = 0.0f;

    switch (state) {
        case FADING_IN:
            elapsed = now - lastChangeTime;
            alpha = ofClamp(elapsed / FADE_IN_DURATION, 0.0f, 1.0f);
            if (alpha >= 1.0f) {
                state = VISIBLE;
                visibleStartTime = now;
                alpha = 1.0f;
            }
            break;

        case VISIBLE:
            alpha = 1.0f;
            if (now - visibleStartTime >= VISIBLE_DURATION) {
                state = FADING_OUT;
                lastChangeTime = now;
            }
            break;

        case FADING_OUT:
            elapsed = now - lastChangeTime;
            alpha = 1.0f - ofClamp(elapsed / FADE_OUT_DURATION, 0.0f, 1.0f);
            if (alpha <= 0.0f) {
                state = HIDDEN;
                alpha = 0.0f;
            }
            break;

        case HIDDEN:
        default:
            alpha = 0.0f;
            break;
    }
}

// =============== TransformFeedback3D Implementation ===============

void TransformFeedback3D::setup() {
    // Cargar fuente CON antialiasing para bordes suaves
    // true = antialiasing ON, true = full resolution (bordes suaves sin pixelado)
    bool fontLoaded = font.load("arial.ttf", fontSize, true, true);
    if (!fontLoaded) {
        fontLoaded = font.load("verdana.ttf", fontSize, true, true);
    }
    if (!fontLoaded) {
        fontLoaded = font.load(OF_TTF_SANS, fontSize, true, true);
    }

    if (fontLoaded) {
        ofLogNotice("TransformFeedback3D") << "Font loaded successfully, size: " << fontSize;
    } else {
        ofLogWarning("TransformFeedback3D") << "Failed to load font";
    }
}

void TransformFeedback3D::update(const RotationTool* rotTool,
                                   RotationAxis rotAxis,
                                   const ScaleTool* scaleTool,
                                   const TranslateTool* transTool,
                                   const Shape3D* shape,
                                   const ofEasyCam& cam) {

    // =============== ROTACIÓN ===============
    if (rotTool && rotAxis != RotationAxis::None && shape) {
        // Obtener ángulo acumulado según el eje activo
        float angle = 0.0f;
        switch (rotAxis) {
            case RotationAxis::X:
                angle = rotTool->rotationX;
                break;
            case RotationAxis::Y:
                angle = rotTool->rotationY;
                break;
            case RotationAxis::Z:
                angle = rotTool->rotationZ;
                break;
            default:
                break;
        }

        // Normalizar ángulo a [0, 360)
        angle = fmod(angle, 360.0f);
        if (angle < 0.0f) angle += 360.0f;

        // Detectar cambio significativo (> 0.5 grados)
        if (std::abs(angle - rotationText.lastValue) > 0.5f) {
            rotationText.content = ofToString((int)std::round(angle)) + "°";
            rotationText.position = calculateRotationTextPosition(shape, rotAxis);
            rotationText.color = getAxisColorRotation(rotAxis);
            rotationText.state.markChanged();
            rotationText.lastValue = angle;
        }
    }

    // =============== TRASLACIÓN ===============
    if (transTool && transTool->currentAxis != TranslateAxis::None && shape) {
        glm::vec3 delta = transTool->getPosition() - transTool->initialPos;

        // Determinar qué eje está activo y su valor
        float value = 0.0f;
        std::string axisName;
        FeedbackText* targetText = nullptr;

        switch (transTool->currentAxis) {
            case TranslateAxis::X:
                value = delta.x;
                axisName = "ΔX";
                targetText = &translationXText;
                break;
            case TranslateAxis::Y:
                value = delta.y;
                axisName = "ΔY";
                targetText = &translationYText;
                break;
            case TranslateAxis::Z:
                value = delta.z;
                axisName = "ΔZ";
                targetText = &translationZText;
                break;
            default:
                break;
        }

        if (targetText) {
            // Detectar cambio significativo (> 0.1 unidades)
            if (std::abs(value - targetText->lastValue) > 0.1f) {
                std::string sign = (value >= 0) ? "+" : "";
                targetText->content = axisName + ": " + sign + ofToString((int)std::round(value));
                targetText->position = calculateTranslateTextPosition(shape, transTool->currentAxis);
                targetText->color = getAxisColorTranslate(transTool->currentAxis);
                targetText->state.markChanged();
                targetText->lastValue = value;
            }
        }
    }

    // =============== ESCALA ===============
    if (scaleTool && scaleTool->currentAxis != ScaleAxis::None && shape) {
        float factor = scaleTool->getCurrentFactor();

        // Detectar cambio significativo (> 0.01x)
        if (std::abs(factor - scaleText.lastValue) > 0.01f) {
            if (scaleTool->currentAxis == ScaleAxis::Uniform) {
                // Escala uniforme: mostrar "1.25×"
                scaleText.content = ofToString(factor, 2) + "×";
                scaleText.color = ofColor(200, 200, 200); // Gris claro
            } else {
                // Escala por eje: mostrar "SX: 1.5"
                std::string axisName = scaleTool->getAxisName();
                scaleText.content = "S" + axisName + ": " + ofToString(factor, 2);
                scaleText.color = getAxisColorScale(scaleTool->currentAxis);
            }

            scaleText.position = calculateScaleTextPosition(shape, scaleTool->currentAxis);
            scaleText.state.markChanged();
            scaleText.lastValue = factor;
        }
    }

    // Actualizar estados de fade
    rotationText.state.update();
    translationXText.state.update();
    translationYText.state.update();
    translationZText.state.update();
    scaleText.state.update();
}

void TransformFeedback3D::draw(const ofEasyCam& cam) {
    ofPushStyle();
    ofEnableAlphaBlending();
    ofDisableLighting();

    // Dibujar cada texto visible
    if (rotationText.state.isVisible()) {
        drawBillboardText(rotationText, cam);
    }
    if (translationXText.state.isVisible()) {
        drawBillboardText(translationXText, cam);
    }
    if (translationYText.state.isVisible()) {
        drawBillboardText(translationYText, cam);
    }
    if (translationZText.state.isVisible()) {
        drawBillboardText(translationZText, cam);
    }
    if (scaleText.state.isVisible()) {
        drawBillboardText(scaleText, cam);
    }

    ofPopStyle();
}

// =============== Cálculo de Billboard Matrix ===============

glm::mat4 TransformFeedback3D::calculateBillboardMatrix(const glm::vec3& position, const ofEasyCam& cam) {
    // Obtener posición de la cámara
    glm::vec3 camPos = cam.getGlobalPosition();
    glm::vec3 toCamera = glm::normalize(camPos - position);

    // Billboard esférico: texto perpendicular a línea de vista
    glm::vec3 worldUp = glm::vec3(0, 1, 0);
    glm::vec3 right = glm::normalize(glm::cross(worldUp, toCamera));
    glm::vec3 up = glm::cross(toCamera, right);

    // Construir matriz de rotación (columnas = ejes locales)
    glm::mat4 orientation(1.0f);
    orientation[0] = glm::vec4(right, 0);
    orientation[1] = glm::vec4(up, 0);
    orientation[2] = glm::vec4(toCamera, 0);

    // Combinar con traslación
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
    return translation * orientation;
}

// =============== Cálculo de Posiciones ===============

glm::vec3 TransformFeedback3D::calculateRotationTextPosition(const Shape3D* shape, RotationAxis axis) {
    glm::vec3 center = shape->getPosition();
    float maxScale = std::max({shape->getScale().x, shape->getScale().y, shape->getScale().z});
    float gizmoRadius = maxScale * 130.0f; // Mismo que drawRotationGizmo

    // Offset adicional para que texto quede fuera del círculo (20% más)
    float textOffset = gizmoRadius * 1.2f;

    switch (axis) {
        case RotationAxis::X:
            // Círculo en plano YZ, colocar texto arriba-derecha
            return center + glm::vec3(0, textOffset * 0.7f, textOffset * 0.7f);

        case RotationAxis::Y:
            // Círculo en plano XZ, colocar texto arriba-derecha
            return center + glm::vec3(textOffset * 0.7f, textOffset * 0.7f, 0);

        case RotationAxis::Z:
            // Círculo en plano XY, colocar texto arriba-derecha
            return center + glm::vec3(textOffset * 0.7f, textOffset * 0.7f, 0);

        default:
            return center;
    }
}

glm::vec3 TransformFeedback3D::calculateTranslateTextPosition(const Shape3D* shape, TranslateAxis axis) {
    glm::vec3 center = shape->getPosition();
    float maxScale = std::max({shape->getScale().x, shape->getScale().y, shape->getScale().z});
    float arrowLength = maxScale * 150.0f; // Mismo que drawTranslationGizmo

    // Colocar en punta de flecha + 10% offset
    float offset = arrowLength * 1.1f;

    switch (axis) {
        case TranslateAxis::X:
            return center + glm::vec3(offset, 0, 0);
        case TranslateAxis::Y:
            return center + glm::vec3(0, offset, 0);
        case TranslateAxis::Z:
            return center + glm::vec3(0, 0, offset);
        default:
            return center;
    }
}

glm::vec3 TransformFeedback3D::calculateScaleTextPosition(const Shape3D* shape, ScaleAxis axis) {
    glm::vec3 center = shape->getPosition();
    float maxScale = std::max({shape->getScale().x, shape->getScale().y, shape->getScale().z});
    float lineLength = maxScale * 120.0f; // Mismo que drawScaleGizmo

    // Colocar en punta de handle + 15% offset
    float offset = lineLength * 1.15f;

    switch (axis) {
        case ScaleAxis::X:
            return center + glm::vec3(offset, 0, 0);
        case ScaleAxis::Y:
            return center + glm::vec3(0, offset, 0);
        case ScaleAxis::Z:
            return center + glm::vec3(0, 0, offset);
        case ScaleAxis::Uniform:
            // Para escala uniforme, posición diagonal arriba-derecha
            return center + glm::vec3(offset * 0.6f, offset * 0.6f, offset * 0.6f);
        default:
            return center;
    }
}

// =============== Renderizado de Texto ===============

void TransformFeedback3D::drawBillboardText(const FeedbackText& text, const ofEasyCam& cam) {
    if (text.state.alpha <= 0.01f) return; // Early exit para optimización

    // Calcular matriz billboard
    glm::mat4 billboardMatrix = calculateBillboardMatrix(text.position, cam);

    // Generar mesh de texto (vFlip=false para coordenadas 3D)
    ofMesh textMesh = font.getStringMesh(text.content, 0, 0, false);

    // Centrar texto (offset horizontal y vertical)
    ofRectangle bbox = font.getStringBoundingBox(text.content, 0, 0);
    float offsetX = -bbox.width / 2.0f;
    float offsetY = bbox.height / 2.0f;

    // Desactivar depth test para evitar z-fighting y transparencia
    ofPushStyle();
    ofDisableDepthTest();

    ofPushMatrix();
    ofMultMatrix(billboardMatrix);
    ofTranslate(offsetX, offsetY, 0);

    // Renderizar con bold
    drawTextMeshWithGlow(textMesh, text.color, text.state.alpha);

    ofPopMatrix();

    ofEnableDepthTest();
    ofPopStyle();
}

void TransformFeedback3D::drawTextMeshWithGlow(const ofMesh& mesh, const ofColor& color, float alpha) {
    // Bindear textura de la fuente (necesario para getStringMesh())
    font.getFontTexture().bind();

    // Texto principal CON BOLD (4 renderizados con offset)
    // Sin glow: el glow negro oscurece el texto en lugar de resaltarlo
    ofColor mainColor = color;
    mainColor.a = (int)(alpha * 255);
    ofSetColor(mainColor);

    for (int dx = 0; dx <= 1; dx++) {
        for (int dy = 0; dy <= 1; dy++) {
            ofPushMatrix();
            ofTranslate(dx, dy, 0);
            mesh.draw();
            ofPopMatrix();
        }
    }

    // Unbindear textura
    font.getFontTexture().unbind();
}

// =============== Helpers de Color ===============

ofColor TransformFeedback3D::getAxisColorRotation(RotationAxis axis) {
    return ofColor(0, 0, 0);  // Negro para todos los ejes
}

ofColor TransformFeedback3D::getAxisColorTranslate(TranslateAxis axis) {
    return ofColor(0, 0, 0);  // Negro para todos los ejes
}

ofColor TransformFeedback3D::getAxisColorScale(ScaleAxis axis) {
    return ofColor(0, 0, 0);  // Negro para todos (incluso uniforme)
}
