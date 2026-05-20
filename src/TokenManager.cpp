#include "TokenManager.hpp"

// ---------------- Constructor / Destructor ----------------
TokenManager::TokenManager() {
}

TokenManager::~TokenManager() {
    ofRemoveListener(tuio.AddTuioCursor, this, &TokenManager::tuioAdded);
    ofRemoveListener(tuio.UpdateTuioCursor, this, &TokenManager::tuioUpdated);
    ofRemoveListener(tuio.RemoveTuioCursor, this, &TokenManager::tuioRemoved);
    tuio.disconnect();
}

void TokenManager::setup() {
    // Initialize TUIO receiver
    tuio.setup(new ofxTuioUdpReceiver(TUI3D::TUIO::DEFAULT_PORT));
    ofAddListener(tuio.AddTuioCursor, this, &TokenManager::tuioAdded);
    ofAddListener(tuio.UpdateTuioCursor, this, &TokenManager::tuioUpdated);
    ofAddListener(tuio.RemoveTuioCursor, this, &TokenManager::tuioRemoved);
    
    tuio.connect(false);
    ofLogNotice("TokenManager") << "TUIO receiver initialized on port " << TUI3D::TUIO::DEFAULT_PORT;
    ofLogNotice("TokenManager") << "Multi-token detection enabled (max " << TUI3D::MultiToken::MAX_TOKENS << " tokens)";
}

void TokenManager::update(bool uiActive) {
    // Main update loop for multi-token detection

    // Store UI state for use in token locking logic
    uiCurrentlyActive = uiActive;

    // Step 1: Assign touches to existing locked tokens
    assignTouchesToTokens();

    // Step 2: Cluster remaining free touches
    auto clusters = clusterTouches();

    // DEBUG: Log clustering info
    if (!clusters.empty() && allTouches.size() >= 3) {
        ofLogNotice("TokenManager") << "Found " << clusters.size() << " clusters from " << allTouches.size() << " touches";
        for (size_t i = 0; i < clusters.size(); ++i) {
            ofLogNotice("TokenManager") << "  Cluster " << i << " has " << clusters[i].size() << " points";
        }
    }

    // Step 3: Update existing tokens with their assigned touches
    updateTokensWithAssignedTouches();

    // Step 4: Try to detect new tokens from free clusters
    detectNewTokens(clusters);

    // Step 5: Remove tokens that have lost ALL their assigned points
    // ✅ FIX: Crear mapa de índices ANTES de std::remove_if para evitar búsqueda durante iteración
    std::map<Token*, int> tokenIndexMap;
    for (size_t i = 0; i < tokens.size(); ++i) {
        tokenIndexMap[tokens[i].get()] = i;
    }

    // PHASE 2: Save removed tokens to history BEFORE erasing
    for (const auto& token : tokens) {
        if (!token->isLocked()) continue;  // Skip unlocked tokens

        auto it = tokenIndexMap.find(token.get());
        if (it == tokenIndexMap.end()) continue;

        int tokenIdx = it->second;
        bool hasAssignedTouch = false;

        // Check if this token has any assigned touches
        for (const auto& touch : allTouches) {
            if (touch.assignedTokenIndex == tokenIdx) {
                hasAssignedTouch = true;
                break;
            }
        }

        // If token will be removed, save to history
        if (!hasAssignedTouch) {
            TokenHistoryEntry entry(
                token->getId(),
                token->getCentroid(),
                ofGetElapsedTimeMillis(),
                token->getLockedPoints()
            );
            tokenHistory.push_back(entry);
            pruneExpiredHistory();

            ofLogNotice("TokenManager") << "💾 Saved token " << token->getId()
                                       << " to history (pos: " << entry.centroid << ")";
        }
    }

    // NOW erase the tokens
    tokens.erase(
        std::remove_if(tokens.begin(), tokens.end(),
            [this, &tokenIndexMap](const std::unique_ptr<Token>& token) {
                // FIX: Don't remove candidate tokens (they need to persist to complete locking)
                if (token->isCandidateForLocking()) {
                    return false;  // Keep candidates alive
                }

                if (!token->isLocked()) return true;  // Remover tokens no locked y no candidatos

                // ✅ Usar el mapa pre-calculado en lugar de buscar dinámicamente
                auto it = tokenIndexMap.find(token.get());
                if (it == tokenIndexMap.end()) return true;  // No encontrado, remover

                int tokenIdx = it->second;

                // Verificar si tiene algún touch asignado
                for (const auto& touch : allTouches) {
                    if (touch.assignedTokenIndex == tokenIdx) {
                        return false;  // Tiene al menos un punto asignado, NO remover
                    }
                }

                // No tiene ningún punto asignado -> remover
                ofLogNotice("TokenManager") << "🗑️ Removing token " << token->getId() << " (no assigned touches)";
                return true;
            }),
        tokens.end()
    );

    // ✅ Step 6: Reindexar assignedTokenIndex después de remover tokens
    reindexAssignedTouches();
}

