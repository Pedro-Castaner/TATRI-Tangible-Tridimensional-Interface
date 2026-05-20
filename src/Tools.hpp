// Tools.hpp
#pragma once
#include "ofMain.h"
#include "TooltipManager.hpp"

// Forward declarations
class Shape3D;

// ---------------- STRUCTURES ----------------
struct TouchPointWithID {
    ofVec2f pos;
    long sessionID;
    
    TouchPointWithID() : pos(0, 0), sessionID(-1) {}
    TouchPointWithID(const ofVec2f& p, long id) : pos(p), sessionID(id) {}
};

// ---------------- ENUMS ----------------
enum class RotationAxis { None, X, Y, Z };
enum class ScaleAxis { None, X, Y, Z, Uniform };
enum class TranslateAxis { None, X, Y, Z };
enum class TranslateInteractionMode { Menu, Gesture, Knob };
enum class ScaleInteractionMode { Menu, Gesture, Knob };

// ---------------- BASE TOOL ----------------
class Tool {
public:
    int id = -1;

    // ✅ Sistema de fade para menús de herramientas
    enum MenuState {
        MENU_HIDDEN,
        MENU_FADING_IN,
        MENU_VISIBLE,
        MENU_HIDING_DELAYED,
        MENU_FADING_OUT
    };

    MenuState menuState = MENU_HIDDEN;
    float menuAlpha = 0.0f;            // 0.0 a 1.0 para fade in/out
    float fadeDuration = 300.0f;        // 300ms para fade (igual que tooltips)
    float hideDelay = 2000.0f;          // 2.0s delay antes de fade-out
    uint64_t fadeStartTime = 0;
    uint64_t hideDelayStartTime = 0;

    Tool() {}
    Tool(int id) : id(id) {}
    virtual ~Tool() {}

    virtual void update(const vector<ofVec2f>& pts, RotationAxis axis = RotationAxis::None) {}
    virtual void draw(int x, int y) {}

    // ✅ Actualizar estado de fade del menú
    void updateMenuFade() {
        float elapsed = 0.0f;

        switch (menuState) {
            case MENU_FADING_IN:
                elapsed = ofGetElapsedTimeMillis() - fadeStartTime;
                menuAlpha = ofClamp(elapsed / fadeDuration, 0.0f, 1.0f);
                if (menuAlpha >= 1.0f) {
                    menuState = MENU_VISIBLE;
                }
                break;

            case MENU_VISIBLE:
                menuAlpha = 1.0f;
                break;

            case MENU_HIDING_DELAYED:
                elapsed = ofGetElapsedTimeMillis() - hideDelayStartTime;
                menuAlpha = 1.0f;  // Mantener visible durante delay
                if (elapsed >= hideDelay) {
                    menuState = MENU_FADING_OUT;
                    fadeStartTime = ofGetElapsedTimeMillis();
                }
                break;

            case MENU_FADING_OUT:
                elapsed = ofGetElapsedTimeMillis() - fadeStartTime;
                menuAlpha = 1.0f - ofClamp(elapsed / fadeDuration, 0.0f, 1.0f);
                if (menuAlpha <= 0.0f) {
                    menuState = MENU_HIDDEN;
                    menuAlpha = 0.0f;
                }
                break;

            case MENU_HIDDEN:
                menuAlpha = 0.0f;
                break;
        }
    }

    // ✅ Mostrar menú (cancela ocultamiento o inicia fade-in)
    void showMenu() {
        // Cancelar ocultamiento si el menú estaba desapareciendo
        if (menuState == MENU_VISIBLE || menuState == MENU_FADING_IN ||
            menuState == MENU_HIDING_DELAYED || menuState == MENU_FADING_OUT) {
            menuState = MENU_VISIBLE;
            menuAlpha = 1.0f;
        } else {
            // Solo desde MENU_HIDDEN iniciamos fade-in
            menuState = MENU_FADING_IN;
            fadeStartTime = ofGetElapsedTimeMillis();
        }
    }

    // ✅ Ocultar menú (inicia delay + fade-out)
    void hideMenu() {
        if (menuState == MENU_VISIBLE || menuState == MENU_FADING_IN) {
            menuState = MENU_HIDING_DELAYED;
            hideDelayStartTime = ofGetElapsedTimeMillis();
        }
    }

