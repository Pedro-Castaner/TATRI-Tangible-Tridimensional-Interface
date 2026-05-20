#pragma once
#include "ofMain.h"
#include "Token.hpp"
#include "Config.hpp"
#include "ofxTuio.h"
#include <vector>
#include <map>
#include <memory>
#include <set>

// Spatial-temporal priority system: Track recently removed tokens
struct TokenHistoryEntry {
    char tokenId;                   // Tool ID (A-L)
    ofVec2f centroid;               // Where it was located
    uint64_t removalTime;           // When it was removed (milliseconds)
    vector<ofVec2f> shapeGeometry;  // Approximate shape for similarity matching

    // Default constructor (required for vector::resize)
    TokenHistoryEntry()
        : tokenId('?'), centroid(0, 0), removalTime(0) {}

    // Parameterized constructor
    TokenHistoryEntry(char id, ofVec2f pos, uint64_t time, const vector<ofVec2f>& pts)
        : tokenId(id), centroid(pos), removalTime(time), shapeGeometry(pts) {}
};

// Manages multiple token detection and clustering
class TokenManager {
public:
    TokenManager();
    ~TokenManager();
    
    void setup();
    void update(bool uiActive = false);
    void draw();
    
    // Get all currently active tokens (locked only)
    std::vector<Token*> getActiveTokens();

    // Get all tokens (both locked and unlocked)
    std::vector<Token*> getAllTokens();

    // Get token by ID (returns nullptr if not found)
    Token* getTokenById(char id);
    
    // Check if a specific token ID is active
    bool isTokenActive(char id) const;

    // Mark token as removing (keeps it "active" during tooltip fadeout)
    void markTokenRemoving(char id);
    void clearTokenRemoving(char id);
    bool isTokenRemoving(char id) const;

    // Check if a token is too close to other locked tokens
    bool isTokenTooCloseToOthers(int tokenIndex) const;

private:
    // TUIO connection
    ofxTuioReceiver tuio;
    
    // All current touch points from TUIO
    struct TouchPoint {
        ofVec2f pos;        // normalized 0..1
        long sessionID;
        int assignedTokenIndex = -1;  // which token owns this point (-1 = unassigned)
    };
    std::vector<TouchPoint> allTouches;
    
    // Active tokens
    std::vector<std::unique_ptr<Token>> tokens;

    // Track tokens that are being removed (during tooltip fadeout)
    std::set<char> tokensBeingRemoved;

    // PHASE 2: Spatial-temporal priority system
    std::vector<TokenHistoryEntry> tokenHistory;

    // UI state tracking (for conditional locking delay)
    bool uiCurrentlyActive = false;

    // TUIO callbacks
    void tuioAdded(ofxTuioCursor & c);
    void tuioUpdated(ofxTuioCursor & c);
    void tuioRemoved(ofxTuioCursor & c);

    // Clustering and assignment
    std::vector<std::vector<int>> clusterTouches();  // returns groups of touch indices
    void assignTouchesToTokens();
    void updateTokensWithAssignedTouches();
    void detectNewTokens(const std::vector<std::vector<int>>& freeClusters);

    // Utility
    ofVec2f touchToScreenCoords(const ofVec2f& normalized) const;
    float distanceBetweenTouches(int idx1, int idx2) const;
    bool isPointReservedByToken(int touchIdx) const;
    void reindexAssignedTouches();  // Update touch indices after token removal

    // PHASE 2: Priority system methods
    TokenHistoryEntry* findNearbyHistory(const ofVec2f& centroid, float radius);
    void pruneExpiredHistory();
    float calculateShapeSimilarity(const vector<ofVec2f>& shape1, const vector<ofVec2f>& shape2);
    char attemptBiasedClassification(char historicalId, char currentId);
};
