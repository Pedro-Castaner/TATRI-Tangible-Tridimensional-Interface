#include "TooltipManager.hpp"

TooltipManager::TooltipManager() {
    state = HIDDEN;
    fadeAlpha = 0.0f;
    fadeDuration = 300.0f;       // 300ms para fade
    visibleDuration = 2000.0f;   // 2.0 segundos visible
    hideDelay = 2000.0f;         // 2.0s delay antes de fade-out
    fadeStartTime = 0;
    visibleStartTime = 0;

    // Posición fija en esquina superior derecha (se ajustará en setup())
    x = 0;
    y = 20;
    width = 400;
    height = 200;
    padding = 30;
    cornerRadius = 15;

    // Colores
    backgroundColor = ofColor(40, 40, 40, 220);
    textColor = ofColor(255, 255, 255);
    separatorColor = ofColor(255, 255, 255, 100);

    fontsLoaded = false;
}

void TooltipManager::setup() {
    // ✅ Ajustar posición a esquina superior IZQUIERDA
    x = 20;
    y = 20;

    // Cargar fuentes
    // Intentar cargar Arial primero, luego Verdana, luego fuente del sistema
    if (titleFont.load("Arial.ttf", 24, true, true)) {
        fontsLoaded = true;
    } else if (titleFont.load("Verdana.ttf", 24, true, true)) {
        fontsLoaded = true;
    } else {
        titleFont.load(OF_TTF_SANS, 24, true, true);
        fontsLoaded = true;
    }

    if (descFont.load("Arial.ttf", 16, true, true)) {
        // Success
    } else if (descFont.load("Verdana.ttf", 16, true, true)) {
        // Success
    } else {
        descFont.load(OF_TTF_SANS, 16, true, true);
    }
}

void TooltipManager::update() {
    updateFade();
}

void TooltipManager::updateFade() {
    switch (state) {
        case FADING_IN: {
            float elapsed = getElapsedTime(fadeStartTime);
            fadeAlpha = ofClamp(elapsed / fadeDuration, 0.0f, 1.0f);

            if (fadeAlpha >= 1.0f) {
                state = VISIBLE;
                visibleStartTime = ofGetElapsedTimeMillis();
            }
            break;
        }

        case VISIBLE: {
            // ✅ NO auto-ocultar - permanecer visible hasta que hide() sea llamado explícitamente
            // El tooltip permanece activo mientras el token esté presente
            fadeAlpha = 1.0f;  // Mantener alpha completo
            break;
        }

        case HIDING_DELAYED: {
            // ✅ Esperar 2.0s después de quitar el token antes de iniciar fade-out
            float elapsed = getElapsedTime(visibleStartTime);
            fadeAlpha = 1.0f;  // Mantener completamente visible durante el delay

            if (elapsed >= hideDelay) {
                // Iniciar fade-out después del delay
                state = FADING_OUT;
                fadeStartTime = ofGetElapsedTimeMillis();
            }
            break;
        }

        case FADING_OUT: {
            float elapsed = getElapsedTime(fadeStartTime);
            fadeAlpha = 1.0f - ofClamp(elapsed / fadeDuration, 0.0f, 1.0f);

            if (fadeAlpha <= 0.0f) {
                state = HIDDEN;
                fadeAlpha = 0.0f;
            }
            break;
        }

        case HIDDEN:
        default:
            fadeAlpha = 0.0f;
            break;
    }
}

void TooltipManager::draw() {
    if (state == HIDDEN || fadeAlpha <= 0.0f) {
        return;
    }

    ofPushStyle();
    ofPushMatrix();

    // Ajustar alpha global según fade
    float globalAlpha = fadeAlpha;

    // Dibujar sombra sutil (offset hacia abajo y derecha)
    ofColor shadowColor = ofColor(0, 0, 0, 60 * globalAlpha);
    drawRoundedRect(x + 5, y + 5, width, height, cornerRadius, shadowColor);

    // Dibujar fondo del tooltip
    ofColor bgColor = backgroundColor;
    bgColor.a = bgColor.a * globalAlpha;
    drawRoundedRect(x, y, width, height, cornerRadius, bgColor);

    // Dibujar contenido de texto
    drawText();

    ofPopMatrix();
    ofPopStyle();
}

