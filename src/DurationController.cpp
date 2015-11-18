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

#include "DurationController.h"
#include "ofxHotKeys.h"

#define DROP_DOWN_WIDTH 250
#define TEXT_INPUT_WIDTH 100

DurationController::DurationController(){
	lastOSCBundleSent = 0;
	shouldStartPlayback = false;
	receivedAddTrack = false;
	receivedPaletteToLoad = false;

	enabled = false;
	shouldCreateNewProject = false;
    shouldLoadProject = false;
	audioTrack = NULL;
}

DurationController::~DurationController(){

}

void DurationController::enableInterface(){
	if(!enabled){
		enabled = true;
		ofAddListener(ofEvents().update, this, &DurationController::update);
		ofAddListener(ofEvents().draw, this, &DurationController::draw);
		ofAddListener(ofEvents().keyPressed, this, &DurationController::keyPressed);
		gui->enable();
		gui->disableAppEventCallbacks();
		timeline.enable();
		map<string,ofPtr<ofxTLUIHeader> >::iterator it = headers.begin();
		while(it != headers.end()){
			it->second->getGui()->enable();
			it++;
		}
	}
}

void DurationController::disableInterface(){
	if(enabled){
		enabled = false;
		ofRemoveListener(ofEvents().update, this, &DurationController::update);
		ofRemoveListener(ofEvents().draw, this, &DurationController::draw);
		ofRemoveListener(ofEvents().keyPressed, this, &DurationController::keyPressed);
		gui->disable();
		timeline.disable();
		map<string,ofPtr<ofxTLUIHeader> >::iterator it = headers.begin();
		while(it != headers.end()){
			it->second->getGui()->disable();
			it++;
		}
	}
}

bool DurationController::isInterfaceEnabled(){
	return enabled;
}

