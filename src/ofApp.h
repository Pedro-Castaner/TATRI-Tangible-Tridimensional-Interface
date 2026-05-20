#pragma once
#include "ofMain.h"
#include "TokenManager.hpp"
#include "Tools.hpp"
#include "Shape3D.hpp"
#include "Config.hpp"
#include "TooltipManager.hpp"
#include "TransformFeedback3D.hpp"
#include <vector>
#include <memory>
#include <map>

// Camera orbital state encapsulation
struct CameraOrbitState {
    ofVec2f lastTokenPos = ofVec2f(0, 0);
    bool tokenActive = false;
    float radius = TUI3D::Camera::DEFAULT_RADIUS;
    float azimuth = 0.0f;
    float elevation = 0.0f;
    float prevAngle = 0.0f;
    bool hasInitialAngle = false;
};

// Bounding box for collision detection
struct BoundingBox {
    glm::vec3 min;
    glm::vec3 max;
};

// Scene snapshot for UNDO system
struct SceneSnapshot {
    struct ShapeSnapshot {
        ShapeType type;
        glm::vec3 position;
        glm::quat rotation;
        glm::vec3 scale;
        ofColor color;
        
        ShapeSnapshot(ShapeType t, glm::vec3 pos, glm::quat rot, glm::vec3 scl, ofColor col)
            : type(t), position(pos), rotation(rot), scale(scl), color(col) {}
    };
    
    std::vector<ShapeSnapshot> shapes;
    int activeShapeIndex;
    
    // Constructor para capturar estado actual
    static SceneSnapshot capture(const std::vector<std::unique_ptr<Shape3D>>& shapes, int activeIdx);
    
    // Restaurar snapshot en shapes
    void restore(std::vector<std::unique_ptr<Shape3D>>& shapes, int& activeIdx) const;
};

class ofApp : public ofBaseApp {
public:
    void setup()  override;
    void update() override;
    void draw()   override;
    void keyPressed(int key) override;

    void updateCameraWithToken(const std::vector<ofVec2f>& current, bool isLocked);
    void reset();  // Reset entire application state

    // Tooltip management
    void reflowTooltips();  // Reordenar tooltips dinámicamente sin gaps
    bool isAnyTooltipActive() const;  // Check if any tooltip is currently active

    // Collision detection and smart positioning
    BoundingBox getBoundingBox(const Shape3D* shape) const;
    bool checkCollision(const BoundingBox& a, const BoundingBox& b) const;
    glm::vec3 findNearestValidPosition(ShapeType type);
    
    // Navigation cube (Fusion 360 style)
    void drawNavigationCube();
    
    // 3D axis gizmo (X, Y, Z with colors and labels)
    void drawAxisGizmo();
    

    // --- Members principales ---
    TokenManager tokenManager;  // Multi-token manager
    std::vector<std::unique_ptr<Tool>> tools; // [0]=RotationTool, [1]=AxisSelectorTool
    std::map<char, std::unique_ptr<Tool>> tokenTools; // mapping token char -> tool (F,G,H)
    std::map<char, TooltipManager> tooltipManagers;  // Múltiples tooltips (uno por token activo)
    std::vector<char> tooltipInsertionOrder;  // Track tooltip placement order (temporal, not alphabetical)
    TransformFeedback3D transformFeedback;  // Sistema de feedback 3D para transformaciones
    
    // Múltiples figuras 3D
    std::vector<std::unique_ptr<Shape3D>> shapes;
    int activeShapeIndex = 0;  // Índice de figura activa (para transformaciones)

    // 6-point lighting system (product visualization - frontal emphasis, todas activas)
    ofLight keyLight;
    ofLight fillLight;
    ofLight rimLight;
    ofLight sideLeftLight;   // Luz lateral izquierda para gradientes suaves
    ofLight sideRightLight;  // Luz lateral derecha para gradientes suaves
    ofLight bottomLight;     // Luz de relleno inferior para iluminar caras inferiores

    // Multiple materials for visual variety
    ofMaterial matPlastic;
    ofMaterial matMetal;
    ofMaterial matGlass;

    // Gradient background mesh
    ofMesh backgroundGradient;

    ofEasyCam cam;

    // Material selection helper
    ofMaterial& getMaterialForShape(size_t index);

    // Setup helpers
    void setupLights();
    void setupMaterials();
    void setupGradientBackground();

    // Shadow rendering
    void drawGroundShadow(Shape3D* shape);

    // --- Estado / UI ---
    glm::vec3 lastScale = glm::vec3(1.0f);
    glm::vec3 lastPosition = glm::vec3(0.0f);
    ofColor shapeColor = ofColor(200);
    TranslateInteractionMode translateInteractionMode = TranslateInteractionMode::Menu;
    ScaleInteractionMode scaleInteractionMode = ScaleInteractionMode::Menu;

    // --- Cámara con token A (encapsulado) ---
    CameraOrbitState cameraState;
    
    // lista de toques actuales para el token de escala
    std::vector<ofVec2f> scaleTokenTouches;
    
    // --- UNDO System ---
    std::vector<SceneSnapshot> undoHistory;
    const int MAX_UNDO_STEPS = 20;
    
    void saveSnapshot();     // Guardar estado actual
    void performUndo();      // Deshacer última acción
    void clearUndoHistory(); // Limpiar historial

    // --- Tooltip System ---
    std::set<char> previousActiveTokens;  // Para detectar nuevos tokens
};