    // ✅ Método virtual para obtener información del tooltip
    virtual TooltipInfo getTooltipInfo() const {
        return TooltipInfo("Herramienta", "Herramienta no identificada", ofColor(255, 255, 255));
    }
};

// ---------------- ROTATION TOOL ----------------
class RotationTool : public Tool {
public:
    bool  active    = false;
    float lastAngle = 0.0f;
    
    // ⚠️ DEPRECADO: Mantener para compatibilidad temporal (solo para HUD)
    float rotationX = 0.0f;
    float rotationY = 0.0f;
    float rotationZ = 0.0f;
    
    // ✅ Rotación incremental (acumulador de cambios)
    float deltaRotation = 0.0f;
    RotationAxis lastAxis = RotationAxis::None;
    glm::vec3 basisRight = glm::vec3(1, 0, 0);
    glm::vec3 basisUp = glm::vec3(0, 1, 0);
    glm::vec3 basisForward = glm::vec3(0, 0, 1);

    void update(const vector<ofVec2f>& pts, RotationAxis currentAxis = RotationAxis::None) override;
    void draw(int x, int y) override;
    TooltipInfo getTooltipInfo() const override;

    // ✅ Aplicar rotación incremental a un cuaternión (espacio global)
    glm::quat applyRotation(const glm::quat& currentRotation, RotationAxis axis);
    void setCameraBasis(const glm::vec3& right, const glm::vec3& up, const glm::vec3& forward);
};

// ---------------- AXIS SELECTOR TOOL ----------------
class AxisSelectorTool : public Tool {
public:
    RotationAxis currentAxis = RotationAxis::None;
    bool menuActive = false;
    bool lockedMenu = false;
    ofVec2f fixedMenuCenter;
    float buttonRadius  = 140.0f;
    float buttonSpacing = 80.0f;

    // ✅ Fuente para texto de botones (compartida entre todas las instancias)
    static ofTrueTypeFont buttonFont;

    void update(const vector<ofVec2f>& pts, RotationAxis axis = RotationAxis::None) override;
    void draw(int x, int y) override;
    void drawMenu();
    TooltipInfo getTooltipInfo() const override;

    void setLockedPoints(const vector<ofVec2f>& lockedPts);
    void clearLocked();
    bool hasLockedMenu() const;
    void updateSelection(const vector<ofVec2f>& currentPts);

private:
    ofVec2f buttonPos(const string& axis) const;
    bool insideCircle(const ofVec2f& p, const ofVec2f& c) const;
    void drawButton(const string& label, const ofColor& color);
};

// ---------------- SCALE TOOL ----------------
class ScaleTool : public Tool {
public:
    glm::vec3 origin = glm::vec3(0,0,0);        // centro inicial del token al colocarlo
    glm::vec3 initialScale = glm::vec3(1.0f);   // escala del objeto al colocar el token
    bool locked = false;                         // ya existente

    glm::vec3 scaleVec = glm::vec3(1.0f);
    glm::vec3 perAxisFactor = glm::vec3(1.0f);  // Factor acumulado por eje (relativo a initialScale)
    float menuScaleAccum = 0.0f;                // Acumulador incremental (modo menú)
    vector<TouchPointWithID> lockedPointsWithIDs; // Snapshot con Session IDs (modo perilla)
    long knobRefSessionId = -1;
    ofVec2f knobLastRefPos = ofVec2f(0, 0);

    // Horizontal translation mode (for physical tokens)
    ofVec2f initialCenter;  // Centro inicial del token
    ofVec2f menuLastCenter = ofVec2f(0, 0);  // Última posición del token para acumulación (modo menú)
    float sensitivity = 100.0f;  // Pixels needed for 1x scale change
    
    // Scale axis selection (menu + bar interface)
    ScaleAxis currentAxis = ScaleAxis::Uniform;
    bool menuActive = false;
    bool lockedMenu = false;
    ofVec2f fixedMenuCenter;
    float buttonWidth = 350.0f;      // Ancho del rectángulo (modo menú)
    float buttonHeight = 150.0f;     // Alto del rectángulo (modo menú)
    float cornerRadius = 50.0f;      // Radio de esquinas redondeadas (modo menú)
    float buttonSpacing = 200.0f;    // Espaciado entre botones (modo menú)
    float buttonRadius = 90.0f;      // Radio de botones circulares (modo perilla)
    float radialDistance = 220.0f;   // Distancia radial desde el centro (modo perilla)
    float motionDeadzone = 10.0f;    // Margen para evitar saltos con pequeños movimientos

