#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main( ){

	ofSetupOpenGL(1300, 700, OF_WINDOW);			// <-------- setup the GL context
    ofSetWindowPosition(0, 0);
	// this kicks off the running of my app
	// can be OF_WINDOW or OF_FULLSCREEN
	// pass in width and height too:
	ofSetWindowTitle("LptmX Duration Controller by Gil@dX");
	ofRunApp( new ofApp());

}
