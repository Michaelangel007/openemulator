
/**
 * libemulation
 * Apple Graphics Tablet
 * (C) 2010-2011 by Marc S. Ressl (mressl@umich.edu)
 * Released under the GPL
 *
 * Controls an Apple Graphics Tablet.
 */

#include "AppleGraphicsTablet.h"

#include "Emulation.h"
#include "CanvasInterface.h"

AppleGraphicsTablet::AppleGraphicsTablet()
{
	emulation = NULL;
	canvas = NULL;
}

bool AppleGraphicsTablet::setValue(string name, string value)
{
	if (name == "viewPath")
		viewPath = value;
	else
		return false;
	
	return true;
}

bool AppleGraphicsTablet::setRef(string name, OEComponent *ref)
{
	if (name == "emulation")
	{
		if (emulation)
			emulation->postMessage(this,
								   EMULATION_DESTROY_CANVAS,
								   &canvas);
		emulation = ref;
		if (emulation)
			emulation->postMessage(this,
								   EMULATION_CREATE_CANVAS,
								   &canvas);
	}
	else
		return false;
	
	return true;
}

bool AppleGraphicsTablet::init()
{
	if (!emulation)
	{
		logMessage("property 'emulation' undefined");
		return false;
	}
	
	if (!canvas)
	{
		logMessage("canvas could not be created");
		return false;
	}
	else
	{
		CanvasConfiguration configuration;
		
		canvas->postMessage(this, CANVAS_LOCK, NULL);
		
		OEImage *frame;
		if (canvas->postMessage(this, CANVAS_GET_FRAME, &frame))
		{
			frame->readFile(viewPath);
			configuration.size = frame->getSize();
			canvas->postMessage(this, CANVAS_UPDATE_FRAME, NULL);
		}
		
		canvas->postMessage(this, CANVAS_CONFIGURE, &configuration);
		
		canvas->postMessage(this, CANVAS_UNLOCK, NULL);
	}
	
	return true;
}