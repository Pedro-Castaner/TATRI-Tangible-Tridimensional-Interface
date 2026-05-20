// Shape3D.hpp
#pragma once
#include "ofMain.h"

// Forward declarations para evitar dependencias circulares
enum class RotationAxis;
enum class TranslateAxis;
enum class ScaleAxis;

enum class ShapeType {
    Box,
    Cylinder,
    Cone,
    Sphere
};

class Shape3D {
public:
    void setup();
    void setup(ShapeType type);  // Setup con tipo específico
    void draw();
    void drawWireframe();  // Dibujar wireframe para feedback visual de selección
    
    // Gizmos de transformación
    void drawRotationGizmo(RotationAxis axis);
    void drawTranslationGizmo(TranslateAxis axis);
    void drawScaleGizmo(ScaleAxis axis);

    void setRotationX(float angle);
    void setRotationY(float angle);
    void setRotationZ(float angle);
    void setRotation(float angle, const glm::vec3& axis);

    void setPosition(const glm::vec3& pos);
    void setScale(const glm::vec3& scaleVec);
    void setShapeType(ShapeType type);
    void setColor(const ofColor& col);
    void resetTransform();

    // getters que usamos desde ofApp
    glm::vec3 getPosition() const { return position; }
    ofColor getColor() const { return color; }
    glm::vec3 getScale() const { return scale; }
    float getRotationX() const { return rotationX; }
    float getRotationY() const { return rotationY; }
    float getRotationZ() const { return rotationZ; }
    glm::vec3 getOrientationEulerDeg() const { return glm::vec3(rotationX, rotationY, rotationZ); }
    ShapeType getShapeType() const { return shapeType; }
    
    // ✅ Acceso al cuaternión de rotación (espacio global)
    glm::quat getRotation() const { return rotation; }
    void setRotation(const glm::quat& rot) { rotation = rot; }

private:
    ShapeType shapeType = ShapeType::Box;
    
    // Primitivas de openFrameworks
    ofBoxPrimitive box;
    ofCylinderPrimitive cylinder;
    ofConePrimitive cone;
    ofSpherePrimitive sphere;
    
    glm::vec3 position;
    glm::vec3 scale;
    
    // ✅ Rotación usando cuaternión (espacio global, sin gimbal lock)
    glm::quat rotation = glm::quat(1, 0, 0, 0);  // Identidad (sin rotación)
    
    // ⚠️ DEPRECADO: Mantener para compatibilidad temporal con getters
    float rotationX = 0, rotationY = 0, rotationZ = 0;
    
    ofColor color = ofColor(180);  // Color individual del objeto
};
