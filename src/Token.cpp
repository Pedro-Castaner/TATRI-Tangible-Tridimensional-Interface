#include "Token.hpp"
#include "Config.hpp"

// ---------------- constructor / destructor ----------------
Token::Token(){
    // Token no longer manages TUIO directly - managed by TokenManager
    candidateForLocking = false;
    candidateStartTime = 0;
    candidateId = '?';
}

Token::~Token(){
    // Nothing to disconnect
}

void Token::setup(const ofVec2f& pos) {
    currentPoints.clear();
    lockedPoints.clear();
    locked = false;
    detectedId = '?';
    candidateForLocking = false;
    candidateStartTime = 0;
    candidateId = '?';
    candidatePoints.clear();
    candidatePointsWithIDs.clear();
}

void Token::updateWithPoints(const vector<ofVec2f>& screenPoints) {
    // Receive pre-processed points from TokenManager
    currentPoints = screenPoints;

    // LOCKING LOGIC (STICKY):
    // - Si no estamos locked y aparece >= 3 puntos -> hacemos snapshot (lock) usando los puntos actuales
    // - Mientras locked = true, no re-clasificamos (la clasificación queda con la snapshot)
    // - NUEVO: Cuando locked==true, NO resetear aunque currentPoints esté vacío
    //   El TokenManager se encarga de remover el token cuando YA NO tiene puntos asignados
    if (!locked) {
        if (currentPoints.size() >= 3) {
            // tomar snapshot (primeros puntos detectados en este instante)
            locked = true;
            lockedPoints = orderByAngle(currentPoints); // ordenados para clasificación estable
            detectedId = classifyShape(lockedPoints);
        } else {
            // no hay token (o no suficientes puntos) -> nada
            detectedId = '?';
        }
    }
    // locked == true: Mantener locked sin importar si currentPoints está vacío
    // El token permanece hasta que TokenManager lo remueva explícitamente
}

void Token::updateWithPointsAndIDs(const vector<TouchPointWithID>& pointsWithIDs, bool allowLocking, bool uiActive) {
    if (pointsWithIDs.empty()) {
        currentPoints.clear();
        currentPointsWithIDs.clear();
        // Reset candidate state if no points
        candidateForLocking = false;
        return;
    }

    // Guardar puntos CON Session IDs
    currentPointsWithIDs = pointsWithIDs;

    // Extraer solo coordenadas para compatibilidad con código existente
    currentPoints.clear();
    for (auto& pt : pointsWithIDs) {
        currentPoints.push_back(pt.pos);
    }

    // If already locked, maintain lock (existing behavior)
    if (locked) {
        return;
    }

    // PHASE 1: Delayed locking with stability verification
    if (!allowLocking) {
        // Not allowed to lock (too close to other token)
        candidateForLocking = false;
        return;
    }

    // Need at least 3 points to consider locking
    if (pointsWithIDs.size() < 3) {
        candidateForLocking = false;
        detectedId = '?';
        return;
    }

    // NEW: If UI is active, skip delay and lock immediately
    if (uiActive) {
        locked = true;
        lockedPoints = orderByAngle(currentPoints);
        lockedPointsWithIDs = pointsWithIDs;
        detectedId = classifyShape(lockedPoints);
        candidateForLocking = false;  // No need for candidate state

        ofLogNotice("Token") << "Token LOCKED immediately (UI active): " << detectedId;
        return;
    }

    // We have >= 3 points and are allowed to lock (NO UI active)
    if (!candidateForLocking) {
        // START candidate period
        candidateForLocking = true;
        candidateStartTime = ofGetElapsedTimeMillis();
        candidatePoints = currentPoints;
        candidateId = classifyShape(orderByAngle(candidatePoints));
        candidatePointsWithIDs = pointsWithIDs;

        ofLogNotice("Token") << "Candidate started: ID=" << candidateId
                            << " (waiting " << TUI3D::MultiToken::LOCKING_DELAY_MS << "ms for stability)";
    } else {
        // CHECK stability during candidate period
        uint64_t now = ofGetElapsedTimeMillis();
        uint64_t elapsed = now - candidateStartTime;
        float movement = calculateCentroidShift(candidatePoints, currentPoints);

        if (movement > TUI3D::MultiToken::STABILITY_THRESHOLD) {
            // Shape moved significantly - RESTART candidate period
            candidateStartTime = now;
            candidatePoints = currentPoints;
            candidateId = classifyShape(orderByAngle(candidatePoints));
            candidatePointsWithIDs = pointsWithIDs;

            ofLogNotice("Token") << "Candidate RESET due to movement (" << movement << "px): ID=" << candidateId;
        } else if (elapsed >= TUI3D::MultiToken::LOCKING_DELAY_MS) {
            // Stability period passed - verify classification consistency
            char newId = classifyShape(orderByAngle(currentPoints));

            if (newId == candidateId) {
                // Classification stable - LOCK IT
                locked = true;
                lockedPoints = orderByAngle(currentPoints);
                lockedPointsWithIDs = pointsWithIDs;
                detectedId = newId;

                ofLogNotice("Token") << "Token LOCKED after stability check: " << detectedId
                                    << " (delay: " << elapsed << "ms)";
            } else {
                // Classification changed during delay - RESTART
                candidateStartTime = now;
                candidatePoints = currentPoints;
                candidateId = newId;
                candidatePointsWithIDs = pointsWithIDs;

                ofLogNotice("Token") << "Candidate RESET due to classification change: "
                                    << candidateId << " -> " << newId;
            }
        }
    }
}