void TokenManager::draw() {
    // Draw ALL touch points first (even unassigned ones)
    ofSetColor(200, 100, 255);
    ofFill();
    for (const auto& touch : allTouches) {
        ofVec2f screenPos = touchToScreenCoords(touch.pos);
        ofDrawCircle(screenPos, 15);
        
        // Draw session ID for debug
        ofSetColor(255);
        ofDrawBitmapString(ofToString(touch.sessionID), screenPos.x + 20, screenPos.y);
        ofSetColor(200, 100, 255);
    }
    
    // Draw all active tokens (will draw on top of touch points)
    for (auto& token : tokens) {
        token->draw();
    }
    
    // Draw debug info - BOTTOM-LEFT para no solapar con tooltips
    ofSetColor(255, 100, 100, 150);
    int yPos = ofGetHeight() - 50;  // Empezar desde abajo
    ofDrawBitmapString("Active tokens: " + ofToString(tokens.size()), 20, yPos);
    yPos += 15;
    ofDrawBitmapString("Touch points: " + ofToString(allTouches.size()), 20, yPos);
}

std::vector<Token*> TokenManager::getActiveTokens() {
    std::vector<Token*> activeList;
    for (auto& token : tokens) {
        if (token->isLocked()) {
            activeList.push_back(token.get());
        }
    }
    return activeList;
}

std::vector<Token*> TokenManager::getAllTokens() {
    std::vector<Token*> allList;
    for (auto& token : tokens) {
        allList.push_back(token.get());
    }

    ofLogNotice("TokenManager") << "getAllTokens() returning " << allList.size() << " tokens";
    for (auto* t : allList) {
        ofLogNotice("TokenManager") << "  - Token " << t->getId() << " (locked: " << t->isLocked() << ")";
    }

    return allList;
}

Token* TokenManager::getTokenById(char id) {
    for (auto& token : tokens) {
        if (token->getId() == id && token->isLocked()) {
            return token.get();
        }
    }
    return nullptr;
}

bool TokenManager::isTokenActive(char id) const {
    // Tokens being removed are still "active" for UI rendering during fadeout
    if (tokensBeingRemoved.find(id) != tokensBeingRemoved.end()) {
        return true;
    }

    for (const auto& token : tokens) {
        if (token->getId() == id && token->isLocked()) {
            return true;
        }
    }
    return false;
}

bool TokenManager::isTokenRemoving(char id) const {
    return tokensBeingRemoved.find(id) != tokensBeingRemoved.end();
}

// ---------------- TUIO Callbacks ----------------
void TokenManager::tuioAdded(ofxTuioCursor & c) {
    TouchPoint tp;
    tp.pos = ofVec2f(c.getX(), c.getY());
    tp.sessionID = c.getSessionID();
    tp.assignedTokenIndex = -1;
    allTouches.push_back(tp);
}

void TokenManager::tuioUpdated(ofxTuioCursor & c) {
    long sid = c.getSessionID();
    for (auto& touch : allTouches) {
        if (touch.sessionID == sid) {
            touch.pos = ofVec2f(c.getX(), c.getY());
            return;
        }
    }
}

void TokenManager::tuioRemoved(ofxTuioCursor & c) {
    long sid = c.getSessionID();
    allTouches.erase(
        std::remove_if(allTouches.begin(), allTouches.end(),
            [sid](const TouchPoint& tp) { return tp.sessionID == sid; }),
        allTouches.end()
    );
}