    ScaleTool() : Tool() {}
    ScaleTool(int id) : Tool(id) {}

    void setLockedPoints(const vector<ofVec2f>& lockedPts, const glm::vec3& currentScale);
    void setLockedPointsWithIDs(const vector<TouchPointWithID>& lockedPts, const glm::vec3& currentScale);
    void reset();
    void update(const vector<ofVec2f>& pts, RotationAxis axis = RotationAxis::None) override;
    void updateWithIDs(const vector<TouchPointWithID>& pts);
    void draw(int x, int y) override;
    void drawMenu();
    void drawScaleBar();
    void updateSelection(const vector<ofVec2f>& currentPts);
    void setInteractionMode(ScaleInteractionMode mode);
    ScaleInteractionMode getInteractionMode() const { return interactionMode; }
    bool isGestureMode() const { return interactionMode == ScaleInteractionMode::Gesture; }
    TooltipInfo getTooltipInfo() const override;

    glm::vec3 getScale() const { return scaleVec; }
    float getCurrentFactor() const;
    string getAxisName() const;

    // ✅ Fuente para texto de botones (compartida entre todas las instancias)
    static ofTrueTypeFont buttonFont;

private:
    // Gesture-based interaction
    ScaleInteractionMode interactionMode = ScaleInteractionMode::Menu;
    float gestureThreshold = 20.0f;
    float dominantAxisRatio = 1.35f;
    ofVec2f gestureStart;
    bool gestureAxisChosen = false;
    float knobLastOrientation = 0.0f;
    bool knobHasOrientation = false;
    ofVec2f knobLastCenter = ofVec2f(0, 0);

    ofVec2f buttonPos(const string& axis) const;
    bool insideCircle(const ofVec2f& p, const ofVec2f& c) const;
    void drawButton(const string& label, const ofColor& color);
};

// ---------------- TRANSLATE TOOL ----------------
class TranslateTool : public Tool {
public:
    bool locked = false;
    ofVec2f initialCenter;
    glm::vec3 initialPos = glm::vec3(0.0f);
    glm::vec3 perAxisDelta = glm::vec3(0.0f);  // Delta acumulado por eje

    ofVec2f menuLastCenter = ofVec2f(0, 0);  // Última posición del token para acumulación (modo menú)
    float menuAxisAccum = 0.0f;              // Acumulador incremental en eje activo (modo menú)

    // Axis selection (menu interface - ahora con rectángulos redondeados)
    TranslateAxis currentAxis = TranslateAxis::None;
    bool menuActive = false;
    bool lockedMenu = false;
    ofVec2f fixedMenuCenter;
    
    // Rectángulos redondeados con orientaciones diferentes (modo menú)
    float buttonWidthH = 400.0f;     // Ancho botones horizontales (X, Z)
    float buttonHeightH = 120.0f;    // Alto botones horizontales
    float buttonWidthV = 120.0f;     // Ancho botón vertical (Y)
    float buttonHeightV = 400.0f;    // Alto botón vertical
    float cornerRadius = 50.0f;      // Radio de esquinas redondeadas
    float buttonSpacingH = 300.0f;   // Espaciado horizontal (X, Z)
    float buttonSpacingV = 280.0f;   // Espaciado vertical (Y)
    // Circular menu layout (modo perilla)
    float buttonRadius = 90.0f;      // Radio de botones circulares
    float radialDistance = 220.0f;   // Distancia radial desde el centro
    float sensitivity = 0.4f;        // Menor sensibilidad para traslación
    float motionDeadzone = 10.0f;    // Margen para filtrar ruido