void DurationController::setup(){

	#ifdef TARGET_WIN32
	FreeConsole();
	#endif
	if(!translation.load("languageFile.csv")){
		ofLogError("DurationController::setup") << "error setting up translation, unpredictable stuff will happen" << endl;
	}

	ofxXmlSettings defaultSettings;
	bool settingsLoaded = defaultSettings.loadFile("settings.xml");	;
	translation.setCurrentLanguage(defaultSettings.getValue("language", "english"));

	if(!settingsLoaded){
		defaultSettings.setValue("language", "english");
		defaultSettings.saveFile();
	}

    // MIDI setup
    #ifdef WITH_MIDI
    // print input ports to console
    midiIn.listPorts();
    // open port by number
    //midiIn.openPort(1);
    //midiIn.openPort("IAC Pure Data In");	// by name
    midiIn.openVirtualPort("Input");	// open a virtual port
    // don't ignore sysex, timing, & active sense messages,
    // these are ignored by default
    midiIn.ignoreTypes(false, false, false);
    // add testApp as a listener
    midiIn.addListener(this);
    // print received messages to the console
    midiIn.setVerbose(true);
    //clear vectors used for midi-hotkeys coupling
    midiHotkeyMessages.clear();
    midiHotkeyKeys.clear();
    #endif
    bMidiHotkeyCoupling = false;
    bMidiHotkeyLearning = false;
    midiHotkeyPressed = -1;

    //populate projects
    vector<string> projects;
    projects.push_back(translation.translateKey("new project..."));
    projects.push_back(translation.translateKey("open project..."));

#ifdef TARGET_WIN32
    defaultProjectDirectoryPath = ofToDataPath(ofFilePath::getUserHomeDir() + "\\Documents\\Duration\\");
#else
    defaultProjectDirectoryPath = ofToDataPath(ofFilePath::getUserHomeDir() + "/Documents/Duration/");
#endif
	ofDirectory projectDirectory = ofDirectory(defaultProjectDirectoryPath);

    if(!projectDirectory.exists()){
        projectDirectory.create(true);
    }

    projectDirectory.listDir();
    for(int i = 0; i < projectDirectory.size(); i++){
        if(projectDirectory.getFile(i).isDirectory()){
            ofDirectory subDir = ofDirectory(projectDirectory.getPath(i));
			//cout << "checking path " << projectDirectory.getPath(i) << endl;
            subDir.allowExt("durationproj");
            subDir.setShowHidden(true);
            subDir.listDir();
            if(subDir.size() > 0){
                projects.push_back(projectDirectory.getName(i));
            }
        }
    }

#ifdef TARGET_WIN32
	timeline.setupFont("GUI/mplus-1c-regular.ttf", 9);
	tooltipFont.loadFont("GUI/mplus-1c-regular.ttf", 7);
#else
	timeline.setupFont("GUI/mplus-1c-regular.ttf", 10);
	tooltipFont.loadFont("GUI/mplus-1c-regular.ttf", 5);
#endif
	//setup timeline
	timeline.setup();
//	timeline.curvesUseBinary = true; //ELOI SWITCH THIS HERE
//	timeline.enableUndo(false);
    timeline.setSpacebarTogglePlay(false);
    timeline.setFrameRate(30);
	timeline.setDurationInSeconds(30);
	timeline.setOffset(ofVec2f(0, 90));
    timeline.setBPM(120.f);
	timeline.setAutosave(false);
	timeline.setEditableHeaders(true);
	timeline.moveToThread(); //increases accuracy of bang call backs

	//Set up top GUI
    gui = new ofxUICanvas(0,0,ofGetWidth(), 90);

    //ADD PROJECT DROP DOWN
    projectDropDown = new ofxUIDropDownList(DROP_DOWN_WIDTH, "PROJECT", projects, OFX_UI_FONT_LARGE);
    projectDropDown->setAutoClose(true);
    gui->addWidgetDown(projectDropDown);
    //ADD TRACKS
    vector<string> trackTypes;
    trackTypes.push_back(translation.translateKey("bangs"));
    trackTypes.push_back(translation.translateKey("flags"));
    trackTypes.push_back(translation.translateKey("switches"));
    trackTypes.push_back(translation.translateKey("curves"));
    trackTypes.push_back(translation.translateKey("colors"));
	trackTypes.push_back(translation.translateKey("lfo"));
	trackTypes.push_back(translation.translateKey("audio"));

    addTrackDropDown = new ofxUIDropDownList(DROP_DOWN_WIDTH, translation.translateKey("ADD TRACK"), trackTypes, OFX_UI_FONT_MEDIUM);
    addTrackDropDown->setAllowMultiple(false);
    addTrackDropDown->setAutoClose(true);
	//    gui->addWidgetRight(addTrackDropDown);
	gui->addWidgetSouthOf(addTrackDropDown, "PROJECT");

    saveButton = new ofxUIMultiImageButton(32, 32, false, "GUI/save_.png", "SAVE");
    saveButton->setLabelVisible(false);
    gui->addWidgetEastOf(saveButton, "PROJECT");


    //ADD TIMECODE
    string zeroTimecode = "00:00:00:000";
    timeLabel = new ofxUILabel(zeroTimecode, OFX_UI_FONT_LARGE);
    gui->addWidgetRight(timeLabel);
	//durationLabel = new ofxUILabel(" / "+zeroTimecode, OFX_UI_FONT_SMALL);
    durationLabel = new ofxUITextInput("DURATION", zeroTimecode, timeLabel->getRect()->width,0,0,0,OFX_UI_FONT_MEDIUM);
    durationLabel->setAutoClear(false);
    gui->addWidgetSouthOf(durationLabel, zeroTimecode);

    //ADD PLAY/PAUSE
    playpauseToggle = new ofxUIMultiImageToggle(32, 32, false, "GUI/play_.png", "PLAYPAUSE");
    playpauseToggle->setLabelVisible(false);
    gui->addWidgetEastOf(playpauseToggle, zeroTimecode);
    stopButton = new ofxUIMultiImageButton(32, 32, false, "GUI/stop_.png", "STOP");
    stopButton->setLabelVisible(false);
    gui->addWidgetRight(stopButton);
	loopToggle = new ofxUIMultiImageToggle(32, 32, false, "GUI/loop_.png", "LOOP");
	loopToggle->setLabelVisible(false);
    gui->addWidgetRight(loopToggle);


    //SETUP BPM CONTROLS
	useBPMToggle = new ofxUILabelToggle(translation.translateKey("BPM"), false);
    gui->addWidgetRight(useBPMToggle);
	bpmDialer = new ofxUINumberDialer(0., 250., 120., 2, "BPM_VALUE", OFX_UI_FONT_MEDIUM);
    gui->addWidgetEastOf(bpmDialer, translation.translateKey("BPM"));
    //figure out where to put this
//	snapToKeysToggle = new ofxUILabelToggle("Snap to Keys",false,0,0,00,OFX_UI_FONT_MEDIUM);
//	gui->addWidgetSouthOf(snapToKeysToggle, "BPM");
//    snapToBPMToggle = new ofxUILabelToggle("Snap to BPM",false,0,0,0,0,OFX_UI_FONT_SMALL);
//    gui->addWidgetSouthOf(snapToBPM, "BPM");

    //SETUP OSC CONTROLS
    enableOSCInToggle = new ofxUILabelToggle(translation.translateKey("OSC IN"),false,0,0,0,0, OFX_UI_FONT_MEDIUM);
    enableOSCOutToggle = new ofxUILabelToggle(translation.translateKey("OSC OUT"),false,0,0,0,0, OFX_UI_FONT_MEDIUM);
    oscOutIPInput = new ofxUITextInput("OSCIP", "127.0.0.1",TEXT_INPUT_WIDTH*1.5,0,0,0, OFX_UI_FONT_MEDIUM);
    oscOutIPInput->setAutoClear(false);
    oscOutIPInput = new ofxUITextInput("DISPLAY2IP", "127.0.0.1",TEXT_INPUT_WIDTH*1.5, 0, 150, 150,OFX_UI_FONT_MEDIUM );
    oscOutIPInput->setAutoClear(false);

    oscInPortInput = new ofxUITextInput("OSCINPORT", "12346",TEXT_INPUT_WIDTH*.8,0,0,0, OFX_UI_FONT_MEDIUM);
    oscInPortInput->setAutoClear(false);

    oscOutPortInput = new ofxUITextInput("OSCOUTPORT", "12345",TEXT_INPUT_WIDTH*.8,0,0,0, OFX_UI_FONT_MEDIUM);
    oscOutPortInput->setAutoClear(false);

	gui->addWidgetRight(enableOSCInToggle);
    gui->addWidgetRight(oscInPortInput);
    gui->addWidgetRight(enableOSCOutToggle);
    gui->addWidgetRight(oscOutIPInput);
    gui->addWidgetRight(oscOutPortInput);

	ofAddListener(gui->newGUIEvent, this, &DurationController::guiEvent);

	//add events
    ofAddListener(timeline.events().bangFired, this, &DurationController::bangFired);
	ofAddListener(ofEvents().exit, this, &DurationController::exit);

    //SET UP LISENTERS
	enableInterface();

    if(settingsLoaded){
        string lastProjectPath = defaultSettings.getValue("lastProjectPath", "");
        string lastProjectName = defaultSettings.getValue("lastProjectName", "");
        if(lastProjectPath != "" && lastProjectName != "" && ofDirectory(lastProjectPath).exists()){
            loadProject(lastProjectPath, lastProjectName);
        }
        else{
            ofLogError() << "Duration -- Last project was not found, creating a new project";
            loadProject(ofToDataPath(defaultProjectDirectoryPath+"Sample Project"), "Sample Project", true);
        }
    }
    else {
//        cout << "Loading sample project " << defaultProjectDirectoryPath << endl;
        loadProject(ofToDataPath(defaultProjectDirectoryPath+"Sample Project"), "Sample Project", true);
    }
        //--GUI0--------------------------------------------------------
    gui0 = new ofxUISuperCanvas("Input");
    gui0->setPosition(0, 90);
    gui0->setVisible(false);
    gui0->addSpacer();
    gui0->addLabel("Video");
    gui0->addSpacer();
    gui0->addFPSSlider("fps");
    gui0->addToggle( "v on/off", false);
    gui0->addToggle( "v load", false);
    gui0->addMinimalSlider("v x scale", 0.1, 10.0, 1.0);
    gui0->addMinimalSlider("v y scale", 0.1, 10.0, 1.0);
    gui0->addToggle("v fit", false);
    gui0->addToggle("v keep aspect", false);
    gui0->addToggle("v hflip", false );
    gui0->addToggle("v vflip", false );
    gui0->addMinimalSlider("v red", 0.0, 1.0, 1.0);
    gui0->addMinimalSlider("v green", 0.0, 1.0, 1.0);
    gui0->addMinimalSlider("v blue", 0.0, 1.0, 1.0);
    gui0->addMinimalSlider("v alpha", 0.0, 1.0, 1.0);
    gui0->addMinimalSlider("speed", -2.0, 4.0, 1.0);
    gui0->addToggle( "v loop", true);
    gui0->addToggle( "v greenscreen", false);
    gui0->addSpacer();
    gui0->addLabel("Audio");
    gui0->addSpacer();
	gui0->addMinimalSlider("audio", 0.0, 1.0, 1 &video_volume);
    gui0->addSpacer();

    gui0->autoSizeToFitWidgets();

    //gui0->getRect()->setWidth(ofGetWidth());
    ofAddListener(gui0->newGUIEvent,this,&DurationController::guiEvent);

    //--GUI1--------------------------------------------------------

    gui1 = new ofxUISuperCanvas("Draw");
    gui1->setPosition(210, 90);
    gui1->setVisible(false);
    gui1->addSpacer();
    gui1->addLabel("Image");
    gui1->addSpacer();
    gui1->addToggle( "i on/off", false);
    gui1->addToggle( "i load", false);
    gui1->addMinimalSlider("i scale x", 0.1, 10.0, 1);
    gui1->addMinimalSlider("i scale y", 0.1, 10.0, 1);
    gui1->addToggle( "i fit", false);
    gui1->addToggle( "i aspect ratio", false);
    gui1->addToggle( "i hflip", false);
    gui1->addToggle( "i vflip", false);
    gui1->addToggle( "i greenscreen", false);
    gui1->addMinimalSlider("i red", 0.0, 1.0, 1.0);
    gui1->addMinimalSlider("i green", 0.0, 1.0, 1.0);
    gui1->addMinimalSlider("i blue", 0.0, 1.0, 1.0);
    gui1->addMinimalSlider("i alpha", 0.0, 1.0, 1.0);
    gui1->addSpacer();
    gui1->autoSizeToFitWidgets();
//    gui1->getRect()->setWidth(ofGetWidth());
    ofAddListener(gui1->newGUIEvent,this,&DurationController::guiEvent);

    //--GUI2--------------------------------------------------------

    gui2 = new ofxUISuperCanvas("Global");
    gui2->setPosition(840, 90);
    gui2->setVisible(false);
    gui2->addSpacer();
    gui2->addLabel("Live Projection");
    gui2->addSpacer();
    gui2->addToggle("live stop/start", false);
    gui2->addToggle("live resync", false);
    gui2->addToggle("live fc on/off", false);
    gui2->addToggle("display gui", true);
//    gui2->addToggle("modesetup on/off", false);
    gui2->addToggle("mpe", false);
    gui2->addSpacer();
    gui2->addLabel("Project");
    gui2->addSpacer();
    gui2->addButton("direct save", false);
    gui2->addButton("direct load", false);
    gui2->addToggle("save file", false);
    gui2->addToggle("load file", false);
    gui2->addSpacer();
    gui2->addLabel("Edge Blend");
    gui2->addSpacer();
    gui2->addToggle( "eb on/off", false);
    gui2->addMinimalSlider("power", 0.0, 4.0, 1.0);
    gui2->addMinimalSlider("gamma", 0.0, 4.0, 1.8);
    gui2->addMinimalSlider("luminance", -4.0, 4.0, 1.0);
    gui2->addMinimalSlider("left edge", 0.0, 0.5, 0.3);
    gui2->addMinimalSlider("right edge", 0.0, 0.5, 0.3);
    gui2->addMinimalSlider("top edge", 0.0, 0.5, 0.3);
    gui2->addMinimalSlider("bottom edge", 0.0, 0.5, 0.3);
    gui2->addSpacer();
    gui2->autoSizeToFitWidgets();
    ofAddListener(gui2->newGUIEvent,this,&DurationController::guiEvent);



//--GUI3--------------------------------------------------------

    gui3 = new ofxUISuperCanvas("Quad");
    gui3->setPosition(630, 90);
    gui3->setVisible(false);
    gui3->addMinimalSlider("active quad", 0.0, 72.0, 3.0);
    //textinput->setAutoUnfocus(false);
    gui3->addSpacer();
    gui3->addLabel("Surface");
    gui3->addSpacer();
    gui3->addToggle( "show/hide", true);
    gui3->addToggle( "use timeline", true);
    gui3->addMinimalSlider("seconds", 10, 3600, 100);
    gui3->addToggle( "tl tint", false);
    gui3->addToggle( "tl color", false);
    gui3->addToggle( "tl alpha", false);
    gui3->addToggle( "tl 4 slides", false);

    float dim = (gui3->getGlobalCanvasWidth() - gui3->getPadding()*7.0)*0.5;

    gui3->addSpacer();
    gui3->addLabel("Blending modes");
    gui3->addSpacer();
    gui3->addToggle( "bm on/off", false);

    vector<string> items;
    items.push_back("screen");
    items.push_back("add");
    items.push_back("subtract");
    items.push_back("multiply");

    gui3->addDropDownList("blending mode", items, dim);
    gui3->addLabel("");
    gui3->addLabel("");
    gui3->addLabel("");
    gui3->addLabel("");
    gui3->addSpacer();
    gui3->addLabel("Mask");
    gui3->addSpacer();
    gui3->addToggle("m on/off", false);
    gui3->addToggle("m invert", false);
    gui3->addToggle("mask edit on/off", false);
    gui3->addSpacer();
    gui3->autoSizeToFitWidgets();
    ofAddListener(gui3->newGUIEvent,this,&DurationController::guiEvent);


//----GUI4--------------------------------------------------------

    gui4 = new ofxUISuperCanvas("Capture & CV");
    gui4->setPosition(420, 90);
    gui4->setVisible(false);
    gui4->addSpacer();
    gui4->addLabel("Camera");
    gui4->addSpacer();
    gui4->addToggle( "c on/off", false);

    float dime = (gui4->getGlobalCanvasWidth() - gui4->getPadding()*7.0)*0.5;

    vector<string> itemos;
    itemos.push_back("camera 0");
    itemos.push_back("camera 1");
    itemos.push_back("camera 2");
    itemos.push_back("camera 3");


    gui4->addDropDownList("device num", itemos, dime);
    gui4->addLabel("");
    gui4->addLabel("");
    gui4->addLabel("");
    gui4->addLabel("");
    gui4->addSpacer();


    gui4->addMinimalSlider("c scale x", 0.1, 10.0, 1);
    gui4->addMinimalSlider("c scale y", 0.1, 10.0, 1);
    gui4->addToggle( "c fit", false);
    gui4->addToggle( "c aspect ratio", false);
    gui4->addToggle( "c hflip", false);
    gui4->addToggle( "c vflip", false);
    gui4->addMinimalSlider("c red", 0.0, 1.0, 1.0);
    gui4->addMinimalSlider("c green", 0.0, 1.0, 1.0);
    gui4->addMinimalSlider("c blue", 0.0, 1.0, 1.0);
    gui4->addMinimalSlider("c alpha", 0.0, 1.0, 1.0);
    gui4->addToggle( "c greenscreen", false);
    gui4->addSpacer();
    gui4->addLabel("Greenscreen");
    gui4->addSpacer();
	gui4->addMinimalSlider("threshold", 0.0, 255.0, 10.0);
    gui4->addMinimalSlider("gs red", 0.0, 1.0, 0.0);
    gui4->addMinimalSlider("gs green", 0.0, 1.0, 0.0);
    gui4->addMinimalSlider("gs blue", 0.0, 1.0, 0.0);
    gui4->addMinimalSlider("gs alpha", 0.0, 1.0, 0.0);
    gui4->addSpacer();
    gui4->autoSizeToFitWidgets();
    //gui4->getRect()->setWidth(ofGetWidth());
    ofAddListener(gui4->newGUIEvent,this,&DurationController::guiEvent);


//----GUI5------------------------------------------------------

    gui5 = new ofxUISuperCanvas("Active Surface");
    gui5->setDimensions(750, 50);
    gui5->setPosition(0, 0);
    gui5->setVisible(false);
    gui5->addMinimalSlider("Number", 0.0, 72.0, 3.0 , 350 , 20);
    gui5->addSpacer();
    gui5->autoSizeToFitWidgets();
    //gui5->getRect()->setWidth(ofGetWidth());
    ofAddListener(gui5->newGUIEvent,this,&DurationController::guiEvent);

//----GUI6------------------------------------------------------

    gui6 = new ofxUISuperCanvas("Capture & CV");
    gui6->setPosition(420, 90);
//    gui6->setDimensions(150, ofGetHeight());
    gui6->setVisible(false);
    gui6->addSpacer();
    gui6->addLabel("Kinect");
    gui6->addSpacer();
    gui6->addToggle("k on/off", false);
    gui6->addToggle("k close/open", false);
    gui6->addToggle("k show img", false);
    gui6->addToggle("k grayscale", false);
    gui6->addToggle("k mask", false);
    gui6->addToggle("k detect", false);
    gui6->addMinimalSlider("k scale x", 0.1, 10.0, 1.0);
    gui6->addMinimalSlider("k scale y", 0.1, 10.0, 1.0);
    gui6->addMinimalSlider("k red", 0.0, 1.0, 1.0);
    gui6->addMinimalSlider("k green", 0.0, 1.0, 1.0);
    gui6->addMinimalSlider("k blue", 0.0, 1.0, 1.0);
    gui6->addMinimalSlider("k alpha", 0.0, 1.0, 1.0);
    gui6->addMinimalSlider("k threshold near", 0.0, 255.0, 255.0);
    gui6->addMinimalSlider("k threshold far", 0.0, 255.0, 0.0);
    gui6->addMinimalSlider("k angle", -30.0, 30.0, 0.0);
    gui6->addMinimalSlider("k blur", 0.0, 10.0,  3.0);
    gui6->addMinimalSlider("k min blob", 0.01, 1.0, 0.01);
    gui6->addMinimalSlider("k max blob", 0.01, 1.0, 1.0);
    gui6->addMinimalSlider("k smooth", 0.0, 20.0, 10.0);
    gui6->addMinimalSlider("k simplify", 0.0, 2.0, 0.0);
    gui6->addSpacer();
    gui6->autoSizeToFitWidgets();
    ofAddListener(gui6->newGUIEvent,this,&DurationController::guiEvent);

//----GUI7------------------------------------------------------

    gui7 = new ofxUISuperCanvas("Draw");
    gui7->setPosition(210, 90);
//    gui7->setDimensions(150, ofGetHeight());
    gui7->setVisible(false);
    gui7->addSpacer();
    gui7->addLabel("Slideshow");
    gui7->addSpacer();
    gui7->addToggle("sh on/off", false);
    gui7->addToggle("sh load", false);
    gui7->addMinimalSlider("sh duration", 0.1, 15.0, 1.0);
    gui7->addToggle("sh fit", false);
    gui7->addToggle("sh aspect ratio", false);
    gui7->addToggle("sh greenscreen", false);
    gui7->addSpacer();
    gui7->addLabel("Solid Color");
    gui7->addSpacer();
    gui7->addToggle( "sc on/off", false);
    gui7->addMinimalSlider("sc red", 0.0, 1.0, 0.0);
    gui7->addMinimalSlider("sc green", 0.0, 1.0, 0.0);
    gui7->addMinimalSlider("sc blue", 0.0, 1.0, 0.0);
    gui7->addMinimalSlider("sc alpha", 0.0, 1.0, 0.0);
    gui7->addSpacer();
    gui7->addLabel("Transition");
    gui7->addSpacer();
    gui7->addToggle("tr on/off",false);
    gui7->addMinimalSlider("tr red", 0.0, 1.0, 0.0);
    gui7->addMinimalSlider("tr green", 0.0, 1.0, 0.0);
    gui7->addMinimalSlider("tr blue", 0.0, 1.0, 0.0);
    gui7->addMinimalSlider("tr alpha", 0.0, 1.0, 0.0);
    gui7->addMinimalSlider("tr duration", 0.1, 60.0, 1.0);
    gui7->addSpacer();
    gui7->autoSizeToFitWidgets();
    ofAddListener(gui7->newGUIEvent,this,&DurationController::guiEvent);

//----GUI8------------------------------------------------------
    gui8 = new ofxUISuperCanvas("Quad");
//    gui8->setDimensions(150, ofGetHeight());
//    gui3->setScrollAreaToScreen();
    gui8->setPosition(630, 90);
    gui8->setVisible(false);
    gui8->addMinimalSlider("active quad", 0.0, 72.0, 3.0);
    gui8->addSpacer();
    gui8->addLabel("rectangular crop");
    gui8->addSpacer();
    gui8->addMinimalSlider("top", 0.0, 1.0, 0.0 );
    gui8->addMinimalSlider("right", 0.0, 1.0, 0.0 );
    gui8->addMinimalSlider("left", 0.0, 1.0, 0.0 );
    gui8->addMinimalSlider("bottom", 0.0, 1.0, 0.0 );
    gui8->addSpacer();
    gui8->addLabel("circular crop");
    gui8->addSpacer();
    gui8->addMinimalSlider("x", 0.0, 1.0, 0.5 );
    gui8->addMinimalSlider("y", 0.0, 1.0, 0.5 );
    gui8->addMinimalSlider("radius", 0.0, 2.0, 0.0 );
    gui8->addSpacer();
    gui8->addLabel("Deform");
    gui8->addSpacer();
    gui8->addToggle("d on/off", false);
    gui8->addToggle("bezier", false);
    gui8->addButton("spherize light", false);
    gui8->addButton("spherize strong", false);
    gui8->addButton("bezier reset", false);
    gui8->addToggle("grid", false);
    gui8->addMinimalSlider("rows num", 2, 15, 5 );
    gui8->addMinimalSlider("columns num", 2, 20, 5 );
    gui8->addToggle("edit", false);
    gui8->addSpacer();
    gui8->autoSizeToFitWidgets();
    ofAddListener(gui8->newGUIEvent,this,&DurationController::guiEvent);

//----GUI9------------------------------------------------------

    gui9 = new ofxUISuperCanvas("Global");
    //gui9->setDimensions(150, ofGetHeight());
    gui9->setPosition(840, 90);
    gui9->setVisible(false);
    gui9->addSpacer();
    gui9->addLabel("Placement");
    gui9->addSpacer();
    gui9->addMinimalSlider("move x", -1600.0, 1600.0, 0.0);
    gui9->addMinimalSlider("move y", -1600.0, 1600.0, 0.0);
    gui9->addMinimalSlider("width", 0.0, 2400.0, 1280.0);
    gui9->addMinimalSlider("height", 0.0, 2400.0, 1024.0);
    gui9->addButton( "reset", false);
    gui9->addSpacer();
    gui9->addLabel("Help");
    gui9->addSpacer();
    string texthere = "Here all the shortcuts will be presented.";
    gui9->addTextArea("Help text",texthere, OFX_UI_FONT_SMALL);
    gui9->autoSizeToFitWidgets();
    ofAddListener(gui9->newGUIEvent,this,&DurationController::guiEvent);
   // guiTabBar->loadSettings("settings/", "ui-");

    //----GUI11

    gui11 = new ofxUISuperCanvas("Input");
    gui11->setPosition(0, 90);
    gui11->setVisible(false);
    gui11->addSpacer();
    gui11->addLabel("3D Model");
    gui11->addSpacer();
    gui11->addToggle("3d load", false);
    gui11->addMinimalSlider("3d scale x", 0.1, 20.0, 1.0);
    gui11->addMinimalSlider("3d scale y", 0.1, 20.0, 1.0);
    gui11->addMinimalSlider("3d scale z", 0.1, 20.0, 1.0);
    gui11->addMinimalSlider("3d rotate x", 0.0, 360.0, 0.0);
    gui11->addMinimalSlider("3d rotate y", 0.0, 360.0, 0.0);
    gui11->addMinimalSlider("3d rotate z", 0.0, 360.0, 0.0);
    gui11->addMinimalSlider("3d move x", 0.0, 3600.0, 612.0);
    gui11->addMinimalSlider("3d move y", 0.0, 3600.0, 612.0);
    gui11->addMinimalSlider("3d move z", -3600.0, 3600.0, 0.0);

    gui11->addToggle("animation", true);

    float dima = (gui11->getGlobalCanvasWidth() - gui11->getPadding()*7.0)*0.5;

    vector<string> itemus;
    itemus.push_back("smooth");
    itemus.push_back("wire");
    itemus.push_back("dots");

    gui11->addDropDownList("render mode", itemus, dima);
    gui11->addLabel("");
    gui11->addLabel("");
    gui11->addLabel("");
    gui11->addSpacer();

    gui11->autoSizeToFitWidgets();
    ofAddListener(gui11->newGUIEvent,this,&DurationController::guiEvent);



	createTooltips();

	startThread();
}
void DurationController::panelGui(){


}
void DurationController::threadedFunction(){
	while(isThreadRunning()){
		lock();
		oscLock.lock();
		handleOscIn();
		handleOscOut();
		oscLock.unlock();
		unlock();

		ofSleepMillis(1);
	}
}