// ---------------- Clustering ----------------
std::vector<std::vector<int>> TokenManager::clusterTouches() {
    // Returns clusters of UNASSIGNED touch indices
    std::vector<std::vector<int>> clusters;
    std::vector<bool> visited(allTouches.size(), false);
    
    for (size_t i = 0; i < allTouches.size(); ++i) {
        if (visited[i] || allTouches[i].assignedTokenIndex != -1) {
            continue;  // skip assigned or visited
        }
        
        // Start new cluster
        std::vector<int> cluster;
        std::vector<int> queue = {(int)i};
        visited[i] = true;
        
        while (!queue.empty()) {
            int current = queue.back();
            queue.pop_back();
            cluster.push_back(current);
            
            // Find nearby unassigned touches
            for (size_t j = 0; j < allTouches.size(); ++j) {
                if (visited[j] || allTouches[j].assignedTokenIndex != -1) {
                    continue;
                }
                
                float dist = distanceBetweenTouches(current, j);
                if (dist < TUI3D::MultiToken::CLUSTER_THRESHOLD) {
                    visited[j] = true;
                    queue.push_back(j);
                }
            }
        }
        
        clusters.push_back(cluster);
    }
    
    return clusters;
}

void TokenManager::assignTouchesToTokens() {
    // ❌ NO resetear todas las asignaciones - mantener asignaciones persistentes
    // Solo actualizar/verificar las existentes
    
    // NUEVA LÓGICA: Asignación permanente - puntos asignados NUNCA se liberan
    // hasta que el punto salga de la pantalla o el token se remueva
    
    for (size_t tokenIdx = 0; tokenIdx < tokens.size(); ++tokenIdx) {
        if (!tokens[tokenIdx]->isLocked()) continue;
        
        auto lockedPts = tokens[tokenIdx]->getLockedPoints();
        if (lockedPts.empty()) continue;
        
        // Para cada punto locked original del token
        for (const auto& lockedPt : lockedPts) {
            // Buscar el touch más cercano a este punto locked
            float minDist = FLT_MAX;
            int bestTouchIdx = -1;
            
            for (size_t touchIdx = 0; touchIdx < allTouches.size(); ++touchIdx) {
                // ✅ Solo considerar touches NO asignados (asignación exclusiva permanente)
                if (allTouches[touchIdx].assignedTokenIndex != -1) continue;
                
                ofVec2f screenPos = touchToScreenCoords(allTouches[touchIdx].pos);
                float dist = lockedPt.distance(screenPos);
                
                // Dentro del radio de tracking y mejor que anteriores
                if (dist < TUI3D::MultiToken::POINT_TRACKING_RADIUS && dist < minDist) {
                    minDist = dist;
                    bestTouchIdx = touchIdx;
                }
            }
            
            // Asignar el mejor match (si existe)
            // ✅ Una vez asignado, NUNCA se libera hasta que el punto salga
            if (bestTouchIdx != -1) {
                allTouches[bestTouchIdx].assignedTokenIndex = tokenIdx;
            }
        }
    }
}

void TokenManager::updateTokensWithAssignedTouches() {
    for (size_t tokenIdx = 0; tokenIdx < tokens.size(); ++tokenIdx) {
        // Collect touches assigned to this token WITH Session IDs
        std::vector<TouchPointWithID> assignedPtsWithIDs;
        for (const auto& touch : allTouches) {
            if (touch.assignedTokenIndex == (int)tokenIdx) {
                TouchPointWithID pt;
                pt.pos = touchToScreenCoords(touch.pos);
                pt.sessionID = touch.sessionID;
                assignedPtsWithIDs.push_back(pt);
            }
        }
        
        // Validar distancia ANTES de permitir bloqueo
        bool allowLocking = true;
        if (!tokens[tokenIdx]->isLocked() && !assignedPtsWithIDs.empty()) {
            // Solo validar si el token aún NO está bloqueado
            allowLocking = !isTokenTooCloseToOthers(tokenIdx);

            if (!allowLocking) {
                ofLogNotice("TokenManager") << "Token " << tokenIdx
                    << " bloqueado por proximidad a otro token";
            }
        }

        // ALWAYS update token, even with empty list
        // Use new method that includes Session IDs
        tokens[tokenIdx]->updateWithPointsAndIDs(assignedPtsWithIDs, allowLocking, uiCurrentlyActive);
    }
}