    // Gesture-based interaction
    TranslateInteractionMode interactionMode = TranslateInteractionMode::Menu;
    float gestureThreshold = 20.0f;        // Movimiento mínimo para fijar eje por gesto
    float dominantAxisRatio = 1.35f;       // Si un eje es 1.35x más grande, es el dominante
    ofVec2f gestureStart;
    bool gestureAxisChosen = false;
    float knobLastOrientation = 0.0f;
    bool knobHasOrientation = false;
    ofVec2f knobLastCenter = ofVec2f(0, 0);
    glm::vec3 basisRight = glm::vec3(1, 0, 0);    // Vector de traslación para eje X (vista o mundo)
    glm::vec3 basisUp = glm::vec3(0, 1, 0);       // Vector de traslación para eje Y (vista o mundo)
    glm::vec3 basisForward = glm::vec3(0, 0, 1);  // Vector de traslación para eje Z (vista o mundo)

    void setLockedPoints(const vector<ofVec2f>& lockedPts, const glm::vec3& currentPos);
    void reset();
    void update(const vector<ofVec2f>& pts, RotationAxis axis = RotationAxis::None) override;
    void draw(int x, int y) override;
    void drawMenu();
    void updateSelection(const vector<ofVec2f>& currentPts);
    void setInteractionMode(TranslateInteractionMode mode);
    TranslateInteractionMode getInteractionMode() const { return interactionMode; }
    bool isGestureMode() const { return interactionMode == TranslateInteractionMode::Gesture; }
    void setCameraBasis(const glm::vec3& right, const glm::vec3& up, const glm::vec3& forward);
    glm::vec3 getBasisRight() const { return basisRight; }
    glm::vec3 getBasisUp() const { return basisUp; }
    glm::vec3 getBasisForward() const { return basisForward; }
    glm::vec3 getPosition() const { return position; }
    TooltipInfo getTooltipInfo() const override;
    
    // ✅ Fuente para texto de botones (compartida entre todas las instancias)
    static ofTrueTypeFont buttonFont;

private:
    glm::vec3 position = glm::vec3(0.0f);
    ofVec2f buttonPos(const string& axis) const;
    bool insideCircle(const ofVec2f& p, const ofVec2f& c) const;
    void drawButton(const string& label, const ofColor& color);
};

// ---------------- SPAWN TOOL ----------------
enum class SpawnShape { Box, Cylinder, Cone, Sphere };

class SpawnTool : public Tool {
public:
    SpawnTool() : Tool() {}
    SpawnTool(int id) : Tool(id) {}
    
    void setLockedPoints(const vector<ofVec2f>& lockedPts);
    void reset();
    void update(const vector<ofVec2f>& pts, RotationAxis axis = RotationAxis::None) override;
    void draw(int x, int y) override;
    void drawMenu();
    void updateSelection(const vector<ofVec2f>& currentPts);
    TooltipInfo getTooltipInfo() const override;

    SpawnShape getSelectedShape() const { return selectedShape; }
    bool hasSelection() const { return selectedShape != SpawnShape::Box || selectionMade; }
    void clearSelection() { selectionMade = false; selectedShape = SpawnShape::Box; }
    
    // Public access (consistent with other tools like ScaleTool)
    bool menuActive = false;
    bool lockedMenu = false;
    
    // ✅ Fuente para texto de botones (compartida entre todas las instancias)
    static ofTrueTypeFont buttonFont;
    
private:
    SpawnShape selectedShape = SpawnShape::Box;
    bool selectionMade = false;  // Flag para saber si se hizo una selección
    ofVec2f fixedMenuCenter;
    
    // Layout radial circular (como AxisSelectorTool)
    float buttonRadius = 80.0f;     // Reducido de 140 a 80 para evitar sobreposición
    float radialDistance = 280.0f;  // Aumentado de 200 a 280 para más separación

    ofVec2f buttonPos(const string& label) const;
    bool insideCircle(const ofVec2f& p, const ofVec2f& c) const;
    void drawButton(const string& label, const ofColor& color);
    void drawShapeIcon(SpawnShape shape, const ofVec2f& pos, float size) const;
};

// ---------------- UTILITY TOOL ----------------
enum class UtilityAction { None, Delete, Undo, Reset };

class UtilityTool : public Tool {
public:
    UtilityTool() : Tool() {}
    UtilityTool(int id) : Tool(id) {}
    
    void setLockedPoints(const vector<ofVec2f>& lockedPts);
    void reset();
    void update(const vector<ofVec2f>& pts, RotationAxis axis = RotationAxis::None) override;
    void draw(int x, int y) override;
    void drawMenu();
    void updateSelection(const vector<ofVec2f>& currentPts);
    TooltipInfo getTooltipInfo() const override;

