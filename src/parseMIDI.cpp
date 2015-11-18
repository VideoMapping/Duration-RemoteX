#include "DurationController.h"

#ifdef WITH_MIDI


void DurationController::newMidiMessage(ofxMidiMessage& msg) {


	midiMessage = msg;

    if(bMidiHotkeyCoupling && midiHotkeyPressed >= 0)
    {
        if(midiHotkeyMessages.size()>0 && midiHotkeyMessages.size() == midiHotkeyKeys.size())
        {
        for(int i=0; i < midiHotkeyMessages.size(); i++)
        {
            if(midiHotkeyKeys[i] == midiHotkeyPressed)
            {
                midiHotkeyKeys.erase(midiHotkeyKeys.begin()+i);
                midiHotkeyMessages.erase(midiHotkeyMessages.begin()+i);
            }
        }
        }
        midiHotkeyMessages.push_back(midiMessage);
        midiHotkeyKeys.push_back(midiHotkeyPressed);
        midiHotkeyPressed = -1;
        bMidiHotkeyCoupling = false;
        bMidiHotkeyLearning = false;
        return;
    }

	if(midiHotkeyMessages.size()>0 && midiHotkeyMessages.size() == midiHotkeyKeys.size())
    {
        for(int i=0; i < midiHotkeyMessages.size(); i++)
        {
            ofxMidiMessage midiControl = midiHotkeyMessages[i];
            if(midiMessage.velocity >0 && midiMessage.status == midiControl.status && midiMessage.pitch == midiControl.pitch && midiMessage.channel == midiControl.channel)
            {
                keyPressed(midiHotkeyKeys[i]);
            }
        }
    }

}


#endif
