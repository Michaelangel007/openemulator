
/**
 * libemulation
 * MOS KIM-1 I/O
 * (C) 2010 by Marc S. Ressl (mressl@umich.edu)
 * Released under the GPL
 *
 * Implements MOS KIM-1 input/output
 */

#include "MOSKIM1IO.h"

#include "OEEmulation.h"
#include "HostCanvasInterface.h"
#include "RS232Interface.h"

MOSKIM1IO::MOSKIM1IO()
{
	emulation = NULL;
	serialPort = NULL;
	audioOut = NULL;
	audioIn = NULL;
	
	view = NULL;
	
	canvas = NULL;
}

MOSKIM1IO::~MOSKIM1IO()
{
	delete view;
}

bool MOSKIM1IO::setValue(string name, string value)
{
	return false;
}

bool MOSKIM1IO::getValue(string name, string &value)
{
	if (name == "windowFrame")
	{
		if (canvas)
			canvas->postMessage(this,
								HOST_CANVAS_GET_WINDOWFRAME,
								&windowFrame);
	}
	else
		return false;
	
	return true;
}

bool MOSKIM1IO::setRef(string name, OEComponent *ref)
{
	if (name == "emulation")
	{
		if (emulation)
			emulation->postMessage(this,
								   EMULATION_REMOVE_CANVAS,
								   &canvas);
		if (ref)
			ref->postMessage(this,
							 EMULATION_ADD_CANVAS,
							 &canvas);
		emulation = ref;
	}
	else if (name == "serialPort")
	{
		replaceObserver(serialPort, ref, RS232_DID_RECEIVE_DATA);
		serialPort = ref;
	}
	else if (name == "audioOut")
		audioOut = ref;
	else if (name == "audioIn")
		audioIn = ref;
	else
		return false;
	
	return true;
}

bool MOSKIM1IO::setData(string name, OEData *data)
{
	if (name == "view")
		view = data;
	else
		return OEComponent::setData(name, data);
	
	return true;
}

bool MOSKIM1IO::init()
{
	if (canvas)
	{
		canvas->postMessage(this,
							HOST_CANVAS_SET_WINDOWFRAME,
							&windowFrame);
		canvas->postMessage(this,
							HOST_CANVAS_SET_DEFAULTWINDOWSIZE,
							&defaultWindowSize);
	}
	
	return true;
}

void MOSKIM1IO::notify(OEComponent *sender, int notification, void *data)
{
}
