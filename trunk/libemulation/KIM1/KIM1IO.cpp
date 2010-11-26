
/**
 * libemulation
 * KIM-1 I/O
 * (C) 2010 by Marc S. Ressl (mressl@umich.edu)
 * Released under the GPL
 *
 * Implements KIM-1 input/output
 */

#include "KIM1IO.h"

#include "HostEmulationControllerInterface.h"
#include "HostCanvasInterface.h"
#include "RS232Interface.h"

KIM1IO::KIM1IO()
{
	hostEmulationController = NULL;
	serialPort = NULL;
	audioOut = NULL;
	audioIn = NULL;
	
	view = NULL;
	
	canvas = NULL;
}

KIM1IO::~KIM1IO()
{
	delete view;
}

bool KIM1IO::setValue(string name, string value)
{
	return false;
}

bool KIM1IO::getValue(string name, string &value)
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

bool KIM1IO::setRef(string name, OEComponent *ref)
{
	if (name == "hostEmulationController")
	{
		if (hostEmulationController)
			hostEmulationController->postMessage(this,
												 HOST_EMULATIONCONTROLLER_REMOVE_CANVAS,
												 &canvas);
		if (ref)
			ref->postMessage(this,
							 HOST_EMULATIONCONTROLLER_ADD_CANVAS,
							 &canvas);
		hostEmulationController = ref;
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

bool KIM1IO::setData(string name, OEData *data)
{
	if (name == "view")
		view = data;
	else
		return OEComponent::setData(name, data);
	
	return true;
}

bool KIM1IO::init()
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

void KIM1IO::notify(OEComponent *sender, int notification, void *data)
{
}