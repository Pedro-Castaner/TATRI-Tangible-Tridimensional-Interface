#include "Shape3D.hpp"
#include "Tools.hpp"  // Para los enums de ejes

void Shape3D::setup() {
    setup(ShapeType::Box);  // Default: Box
}

void Shape3D::setup(ShapeType type) {
    shapeType = type;

    // Inicializar todas las primitivas con tamaño consistente y mayor resolución para smooth shading
    box.set(150);
    box.setResolution(8);  // Aumentado de 4 a 8 (2x) - normales más suaves

    cylinder.set(75, 150, 32, 8);  // radius, height, radiusSegments (20→32), heightSegments (4→8) - smooth shading

    cone.set(75, 150, 32, 8, 4);  // radius, height, radiusSegments (20→32), heightSegments (4→8), capSegments

    sphere.set(75, 64);  // radius, resolution (48→64) - normales más suaves, reduce parpadeo

    resetTransform();
}

void Shape3D::draw() {
    ofPushMatrix();
    ofTranslate(position);
    ofScale(scale);
    
    // ✅ Aplicar rotación con cuaternión (espacio global)
    glm::mat4 rotMat = glm::toMat4(rotation);
    ofMultMatrix(rotMat);
    
    // Dibujar la primitiva correcta según el tipo
    switch (shapeType) {
        case ShapeType::Box:
            box.draw();
            break;
        case ShapeType::Cylinder:
            cylinder.draw();
            break;
        case ShapeType::Cone:
            cone.draw();
            break;
        case ShapeType::Sphere:
            sphere.draw();
            break;
    }
    
    ofPopMatrix();
}

// ========== GIZMOS DE TRANSFORMACIÓN ==========

void Shape3D::drawRotationGizmo(RotationAxis axis) {
    // Calcular tamaño base adaptativo según la escala del objeto
    float maxScale = std::max(std::max(scale.x, scale.y), scale.z);
    float gizmoRadius = maxScale * 130.0f;  // Aumentado de 100 a 130 para evitar colisión
    
    // Constantes visuales
    const float LINE_WIDTH_NORMAL = 2.0f;
    const float LINE_WIDTH_ACTIVE = 5.0f;
    const ofColor COLOR_X = ofColor(255, 0, 0);
    const ofColor COLOR_Y = ofColor(0, 255, 0);
    const ofColor COLOR_Z = ofColor(0, 0, 255);
    const int ALPHA_INACTIVE = 128;
    
    ofPushMatrix();
    ofPushStyle();
    
    ofTranslate(position);
    ofNoFill();
    ofDisableLighting();
    
    // -------- Círculo X (rojo) - Plano YZ --------
    ofSetColor(COLOR_X, (axis == RotationAxis::X) ? 255 : ALPHA_INACTIVE);
    ofSetLineWidth((axis == RotationAxis::X) ? LINE_WIDTH_ACTIVE : LINE_WIDTH_NORMAL);
    
    ofPushMatrix();
    ofRotateDeg(90, 0, 1, 0);  // Rotar para que quede en plano YZ
    ofDrawCircle(0, 0, gizmoRadius);
    ofPopMatrix();
    
    // -------- Círculo Y (verde) - Plano XZ --------
    ofSetColor(COLOR_Y, (axis == RotationAxis::Y) ? 255 : ALPHA_INACTIVE);
    ofSetLineWidth((axis == RotationAxis::Y) ? LINE_WIDTH_ACTIVE : LINE_WIDTH_NORMAL);
    
    ofPushMatrix();
    ofRotateDeg(90, 1, 0, 0);  // Rotar para que quede en plano XZ
    ofDrawCircle(0, 0, gizmoRadius);
    ofPopMatrix();
    
    // -------- Círculo Z (azul) - Plano XY --------
    ofSetColor(COLOR_Z, (axis == RotationAxis::Z) ? 255 : ALPHA_INACTIVE);
    ofSetLineWidth((axis == RotationAxis::Z) ? LINE_WIDTH_ACTIVE : LINE_WIDTH_NORMAL);
    
    ofDrawCircle(0, 0, gizmoRadius);  // Ya está en plano XY
    
    ofPopStyle();
    ofPopMatrix();
}

