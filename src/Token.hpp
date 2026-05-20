#pragma once
#include "ofMain.h"
#include "Tools.hpp"  // Para TouchPointWithID

class Token {
public:
    Token();
    ~Token();

    void setup(const ofVec2f& pos);
    void updateWithPoints(const vector<ofVec2f>& screenPoints);  // Updated by TokenManager
    void updateWithPointsAndIDs(const vector<TouchPointWithID>& pointsWithIDs, bool allowLocking = true, bool uiActive = false);  // Con Session IDs
    void draw();

    void setPoints(const vector<ofVec2f>& newPts); // opcional (debug)
    vector<ofVec2f> getPoints() const;             // puntos actuales (escala a pantalla)
    vector<TouchPointWithID> getPointsWithIDs() const;  // puntos CON Session IDs
    vector<ofVec2f> getLockedPoints() const;       // snapshot inicial (si hay)
    vector<TouchPointWithID> getLockedPointsWithIDs() const;  // snapshot CON Session IDs
    bool isLocked() const;
    char getId() const;                            // A..Z o '?'
    ofVec2f getCentroid() const;                   // centroide de currentPoints
    void forceClassification(char id);             // PHASE 2: Force classification override
    bool isCandidateForLocking() const;            // Check if token is in candidate period

private:
    vector<ofVec2f> currentPoints;  // coords en pixeles (current)
    vector<TouchPointWithID> currentPointsWithIDs;  // coords CON Session IDs

    // locked (snapshot) data
    bool locked = false;
    vector<ofVec2f> lockedPoints;   // snapshot en pixeles (solo si locked==true)
    vector<TouchPointWithID> lockedPointsWithIDs;  // snapshot CON Session IDs
    char detectedId = '?';

    // Stability verification for delayed locking (Phase 1)
    bool candidateForLocking = false;      // true if token is in candidate period
    uint64_t candidateStartTime = 0;       // timestamp when candidate period started
    char candidateId = '?';                // classification during candidate period
    vector<ofVec2f> candidatePoints;       // points snapshot at candidate start
    vector<TouchPointWithID> candidatePointsWithIDs;  // candidate points with session IDs

    // clasificación
    char classifyShape(const vector<ofVec2f>& pts) const;
    float angleBetween(const ofVec2f& a, const ofVec2f& b, const ofVec2f& c) const;
    bool approxEqual(float a, float b, float tolerance = 12.0f) const;

    // utils
    ofVec2f centroid(const vector<ofVec2f>& pts) const;
    vector<ofVec2f> orderByAngle(const vector<ofVec2f>& pts) const;
    float calculateCentroidShift(const vector<ofVec2f>& pts1, const vector<ofVec2f>& pts2) const;


};
