/**
 * Duration
 * Standalone timeline for Creative Code
 *
 * Copyright (c) 2012 James George
 * Development Supported by YCAM InterLab http://interlab.ycam.jp/en/
 * http://jamesgeorge.org + http://flightphase.com
 * http://github.com/obviousjim + http://github.com/flightphase
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#pragma once

#ifdef WITH_MIDI
#include "ofxMidi.h"
#endif

#include "ofMain.h"
#include "ofxOsc.h"
#include "ofxTimeline.h"
#include "ofxMSATimer.h"
#include "ofxTLUIHeader.h"
#include "ofxUI.h"
#include "ofxLocalization.h"
#include "ofxFTGLFont.h"
#include "ofxTLAudioTrack.h"
#include "ofxUIMidiMapper.h"

typedef struct {
    string path; //full project path
    string name;
    string settingsPath;

    bool useBPM;
    float bpm;
    bool snapToBPM;
    bool snapToKeys;

	float oscRate; // BUNDLES PER SECOND
    bool oscInEnabled;
	bool oscOutEnabled;
	int oscInPort;
    string oscIP;
    int oscOutPort;

} DurationProjectSettings;

typedef struct{
	ofxUIRectangle sourceRect;
	ofVec2f displayPoint;
	string text;
	ofColor debugColor;
} Tooltip;

#ifdef WITH_MIDI
class DurationController : public ofThread, public ofxMidiListener
#else
class DurationController : public ofThread
#endif

{
  public:
	DurationController();
	~DurationController();

	void setup();

	void enableInterface();
	void disableInterface();
	bool isInterfaceEnabled();
	void panelGui();

	void update(ofEventArgs& args);
	void draw(ofEventArgs& args);

	void keyPressed(ofKeyEventArgs& keyArgs);

	ofxTimeline& getTimeline();

    ofRectangle canvasRectangle;
    ofxUICanvas* gui;
	void guiEvent(ofxUIEventArgs& e);
    void exit(ofEventArgs& e);

	ofxTLTrack* addTrack(string trackType, string trackName = "", string xmlFileName = "");

    //control elements
    ofxUIDropDownList* projectDropDown;
	ofxUIMultiImageButton* saveButton;
    ofxUILabel* timeLabel;
    ofxUITextInput* durationLabel;
    ofxUIMultiImageToggle* playpauseToggle;
	ofxUIMultiImageButton* stopButton;
    ofxUIMultiImageToggle* loopToggle;

	//project settings elements
    ofxUILabelToggle* useBPMToggle;
	ofxUINumberDialer* bpmDialer;

	//TODO: find a place for these
    ofxUILabelToggle* snapToKeysToggle;

    ofxUILabelToggle* enableOSCInToggle;
	ofxUILabelToggle* enableOSCOutToggle;
    ofxUITextInput* oscInPortInput;
    ofxUITextInput* oscOutIPInput;
    ofxUITextInput* oscOutPortInput;

    vector<string> trackTypes;
    ofxUIDropDownList* addTrackDropDown;

    string defaultProjectDirectoryPath;
	DurationProjectSettings settings;

	void newProject(string projectPath);
    void newProject(string newProjectPath, string newProjectName);
    void loadProject(string projectPath, bool forceCreate = false);
    void loadProject(string projectPath, string projectName, bool forceCreate = false);
    void saveProject();

    DurationProjectSettings defaultProjectSettings();

	void startRecording();
	void stopRecording();

  protected:
	ofxTimeline timeline;
    void bangFired(ofxTLBangEventArgs& bang);
	vector<string> trackAddresses;

	bool shouldStartPlayback;
	void startPlayback();
	void sendInfoMessage();
	bool refreshAllOscOut;

    bool shouldCreateNewProject;
	string newProjectPath;
    bool shouldLoadProject;
	string projectToLoad;

	ofxOscReceiver receiver;
	ofxOscSender sender;

	void threadedFunction();
	void handleOscOut();
	void handleOscIn();
	bool enabled;

	unsigned long recordTimeOffset;
	ofxMSATimer recordTimer;

	void createTooltips();
	void drawTooltips();
	void drawTooltipDebug();

	ofxLocalization translation;
	ofMutex oscLock;

	vector<Tooltip> tooltips;
	//only can have one of these!
	ofxTLAudioTrack* audioTrack;

	vector<ofxOscMessage> bangsReceived;
	map<string, ofPtr<ofxTLUIHeader> > headers;

	ofxTLUIHeader* createHeaderForTrack(ofxTLTrack* track);
	ofPtr<ofxTLUIHeader> getHeaderWithDisplayName(string name);

	float oscFrequency; // 1 / BUNDLES PER SECOND * 1000
	unsigned long lastOSCBundleSent;
	ofxFTGLFont tooltipFont;
	bool needsSave;
    bool allgui;
	bool receivedAddTrack;
	string oscTrackTypeReceived;
	string oscTrackNameReceived;
	string oscTrackFilePathReceived;

	bool receivedPaletteToLoad;
	ofxTLColorTrack* paletteTrack;
	string palettePath;

    bool bMidiHotkeyCoupling;
    bool bMidiHotkeyLearning;
    int midiHotkeyPressed;

    // MIDI stuff
    #ifdef WITH_MIDI
    void newMidiMessage(ofxMidiMessage& eventArgs);
    ofxMidiIn midiIn;
	ofxMidiMessage midiMessage;
	vector<ofxMidiMessage> midiHotkeyMessages;
	vector<int> midiHotkeyKeys;
	#endif

	//lpmtx gui active

	ofxUISuperCanvas *gui0;
	ofxUISuperCanvas *gui1;
    ofxUISuperCanvas *gui2;
    ofxUISuperCanvas *gui3;
    ofxUISuperCanvas *gui4;
    ofxUISuperCanvas *gui5;
    ofxUISuperCanvas *gui6;
    ofxUISuperCanvas *gui7;
    ofxUISuperCanvas *gui8;
    ofxUISuperCanvas *gui9;
    ofxUISuperCanvas *gui11;

    ofxUITextInput *textinput;

    bool drawPadding;
    bool bFullscreen;

	float red, green, blue;
	float videox;
    float videoy;
    float video_color_r;
    float video_color_g;
    float video_color_b;
    float video_color_a;
    float video_speed;

    int video_volume;


};