void Shape3D::drawTranslationGizmo(TranslateAxis axis) {
    // Calcular tamaño base adaptativo
    float maxScale = std::max(std::max(scale.x, scale.y), scale.z);
    float arrowLength = maxScale * 150.0f;
    float arrowHeadSize = maxScale * 20.0f;
    
    // Constantes visuales
    const float LINE_WIDTH_NORMAL = 3.0f;
    const float LINE_WIDTH_ACTIVE = 6.0f;
    const ofColor COLOR_X = ofColor(255, 0, 0);
    const ofColor COLOR_Y = ofColor(0, 255, 0);
    const ofColor COLOR_Z = ofColor(0, 0, 255);
    const int ALPHA_INACTIVE = 128;
    
    ofPushMatrix();
    ofPushStyle();
    
    ofTranslate(position);
    ofDisableLighting();
    
    // ======== EJE X (rojo) - BIDIRECCIONAL ========
    ofSetColor(COLOR_X, (axis == TranslateAxis::X) ? 255 : ALPHA_INACTIVE);
    ofSetLineWidth((axis == TranslateAxis::X) ? LINE_WIDTH_ACTIVE : LINE_WIDTH_NORMAL);
    
    // Flecha +X (derecha) →
    ofDrawLine(glm::vec3(0, 0, 0), glm::vec3(arrowLength, 0, 0));
    ofPushMatrix();
    ofTranslate(arrowLength, 0, 0);
    ofRotateDeg(90, 0, 0, 1);
    ofFill();
    ofDrawCone(0, 0, 0, arrowHeadSize, arrowHeadSize * 1.5f);
    ofNoFill();
    ofPopMatrix();
    
    // Flecha -X (izquierda) ←
    ofDrawLine(glm::vec3(0, 0, 0), glm::vec3(-arrowLength, 0, 0));
    ofPushMatrix();
    ofTranslate(-arrowLength, 0, 0);
    ofRotateDeg(-90, 0, 0, 1);
    ofFill();
    ofDrawCone(0, 0, 0, arrowHeadSize, arrowHeadSize * 1.5f);
    ofNoFill();
    ofPopMatrix();
    
    // ======== EJE Y (verde) - BIDIRECCIONAL ========
    ofSetColor(COLOR_Y, (axis == TranslateAxis::Y) ? 255 : ALPHA_INACTIVE);
    ofSetLineWidth((axis == TranslateAxis::Y) ? LINE_WIDTH_ACTIVE : LINE_WIDTH_NORMAL);
    
    // Flecha +Y (arriba) ↑
    ofDrawLine(glm::vec3(0, 0, 0), glm::vec3(0, arrowLength, 0));
    ofPushMatrix();
    ofTranslate(0, arrowLength, 0);
    ofRotateDeg(180, 1, 0, 0);
    ofFill();
    ofDrawCone(0, 0, 0, arrowHeadSize, arrowHeadSize * 1.5f);
    ofNoFill();
    ofPopMatrix();
    
    // Flecha -Y (abajo) ↓
    ofDrawLine(glm::vec3(0, 0, 0), glm::vec3(0, -arrowLength, 0));
    ofPushMatrix();
    ofTranslate(0, -arrowLength, 0);
    // Sin rotación - el cono apunta hacia arriba por defecto
    ofFill();
    ofDrawCone(0, 0, 0, arrowHeadSize, arrowHeadSize * 1.5f);
    ofNoFill();
    ofPopMatrix();
    
    // ======== EJE Z (azul) - BIDIRECCIONAL ========
    ofSetColor(COLOR_Z, (axis == TranslateAxis::Z) ? 255 : ALPHA_INACTIVE);
    ofSetLineWidth((axis == TranslateAxis::Z) ? LINE_WIDTH_ACTIVE : LINE_WIDTH_NORMAL);
    
    // Flecha +Z (adelante) ⟶
    ofDrawLine(glm::vec3(0, 0, 0), glm::vec3(0, 0, arrowLength));
    ofPushMatrix();
    ofTranslate(0, 0, arrowLength);
    ofRotateDeg(-90, 1, 0, 0);
    ofFill();
    ofDrawCone(0, 0, 0, arrowHeadSize, arrowHeadSize * 1.5f);
    ofPopMatrix();
    
    // Flecha -Z (atrás) ⟵
    ofDrawLine(glm::vec3(0, 0, 0), glm::vec3(0, 0, -arrowLength));
    ofPushMatrix();
    ofTranslate(0, 0, -arrowLength);
    ofRotateDeg(90, 1, 0, 0);
    ofFill();
    ofDrawCone(0, 0, 0, arrowHeadSize, arrowHeadSize * 1.5f);
    ofPopMatrix();
    
    ofPopStyle();
    ofPopMatrix();
}

