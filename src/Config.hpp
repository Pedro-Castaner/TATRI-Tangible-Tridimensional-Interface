#pragma once

// TUI3D Configuration Constants
namespace TUI3D {
    
    // Camera orbital controls
    namespace Camera {
        constexpr float DEFAULT_RADIUS = 600.0f;
        constexpr float SENSITIVITY = 0.005f;        // radians per pixel
        constexpr float ZOOM_SPEED = 1500.0f;        // zoom adjustment per radian
        constexpr float MIN_RADIUS = 100.0f;
        constexpr float MAX_RADIUS = 2000.0f;
        constexpr float MAX_ELEVATION_DEG = 89.0f;   // prevent gimbal lock
        constexpr float SMOOTHING = 0.1f;            // smoothing factor [0..1]
        constexpr float JUMP_THRESHOLD = 100.0f;     // pixels - max delta for normal movement (detect jumps/repositioning)
    }
    
    // TUIO connection settings
    namespace TUIO {
        constexpr int DEFAULT_PORT = 3333;
    }
    
    // Axis selector tool settings
    namespace AxisSelector {
        constexpr float MIN_BUTTON_RADIUS = 80.0f;
        constexpr float MAX_BUTTON_RADIUS = 200.0f;
        constexpr float RADIUS_SCALE_FACTOR = 0.9f;
        constexpr float SPACING_FACTOR = 1.8f;  // ✅ Aumentado de 1.4f a 2.0f para mayor espaciado
    }
    
    // Token classification tolerances
    namespace TokenClassification {
        constexpr float SIDE_TOLERANCE_BASE = 12.0f;
        constexpr float SIDE_TOLERANCE_PERCENT = 0.15f;
        constexpr float ANGLE_TOLERANCE = 15.0f;
    }
    
    // Scale tool settings
    namespace ScaleTool {
        constexpr float SENSITIVITY = 200.0f;        // pixels for scale change
        constexpr float MIN_SCALE = 0.01f;
        constexpr float JUMP_THRESHOLD = 180.0f;     // pixels - detect token repositioning without applying change
        constexpr float KNOB_SENSITIVITY_DEG = 180.0f; // degrees per 1.0x factor change (knob mode)
        constexpr float KNOB_DEADZONE_DEG = 1.0f;    // ignore tiny rotation noise (knob mode)
        constexpr float KNOB_MAX_DELTA_DEG = 20.0f;  // clamp large jumps (knob mode)
    }

    // Translate tool settings
    namespace TranslateTool {
        constexpr float JUMP_THRESHOLD = 180.0f;     // pixels - detect token repositioning without applying change
        constexpr float KNOB_SENSITIVITY_DEG = 0.5f;  // degrees per world-unit change (knob mode)
        constexpr float KNOB_DEADZONE_DEG = 1.0f;     // ignore tiny rotation noise (knob mode)
        constexpr float KNOB_MAX_DELTA_DEG = 20.0f;   // clamp large jumps (knob mode)
    }
    
    // Visual feedback
    namespace Visual {
        constexpr int GIZMO_SIZE = 100;
        constexpr int GIZMO_MARGIN = 10;
        constexpr int AXIS_LENGTH = 35;
        constexpr int AXIS_TEXT_OFFSET = 10;
        constexpr float AXIS_LINE_WIDTH = 3.0f;
    }
    
    // Grid settings
    namespace Grid {
        constexpr float SIZE = 500.0f;  // Aumentado de 200 a 500 para mejor visibilidad
        constexpr int SUBDIVISIONS = 10;
    }
    
    // Multi-token detection settings
    namespace MultiToken {
        constexpr float CLUSTER_THRESHOLD = 300.0f;      // pixels - max distance between points in same cluster
        constexpr int MAX_TOKENS = 5;                    // maximum simultaneous tokens
        constexpr float POINT_RESERVATION_RADIUS = 80.0f; // pixels - radius around locked token points
        constexpr int MIN_POINTS_FOR_TOKEN = 3;          // minimum points to attempt token detection
        constexpr float POINT_TRACKING_RADIUS = 200.0f;  // pixels - max distance to track a locked point (AUMENTADO para mejor tracking)
        constexpr float MIN_TOKEN_DISTANCE = 150.0f;     // pixels - minimum distance between token centroids at detection time

        // Stability verification for locking (Phase 1: Time-based delay)
        constexpr int LOCKING_DELAY_MS = 175;            // milliseconds - wait before locking to ensure stability
        constexpr float STABILITY_THRESHOLD = 50.0f;     // pixels - max centroid movement during delay period

        // Spatial-temporal priority system (Phase 2: Tool re-detection)
        constexpr uint64_t PRIORITY_WINDOW_MS = 1000;    // milliseconds - remember removed tokens for re-detection
        constexpr float PRIORITY_RADIUS = 200.0f;        // pixels - proximity threshold for priority matching
        constexpr float PRIORITY_BOOST_THRESHOLD = 0.85f; // similarity threshold (0-1) to apply priority
        constexpr int MAX_HISTORY_ENTRIES = 10;          // maximum number of historical tokens to remember
    }
}
