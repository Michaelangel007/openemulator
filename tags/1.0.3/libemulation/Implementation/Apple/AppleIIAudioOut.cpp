
/**
 * libemulator
 * Apple II Audio Output
 * (C) 2010-2012 by Marc S. Ressl (mressl@umich.edu)
 * Released under the GPL
 *
 * Controls Apple II audio output
 */

#include "AppleIIAudioOut.h"

AppleIIAudioOut::AppleIIAudioOut() : Audio1Bit()
{
    floatingBus = NULL;
}

bool AppleIIAudioOut::setRef(string name, OEComponent *id)
{
	if (name == "floatingBus")
		floatingBus = id;
	else
		return Audio1Bit::setRef(name, id);
	
	return true;
}

bool AppleIIAudioOut::init()
{
    if (!Audio1Bit::init())
        return false;
    
    if (!floatingBus)
    {
        logMessage("floatingBus not connected");
        
        return false;
    }
    
    return true;
}

OEChar AppleIIAudioOut::read(OEAddress address)
{
    toggleAudioOutput();
    
	return floatingBus->read(address);
}

void AppleIIAudioOut::write(OEAddress address, OEChar value)
{
    toggleAudioOutput();
}