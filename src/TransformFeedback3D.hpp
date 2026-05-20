#pragma once

#include "ofMain.h"
#include "Tools.hpp"
#include "Shape3D.hpp"

// Estado de fade para un texto individual
struct FeedbackTextState {
    enum State { HIDDEN, FADING_IN, VISIBLE, FADING_OUT };

    State state = HIDDEN;
    float alpha = 0.0f;
    uint64_t lastChangeTime = 0;
    uint64_t visibleStartTime = 0;

    // Constantes de timing (en milisegundos)
    static constexpr float FADE_IN_DURATION = 150.0f;
    static constexpr float VISIBLE_DURATION = 1500.0f;
    static constexpr float FADE_OUT_DURATION = 150.0f;

    void markChanged();
    void update();
    bool isVisible() const { return state != HIDDEN; }
};

// Información de un texto a renderizar
struct FeedbackText {
    std::string content;      // Texto a mostrar (ej: "195°", "ΔX: +25")
    glm::vec3 position;       // Posición 3D cerca del gizmo
    ofColor color;            // Color según eje (R/G/B)
    FeedbackTextState state;  // Estado de fade
    float lastValue = 0.0f;   // Para detectar cambios significativos
};

class TransformFeedback3D {
public:
    void setup();

    void update(const RotationTool* rotTool,
                RotationAxis rotAxis,
                const ScaleTool* scaleTool,
                const TranslateTool* transTool,
                const Shape3D* shape,
                const ofEasyCam& cam);

    void draw(const ofEasyCam& cam);

    // Configuración
    void setFontSize(int size) { fontSize = size; }
    void setGlowIntensity(float intensity) { glowIntensity = intensity; }

private:
    ofTrueTypeFont font;
    int fontSize = 48;  // Tamaño más grande para mejor calidad en 3D
    float glowIntensity = 0.3f;

    // Textos individuales por tipo de transformación
    FeedbackText rotationText;
    FeedbackText translationXText;
    FeedbackText translationYText;
    FeedbackText translationZText;
    FeedbackText scaleText;

    // Métodos de cálculo de matrices y posiciones
    glm::mat4 calculateBillboardMatrix(const glm::vec3& position, const ofEasyCam& cam);

    glm::vec3 calculateRotationTextPosition(const Shape3D* shape, RotationAxis axis);
    glm::vec3 calculateTranslateTextPosition(const Shape3D* shape, TranslateAxis axis);
    glm::vec3 calculateScaleTextPosition(const Shape3D* shape, ScaleAxis axis);

    // Métodos de renderizado
    void drawBillboardText(const FeedbackText& text, const ofEasyCam& cam);
    void drawTextMeshWithGlow(const ofMesh& mesh, const ofColor& color, float alpha);

    // Helpers de color
    ofColor getAxisColorRotation(RotationAxis axis);
    ofColor getAxisColorTranslate(TranslateAxis axis);
    ofColor getAxisColorScale(ScaleAxis axis);
};