void TokenManager::detectNewTokens(const std::vector<std::vector<int>>& freeClusters) {
    if (tokens.size() >= TUI3D::MultiToken::MAX_TOKENS) {
        ofLogWarning("TokenManager") << "Max tokens reached (" << TUI3D::MultiToken::MAX_TOKENS << ")";
        return;  // Max tokens reached
    }
    
    for (const auto& cluster : freeClusters) {
        ofLogVerbose("TokenManager") << "Trying cluster with " << cluster.size() << " points";
        
        if (cluster.size() < TUI3D::MultiToken::MIN_POINTS_FOR_TOKEN) {
            ofLogVerbose("TokenManager") << "  Cluster too small (need " << TUI3D::MultiToken::MIN_POINTS_FOR_TOKEN << ")";
            continue;  // Not enough points
        }
        
        if (tokens.size() >= TUI3D::MultiToken::MAX_TOKENS) {
            break;
        }
        
        // Convert cluster indices to screen coordinates WITH Session IDs
        std::vector<TouchPointWithID> clusterPtsWithIDs;
        for (int idx : cluster) {
            TouchPointWithID pt;
            pt.pos = touchToScreenCoords(allTouches[idx].pos);
            pt.sessionID = allTouches[idx].sessionID;
            clusterPtsWithIDs.push_back(pt);
        }
        
        ofLogNotice("TokenManager") << "Attempting token detection with " << clusterPtsWithIDs.size() << " points";

        // Create new token and try to detect (using Session IDs)
        auto newToken = std::make_unique<Token>();
        newToken->setup(ofVec2f(0, 0));
        newToken->updateWithPointsAndIDs(clusterPtsWithIDs, true, uiCurrentlyActive);

        char detectedId = newToken->getId();
        bool isLocked = newToken->isLocked();

        ofLogNotice("TokenManager") << "  Token result: ID=" << detectedId << ", locked=" << isLocked;

        // PHASE 2: Apply spatial-temporal priority if token locked
        if (isLocked && detectedId != '?') {
            ofVec2f clusterCentroid = newToken->getCentroid();
            TokenHistoryEntry* nearbyHistory = findNearbyHistory(clusterCentroid, TUI3D::MultiToken::PRIORITY_RADIUS);

            if (nearbyHistory != nullptr) {
                // Calculate shape similarity
                float similarity = calculateShapeSimilarity(
                    newToken->getLockedPoints(),
                    nearbyHistory->shapeGeometry
                );

                ofLogNotice("TokenManager") << "  Shape similarity with history: " << similarity
                                           << " (threshold: " << TUI3D::MultiToken::PRIORITY_BOOST_THRESHOLD << ")";

                if (similarity >= TUI3D::MultiToken::PRIORITY_BOOST_THRESHOLD) {
                    // Apply priority bias
                    char biasedId = attemptBiasedClassification(nearbyHistory->tokenId, detectedId);

                    if (biasedId != detectedId) {
                        // Override detection with historical tool
                        newToken->forceClassification(biasedId);
                        detectedId = biasedId;
                        ofLogNotice("TokenManager") << "  ⭐ Priority override applied: " << biasedId;
                    }
                }
            }
        }

        // FIX: Add token to vector IMMEDIATELY (even if not locked yet) to persist candidate state
        // This allows the candidate timer to work across frames
        if (newToken->isCandidateForLocking() || isLocked) {
            if (isLocked && detectedId != '?') {
                ofLogNotice("TokenManager") << "✓ New token LOCKED: " << detectedId;
            } else if (newToken->isCandidateForLocking()) {
                ofLogNotice("TokenManager") << "⏳ Candidate token added (waiting for stability)";
            }

            tokens.push_back(std::move(newToken));

            // Assign these touches to the new token
            int newTokenIdx = tokens.size() - 1;
            for (int idx : cluster) {
                allTouches[idx].assignedTokenIndex = newTokenIdx;
            }
        } else {
            ofLogNotice("TokenManager") << "✗ Token not detected (ID=" << detectedId << ", locked=" << isLocked << ")";
        }
    }
}

