
/**
 * OpenEmulator
 * Mac OS X Document
 * (C) 2009-2011 by Marc S. Ressl (mressl@umich.edu)
 * Released under the GPL
 *
 * Handles an emulation.
 */

#import <Cocoa/Cocoa.h>

#define USER_TEMPLATES_FOLDER @"~/Library/Application Support/OpenEmulator/Templates"

@class EmulationWindowController;

@interface Document : NSDocument
{
	void *emulation;
	
	EmulationWindowController *emulationWindowController;
	
	NSMutableArray *canvases;
	NSMutableArray *canvasWindowControllers;
}

- (id)initWithTemplateURL:(NSURL *)templateURL error:(NSError **)outError;
- (IBAction)saveDocumentAsTemplate:(id)sender;
- (void)showEmulation:(id)sender;
- (void)showCanvas:(void *)canvas;

- (void)createEmulation:(NSURL *)url;
- (void)destroyEmulation;
- (void)lockEmulation;
- (void)unlockEmulation;
- (void *)emulation;

- (BOOL)isMountable:(NSString *)path;
- (BOOL)mount:(NSString *)path;
- (BOOL)testMount:(NSString *)path;

@end