void Token::draw() {
    // Dibujar puntos actuales (círculos)
    ofSetLineWidth(2);
    ofFill();
    ofSetColor(200, 100, 255);
    for (auto &p : currentPoints) {
        ofDrawCircle(p, 15);
    }

    // Si hay snapshot locked, dibujar centroid del snapshot y texto desplazado
    if (locked && !lockedPoints.empty()) {
        ofVec2f c = centroid(lockedPoints);
        ofNoFill();
        ofSetLineWidth(2);
        ofSetColor(120, 120, 120);
        ofDrawCircle(c, 4); // marca del centro del token (snapshot)

        // texto desplazado a la derecha y abajo para evitar colisiones con ejes
        ofSetColor(255);
        float offsetX = 40;
        float offsetY = 20;
        ofDrawBitmapString(string("Token: ") + detectedId, c.x + offsetX, c.y + offsetY);
    } else {
        // Token NO bloqueado
        if (!currentPoints.empty()) {
            ofVec2f base = currentPoints[0];
            ofSetColor(255);
            ofDrawBitmapString(string("Token: ?"), base.x + 25, base.y + 20);

            // Indicador visual si tiene ≥3 puntos pero no se ha bloqueado (posiblemente por proximidad)
            if (currentPoints.size() >= 3) {
                ofVec2f c = centroid(currentPoints);
                ofPushStyle();
                ofNoFill();
                ofSetLineWidth(3);
                ofSetColor(255, 0, 0, 150);  // Rojo semi-transparente
                ofDrawCircle(c, 50);          // Círculo de advertencia
                ofPopStyle();
            }
        }
    }
}

// setPoints: opcional (debug)
void Token::setPoints(const vector<ofVec2f>& newPts){
    // sustituye currentPoints (útil para debug)
    currentPoints = newPts;
}

vector<ofVec2f> Token::getPoints() const {
    return currentPoints;
}

vector<TouchPointWithID> Token::getPointsWithIDs() const {
    return currentPointsWithIDs;
}

vector<ofVec2f> Token::getLockedPoints() const {
    return locked ? lockedPoints : vector<ofVec2f>{}; // devuelve vacío si no hay lock
}

vector<TouchPointWithID> Token::getLockedPointsWithIDs() const {
    return locked ? lockedPointsWithIDs : vector<TouchPointWithID>{};
}

bool Token::isLocked() const {
    return locked;
}

char Token::getId() const {
    return detectedId;
}

ofVec2f Token::getCentroid() const {
    if (currentPoints.empty()) return ofVec2f(0, 0);

    ofVec2f sum(0, 0);
    for (const auto& pt : currentPoints) {
        sum += pt;
    }
    return sum / currentPoints.size();
}

void Token::forceClassification(char id) {
    if (!locked) {
        ofLogWarning("Token") << "Cannot force classification on unlocked token";
        return;
    }
    detectedId = id;
    ofLogNotice("Token") << "Classification forced to: " << id;
}

bool Token::isCandidateForLocking() const {
    return candidateForLocking;
}

// ----------------- utilidades geométricas -----------------
ofVec2f Token::centroid(const vector<ofVec2f>& pts) const {
    ofVec2f c(0,0);
    for (auto &p: pts) c += p;
    c /= (float)pts.size();
    return c;
}

vector<ofVec2f> Token::orderByAngle(const vector<ofVec2f>& pts) const {
    if (pts.size() <= 1) return pts;
    auto c = centroid(pts);
    vector<pair<float, ofVec2f>> tmp;
    tmp.reserve(pts.size());
    for (auto &p: pts){
        float a = atan2(p.y - c.y, p.x - c.x);
        tmp.emplace_back(a, p);
    }
    sort(tmp.begin(), tmp.end(), [](auto &a, auto &b){ return a.first < b.first; });
    vector<ofVec2f> out;
    out.reserve(tmp.size());
    for (auto &t: tmp) out.push_back(t.second);
    return out;
}

float Token::angleBetween(const ofVec2f& a, const ofVec2f& b, const ofVec2f& c) const {
    ofVec2f ab = (a - b).getNormalized();
    ofVec2f cb = (c - b).getNormalized();
    float dot = ab.dot(cb);
    return acos(ofClamp(dot, -1.f, 1.f)) * RAD_TO_DEG;
}

bool Token::approxEqual(float a, float b, float tolerance) const {
    return fabs(a - b) < tolerance;
}