// ---------------- Utility ----------------
ofVec2f TokenManager::touchToScreenCoords(const ofVec2f& normalized) const {
    return ofVec2f(normalized.x * ofGetWidth(), normalized.y * ofGetHeight());
}

float TokenManager::distanceBetweenTouches(int idx1, int idx2) const {
    ofVec2f p1 = touchToScreenCoords(allTouches[idx1].pos);
    ofVec2f p2 = touchToScreenCoords(allTouches[idx2].pos);
    return p1.distance(p2);
}

bool TokenManager::isTokenTooCloseToOthers(int tokenIndex) const {
    if (tokenIndex < 0 || tokenIndex >= tokens.size()) {
        return false;  // Index inválido
    }

    Token* candidateToken = tokens[tokenIndex].get();
    if (!candidateToken || candidateToken->getPoints().empty()) {
        return false;  // Token sin puntos
    }

    ofVec2f candidateCentroid = candidateToken->getCentroid();

    // Verificar distancia a todos los otros tokens YA BLOQUEADOS
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i == tokenIndex) continue;  // Saltar a sí mismo

        Token* otherToken = tokens[i].get();
        if (otherToken && otherToken->isLocked()) {
            ofVec2f otherCentroid = otherToken->getCentroid();
            float distance = candidateCentroid.distance(otherCentroid);

            if (distance < TUI3D::MultiToken::MIN_TOKEN_DISTANCE) {
                return true;  // Demasiado cerca de otro token
            }
        }
    }

    return false;  // Todas las distancias son válidas
}

bool TokenManager::isPointReservedByToken(int touchIdx) const {
    return allTouches[touchIdx].assignedTokenIndex != -1;
}

// ---------------- Token Removal Tracking ----------------
void TokenManager::markTokenRemoving(char id) {
    tokensBeingRemoved.insert(id);
    ofLogNotice("TokenManager") << "Token " << id << " marked as removing (UI will stay active during fadeout)";
}

void TokenManager::clearTokenRemoving(char id) {
    tokensBeingRemoved.erase(id);
    ofLogNotice("TokenManager") << "Token " << id << " fully removed from removing set";
}

// ✅ Reindexar assignedTokenIndex después de remover tokens del vector
void TokenManager::reindexAssignedTouches() {
    // Crear mapa: Token* → nuevo índice en el vector
    std::map<Token*, int> newIndexMap;
    for (size_t i = 0; i < tokens.size(); ++i) {
        newIndexMap[tokens[i].get()] = i;
    }

    // Actualizar todos los assignedTokenIndex en allTouches
    for (auto& touch : allTouches) {
        if (touch.assignedTokenIndex == -1) continue;  // No asignado, skip

        // Verificar si el índice viejo todavía es válido
        if (touch.assignedTokenIndex >= (int)tokens.size()) {
            // Índice fuera de rango - el token fue removido físicamente
            touch.assignedTokenIndex = -1;
            ofLogVerbose("TokenManager") << "  Touch " << touch.sessionID << " liberado (token removido)";
            continue;
        }

        // El token todavía existe, actualizar su índice
        Token* ownerToken = tokens[touch.assignedTokenIndex].get();
        auto it = newIndexMap.find(ownerToken);

        if (it != newIndexMap.end()) {
            int oldIdx = touch.assignedTokenIndex;
            int newIdx = it->second;

            if (oldIdx != newIdx) {
                touch.assignedTokenIndex = newIdx;
                ofLogNotice("TokenManager") << "  🔄 Touch " << touch.sessionID
                    << " reindexado: " << oldIdx << " → " << newIdx
                    << " (Token " << ownerToken->getId() << ")";
            }
        } else {
            // Token no encontrado en el nuevo mapa (fue removido)
            touch.assignedTokenIndex = -1;
            ofLogVerbose("TokenManager") << "  Touch " << touch.sessionID << " liberado (token no encontrado)";
        }
    }

    ofLogNotice("TokenManager") << "✅ Reindexed assigned touches after token removal";
}