void DurationController::handleOscIn(){
	if(!settings.oscInEnabled){
		return;
	}

	//TODO: move parsing and receing to separate different threads?
	long timelineStartTime = timeline.getCurrentTimeMillis();
	while(receiver.hasWaitingMessages()){

		ofxOscMessage m;
		receiver.getNextMessage(&m);
		bool handled = false;
		long startTime = recordTimer.getAppTimeMicros();
		vector<ofxTLPage*>& pages = timeline.getPages();
		for(int i = 0; i < pages.size(); i++){
			vector<ofxTLTrack*>& tracks = pages[i]->getTracks();
			for(int t = 0; t < tracks.size(); t++){
//				cout << " testing against " << "/"+tracks[t]->getDisplayName() << endl;
				ofxTLTrack* track = tracks[t];
				ofPtr<ofxTLUIHeader> header = headers[track->getName()];
				if(header->receiveOSC() && m.getAddress() == ofFilePath::addLeadingSlash(track->getDisplayName()) ){

					if(timeline.getIsPlaying() ){ //TODO: change to isPlaying() && isRecording()
						if(track->getTrackType() == "Curves"){
							ofxTLCurves* curves = (ofxTLCurves*)track;
//							cout << "adding value " << m.getArgAsFloat(0) << endl;
							if(m.getArgType(0) == OFXOSC_TYPE_FLOAT){
								float value = m.getArgAsFloat(0);
								if(value != header->lastValueReceived || !header->hasReceivedValue){
									curves->addKeyframeAtMillis(value, timelineStartTime);
									header->lastValueReceived = value;
									header->hasReceivedValue = true;
								}
							}
						}
						else if(track->getTrackType() == "Bangs"){
							ofxTLBangs* bangs = (ofxTLBangs*)track;
							bangs->addKeyframeAtMillis(0,timelineStartTime);
						}
					}

					header->lastInputReceivedTime = recordTimer.getAppTimeSeconds();
					handled = true;
				}
			}
		}

		long endTime = recordTimer.getAppTimeMicros();
//		cout << "receiving message took " << (endTime - startTime) << " micros " << endl;
		if(handled){
			return;
		}

		//check for playback messages
		if(m.getAddress() == "/duration/open"){
			if(m.getNumArgs() == 1 && m.getArgType(0) == OFXOSC_TYPE_STRING){
				string projectPath = m.getArgAsString(0);
				shouldLoadProject = true;
				if(ofFilePath::isAbsolute(projectPath)){
					projectToLoad = projectPath;
				}
				else{
					projectToLoad = defaultProjectDirectoryPath+projectPath;
				}
			}
			else{
				ofLogError("Duration:OSC") << " Open Project Failed - must have on string argument specifying project name or absolute path";
			}
		}
		else if(m.getAddress() == "/duration/new"){
			if(m.getNumArgs() == 1 && m.getArgType(0) == OFXOSC_TYPE_STRING){
				string path = m.getArgAsString(0);
				shouldCreateNewProject = true;
				if(ofFilePath::isAbsolute(path)){
					newProjectPath = path;
				}
				else{
					newProjectPath = defaultProjectDirectoryPath+path;
				}
				cout << "creating new project at path " << newProjectPath << endl;
			}
			else{
				ofLogError("Duration:OSC") << " New Project Failed - must have on string argument specifying the new project path";
			}
		}
		else if(m.getAddress() == "/duration/save"){
			saveProject();
		}
		else if(m.getAddress() == "/duration/setduration"){
			if(m.getNumArgs() == 1){
				//seconds
				if(m.getArgType(0) == OFXOSC_TYPE_FLOAT){
					timeline.setDurationInSeconds(m.getArgAsFloat(0));
					durationLabel->setTextString(timeline.getDurationInTimecode());
				}
				//timecode
				else if(m.getArgType(0) == OFXOSC_TYPE_STRING){
					timeline.setDurationInTimecode(m.getArgAsString(0));
					durationLabel->setTextString(timeline.getDurationInTimecode());
				}
				//millis
				else if(m.getArgType(0) == OFXOSC_TYPE_INT32){
					timeline.setDurationInMillis(m.getArgAsInt32(0));
					durationLabel->setTextString(timeline.getDurationInTimecode());
				}
				else if(m.getArgType(0) == OFXOSC_TYPE_INT64){
					timeline.setDurationInMillis(m.getArgAsInt64(0));
					durationLabel->setTextString(timeline.getDurationInTimecode());
				}
			}
			else {
				ofLogError("Duration:OSC") << " Set Duration failed - must have one argument. seconds as float, timecode string HH:MM:SS:MILS, or integer as milliseconds";
			}
		}
		else if(m.getAddress() == "/duration/play"){
			if(m.getNumArgs() == 0){
				if(!timeline.getIsPlaying()){
					shouldStartPlayback = true;
				}
			}
			else {
				for(int i = 0; i < m.getNumArgs(); i++){
					if(m.getArgType(i) == OFXOSC_TYPE_STRING){
						ofPtr<ofxTLUIHeader> header = getHeaderWithDisplayName(m.getArgAsString(i));
						if(header != NULL){
							header->getTrack()->play();
						}
					}
				}
			}
		}
		else if(m.getAddress() == "/duration/stop"){
			if(m.getNumArgs() == 0){
				if(timeline.getIsPlaying()){
					timeline.stop();
				}
				else{
					timeline.setCurrentTimeMillis(0);
				}
			}
			else{
				for(int i = 0; i < m.getNumArgs(); i++){
					if(m.getArgType(i) == OFXOSC_TYPE_STRING){
						ofPtr<ofxTLUIHeader> header = getHeaderWithDisplayName(m.getArgAsString(i));
						if(header != NULL){
							header->getTrack()->stop();
						}
					}
				}
			}
		}
		else if(m.getAddress() == "/duration/record"){
			//TODO: turn on record mode
			shouldStartPlayback = true;
			//startPlayback();
		}
		else if(m.getAddress() == "/duration/seektosecond"){
			if(m.getArgType(0) == OFXOSC_TYPE_FLOAT){
				timeline.setCurrentTimeSeconds(m.getArgAsFloat(0));
			}
			else{
				ofLogError("Duration:OSC") << " Seek to Second failed: first argument must be a float";
			}
		}
		else if(m.getAddress() == "/duration/seektoposition"){
			if(m.getArgType(0) == OFXOSC_TYPE_FLOAT){
				float percent = ofClamp(m.getArgAsFloat(0),0.0,1.0);
				timeline.setPercentComplete(percent);
			}
			else{
				ofLogError("Duration:OSC") << " Seek to Position failed: first argument must be a float between 0.0 and 1.0";
			}
		}
		else if(m.getAddress() == "/duration/seektomillis"){
			if(m.getArgType(0) == OFXOSC_TYPE_INT32){
				timeline.setCurrentTimeMillis(m.getArgAsInt32(0));
			}
			else if(m.getArgType(0) == OFXOSC_TYPE_INT64){
				timeline.setCurrentTimeMillis(m.getArgAsInt64(0));
			}
			else{
				ofLogError("Duration:OSC") << " Seek to Millis failed: first argument must be a int 32 or in 64";
			}
		}
		else if(m.getAddress() == "/duration/seektotimecode"){
			if(m.getArgType(0) == OFXOSC_TYPE_STRING){
				long millis = ofxTimecode::millisForTimecode(m.getArgAsString(0));
				if(millis > 0){
					timeline.setCurrentTimeMillis(millis);
				}
				else{
					ofLogError("Duration:OSC") << " Seek to Timecode failed: bad timecode. Please format HH:MM:SS:MMM";
				}
			}
			else{
				ofLogError("Duration:OSC") << " Seek to Timecode failed: first argument must be a string";
			}
		}
		//enable and disable OSC
		else if(m.getAddress() == "/duration/enableoscout"){
			//system wide
			if(m.getNumArgs() == 1 && m.getArgType(0) == OFXOSC_TYPE_INT32){
				settings.oscOutEnabled = m.getArgAsInt32(0) != 0;
				enableOSCOutToggle->setValue(settings.oscOutEnabled);
			}
			//per track
			else if(m.getNumArgs() == 2 &&
					m.getArgType(0) == OFXOSC_TYPE_STRING &&
					m.getArgType(1) == OFXOSC_TYPE_INT32)
			{
				string trackName = m.getArgAsString(0);
				ofPtr<ofxTLUIHeader> header = getHeaderWithDisplayName(trackName);
				if(header != NULL){
					header->setSendOSC(m.getArgAsInt32(1) != 0);
				}
				else {
					ofLogError("Duration:OSC") << " Enable OSC out failed. track not found " << trackName;
				}
			}
			else{
				ofLogError("Duration:OSC") << " Enable OSC out incorrectly formatted arguments. usage: /duration/enableoscout enable:int32 == (1 or 0), or /duration/enableoscout trackname:string enable:int32 (1 or 0)";
			}
		}
		else if(m.getAddress() == "/duration/oscrate"){
			if(m.getNumArgs() == 1){
				if(m.getArgType(0) == OFXOSC_TYPE_INT32){
					settings.oscRate = m.getArgAsInt32(0);
					oscFrequency = 1000*1/settings.oscRate;
				}
				else if(m.getArgType(0) == OFXOSC_TYPE_INT64){
					settings.oscRate = m.getArgAsInt64(0);
					oscFrequency = 1000*1/settings.oscRate;
				}
				else if(m.getArgType(0) == OFXOSC_TYPE_FLOAT){
					settings.oscRate = m.getArgAsFloat(0);
					oscFrequency = 1000*1/settings.oscRate;
				}
				else {
					ofLogError("Duration:OSC") << " Set OSC rate failed. must specify an int or a float as the first parameter";
				}
			}
		}
		else if(m.getAddress() == "/duration/enableoscin"){
			//system wide -- don't quite know what to do as this will turn off all osc
			if(m.getNumArgs() == 1 && m.getArgType(0) == OFXOSC_TYPE_INT32){
				settings.oscInEnabled = m.getArgAsInt32(0) != 0;
				enableOSCInToggle->setValue(settings.oscInEnabled);
			}
			//per track
			else if(m.getNumArgs() == 2 && m.getArgType(0) == OFXOSC_TYPE_STRING && m.getArgType(1) == OFXOSC_TYPE_INT32){
				string trackName = m.getArgAsString(0);
				ofPtr<ofxTLUIHeader> header = getHeaderWithDisplayName(trackName);
				if(header != NULL){
					header->setReceiveOSC(m.getArgAsInt32(1) == 1);
				}
				else {
					ofLogError("Duration:OSC") << " Enable in out failed. track not found " << trackName;
				}
			}
			else{
				ofLogError("Duration:OSC") << "Enable OSC in incorrectly formatted arguments. usage: /duration/enableoscout enable:int32 == (1 or 0), or /duration/enableoscout trackname:string enable:int32 (1 or 0)";
			}
		}
		//adding and removing tracks
		else if(m.getAddress() == "/duration/addtrack"){
			//type,
			receivedAddTrack = false;
			oscTrackTypeReceived = "";
			oscTrackNameReceived = "";
			oscTrackFilePathReceived = "";
			//type
			if(m.getNumArgs() > 0 && m.getArgType(0) == OFXOSC_TYPE_STRING) {
				oscTrackTypeReceived = m.getArgAsString(0);
				receivedAddTrack = true;
			}
			//type, name
			if(m.getNumArgs() > 1 &&m.getArgType(1) == OFXOSC_TYPE_STRING) {
				oscTrackNameReceived = m.getArgAsString(1);
				receivedAddTrack = true;
			}
			//type, name, file path
			if(m.getNumArgs() > 2 && m.getArgType(2) == OFXOSC_TYPE_STRING)
			{
				oscTrackFilePathReceived = m.getArgAsString(2);
				receivedAddTrack = true;
			}
			if(!receivedAddTrack){
				ofLogError("Duration:OSC") << "Add track failed, incorrectly formatted arguments. \n usage: /duration/addtrack type:string [optional name:string ] [optional filepath:string ]";
			}
		}
		else if(m.getAddress() == "/duration/removetrack"){
			if(m.getNumArgs() == 1 && m.getArgType(0) == OFXOSC_TYPE_STRING){
				string trackName = m.getArgAsString(0);
				ofPtr<ofxTLUIHeader> header = getHeaderWithDisplayName(trackName);
				if(header != NULL){
					header->setShouldDelete(true);
				}
				else{
					ofLogError("Duration:OSC") << "Remove track failed, could not find track " << trackName;
				}
			}
			else {
				ofLogError("Duration:OSC") << "Remove track failed, incorrectly formatted arguments. \n usage: /duration/removetrack name:string";
			}
		}
		else if(m.getAddress() == "/duration/trackname"){
			if(m.getNumArgs() == 2 &&
			   m.getArgType(0) == OFXOSC_TYPE_STRING &&
			   m.getArgType(1) == OFXOSC_TYPE_STRING)
			{
				string trackName = m.getArgAsString(0);
				ofPtr<ofxTLUIHeader> header = getHeaderWithDisplayName(trackName);
				if(header != NULL){
					header->getTrack()->setDisplayName(m.getArgAsString(1));
				}
				else{
					ofLogError("Duration:OSC") << "Set Track Name failed, could not find track " << trackName;
				}
			}
			else{
				ofLogError("Duration:OSC") << "Set Track Name failed, incorrectly formatted arguments. \n usage: /duration/trackname oldname:string newname:string";
			}
		}
		else if(m.getAddress() == "/duration/valuerange"){
			if(m.getNumArgs() == 3 &&
			   m.getArgType(0) == OFXOSC_TYPE_STRING && //track name
			   m.getArgType(1) == OFXOSC_TYPE_FLOAT && //min
			   m.getArgType(2) == OFXOSC_TYPE_FLOAT) //max
			{
				string trackName = m.getArgAsString(0);
				ofPtr<ofxTLUIHeader> header = getHeaderWithDisplayName(trackName);
				if(header != NULL){
					if(header->getTrackType() == "Curves" || header->getTrackType() == "LFO"){
						header->setValueRange(ofRange(m.getArgAsFloat(1),m.getArgAsFloat(2)));
					}
					else {
						ofLogError("Duration:OSC") << "Set value range failed, track is not a Curves track " << trackName;
					}
				}
				else{
					ofLogError("Duration:OSC") << "Set value range failed, could not find track " << trackName;
				}
			}
			else {
				ofLogError("Duration:OSC") << "Set value range failed, incorrectly formatted message. \n usage: /duration/valuerange trackname:string min:float max:float";
			}
		}
		else if(m.getAddress() == "/duration/valuerange/min"){
			if(m.getNumArgs() == 2 &&
			   m.getArgType(0) == OFXOSC_TYPE_STRING && //track name
			   m.getArgType(1) == OFXOSC_TYPE_FLOAT) //min
			{
				string trackName = m.getArgAsString(0);
				ofPtr<ofxTLUIHeader> header = getHeaderWithDisplayName(trackName);
				if(header != NULL){
					if(header->getTrackType() == "Curves" || header->getTrackType() == "LFO"){
						header->setValueMin(m.getArgAsFloat(1));
					}
					else{
						ofLogError("Duration:OSC") << "Set value range min failed, track is not a Curves track " << trackName;
					}
				}
				else{
					ofLogError("Duration:OSC") << "Set value range min failed, could not find track " << trackName;
				}
			}
			else{
				ofLogError("Duration:OSC") << "Set value range min failed. Incorrectly formatted arguments \n usage: /duration/valuerange/min trackname:string";
			}
		}
		else if(m.getAddress() == "/duration/valuerange/max"){
			if(m.getNumArgs() == 2 &&
			   m.getArgType(0) == OFXOSC_TYPE_STRING && //track name
			   m.getArgType(1) == OFXOSC_TYPE_FLOAT) //max
			{
				string trackName = m.getArgAsString(0);
				ofPtr<ofxTLUIHeader> header = getHeaderWithDisplayName(trackName);
				if(header != NULL){
					if(header->getTrackType() == "Curves" || header->getTrackType() == "LFO"){
						header->setValueMax(m.getArgAsFloat(1));
					}
					else{
						ofLogError("Duration:OSC") << "Set value range max failed, track is not a Curves track " << trackName;
					}
				}
				else{
					ofLogError("Duration:OSC") << "Set value range min failed, could not find track " << trackName;
				}
			}
			else{
				ofLogError("Duration:OSC") << "Set value range min failed. Incorrectly formatted arguments \n usage: /duration/valuerange/min trackname:string";
			}
		}
		else if(m.getAddress() == "/duration/colorpalette"){
			if(m.getNumArgs() == 2 &&
			   m.getArgType(0) == OFXOSC_TYPE_STRING && //track name
			   m.getArgType(1) == OFXOSC_TYPE_STRING) //file path
			{
				string trackName = m.getArgAsString(0);
				ofPtr<ofxTLUIHeader> header = getHeaderWithDisplayName(trackName);
				if(header != NULL){
					if(header->getTrackType() == "Colors"){
						receivedPaletteToLoad = true;
						paletteTrack = (ofxTLColorTrack*)header->getTrack();
						palettePath  = m.getArgAsString(1);
					}
				}
				else {
					ofLogError("Duration:OSC") << "Set color palette failed, could not find track " << trackName;
				}
			}
			else{
				ofLogError("Duration:OSC") << "Set color palette failed, incorrectly formatted arguments \n usage: /duration/colorpalette trackname:string imagefilepath:string";
			}
		}
		else if(m.getAddress() == "/duration/audioclip"){
			if(m.getNumArgs() == 1 && m.getArgType(0) == OFXOSC_TYPE_STRING){
				if(audioTrack != NULL){
					if(!audioTrack->loadSoundfile(m.getArgAsString(0))){
						ofLogError("Duration:OSC") << "Set audio clip failed, clip failed to load. " << m.getArgAsString(0);
					}
				}
				else {
					ofLogError("Duration:OSC") << "Set audio clip failed, first add an audio track to the composition.";
				}
			}
			else{
				ofLogError("Duration:OSC") << "Set audio clip failed, incorrectly formatted arguments. \n usage /duration/audioclip filepath:string ";
			}
		}
	}
}