    UtilityAction getSelectedAction() const { return selectedAction; }
    bool hasSelection() const { return selectedAction != UtilityAction::None; }
    void clearSelection() { selectedAction = UtilityAction::None; }
    
    // Public access (consistent with other tools)
    bool menuActive = false;
    bool lockedMenu = false;
    
    // ✅ Fuente para texto de botones (compartida entre todas las instancias)
    static ofTrueTypeFont buttonFont;
    
private:
    UtilityAction selectedAction = UtilityAction::None;
    ofVec2f fixedMenuCenter;
    
    // Layout radial (3 círculos en arco)
    float buttonRadius = 80.0f;
    float radialDistance = 280.0f;
    
    ofVec2f buttonPos(const string& label) const;
    bool insideCircle(const ofVec2f& p, const ofVec2f& c) const;
    void drawButton(const string& label, const ofColor& color);
};

// ---------------- SELECTION TOOL ----------------
class SelectionTool : public Tool {
public:
    SelectionTool() : Tool() {}
    SelectionTool(int id) : Tool(id) {}
    
    void update(const vector<ofVec2f>& pts, RotationAxis axis = RotationAxis::None) override;
    void draw(int x, int y) override;
    TooltipInfo getTooltipInfo() const override;

    // Raycast 3D para detectar objetos
    int performRaycast(const ofVec2f& screenPos,
                       const ofEasyCam& cam,
                       const vector<std::unique_ptr<Shape3D>>& shapes);

    int getSelectedIndex() const { return selectedIndex; }
    bool hasSelection() const { return selectedIndex >= 0; }
    void clearSelection() { selectedIndex = -1; }
    
private:
    int selectedIndex = -1;  // Índice de figura seleccionada (-1 = ninguna)
    ofVec2f lastTokenCenter;
    bool hasPerformedSelection = false;
};

// ---------------- COLOR TOOL ----------------
class ColorTool : public Tool {
public:
    bool locked = false;
    ofVec2f initialCenter;
    ofColor currentColor = ofColor::white;
    
    // Color wheel variables
    float wheelRadius = 200.0f;  // ✅ Aumentado de 120 a 200 para que el token no la tape
    float indicatorRadius = 25.0f;
    float currentHue = 0.0f; // Hue actual (0-255)
    int currentShapeIndex = -1;  // Track which shape this tool is modifying
    
    // Token B orientation tracking (principal axis, no edge/vertex dependency)
    float initialOrientation = 0.0f;  // Orientación inicial del token (ángulo centroide→vértice20)
    float lastOrientation = 0.0f;     // Última orientación válida
    vector<ofVec2f> lockedPoints;     // Snapshot de puntos (compatibilidad)
    vector<TouchPointWithID> lockedPointsWithID; // Snapshot de puntos CON Session IDs
    float jumpThreshold = 100.0f;     // Threshold para detectar saltos (similar a cámara)

    // Sobrecarga para mantener compatibilidad con código existente
    void setLockedPoints(const vector<ofVec2f>& lockedPts, const ofColor& startColor);
    void setLockedPointsWithIDs(const vector<TouchPointWithID>& lockedPts, const ofColor& startColor, int shapeIndex);
    void reset();
    void update(const vector<ofVec2f>& pts, RotationAxis axis = RotationAxis::None) override;
    void updateWithIDs(const vector<TouchPointWithID>& pts);
    void draw(int x, int y) override;
    TooltipInfo getTooltipInfo() const override;

    // Métodos de dibujo de la rueda cromática
    void drawColorWheel();
    void drawColorIndicator();
    void drawAngleIndicator(const vector<ofVec2f>& pts);
    void drawAngleIndicatorWithIDs(const vector<TouchPointWithID>& pts);

    ofColor getColor() const { return currentColor; }
    
private:
    ofVec2f centroid(const vector<ofVec2f>& pts) const;
    float principalAxisAngle(const vector<ofVec2f>& pts, float fallbackAngle) const;
};

// ---------------- UTILS ----------------
std::vector<std::vector<ofVec2f>> clusterTouches(const std::vector<ofVec2f>& pts, float threshold);