// ============================================================================
// PHASE 2: Spatial-Temporal Priority System Methods
// ============================================================================

TokenHistoryEntry* TokenManager::findNearbyHistory(const ofVec2f& centroid, float radius) {
    uint64_t now = ofGetElapsedTimeMillis();

    for (auto& entry : tokenHistory) {
        // Check if entry is still within priority window
        if (now - entry.removalTime > TUI3D::MultiToken::PRIORITY_WINDOW_MS) {
            continue;
        }

        // Check spatial proximity
        float dist = entry.centroid.distance(centroid);
        if (dist <= radius) {
            ofLogNotice("TokenManager") << "🎯 Found nearby history: Token " << entry.tokenId
                                       << " at distance " << dist << "px (age: "
                                       << (now - entry.removalTime) << "ms)";
            return &entry;
        }
    }

    return nullptr;
}

void TokenManager::pruneExpiredHistory() {
    uint64_t now = ofGetElapsedTimeMillis();
    size_t sizeBefore = tokenHistory.size();

    // Remove entries older than PRIORITY_WINDOW_MS
    tokenHistory.erase(
        std::remove_if(tokenHistory.begin(), tokenHistory.end(),
            [now](const TokenHistoryEntry& entry) {
                return (now - entry.removalTime) > TUI3D::MultiToken::PRIORITY_WINDOW_MS;
            }),
        tokenHistory.end()
    );

    // Keep only MAX_HISTORY_ENTRIES most recent
    if (tokenHistory.size() > TUI3D::MultiToken::MAX_HISTORY_ENTRIES) {
        // Sort by removal time (newest first)
        std::sort(tokenHistory.begin(), tokenHistory.end(),
            [](const auto& a, const auto& b) {
                return a.removalTime > b.removalTime;
            });
        tokenHistory.resize(TUI3D::MultiToken::MAX_HISTORY_ENTRIES);
    }

    if (tokenHistory.size() < sizeBefore) {
        ofLogVerbose("TokenManager") << "🧹 Pruned history: " << sizeBefore
                                    << " -> " << tokenHistory.size() << " entries";
    }
}

float TokenManager::calculateShapeSimilarity(const vector<ofVec2f>& shape1,
                                             const vector<ofVec2f>& shape2) {
    // Different number of points = different shape
    if (shape1.size() != shape2.size()) {
        return 0.0f;
    }

    if (shape1.empty() || shape2.empty()) {
        return 0.0f;
    }

    // Normalize both shapes to unit circle (scale/translation invariant)
    auto normalize = [](const vector<ofVec2f>& pts) -> vector<ofVec2f> {
        ofVec2f centroid(0, 0);
        for (auto& p : pts) centroid += p;
        centroid /= pts.size();

        float maxDist = 0;
        for (auto& p : pts) {
            maxDist = std::max(maxDist, p.distance(centroid));
        }

        if (maxDist < 1e-6) return pts;  // Avoid division by zero

        vector<ofVec2f> normalized;
        for (auto& p : pts) {
            normalized.push_back((p - centroid) / maxDist);
        }
        return normalized;
    };

    auto norm1 = normalize(shape1);
    auto norm2 = normalize(shape2);

    // Calculate average distance between corresponding points
    float totalError = 0;
    for (size_t i = 0; i < norm1.size(); i++) {
        totalError += norm1[i].distance(norm2[i]);
    }
    float avgError = totalError / norm1.size();

    // Convert error to similarity (0 error = 1.0 similarity)
    return std::max(0.0f, 1.0f - avgError);
}

char TokenManager::attemptBiasedClassification(char historicalId, char currentId) {
    // If classifications already match, no bias needed
    if (currentId == historicalId) {
        return historicalId;
    }

    // Only bias for known ambiguous cases (B vs C triangles)
    if ((currentId == 'B' && historicalId == 'C') ||
        (currentId == 'C' && historicalId == 'B')) {
        ofLogNotice("TokenManager") << "🔄 Biasing ambiguous triangle: "
                                   << currentId << " -> " << historicalId;
        return historicalId;  // Trust history for ambiguous cases
    }

    // For other mismatches, trust current detection
    // (user may have intentionally placed different token)
    return currentId;
}