void DurationController::handleOscOut(){

	if(!settings.oscOutEnabled){
		return;
	}

	unsigned long bundleTime = recordTimer.getAppTimeMillis();
	if(lastOSCBundleSent+oscFrequency > bundleTime){
		return;
	}
	//cout << "OSC RATE IS " << settings.oscRate << " osc FREQUENCY is " << oscFrequency << " sending num at record timer " << recordTimer.getAppTimeMillis() << endl;

	unsigned long timelineSampleTime = timeline.getCurrentTimeMillis();
	int numMessages = 0;
	ofxOscBundle bundle;

	//lock();
	vector<ofxTLPage*>& pages = timeline.getPages();
	for(int i = 0; i < pages.size(); i++){
		vector<ofxTLTrack*>& tracks = pages[i]->getTracks();
		for(int t = 0; t < tracks.size(); t++){
			ofPtr<ofxTLUIHeader> header = headers[tracks[t]->getName()];
			if(!header->sendOSC()){
				continue;
			}
			unsigned long trackSampleTime = tracks[t]->getIsPlaying() ? tracks[t]->currentTrackTime() : timelineSampleTime;
			string trackType = tracks[t]->getTrackType();
			if(trackType == "Curves" || trackType == "Switches" || trackType == "Colors" || trackType == "Audio" || trackType == "LFO"){
				bool messageValid = false;
				ofxOscMessage m;
				if(trackType == "Curves" || trackType == "LFO"){
					ofxTLKeyframes* curves = (ofxTLKeyframes*)tracks[t];
					float value = curves->getValueAtTimeInMillis(trackSampleTime);
					if(value != header->lastFloatSent || !header->hasSentValue || refreshAllOscOut){
						m.addFloatArg(value);
						header->lastFloatSent = value;
						header->hasSentValue = true;
						messageValid = true;
					}
				}
				else if(trackType == "Switches"){
					ofxTLSwitches* switches = (ofxTLSwitches*)tracks[t];
					bool on = switches->isOnAtMillis(trackSampleTime);
					if(on != header->lastBoolSent || !header->hasSentValue || refreshAllOscOut){
						m.addIntArg(on ? 1 : 0);
						header->lastBoolSent = on;
						header->hasSentValue = true;
						messageValid = true;
					}
				}
				else if(trackType == "Colors"){
					ofxTLColorTrack* colors = (ofxTLColorTrack*)tracks[t];
					ofColor color = colors->getColorAtMillis(trackSampleTime);
					if(color != header->lastColorSent || !header->hasSentValue || refreshAllOscOut){
						m.addIntArg(color.r);
						m.addIntArg(color.g);
						m.addIntArg(color.b);
						header->lastColorSent = color;
						header->hasSentValue = true;
						messageValid = true;
					}
				}
				else if(trackType == "Audio"){
					ofxTLAudioTrack* audio = (ofxTLAudioTrack*)tracks[t];
					if(audio->getIsPlaying() || timeline.getIsPlaying()){
						vector<float>& bins = audio->getFFT();
						for(int b = 0; b < bins.size(); b++){
							m.addFloatArg(bins[b]);
						}
						messageValid = true;
					}
				}
				if(messageValid){
					m.setAddress(ofFilePath::addLeadingSlash(tracks[t]->getDisplayName()));
					bundle.addMessage(m);
					numMessages++;
				}
			}
		}
	}
	//unlock();

	//any bangs that came our way this frame send them out too
	for(int i = 0; i < bangsReceived.size(); i++){
//		cout << "FOUND BANGS!" << endl;
		bundle.addMessage(bangsReceived[i]);
	}
	numMessages += bangsReceived.size();
	if(numMessages > 0){
		sender.sendBundle(bundle);
		refreshAllOscOut = false;

	}
	lastOSCBundleSent = bundleTime;
	bangsReceived.clear();
}

//TODO: hook up to record button
//and make NO LOOP
void DurationController::startRecording(){
	recordTimer.setStartTime();
	recordTimeOffset = timeline.getCurrentTimeMillis();
	//timeline.play();
	startPlayback();
}

void DurationController::stopRecording(){

}

//--------------------------------------------------------------
void DurationController::bangFired(ofxTLBangEventArgs& bang){
// 	ofLogNotice() << "Bang from " << bang.track->getDisplayName() << " at time " << bang.currentTime << " with flag " << bang.flag;
	if(!settings.oscOutEnabled){
		return;
	}

    string trackType = bang.track->getTrackType();
    if(!headers[bang.track->getName()]->sendOSC()){
        return;
    }
    ofxOscMessage m;
    m.setAddress( ofFilePath::addLeadingSlash(bang.track->getDisplayName()) );

    if(trackType == "Flags"){
        m.addStringArg(bang.flag);
    }

	bangsReceived.push_back(m);
}

