
/**
 * libemulator
 * Generic memory offset logic
 * (C) 2010 by Marc S. Ressl (mressl@umich.edu)
 * Released under the GPL
 *
 * Controls a generic memory offset logic
 */

#include "OEComponent.h"

class MemoryOffset : public OEComponent
{
public:
	MemoryOffset();
	
	int ioctl(int message, void *data);
	int read(int address);
	void write(int address, int value);
	
private:
	OEMemoryRange mappedRange;
	
	int offset;
	
	OEComponent *component;
};