float Token::calculateCentroidShift(const vector<ofVec2f>& pts1, const vector<ofVec2f>& pts2) const {
    if (pts1.empty() || pts2.empty()) return 0.0f;
    ofVec2f c1 = centroid(pts1);
    ofVec2f c2 = centroid(pts2);
    return c1.distance(c2);
}

// ----------------- clasificación -----------------
// Classifies geometric shape based on sides and angles
// Uses adaptive tolerances based on token size to handle different scales
char Token::classifyShape(const vector<ofVec2f>& pts) const {
    int n = pts.size();
    if (n < 3) return '?';

    // calcular lados (distancias entre consecutivos)
    vector<float> sides; sides.reserve(n);
    for (int i = 0; i < n; ++i) sides.push_back(pts[i].distance(pts[(i+1)%n]));

    // calcular ángulos internos
    vector<float> angles; angles.reserve(n);
    for (int i = 0; i < n; ++i) angles.push_back(angleBetween(pts[(i-1+n)%n], pts[i], pts[(i+1)%n]));

    // Adaptive tolerance: larger tokens get larger absolute tolerance
    float avgSide = 0; for (auto s: sides) avgSide += s; avgSide /= sides.size();
    float sideTol = max(TUI3D::TokenClassification::SIDE_TOLERANCE_BASE, 
                        avgSide * TUI3D::TokenClassification::SIDE_TOLERANCE_PERCENT);
    float angleTol = TUI3D::TokenClassification::ANGLE_TOLERANCE;

    auto allSidesEqual = [&](float tol){
        for (auto s: sides) if (!approxEqual(s, sides[0], tol)) return false;
        return true;
    };
    auto anglesAllNear = [&](float target){
        for (auto a: angles) if (!approxEqual(a, target, angleTol)) return false;
        return true;
    };

    if (n == 3) {
        if (allSidesEqual(sideTol) && anglesAllNear(60.0f)) return 'A';              // equilátero
        int eqCount = 0;
        if (approxEqual(sides[0], sides[1], sideTol)) ++eqCount;
        if (approxEqual(sides[1], sides[2], sideTol)) ++eqCount;
        if (approxEqual(sides[2], sides[0], sideTol)) ++eqCount;
        if (eqCount >= 1) {
            // Clasificar usando ratio de lados + ángulo mínimo (multi-criterio más robusto)
            float minSide = *min_element(sides.begin(), sides.end());
            float maxSide = *max_element(sides.begin(), sides.end());
            float ratio = minSide / maxSide;

            // Calcular ángulo mínimo del triángulo
            float minAngle = *min_element(angles.begin(), angles.end());

            // Token B (35-35-10): ratio ≈ 0.286, ángulo mín ≈ 16.4°
            // Token C (25-25-35): ratio ≈ 0.714, ángulo mín ≈ 45.5°

            // UMBRALES ÓPTIMOS (calculados como puntos medios geométricos):
            // - Ratio: 0.50 (punto medio entre 0.286 y 0.714)
            // - Ángulo: 31.0° (punto medio entre 16.4° y 45.5°)
            bool ratioSaysB = (ratio < 0.50f);      // Umbral calibrado al punto medio
            bool angleSaysB = (minAngle < 31.0f);   // Umbral calibrado al punto medio

            // Estrategia de decisión: sin prioridad entre tokens
            if (ratioSaysB && angleSaysB) {
                return 'B';  // Ambos criterios confirman Token B
            } else if (!ratioSaysB && !angleSaysB) {
                return 'C';  // Ambos criterios confirman Token C
            } else {
                // Criterios en conflicto - usar sistema de tres zonas
                if (ratio < 0.40f) {
                    return 'B';  // Zona fuerte de B
                } else if (ratio > 0.60f) {
                    return 'C';  // Zona fuerte de C
                } else {
                    // Zona ambigua (0.40-0.60) - usar criterio de ángulo
                    return (minAngle < 31.0f) ? 'B' : 'C';
                }
            }
        }
        for (auto a: angles) if (approxEqual(a, 90.0f, 12.0f)) return 'D';           // rectángulo
        return 'E';                                                                  // escaleno
    }

    if (n == 4) {
        if (allSidesEqual(sideTol) && anglesAllNear(90.0f)) return 'F';             // cuadrado
        if (approxEqual(sides[0], sides[2], sideTol) && approxEqual(sides[1], sides[3], sideTol)
            && anglesAllNear(90.0f)) return 'G';                                    // rectángulo
        if (allSidesEqual(sideTol) && !anglesAllNear(90.0f)) return 'H';            // rombo
        // Trapecio isósceles: buscar pares de ángulos iguales opuestos (invariante a la rotación)
        bool hasPairOpposite = (approxEqual(angles[0], angles[2], angleTol) || 
                                approxEqual(angles[1], angles[3], angleTol));
        if (hasPairOpposite && !allSidesEqual(sideTol)) return 'I'; // trapecio isósceles
        return 'J';
    }

    if (n == 5) return 'K';
    if (n == 6) return 'L';

    return '?';
}