void Shape3D::drawScaleGizmo(ScaleAxis axis) {
    // Calcular tamaño base adaptativo
    float maxScale = std::max(std::max(scale.x, scale.y), scale.z);
    float lineLength = maxScale * 120.0f;
    float cubeSize = maxScale * 15.0f;
    
    // Constantes visuales
    const float LINE_WIDTH_NORMAL = 3.0f;
    const float LINE_WIDTH_ACTIVE = 6.0f;
    const ofColor COLOR_X = ofColor(255, 0, 0);
    const ofColor COLOR_Y = ofColor(0, 255, 0);
    const ofColor COLOR_Z = ofColor(0, 0, 255);
    const int ALPHA_INACTIVE = 128;
    
    ofPushMatrix();
    ofPushStyle();
    
    ofTranslate(position);
    ofDisableLighting();
    
    // Determinar qué ejes están activos
    bool xActive = (axis == ScaleAxis::X || axis == ScaleAxis::Uniform);
    bool yActive = (axis == ScaleAxis::Y || axis == ScaleAxis::Uniform);
    bool zActive = (axis == ScaleAxis::Z || axis == ScaleAxis::Uniform);
    
    // -------- Línea X (roja) con cubo --------
    ofSetColor(COLOR_X, xActive ? 255 : ALPHA_INACTIVE);
    ofSetLineWidth(xActive ? LINE_WIDTH_ACTIVE : LINE_WIDTH_NORMAL);
    
    // Línea
    ofDrawLine(glm::vec3(0, 0, 0), glm::vec3(lineLength, 0, 0));
    
    // Cubo en la punta
    ofPushMatrix();
    ofTranslate(lineLength, 0, 0);
    ofFill();
    ofDrawBox(0, 0, 0, cubeSize);
    ofNoFill();
    ofPopMatrix();
    
    // -------- Línea Y (verde) con cubo --------
    ofSetColor(COLOR_Y, yActive ? 255 : ALPHA_INACTIVE);
    ofSetLineWidth(yActive ? LINE_WIDTH_ACTIVE : LINE_WIDTH_NORMAL);
    
    // Línea
    ofDrawLine(glm::vec3(0, 0, 0), glm::vec3(0, lineLength, 0));
    
    // Cubo en la punta
    ofPushMatrix();
    ofTranslate(0, lineLength, 0);
    ofFill();
    ofDrawBox(0, 0, 0, cubeSize);
    ofNoFill();
    ofPopMatrix();
    
    // -------- Línea Z (azul) con cubo --------
    ofSetColor(COLOR_Z, zActive ? 255 : ALPHA_INACTIVE);
    ofSetLineWidth(zActive ? LINE_WIDTH_ACTIVE : LINE_WIDTH_NORMAL);
    
    // Línea
    ofDrawLine(glm::vec3(0, 0, 0), glm::vec3(0, 0, lineLength));
    
    // Cubo en la punta
    ofPushMatrix();
    ofTranslate(0, 0, lineLength);
    ofFill();
    ofDrawBox(0, 0, 0, cubeSize);
    ofPopMatrix();
    
    ofPopStyle();
    ofPopMatrix();
}

void Shape3D::setRotationX(float angle) { 
    rotationX = angle;
    // ⚠️ DEPRECADO: Actualizar cuaternión desde ángulos Euler (temporal)
    rotation = glm::quat(glm::radians(glm::vec3(rotationX, rotationY, rotationZ)));
}

void Shape3D::setRotationY(float angle) { 
    rotationY = angle;
    // ⚠️ DEPRECADO: Actualizar cuaternión desde ángulos Euler (temporal)
    rotation = glm::quat(glm::radians(glm::vec3(rotationX, rotationY, rotationZ)));
}

void Shape3D::setRotationZ(float angle) { 
    rotationZ = angle;
    // ⚠️ DEPRECADO: Actualizar cuaternión desde ángulos Euler (temporal)
    rotation = glm::quat(glm::radians(glm::vec3(rotationX, rotationY, rotationZ)));
}
void Shape3D::setRotation(float angle, const glm::vec3& axis) { /* opcional */ }
void Shape3D::setPosition(const glm::vec3& pos) { position = pos; }
void Shape3D::setScale(const glm::vec3& scaleVec) { scale = scaleVec; }

void Shape3D::setShapeType(ShapeType type) {
    shapeType = type;
}

void Shape3D::setColor(const ofColor& col) {
    color = col;
}

void Shape3D::resetTransform() {
    position = glm::vec3(0);
    scale = glm::vec3(1);
    rotationX = rotationY = rotationZ = 0;
    rotation = glm::quat(1, 0, 0, 0);  // Identidad (sin rotación)
    color = ofColor(180);  // Resetear color también
}

void Shape3D::drawWireframe() {
    ofPushMatrix();
    ofTranslate(position);
    ofScale(scale);
    
    // ✅ Aplicar rotación con cuaternión (espacio global)
    glm::mat4 rotMat = glm::toMat4(rotation);
    ofMultMatrix(rotMat);
    
    // Dibujar wireframe según el tipo de figura
    switch (shapeType) {
        case ShapeType::Box:
            box.drawWireframe();
            break;
        case ShapeType::Cylinder:
            cylinder.drawWireframe();
            break;
        case ShapeType::Cone:
            cone.drawWireframe();
            break;
        case ShapeType::Sphere:
            sphere.drawWireframe();
            break;
    }
    
    ofPopMatrix();
}
