#include "ofMain.h"
#include "ofApp.h"
#include "ofAppGLFWWindow.h"

//========================================================================
int main( ){

    ofGLFWWindowSettings settings;
    settings.setSize(1920, 1080);
    settings.monitor = 0;
    settings.windowMode = OF_FULLSCREEN;
    
    auto mainWindow = ofCreateWindow(settings);

    //run the app
    shared_ptr<ofApp> mainApp(new ofApp);

    ofRunApp(mainWindow, mainApp);
    ofRunMainLoop();
}