void TooltipManager::drawRoundedRect(float x, float y, float w, float h, float r, const ofColor& color) {
    ofSetColor(color);
    ofFill();

    // Dibujar rectángulo con esquinas redondeadas usando path
    ofPath path;
    path.setFillColor(color);
    path.moveTo(x + r, y);
    path.lineTo(x + w - r, y);
    path.arc(x + w - r, y + r, r, r, 270, 360);
    path.lineTo(x + w, y + h - r);
    path.arc(x + w - r, y + h - r, r, r, 0, 90);
    path.lineTo(x + r, y + h);
    path.arc(x + r, y + h - r, r, r, 90, 180);
    path.lineTo(x, y + r);
    path.arc(x + r, y + r, r, r, 180, 270);
    path.close();
    path.draw();
}

void TooltipManager::drawText() {
    if (!fontsLoaded) {
        return;
    }

    float contentX = x + padding;
    float contentY = y + padding;
    float contentWidth = width - (padding * 2);

    // Aplicar alpha a los colores de texto
    ofColor titleColor = currentInfo.accentColor;
    titleColor.a = 255 * fadeAlpha;

    ofColor descColor = textColor;
    descColor.a = 255 * fadeAlpha;

    ofColor sepColor = separatorColor;
    sepColor.a = 100 * fadeAlpha;

    // Dibujar título con efecto bold (dibujado múltiple)
    ofSetColor(titleColor);
    for (int dx = 0; dx <= 1; dx++) {
        for (int dy = 0; dy <= 1; dy++) {
            titleFont.drawString(currentInfo.title, contentX + dx, contentY + 30 + dy);
        }
    }

    // Dibujar línea separadora
    float separatorY = contentY + 50;
    ofSetColor(sepColor);
    ofSetLineWidth(2);
    ofDrawLine(contentX, separatorY, contentX + contentWidth, separatorY);

    // Dibujar descripción (permitir texto multi-línea)
    ofSetColor(descColor);

    // Calcular wrapping manual del texto
    float descY = separatorY + 30;
    float lineHeight = 24;
    float maxLineWidth = contentWidth;

    // Split por palabras
    vector<string> words = ofSplitString(currentInfo.description, " ");
    string currentLine = "";

    for (size_t i = 0; i < words.size(); i++) {
        string testLine = currentLine.empty() ? words[i] : currentLine + " " + words[i];
        ofRectangle bounds = descFont.getStringBoundingBox(testLine, 0, 0);

        if (bounds.width <= maxLineWidth) {
            currentLine = testLine;
        } else {
            // Dibujar línea actual y empezar nueva
            if (!currentLine.empty()) {
                descFont.drawString(currentLine, contentX, descY);
                descY += lineHeight;
                currentLine = words[i];
            } else {
                // Palabra demasiado larga, dibujar de todas formas
                descFont.drawString(words[i], contentX, descY);
                descY += lineHeight;
                currentLine = "";
            }
        }
    }

    // Dibujar última línea
    if (!currentLine.empty()) {
        descFont.drawString(currentLine, contentX, descY);
    }
}

void TooltipManager::show(const TooltipInfo& info) {
    currentInfo = info;

    // Cancelar ocultamiento si el token vuelve a aparecer
    if (state == VISIBLE || state == FADING_IN ||
        state == HIDING_DELAYED || state == FADING_OUT) {
        state = VISIBLE;
        fadeAlpha = 1.0f;
        visibleStartTime = ofGetElapsedTimeMillis();
    } else {
        // Solo desde HIDDEN iniciamos fade-in
        state = FADING_IN;
        fadeStartTime = ofGetElapsedTimeMillis();
    }
}

void TooltipManager::hide() {
    // ✅ Iniciar delay de 0.5s antes de fade-out (cuando se retira el token)
    if (state == VISIBLE || state == FADING_IN) {
        state = HIDING_DELAYED;
        visibleStartTime = ofGetElapsedTimeMillis();  // Reiniciar timer para el delay
    }
    // Si ya está en HIDING_DELAYED o FADING_OUT, no hacer nada (dejar que termine)
}

void TooltipManager::setPosition(float newX, float newY) {
    x = newX;
    y = newY;
}

void TooltipManager::setSize(float w, float h) {
    width = w;
    height = h;
}

void TooltipManager::setFadeDuration(float ms) {
    fadeDuration = ms;
}

void TooltipManager::setVisibleDuration(float ms) {
    visibleDuration = ms;
}

float TooltipManager::getElapsedTime(uint64_t startTime) {
    return ofGetElapsedTimeMillis() - startTime;
}
