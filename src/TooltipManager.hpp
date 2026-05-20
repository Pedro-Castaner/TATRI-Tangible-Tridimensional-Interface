#pragma once

#include "ofMain.h"

// Estructura para la información del tooltip
struct TooltipInfo {
    string title;       // Título de la herramienta (ej: "Rotación")
    string description; // Descripción breve de uso
    ofColor accentColor; // Color de acento (asociado al eje o herramienta)

    TooltipInfo() : title(""), description(""), accentColor(255, 255, 255) {}
    TooltipInfo(const string& t, const string& d, const ofColor& c = ofColor(255, 255, 255))
        : title(t), description(d), accentColor(c) {}
};

class TooltipManager {
public:
    // Estados del tooltip
    enum State {
        HIDDEN,
        FADING_IN,
        VISIBLE,
        HIDING_DELAYED,  // Esperando 0.5s después de quitar token antes de fade-out
        FADING_OUT
    };

    TooltipManager();

    // Métodos principales
    void setup();
    void update();
    void draw();

    // Control de visibilidad
    void show(const TooltipInfo& info);
    void hide();

    // Configuración
    void setPosition(float x, float y); // Posición fija en pantalla
    void setSize(float w, float h);     // Tamaño del tooltip
    void setFadeDuration(float ms);     // Duración del fade (ms)
    void setVisibleDuration(float ms);  // Tiempo antes de auto-ocultar (ms)

    // Estado (para reordenamiento dinámico)
    bool isActive() const {
        return state == VISIBLE || state == FADING_IN || state == HIDING_DELAYED || state == FADING_OUT;
    }
    State getState() const { return state; }

private:
    // Información actual
    TooltipInfo currentInfo;

    // Estado y animación
    State state;
    float fadeAlpha;          // 0.0 a 1.0
    float fadeDuration;       // Duración del fade en ms
    float visibleDuration;    // Tiempo visible antes de auto-ocultar
    float hideDelay;          // Delay antes de fade-out cuando se oculta
    uint64_t fadeStartTime;   // Timestamp inicio de fade
    uint64_t visibleStartTime; // Timestamp inicio de visibilidad

    // Posición y tamaño
    float x, y;               // Posición en pantalla
    float width, height;      // Dimensiones del tooltip
    float padding;            // Padding interno
    float cornerRadius;       // Radio de bordes redondeados

    // Tipografía
    ofTrueTypeFont titleFont;
    ofTrueTypeFont descFont;
    bool fontsLoaded;

    // Colores
    ofColor backgroundColor;
    ofColor textColor;
    ofColor separatorColor;

    // Métodos privados
    void updateFade();
    void drawRoundedRect(float x, float y, float w, float h, float r, const ofColor& color);
    void drawText();
    float getElapsedTime(uint64_t startTime);
};