//--------------------------------------------------------------
void DurationController::guiEvent(ofxUIEventArgs &e){
    string name = e.widget->getName();
	int kind = e.widget->getKind();

    if(name == "active quad")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/set");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "v on/off")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/video/show");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "v load")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/video/load");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "v x scale")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/video/mult/x");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "v y scale")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/video/mult/y");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "v fit")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/video/fit");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "v keep aspect")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/video/keepaspect");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "v hflip")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/video/hmirror");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "v vflip")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/video/vmirror");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "v red")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/video/color/1");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "v green")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/video/color/2");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "v blue")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/video/color/3");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "v alpha")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/video/color/4");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "audio")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/video/volume");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "speed")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/video/speed");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "v loop")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/video/loop");
    m.addFloatArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "v greenscreen")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/video/greenscreen");
    m.addFloatArg(toggle->getValue());
    sender.sendMessage(m);
    }
        //greenscreen

    if(name == "threshold")
    {
    ofxUICircleSlider *csliderValue = (ofxUICircleSlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/greenscreen/threshold");
    m.addFloatArg(csliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "gs red")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/greenscreen/color/1");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "gs green")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/greenscreen/color/2");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "gs blue")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/greenscreen/color/3");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "gs alpha")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/greenscreen/color/4");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }

    //Kinect
    if(name == "k on/off")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/show");
    m.addFloatArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "k close/open")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/close");
    m.addFloatArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "k show img")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/show/image");
    m.addFloatArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "k grayscale")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/show/grayscale");
    m.addFloatArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "k mask")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/mask");
    m.addFloatArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "k detect")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/contour");
    m.addFloatArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "k scale x")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/mult/x");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "k scale y")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/mult/y");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "k threshold near")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/threshold/near");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "k threshold far")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/threshold/far");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "k angle")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/angle");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "k blur")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/blur");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "k smooth")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/contour/smooth");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "k simplify")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/contour/simplify");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "k min blob")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/contour/area/min");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "k max blob")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/contour/area/max");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "k red")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/color/1");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "k green")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/color/2");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "k blue")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/color/3");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "k alpha")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/kinect/color/4");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }




    //slideshow
    if(name == "sh on/off")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/slideshow/show");
    m.addFloatArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "sh load")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/slideshow/folder");
    m.addFloatArg(toggle->getValue());
    sender.sendMessage(m);
    }

    if(name == "sh fit")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/slideshow/fit");
    m.addFloatArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "sh aspect ratio")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/slideshow/keep_aspect");
    m.addFloatArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "sh greenscreen")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/slideshow/greenscreen");
    m.addFloatArg(toggle->getValue());
    sender.sendMessage(m);
    }


    if(name == "sh duration")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/slideshow/duration");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
/*    if(name == "show/hide")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/show");
    m.addFloatArg(toggle->getValue());
    sender.sendMessage(m);
    }
*/
    //timeline

    if(name == "use timeline")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/projection/timeline/toggle");
    m.addFloatArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "seconds")
    {
    ofxUIMinimalSlider *sliderValue = (ofxUIMinimalSlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/projection/timeline/duration");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "tl tint")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/timeline/tint");
    m.addFloatArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "tl color")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/timeline/color");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "tl alpha")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/timeline/alpha");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "tl 4 slides")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/timeline/slides");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }

    //camera

    if(name == "c on/off")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/show");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "c load")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/show");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "c scale x")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/mult/x");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "c scale y")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/mult/y");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "c fit")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/fit");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "c aspect ratio")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/keepaspect");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "c hflip")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/hmirror");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "c vflip")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/vmirror");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "c red")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/color/1");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "c green")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/color/2");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "c blue")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/color/3");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "c alpha")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/color/4");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "cam audio")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/volume");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "c greenscreen")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/greenscreen");
    m.addFloatArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "camera 0")
    {
    ofxUIDropDownList *ddl = (ofxUIDropDownList *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/num");
    m.addIntArg(0);
    //m.addIntArg(1);
    //m.addIntArg(3);
    //m.addIntArg(4);
    m.addIntArg(ddl->getValue());
    sender.sendMessage(m);
    }
    if(name == "camera 1")
    {
    ofxUIDropDownList *ddl = (ofxUIDropDownList *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/num");
    m.addIntArg(1);
    //m.addIntArg(1);
    //m.addIntArg(3);
    //m.addIntArg(4);
    m.addIntArg(ddl->getValue());
    sender.sendMessage(m);
    }
    if(name == "camera 2")
    {
    ofxUIDropDownList *ddl = (ofxUIDropDownList *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/num");
    m.addIntArg(2);
    //m.addIntArg(1);
    //m.addIntArg(3);
    //m.addIntArg(4);
    m.addIntArg(ddl->getValue());
    sender.sendMessage(m);
    }
    if(name == "camera 3")
    {
    ofxUIDropDownList *ddl = (ofxUIDropDownList *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/cam/num");
    m.addIntArg(3);
    //m.addIntArg(1);
    //m.addIntArg(3);
    //m.addIntArg(4);
    m.addIntArg(ddl->getValue());
    sender.sendMessage(m);
    }


    //image

    if(name == "i on/off")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/img/show");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "i load")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/img/load");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "i fit")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/img/fit");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "i aspect ratio")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/img/keepaspect");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "i scale x")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/img/mult/x");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "i scale y")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/img/mult/y");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "i hflip")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/img/hmirror");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "i vflip")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/img/vmirror");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "i greenscreen")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/img/greenscreen");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "i red")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/img/color/1");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "i green")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/img/color/2");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "i blue")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/img/color/3");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "i alpha")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/img/color/4");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }

    //placement
    if(name == "move x")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/placement/x");
    m.addIntArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
     if(name == "move y")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/placement/y");
    m.addIntArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "width")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/placement/w");
    m.addIntArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "height")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/placement/h");
    m.addIntArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "reset")
    {
    ofxUIButton *button = (ofxUIButton *) e.getButton();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/placement/reset");
    m.addIntArg(button->getValue());
    sender.sendMessage(m);
    }

    //edge blend

    if(name == "eb on/off")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/edgeblend/show");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
     if(name == "power")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/edgeblend/power");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "gamma")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/edgeblend/gamma");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "luminance")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/edgeblend/luminance");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "left edge")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/edgeblend/amount/left");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "right edge")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/edgeblend/amount/right");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "top edge")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/edgeblend/amount/top");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "bottom edge")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/edgeblend/amount/bottom");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }

     //blend modes

     if(name == "bm on/off")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/blendmodes/show");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
     if(name == "screen")
    {
    ofxUIDropDownList *ddl = (ofxUIDropDownList *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/blendmodes/mode");
    m.addIntArg(0);
    //m.addIntArg(1);
    //m.addIntArg(3);
    //m.addIntArg(4);
    m.addIntArg(ddl->getValue());
    sender.sendMessage(m);
    }
    if(name == "add")
    {
    ofxUIDropDownList *ddl = (ofxUIDropDownList *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/blendmodes/mode");
    m.addIntArg(1);
    //m.addIntArg(1);
    //m.addIntArg(3);
    //m.addIntArg(4);
    m.addIntArg(ddl->getValue());
    sender.sendMessage(m);
    }
    if(name == "subtract")
    {
    ofxUIDropDownList *ddl = (ofxUIDropDownList *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/blendmodes/mode");
    m.addIntArg(2);
    //m.addIntArg(1);
    //m.addIntArg(3);
    //m.addIntArg(4);
    m.addIntArg(ddl->getValue());
    sender.sendMessage(m);
    }
    if(name == "multiply")
    {
    ofxUIDropDownList *ddl = (ofxUIDropDownList *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/blendmodes/mode");
    m.addIntArg(3);
    //m.addIntArg(1);
    //m.addIntArg(3);
    //m.addIntArg(4);
    m.addIntArg(ddl->getValue());
    sender.sendMessage(m);
    }

    //solid color

    if(name == "sc on/off")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/solid/show");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "sc red")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/solid/color/1");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "sc green")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/solid/color/2");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "sc blue")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/solid/color/3");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "sc alpha")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/solid/color/4");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }

    //Projection

    if(name == "live resync")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/projection/resync");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "live stop/start")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/projection/stop");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "direct save")
    {
    ofxUIButton *button = (ofxUIButton *) e.getButton();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/projection/save");
    m.addIntArg(button->getValue());
    sender.sendMessage(m);
    }
    if(name == "direct load")
    {
    ofxUIButton *button = (ofxUIButton *) e.getButton();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/projection/load");
    m.addIntArg(button->getValue());
    sender.sendMessage(m);
    }
    if(name == "load file")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/projection/loadfile");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "save file")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/projection/savefile");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "live fc on/off")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/projection/fullscreen/toggle");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "display gui")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/projection/gui/toggle");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }

/*    if(name == "modesetup on/off")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/projection/mode/setup/toggle");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
*/
    //mask

    if(name == "m on/off")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/mask/show");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "m invert")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/mask/invert");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "mask edit on/off")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/projection/mode/masksetup/toggle");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }

    //Deform
    if(name == "d on/off")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/deform/show");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "bezier")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/deform/bezier");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "spherize light")
    {
    ofxUIButton *button = (ofxUIButton *) e.getButton();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/deform/bezier/spherize/light");
    m.addIntArg(button->getValue());
    sender.sendMessage(m);
    }
    if(name == "spherize strong")
    {
    ofxUIButton *button = (ofxUIButton *) e.getButton();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/deform/bezier/spherize/strong");
    m.addIntArg(button->getValue());
    sender.sendMessage(m);
    }
    if(name == "bezier reset")
    {
    ofxUIButton *button = (ofxUIButton *) e.getButton();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/deform/bezier/reset");
    m.addIntArg(button->getValue());
    sender.sendMessage(m);
    }
    if(name == "grid")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/deform/grid");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "rows num")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/deform/grid/rows");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "columns num")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/deform/grid/columns");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "edit")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/deform/edit");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }

    //crop

    if(name == "top")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/crop/rectangular/top");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "right")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/crop/rectangular/right");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "left")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/crop/rectangular/left");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "bottom")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/crop/rectangular/bottom");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "x")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/crop/circular/x");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "y")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/crop/circular/y");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "radius")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/crop/circular/radius");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    //active surface
    if(name == "Number")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/set");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    //transition
    if(name == "tr on/off")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/solid/trans/show");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "tr red")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/solid/trans/color/1");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "tr green")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/solid/trans/color/2");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "tr blue")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/solid/trans/color/3");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "tr alpha")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/solid/trans/color/4");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "tr duration")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/solid/trans/duration");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    //3d model
    if(name == "3d load")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/3d/load");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
    if(name == "3d scale x")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/3d/scale/x");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "3d scale y")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/3d/scale/y");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "3d scale z")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/3d/scale/z");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "3d rotate x")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/3d/rotate/x");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "3d rotate y")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/3d/rotate/y");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "3d rotate z")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/3d/rotate/z");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "3d move x")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/3d/move/x");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "3d move y")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/3d/move/y");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "3d move z")
    {
    ofxUISlider *sliderValue = (ofxUISlider *) e.getSlider();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/3d/move/z");
    m.addFloatArg(sliderValue->getValue());
    sender.sendMessage(m);
    }
    if(name == "animation")
    {
    ofxUIToggle *toggle = (ofxUIToggle *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/3d/animation");
    m.addIntArg(toggle->getValue());
    sender.sendMessage(m);
    }
        if(name == "smooth")
    {
    ofxUIDropDownList *ddl = (ofxUIDropDownList *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/3d/texture/mode");
    m.addIntArg(0);
    m.addIntArg(ddl->getValue());
    sender.sendMessage(m);
    }
    if(name == "wire")
    {
    ofxUIDropDownList *ddl = (ofxUIDropDownList *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/3d/texture/mode");
    m.addIntArg(1);
    m.addIntArg(ddl->getValue());
    sender.sendMessage(m);
    }
    if(name == "dots")
    {
    ofxUIDropDownList *ddl = (ofxUIDropDownList *) e.getToggle();
    cout << "got event from: " << name << endl;
    ofxOscMessage m;
    m.setAddress("/active/3d/texture/mode");
    m.addIntArg(2);
    m.addIntArg(ddl->getValue());
    sender.sendMessage(m);
    }




	//	cout << "name is " << name << " kind is " << kind << endl;

	if(e.widget == stopButton && stopButton->getValue()){
		if(timeline.getIsPlaying()){
	        timeline.stop();
		}
		else{
	        timeline.setCurrentTimeMillis(0);
		}
    }
    else if(name == "PLAYPAUSE"){
		if(!timeline.getIsPlaying()){
			startPlayback();
		}
		else{
			timeline.stop();
		}
    }
    else if(name == "DURATION"){
		if(!gui->hasKeyboardFocus()){
			string newDuration = durationLabel->getTextString();
			timeline.setDurationInTimecode(newDuration);
			durationLabel->setTextString(timeline.getDurationInTimecode());
			needsSave = true;
		}
    }
    else if(e.widget == addTrackDropDown){
        if(addTrackDropDown->isOpen()){
            timeline.disable();
        }
        else {
            timeline.enable();
            if(addTrackDropDown->getSelected().size() > 0){
				lock();
                string selectedTrackType = addTrackDropDown->getSelected()[0]->getName();
				addTrack(translation.keyForTranslation(selectedTrackType));
				unlock();

                addTrackDropDown->clearSelected();
            }
        }
    }
    else if(e.widget == projectDropDown){
        if(projectDropDown->isOpen()){
            timeline.disable();
			addTrackDropDown->setVisible(false);
        }
		else {
			addTrackDropDown->setVisible(true);
			addTrackDropDown->close();
            timeline.enable();
            if(projectDropDown->getSelected().size() > 0){
                string selectedProjectName = projectDropDown->getSelected()[0]->getName();
                if(selectedProjectName == translation.translateKey("new project...")){
                    shouldCreateNewProject = true;
                }
                else if(selectedProjectName == translation.translateKey("open project...")){
                    shouldLoadProject = true;
					projectToLoad = "";
                }
                else {
					shouldLoadProject = true;
					projectToLoad = ofToDataPath(defaultProjectDirectoryPath+selectedProjectName);
                }
                projectDropDown->clearSelected();
            }
        }
    }
    else if(e.widget == saveButton && saveButton->getValue()){
        saveProject();
    }
    //LOOP
    else if(e.widget == loopToggle){
        timeline.setLoopType(loopToggle->getValue() ? OF_LOOP_NORMAL : OF_LOOP_NONE);
		needsSave = true;
    }
    //BPM
	else if(e.widget == bpmDialer){
		if(settings.bpm != bpmDialer->getValue()){
	    	timeline.setBPM(settings.bpm = bpmDialer->getValue());
			needsSave = true;
		}
	}
    else if(e.widget == useBPMToggle){
        settings.useBPM = useBPMToggle->getValue();
        timeline.setShowBPMGrid(settings.useBPM);
        timeline.enableSnapToBPM(settings.useBPM);
		needsSave = true;
    }
	else if(e.widget == snapToKeysToggle){
		timeline.enableSnapToOtherKeyframes(snapToKeysToggle->getValue());
	}
    //OSC INPUT
    else if(e.widget == enableOSCInToggle){
		settings.oscInEnabled = enableOSCInToggle->getValue();
        if(settings.oscInEnabled){
			oscLock.lock();
            receiver.setup(settings.oscInPort);
			oscLock.unlock();
        }
		needsSave = true;
    }
	//INCOMING PORT
	else if(e.widget == oscInPortInput){
		if(!gui->hasKeyboardFocus()){
			int newPort = ofToInt(oscInPortInput->getTextString());
			if(newPort > 0 && newPort < 65535 &&
			   newPort != settings.oscInPort &&
			   //don't send messages to ourself
			   (newPort != settings.oscOutPort || (settings.oscIP != "localhost" && settings.oscIP != "127.0.0.1"))){
				settings.oscInPort = newPort;
				oscLock.lock();
				receiver.setup(settings.oscInPort);
				oscLock.unlock();
				needsSave = true;
			}
			else {
				oscInPortInput->setTextString( ofToString(settings.oscInPort) );
			}
		}
    }

	//OSC OUTPUT
    else if(e.widget == enableOSCOutToggle){
		settings.oscOutEnabled = enableOSCOutToggle->getValue();
        if(settings.oscOutEnabled){
			oscLock.lock();
            sender.setup(settings.oscIP, settings.oscOutPort);
			oscLock.unlock();
			needsSave = true;
        }
    }

	//OUTGOING IP
    else if(e.widget == oscOutIPInput && !gui->hasKeyboardFocus()){
        string newIP = ofToLower(oscOutIPInput->getTextString());
        if(newIP == settings.oscIP){
            return;
        }

        bool valid = (newIP == "localhost");
		if(!valid){
			vector<string> ipComponents = ofSplitString(newIP, ".");
			if(ipComponents.size() == 4){
				valid = true;
				for(int i = 0; i < 4; i++){
					int component = ofToInt(ipComponents[i]);
					if (component < 0 || component > 255){
						valid = false;
						break;
					}
				}
			}
		}

		if((newIP == "127.0.0.1" || newIP == "localhost") && settings.oscInPort == settings.oscOutPort){
			//don't allow us to send messages to ourself
			valid = false;
		}

		if(valid){
			settings.oscIP = newIP;
			oscLock.lock();
			sender.setup(settings.oscIP, settings.oscOutPort);
			oscLock.unlock();
			needsSave = true;
		}
		oscOutIPInput->setTextString(settings.oscIP);
    }
	//OUTGOING PORT
	else if(e.widget == oscOutPortInput && !gui->hasKeyboardFocus()){
        int newPort = ofToInt(oscOutPortInput->getTextString());
        if(newPort > 0 && newPort < 65535 &&
		   newPort != settings.oscOutPort &&
		   //don't send messages to ourself
		   (newPort != settings.oscInPort || (settings.oscIP != "localhost" && settings.oscIP != "127.0.0.1"))){
            settings.oscOutPort = newPort;
			oscLock.lock();
			sender.setup(settings.oscIP, settings.oscOutPort);
			oscLock.unlock();
			needsSave = true;
        }
        else {
            oscOutPortInput->setTextString( ofToString(settings.oscOutPort) );
        }
    }
}

//--------------------------------------------------------------
ofxTLTrack* DurationController::addTrack(string trackType, string trackName, string xmlFileName){
	ofxTLTrack* newTrack = NULL;

	trackType = ofToLower(trackType);
	if(trackName == ""){
		trackName = trackType;
	}

	if(xmlFileName == ""){
		string uniqueName = timeline.confirmedUniqueName(trackName);
		xmlFileName = ofToDataPath(settings.path + "/" + uniqueName + "_.xml");
	}

	if(trackType == translation.translateKey("bangs") || trackType == "bangs"){
		newTrack = timeline.addBangs(trackName, xmlFileName);
	}
	else if(trackType == translation.translateKey("flags") || trackType == "flags"){
		newTrack = timeline.addFlags(trackName, xmlFileName);
	}
	else if(trackType == translation.translateKey("curves") || trackType == "curves"){
		newTrack = timeline.addCurves(trackName, xmlFileName);
	}
	else if(trackType == translation.translateKey("switches")|| trackType == "switches"){
		newTrack = timeline.addSwitches(trackName, xmlFileName);
	}
	else if(trackType == translation.translateKey("colors") || trackType == "colors"){
		newTrack = timeline.addColors(trackName, xmlFileName);
	}
	else if(trackType == translation.translateKey("lfo") || trackType == "lfo"){
		newTrack = timeline.addLFO(trackName, xmlFileName);
	}
	else if(trackType == translation.translateKey("audio") || trackType == "audio"){
		if(audioTrack != NULL){
			ofLogError("DurationController::addTrack") << "Trying to add an additional audio track";
		}
		else{
			audioTrack = new ofxTLAudioTrack();
			timeline.addTrack(trackName, audioTrack);
			timeline.bringTrackToTop(audioTrack);
			newTrack = audioTrack;
		}
	}
	else {
		ofLogError("DurationController::addTrack") << "Unsupported track type: " << trackType;
	}

	if(newTrack != NULL){
		createHeaderForTrack(newTrack);
		needsSave = true;
	}
	return newTrack;
}

//--------------------------------------------------------------
void DurationController::update(ofEventArgs& args){
	gui->update();

	if(shouldStartPlayback){
		shouldStartPlayback = false;
		startPlayback();
	}
	timeLabel->setLabel(timeline.getCurrentTimecode());
	playpauseToggle->setValue(timeline.getIsPlaying());

	if(audioTrack != NULL && audioTrack->isSoundLoaded()){

		if(timeline.getTimecontrolTrack() != audioTrack){
			timeline.setTimecontrolTrack(audioTrack);
		}

		if(audioTrack->getDuration() != timeline.getDurationInSeconds()){
			timeline.setDurationInSeconds(audioTrack->getDuration());
		}

		if(durationLabel->getTextString() != timeline.getDurationInTimecode()){
			durationLabel->setTextString(timeline.getDurationInTimecode());
		}
	}

	if(ofGetHeight() < timeline.getDrawRect().getMaxY()){
		ofSetWindowShape(ofGetWidth(), timeline.getDrawRect().getMaxY()+30);
	}
    if(shouldLoadProject){
        shouldLoadProject = false;
		if(projectToLoad != ""){
			loadProject(projectToLoad);
			projectToLoad = "";
		}
		else{
			ofFileDialogResult r = ofSystemLoadDialog("Load Project", true);
			if(r.bSuccess){
				loadProject(r.getPath(), r.getName());
			}
		}
    }

    if(shouldCreateNewProject){
        shouldCreateNewProject = false;
		if(newProjectPath != ""){
			newProject(newProjectPath);
			newProjectPath = "";
		}
		else{
			ofFileDialogResult r = ofSystemSaveDialog("New Project", "NewDuration");
			if(r.bSuccess){
				newProject(r.getPath(), r.getName());
			}
		}
    }

	if(receivedPaletteToLoad){
		receivedPaletteToLoad = false;
		if(!paletteTrack->loadColorPalette(palettePath)){
			ofLogError("Duration:OSC") << "Set color palette failed, file not found";
		}
	}

	if(receivedAddTrack){
		lock();
		receivedAddTrack = false;
		addTrack(oscTrackTypeReceived, oscTrackNameReceived, oscTrackFilePathReceived);
		unlock();
	}

    //check if we deleted an element this frame
    map<string,ofPtr<ofxTLUIHeader> >::iterator it = headers.begin();
    while(it != headers.end()){

		needsSave |= it->second->getModified();

		if(timeline.isModal() && it->second->getGui()->isEnabled()){
			it->second->getGui()->disable();
		}
		else if(!timeline.isModal() && !it->second->getGui()->isEnabled()){
			it->second->getGui()->enable();
		}

		if(it->second->getShouldDelete()){
			lock();
            timeline.removeTrack(it->first);
			timeline.setTimecontrolTrack(NULL);
			if(it->second->getTrackType() == "Audio"){
				if(audioTrack == NULL){
					ofLogError("Audio track inconsistency");
				}
				else{
					delete audioTrack;
					audioTrack = NULL;
				}
			}
            headers.erase(it);
			unlock();
			needsSave = true;
            break;
        }
        it++;
    }
}

//--------------------------------------------------------------
ofPtr<ofxTLUIHeader> DurationController::getHeaderWithDisplayName(string name){
	map<string, ofPtr<ofxTLUIHeader> >::iterator trackit;
	for(trackit = headers.begin(); trackit != headers.end(); trackit++){
		if(trackit->second->getTrack()->getDisplayName() == name){
			return trackit->second;
		}
	}
	//same as null
	return ofPtr<ofxTLUIHeader>();
}

//--------------------------------------------------------------
void DurationController::draw(ofEventArgs& args){

	//go through and draw all the overlay backgrounds to indicate 'hot' track sfor recording
	ofPushStyle();
	map<string, ofPtr<ofxTLUIHeader> >::iterator trackit;
	for(trackit = headers.begin(); trackit != headers.end(); trackit++){
		//TODO: check to make sure recording is enabled on this track
		//TODO: find a way to illustrate 'invalid' output sent to this track
		float timeSinceInput = recordTimer.getAppTimeSeconds() - trackit->second->lastInputReceivedTime;
		if(timeSinceInput > 0 && timeSinceInput < 1.0){
			//oscilating red to indicate active
			ofSetColor(200,20,0,(1-timeSinceInput)*(80 + (20*sin(ofGetElapsedTimef()*8)*.5+.5)));
			ofRect(trackit->second->getTrack()->getDrawRect());

		}
	}
	ofPopStyle();

	timeline.draw();
	gui->draw();

	if(needsSave || timeline.hasUnsavedChanges()){
		ofPushStyle();
		ofSetColor(200,20,0, 40);
		ofFill();
		ofxUIRectangle r = *saveButton->getRect();
		ofRect(r.x,r.y,r.width,r.height);
		ofPopStyle();
	}
	drawTooltips();
	//drawTooltipDebug();

}

//--------------------------------------------------------------
void DurationController::keyPressed(ofKeyEventArgs& keyArgs){
    if(timeline.isModal()){
        return;
    }

	if(gui->hasKeyboardFocus()){
		return;
	}

    int key = keyArgs.key;
	if(key == ' '){
		if(ofGetModifierShiftPressed()){
			timeline.togglePlaySelectedTrack();
		}
		else{
			if(!timeline.getIsPlaying()){
				startPlayback();
			}
			else{
				timeline.stop();
			}
		}
    }

    if(key == 'i'){
		if(ofGetModifierAltPressed()){
			timeline.setInPointAtMillis(0);
		}
		else{
	        timeline.setInPointAtPlayhead();
		}
    }

    if(key == 'o'){
		if(ofGetModifierAltPressed()){
			timeline.setOutPointAtPercent(1.0);
		}
		else{
	        timeline.setOutPointAtPlayhead();
		}
    }


	if(ofGetModifierShortcutKeyPressed() && (key == 's' || key=='s'-96) ){
		saveProject();
	}
	if(key == OF_KEY_F2)
    {

		//enabled = true;
		ofAddListener(ofEvents().update, this, &DurationController::update);
		ofAddListener(ofEvents().draw, this, &DurationController::draw);
		ofAddListener(ofEvents().keyPressed, this, &DurationController::keyPressed);
		//gui->enable();
		//gui->disableAppEventCallbacks();
		timeline.show();
        gui0->setVisible(false);
        gui1->setVisible(false);
        gui4->setVisible(false);
        gui3->setVisible(false);
        gui2->setVisible(false);
        gui6->setVisible(false);
        gui7->setVisible(false);
        gui8->setVisible(false);
        gui9->setVisible(false);
        gui11->setVisible(false);

		//timeline.enable();
		/*map<string,ofPtr<ofxTLUIHeader> >::iterator it = headers.begin();
		while(it != headers.end())
            {
			it->second->getGui()->enable();
			it++;
            }
*/
    }
    if(key == OF_KEY_F3)
    {
        //enabled = false;
		//ofRemoveListener(ofEvents().update, this, &DurationController::update);
		//ofRemoveListener(ofEvents().draw, this, &DurationController::draw);
		//ofRemoveListener(ofEvents().keyPressed, this, &DurationController::keyPressed);
		timeline.hide();
		gui0->setVisible(true);
        gui1->setVisible(true);
        gui4->setVisible(true);
        gui3->setVisible(true);
        gui2->setVisible(true);
        gui6->setVisible(false);
        gui7->setVisible(false);
        gui8->setVisible(false);
        gui9->setVisible(false);
        gui11->setVisible(false);
		//gui->disable();
		//timeline.disable();
		map<string,ofPtr<ofxTLUIHeader> >::iterator it = headers.begin();
		while(it != headers.end()){
			it->second->getGui()->disable();
			it++;
        }
    }
    if(key == OF_KEY_F4)
    {
        timeline.hide();
        gui0->setVisible(false);
        gui1->setVisible(false);
        gui4->setVisible(false);
        gui3->setVisible(false);
        gui2->setVisible(false);
        gui6->setVisible(true);
        gui7->setVisible(true);
        gui8->setVisible(true);
        gui9->setVisible(true);
        gui11->setVisible(true);
    }
        if(key == 's')
            {
            gui0->saveSettings("gui0Settings.xml");
            gui1->saveSettings("gui1Settings.xml");
            gui2->saveSettings("gui2Settings.xml");
            gui3->saveSettings("gui3Settings.xml");
            gui4->saveSettings("gui4Settings.xml");
            gui5->saveSettings("gui5Settings.xml");
            gui6->saveSettings("gui6Settings.xml");
            gui7->saveSettings("gui7Settings.xml");
            gui8->saveSettings("gui8Settings.xml");
            gui9->saveSettings("gui9Settings.xml");
            gui11->saveSettings("gui11Settings.xml");
            }
        if(key == 'l')
            {
            gui0->loadSettings("gui0Settings.xml");
            gui1->loadSettings("gui1Settings.xml");
            gui2->loadSettings("gui2Settings.xml");
            gui3->loadSettings("gui3Settings.xml");
            gui4->loadSettings("gui4Settings.xml");
            gui5->loadSettings("gui5Settings.xml");
            gui6->loadSettings("gui6Settings.xml");
            gui7->loadSettings("gui7Settings.xml");
            gui8->loadSettings("gui8Settings.xml");
            gui9->loadSettings("gui9Settings.xml");
            gui11->loadSettings("gui11Settings.xml");
            }
        if(key == OF_KEY_F6)
            {
                gui0->setMinified(true);
                gui1->setMinified(true);
                gui2->setMinified(true);
                gui3->setMinified(true);
                gui4->setMinified(true);
//                gui5->setMinified(true);
                gui6->setMinified(true);
                gui7->setMinified(true);
                gui8->setMinified(true);
                gui9->setMinified(true);
                gui11->setMinified(true);

            }
         if(key == OF_KEY_F5)
            {
                gui0->setMinified(false);
                gui1->setMinified(false);
                gui2->setMinified(false);
                gui3->setMinified(false);
                gui4->setMinified(false);
//                gui5->setMinified(false);
                gui6->setMinified(false);
                gui7->setMinified(false);
                gui8->setMinified(false);
                gui9->setMinified(false);
                gui11->setMinified(false);
            }
            if(key == '~')
            {
                timeline.collapseAllTracks();
            }

}




//--------------------------------------------------------------
void DurationController::startPlayback(){
	if(!timeline.getIsPlaying()){
		sendInfoMessage();
		timeline.play();
	}
}

//--------------------------------------------------------------
void DurationController::sendInfoMessage(){
	if(settings.oscOutEnabled){
		ofxOscMessage m;
		m.setAddress("/duration/info");
		vector<ofxTLPage*>& pages = timeline.getPages();
		for(int i = 0; i < pages.size(); i++){
			vector<ofxTLTrack*>& tracks = pages[i]->getTracks();
			for (int t = 0; t < tracks.size(); t++) {
				m.addStringArg(tracks[t]->getTrackType());
				m.addStringArg(ofFilePath::addLeadingSlash( tracks[t]->getDisplayName() ));
				if(tracks[t]->getTrackType() == "Curves" || tracks[t]->getTrackType() == "LFO"){
					ofxTLCurves* curves = (ofxTLCurves*)tracks[t];
					m.addFloatArg(curves->getValueRange().min);
					m.addFloatArg(curves->getValueRange().max);
				}
			}
		}
		oscLock.lock();
		sender.sendMessage(m);
		refreshAllOscOut = true;
		oscLock.unlock();
	}
}

//--------------------------------------------------------------
DurationProjectSettings DurationController::defaultProjectSettings(){
    DurationProjectSettings settings;

    settings.name = "newProject";
    settings.path = defaultProjectDirectoryPath + settings.name;

    settings.useBPM = false;
    settings.bpm = 120.0f;
    settings.snapToBPM = false;
    settings.snapToKeys = true;

	settings.oscRate = 30;
    settings.oscOutEnabled = true;
	settings.oscInEnabled = true;
    settings.oscInPort = 12346;
    settings.oscIP = "localhost";
    settings.oscOutPort = 12345;
    return settings;
}

//--------------------------------------------------------------
void DurationController::newProject(string projectPath){
	//scrape off the last component of the filename for the project name
	projectPath = ofFilePath::removeTrailingSlash(projectPath);
#ifdef TARGET_WIN32
	vector<string> pathComponents = ofSplitString(projectPath, "\\");
#else
	vector<string> pathComponents = ofSplitString(projectPath, "/");
#endif
	newProject(projectPath, pathComponents[pathComponents.size()-1]);
}

//--------------------------------------------------------------
void DurationController::newProject(string newProjectPath, string newProjectName){
    DurationProjectSettings newProjectSettings = defaultProjectSettings();
    newProjectSettings.name = newProjectName;
    newProjectSettings.path = ofToDataPath(newProjectPath);
    newProjectSettings.settingsPath = ofToDataPath(newProjectSettings.path + "/.durationproj");
#ifdef TARGET_WIN32
	ofStringReplace(newProjectSettings.path,"/", "\\");
#endif
    ofDirectory newProjectDirectory(newProjectSettings.path);
    if(newProjectDirectory.exists()){
    	ofSystemAlertDialog(translation.translateKey("Error creating new project. The folder already exists.")+" " + newProjectSettings.path);
        return;
    }
    if(!newProjectDirectory.create(true)){
    	ofSystemAlertDialog(translation.translateKey("Error creating new project. The folder could not be created.")+" " + newProjectSettings.path);
        return;
    }

    //TODO: prompt to save existing project
    settings = newProjectSettings;
	lock();
    headers.clear(); //smart pointers will call destructor
    timeline.reset();
	unlock();

    //saves file with default settings to new directory
    saveProject();

    loadProject(settings.path, settings.name);

    projectDropDown->addToggle(newProjectName);
}

//--------------------------------------------------------------
void DurationController::loadProject(string projectPath, bool forceCreate){
	//scrape off the last component of the filename for the project name
	projectPath = ofFilePath::removeTrailingSlash(projectPath);
#ifdef TARGET_WIN32
	vector<string> pathComponents = ofSplitString(projectPath, "\\");
#else
	vector<string> pathComponents = ofSplitString(projectPath, "/");
#endif
	loadProject(projectPath, pathComponents[pathComponents.size()-1], forceCreate);
}

//--------------------------------------------------------------
void DurationController::loadProject(string projectPath, string projectName, bool forceCreate){
    ofxXmlSettings projectSettings;
	string projectDataPath = ofToDataPath(projectPath+"/.durationproj");
	if(!projectSettings.loadFile(projectDataPath)){
        if(forceCreate){
            newProject(projectPath, projectName);
        }
        else{
            ofLogError() << " failed to load project " << ofToDataPath(projectPath+"/.durationproj") << endl;
        }
        return;
    }

	lock();

    timeline.removeFromThread();
    headers.clear(); //smart pointers will call destructor
    timeline.reset();
    timeline.setup();

	if(audioTrack != NULL){
		delete audioTrack;
		audioTrack = NULL;
	}
    timeline.setWorkingFolder(projectPath);

    //LOAD ALL TRACKS
    projectSettings.pushTag("tracks");
    int numPages = projectSettings.getNumTags("page");
    for(int p = 0; p < numPages; p++){
        projectSettings.pushTag("page", p);
        string pageName = projectSettings.getValue("name", "defaultPage");
        if(p == 0){
            timeline.setPageName(pageName, 0);
        }
        else{
            timeline.addPage(pageName, true);
        }

        int numTracks = projectSettings.getNumTags("track");
        for(int i = 0; i < numTracks; i++){
            projectSettings.pushTag("track", i);
            string trackType = ofToLower(projectSettings.getValue("type", ""));
            string xmlFileName = projectSettings.getValue("xmlFileName", "");
            string trackName = projectSettings.getValue("trackName","");
            string trackFilePath = ofToDataPath(projectPath + "/" + xmlFileName);

			//add the track
            ofxTLTrack* newTrack = addTrack(trackType, trackName, trackFilePath);

			//custom setup
			if(newTrack != NULL){
				ofPtr<ofxTLUIHeader> headerTrack = headers[newTrack->getName()];
				if(newTrack->getTrackType() == "Curves" || newTrack->getTrackType() == "LFO"){
					headerTrack->setValueRange(ofRange(projectSettings.getValue("min", 0.0),
													   projectSettings.getValue("max", 1.0)));
				}
				else if(newTrack->getTrackType() == "Colors"){
					ofxTLColorTrack* colors = (ofxTLColorTrack*)newTrack;
					colors->loadColorPalette(projectSettings.getValue("palette", timeline.getDefaultColorPalettePath()));
				}
				else if(newTrack->getTrackType() == "Audio"){
					string clipPath = projectSettings.getValue("clip", "");
					if(clipPath != ""){
						audioTrack->loadSoundfile(clipPath);
					}
//					int numbins = projectSettings.getValue("bins", 256);
//					headerTrack->setNumberOfbins(numbins);
//					cout << "set " << numbins << " after load " << headerTrack->getNumberOfBins() << endl;
					//audioTrack->getFFTSpectrum(projectSettings.getValue("bins", 256));
				}

				string displayName = projectSettings.getValue("displayName","");
				if(displayName != ""){
					newTrack->setDisplayName(displayName);
				}

				headerTrack->setSendOSC(projectSettings.getValue("sendOSC", true));
				headerTrack->setReceiveOSC(projectSettings.getValue("receiveOSC", true));
			}
            projectSettings.popTag(); //track
        }
        projectSettings.popTag(); //page
    }

    timeline.moveToThread(); //increases accuracy of bang call backs


	unlock();

    timeline.setCurrentPage(0);
    projectSettings.popTag(); //tracks

    //LOAD OTHER SETTINGS
    projectSettings.pushTag("timelineSettings");
    timeline.setDurationInTimecode(projectSettings.getValue("duration", "00:00:00:000"));
    timeline.setCurrentTimecode(projectSettings.getValue("playhead", "00:00:00:000"));
    timeline.setInPointAtTimecode(projectSettings.getValue("inpoint", "00:00:00:000"));
    timeline.setOutPointAtTimecode(projectSettings.getValue("outpoint", "00:00:00:000"));

    bool loops = projectSettings.getValue("loop", true);
    timeline.setLoopType(loops ? OF_LOOP_NORMAL : OF_LOOP_NONE);

    durationLabel->setTextString(timeline.getDurationInTimecode());
    loopToggle->setValue( loops );
    projectSettings.popTag(); //timeline settings;

    DurationProjectSettings newSettings;
    projectSettings.pushTag("projectSettings");

    useBPMToggle->setValue( newSettings.useBPM = projectSettings.getValue("useBPM", true) );
    bpmDialer->setValue( newSettings.bpm = projectSettings.getValue("bpm", 120.0f) );
//    snapToBPMToggle->setValue( newSettings.snapToBPM = projectSettings.getValue("snapToBPM", true) );
//    snapToKeysToggle->setValue( newSettings.snapToKeys = projectSettings.getValue("snapToKeys", true) );
    enableOSCInToggle->setValue( newSettings.oscInEnabled = projectSettings.getValue("oscInEnabled", true) );
	enableOSCOutToggle->setValue( newSettings.oscOutEnabled = projectSettings.getValue("oscOutEnabled", true) );
    oscInPortInput->setTextString( ofToString(newSettings.oscInPort = projectSettings.getValue("oscInPort", 12346)) );
    oscOutIPInput->setTextString( newSettings.oscIP = projectSettings.getValue("oscIP", "localhost") );
    oscOutIPInput->setTextString( newSettings.oscIP = projectSettings.getValue("Display2IP", "localhost") );
    oscOutPortInput->setTextString( ofToString(newSettings.oscOutPort = projectSettings.getValue("oscOutPort", 12345)) );
    oscOutPortInput->setTextString( ofToString(newSettings.oscOutPort = projectSettings.getValue("Display2Port", 12345)) );
	newSettings.oscRate = projectSettings.getValue("oscRate", 30.0);
	oscFrequency = 1000 * 1/newSettings.oscRate; //frequence in millis

    projectSettings.popTag(); //project settings;

    newSettings.path = projectPath;
    newSettings.name = projectName;
    newSettings.settingsPath = ofToDataPath(newSettings.path + "/.durationproj");
    settings = newSettings;

    projectDropDown->setLabelText(projectName);
    timeline.setShowBPMGrid(newSettings.useBPM);
    timeline.enableSnapToBPM(newSettings.useBPM);
	timeline.setBPM(newSettings.bpm);

	oscLock.lock();
	if(settings.oscInEnabled){
		receiver.setup(settings.oscInPort);
	}
	if(settings.oscOutEnabled){
        sender.setup(settings.oscIP, settings.oscOutPort);
    }
	oscLock.unlock();

    ofxXmlSettings defaultSettings;
    defaultSettings.loadFile("settings.xml");
    defaultSettings.setValue("lastProjectPath", settings.path);
    defaultSettings.setValue("lastProjectName", settings.name);
    defaultSettings.saveFile();

	needsSave = false;
	sendInfoMessage();
}

//--------------------------------------------------------------
void DurationController::saveProject(){

	timeline.save();

    ofxXmlSettings projectSettings;
    //SAVE ALL TRACKS
    projectSettings.addTag("tracks");
    projectSettings.pushTag("tracks");
    vector<ofxTLPage*>& pages = timeline.getPages();
    for(int i = 0; i < pages.size(); i++){
        projectSettings.addTag("page");
        projectSettings.pushTag("page", i);
        projectSettings.addValue("name", pages[i]->getName());
        vector<ofxTLTrack*>& tracks = pages[i]->getTracks();
        for (int t = 0; t < tracks.size(); t++) {
            projectSettings.addTag("track");
            projectSettings.pushTag("track", t);
            //save track properties
            string trackType = tracks[t]->getTrackType();
            string trackName = tracks[t]->getName();
            projectSettings.addValue("type", trackType);
            projectSettings.addValue("xmlFileName", tracks[t]->getXMLFileName());
            projectSettings.addValue("trackName",tracks[t]->getName());
            projectSettings.addValue("displayName",tracks[t]->getDisplayName());
            //save custom gui props
            projectSettings.addValue("sendOSC", headers[trackName]->sendOSC());
			projectSettings.addValue("receiveOSC", headers[trackName]->receiveOSC());
            if(trackType == "Curves" || trackType == "LFO"){
                ofxTLKeyframes* curves = (ofxTLKeyframes*)tracks[t];
                projectSettings.addValue("min", curves->getValueRange().min);
                projectSettings.addValue("max", curves->getValueRange().max);

            }
			else if(trackType == "Colors"){
				ofxTLColorTrack* colors = (ofxTLColorTrack*)tracks[t];
				projectSettings.addValue("palette", colors->getPalettePath());
			}
			else if(trackType == "Audio"){
				projectSettings.addValue("clip", audioTrack->getSoundfilePath());
//				int numbins = audioTrack->getFFTBinCount();
//				projectSettings.addValue("bins", audioTrack->getFFTBinCount());
			}
            projectSettings.popTag();
        }
        projectSettings.popTag(); //page
    }
	projectSettings.popTag(); //tracks

    //LOAD OTHER SETTINGS
    projectSettings.addTag("timelineSettings");
    projectSettings.pushTag("timelineSettings");
    projectSettings.addValue("duration", timeline.getDurationInTimecode());
    projectSettings.addValue("playhead", timeline.getCurrentTimecode());
    projectSettings.addValue("inpoint", timeline.getInPointTimecode());
    projectSettings.addValue("outpoint", timeline.getOutPointTimecode());
    projectSettings.addValue("loop", timeline.getLoopType() == OF_LOOP_NORMAL);
	projectSettings.popTag();// timelineSettings

    //UI SETTINGS
    projectSettings.addTag("projectSettings");
    projectSettings.pushTag("projectSettings");
    projectSettings.addValue("useBPM", settings.useBPM);
    projectSettings.addValue("bpm", settings.bpm);
    projectSettings.addValue("snapToBPM", settings.snapToBPM);
    projectSettings.addValue("snapToKeys", settings.snapToKeys);

    projectSettings.addValue("oscInEnabled", settings.oscInEnabled);
    projectSettings.addValue("oscOutEnabled", settings.oscOutEnabled);
    projectSettings.addValue("oscInPort", settings.oscInPort);
    projectSettings.addValue("oscIP", settings.oscIP);
    projectSettings.addValue("Display2IP", settings.oscIP);
    projectSettings.addValue("oscOutPort", settings.oscOutPort);
    projectSettings.addValue("Display2Port", settings.oscOutPort);
	projectSettings.addValue("oscRate", settings.oscRate);

//	projectSettings.addValue("zoomViewMin",timeline.getZoomer()->getSelectedRange().min);
//	projectSettings.addValue("zoomViewMax",timeline.getZoomer()->getSelectedRange().max);

	projectSettings.popTag(); //projectSettings
    projectSettings.saveFile(settings.settingsPath);

	needsSave = false;
}

//--------------------------------------------------------------
ofxTLUIHeader* DurationController::createHeaderForTrack(ofxTLTrack* track){
    ofxTLUIHeader* headerGui = new ofxTLUIHeader();
	headerGui->translation = &translation;
    ofxTLTrackHeader* header = timeline.getTrackHeader(track);
    headerGui->setTrackHeader(header);
    headers[track->getName()] = ofPtr<ofxTLUIHeader>( headerGui );
    return headerGui;
}

void DurationController::createTooltips(){

	//switch project
	Tooltip projectTip;
	projectTip.text = translation.translateKey("switch project");
	projectTip.sourceRect = *projectDropDown->getRect();
	projectTip.displayPoint = ofVec2f(projectTip.sourceRect.x, 55);
	tooltips.push_back(projectTip);

	//save
	Tooltip saveTip;
	saveTip.text = translation.translateKey("save");
	saveTip.sourceRect = *saveButton->getRect();
	saveTip.displayPoint = ofVec2f(saveTip.sourceRect.x, 55);
	tooltips.push_back(saveTip);

	//play/pause
	Tooltip playpauseTip;
	playpauseTip.text = translation.translateKey("play")+"/"+translation.translateKey("pause"); //TODO: switch dynamically
	playpauseTip.sourceRect = *playpauseToggle->getRect();
	playpauseTip.displayPoint = ofVec2f(playpauseTip.sourceRect.x, 55);
	tooltips.push_back(playpauseTip);

	ofVec2f zone2 = playpauseTip.displayPoint;

	//edit duration
	Tooltip editDurationTip;
	editDurationTip.text = translation.translateKey("edit duration");
	editDurationTip.displayPoint = zone2;
	editDurationTip.sourceRect = *durationLabel->getRect();
	tooltips.push_back(editDurationTip);

	//current time
	Tooltip currentTimeTip;
	currentTimeTip.text = translation.translateKey("current time");
	currentTimeTip.displayPoint = zone2;
	currentTimeTip.sourceRect = *timeLabel->getRect();
	tooltips.push_back(currentTimeTip);

	//stop
	Tooltip stopTip;
	stopTip.text = translation.translateKey("stop");
	stopTip.sourceRect = *stopButton->getRect();
	stopTip.displayPoint = ofVec2f(stopTip.sourceRect.x, 55);
	tooltips.push_back(stopTip);

	//loop
	Tooltip loopTip;
	loopTip.text = translation.translateKey("toggle loop");
	loopTip.sourceRect = *loopToggle->getRect();
	loopTip.displayPoint = ofVec2f(loopTip.sourceRect.x, 55);
	tooltips.push_back(loopTip);

	//enable Snap to BPM
	Tooltip bpmTip;
	bpmTip.text = translation.translateKey("snap to measures");
	bpmTip.sourceRect = *useBPMToggle->getRect();
	bpmTip.displayPoint = ofVec2f(bpmTip.sourceRect.x, 55);
	tooltips.push_back(bpmTip);

	//set beats per minute
	Tooltip setBpmTip;
	setBpmTip.text = translation.translateKey("set beats per minute");
	setBpmTip.sourceRect = *bpmDialer->getRect();
	setBpmTip.displayPoint = ofVec2f(setBpmTip.sourceRect.x, 55);
	tooltips.push_back(setBpmTip);

	//enable OSC
	Tooltip oscInTip;
	oscInTip.text = translation.translateKey("enable incoming OSC");
	oscInTip.sourceRect = *enableOSCInToggle->getRect();
	oscInTip.displayPoint = ofVec2f(oscInTip.sourceRect.x, 55);
	tooltips.push_back(oscInTip);

	//osc In Port
	Tooltip oscInPortTip;
	oscInPortTip.text = translation.translateKey("incoming OSC port");
	oscInPortTip.sourceRect = *oscInPortInput->getRect();
	oscInPortTip.displayPoint = ofVec2f(oscInPortTip.sourceRect.x, 55);
	tooltips.push_back(oscInPortTip);

	//osc Out
	Tooltip oscOutTip;
	oscOutTip.text = translation.translateKey("enable outgoing OSC");
	oscOutTip.sourceRect = *enableOSCOutToggle->getRect();
	oscOutTip.displayPoint = ofVec2f(oscOutTip.sourceRect.x, 55);
	tooltips.push_back(oscOutTip);

	//osc Out IP
	Tooltip oscOutIPTip;
	oscOutIPTip.text = translation.translateKey("remote IP");
	oscOutIPTip.sourceRect = *oscOutIPInput->getRect();
	oscOutIPTip.displayPoint = ofVec2f(oscOutIPTip.sourceRect.x, 55);
	tooltips.push_back(oscOutIPTip);

	//osc Out IP
	Tooltip oscOutPortTip;
	oscOutPortTip.text = translation.translateKey("remote port");
	oscOutPortTip.sourceRect = *oscOutPortInput->getRect();
	oscOutPortTip.displayPoint = ofVec2f(oscOutPortTip.sourceRect.x, 55);
	tooltips.push_back(oscOutPortTip);


	for(int i = 0; i < tooltips.size(); i++){
		tooltips[i].debugColor = ofColor::fromHsb(ofRandom(255), ofRandom(255,200), ofRandom(255,200));
	}
}

void DurationController::drawTooltips(){

	ofVec2f mousepoint(ofGetMouseX(), ofGetMouseY());
	for(int i = 0; i < tooltips.size(); i++){
		if(tooltips[i].sourceRect.inside(mousepoint)){
			tooltipFont.drawString(tooltips[i].text,
								   tooltips[i].displayPoint.x,
								   tooltips[i].displayPoint.y);
		}
	}
}

void DurationController::drawTooltipDebug(){
	//draw tool tip position finder
	tooltipFont.drawString("("+ofToString(ofGetMouseX())+","+ofToString(ofGetMouseY())+")", ofGetMouseX(), ofGetMouseY());
	//draw tooltip debug balloons
	ofPushStyle();
	for(int i = 0; i < tooltips.size(); i++){
		ofNoFill();
		ofSetColor(tooltips[i].debugColor, 200);
		ofRect(tooltips[i].sourceRect.x,tooltips[i].sourceRect.y,tooltips[i].sourceRect.width,tooltips[i].sourceRect.height);
		ofLine(ofPoint(tooltips[i].sourceRect.getMaxX(),tooltips[i].sourceRect.getMaxX()), tooltips[i].displayPoint);
		ofFill();
		ofSetColor(tooltips[i].debugColor, 50);
		ofRect(tooltips[i].sourceRect.x,tooltips[i].sourceRect.y,tooltips[i].sourceRect.width,tooltips[i].sourceRect.height);
		ofSetColor(255);
		tooltipFont.drawString(tooltips[i].text, tooltips[i].sourceRect.x+5,tooltips[i].sourceRect.y+10);
	}
	ofPopStyle();
}

void DurationController::exit(ofEventArgs& e){
	lock();
	timeline.removeFromThread();
	headers.clear();
	timeline.reset();
	unlock();

	ofLogNotice("DurationController") << "waiting for thread on exit";
	waitForThread(true);
}
