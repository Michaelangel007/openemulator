
/**
 * OpenEmulator
 * Mac OS X Chooser Controller
 * (C) 2009 by Marc S. Ressl (mressl@umich.edu)
 * Released under the GPL
 *
 * Implements an chooser controller for template and device choosing.
 */

#import "ChooserController.h"

#import "OEParser.h"

#import "TemplateChooser.h"
#import "ChooserItem.h"
#import "Document.h"

@implementation ChooserController

- (id) init
{
	self = [super initWithNibName:@"Chooser" bundle:nil];
	
	if (self)
		groups = [[NSMutableDictionary alloc] init];
	
	return self;
}

- (id) initWithTemplates
{
	self = [self init];
	
	if (self)
	{
		NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
		NSString *templatesPath = [resourcePath
								   stringByAppendingPathComponent:@"templates"];
		[self addTemplatesFromPath:templatesPath
						 groupName:nil];
	
		[self updateUserTemplates];
	}
	
	return self;
}

- (id) initWithDevices
{
	self = [self init];
	
	if (self)
	{
		NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
		NSString *templatesPath = [resourcePath
								   stringByAppendingPathComponent:@"templates"];
		[self addTemplatesFromPath:templatesPath
						 groupName:nil];
	}
	
	return self;
}

- (void) dealloc
{
	[groupNames release];
	[groups release];
	
	if (selectedGroup)
		[selectedGroup release];
	
	if (fOutlineView)
		[fOutlineView release];
	if (fImageBrowserView)
		[fImageBrowserView release];
	
	[super dealloc];
}

- (void) loadView
{
	NSSize aSize;
	aSize.width = 96;
	aSize.height = 64;
	NSDictionary *attrs = [NSDictionary dictionaryWithObjectsAndKeys:
						   [NSFont messageFontOfSize:11.0f], NSFontAttributeName,
						   [NSColor blackColor], NSForegroundColorAttributeName,
						   nil];
	NSDictionary *hAttrs = [NSDictionary dictionaryWithObjectsAndKeys:
							[NSFont messageFontOfSize:11.0f], NSFontAttributeName,
							[NSColor whiteColor], NSForegroundColorAttributeName,
							nil];	
	[fImageBrowserView setAllowsEmptySelection:NO];
	[fImageBrowserView setAllowsMultipleSelection:NO];
	[fImageBrowserView setCellSize:aSize];
	[fImageBrowserView setValue:attrs
						 forKey:IKImageBrowserCellsTitleAttributesKey];
	[fImageBrowserView setValue:hAttrs
						 forKey:IKImageBrowserCellsHighlightedTitleAttributesKey];
}

- (void) setDelegate:(id)theDelegate
{
	chooserDelegate = theDelegate;
}

- (void) updateUserTemplates
{
	NSString *selectedItemPath = [self selectedItemPath];
	
	NSString *userTemplatesGroupName = NSLocalizedString(@"My Templates",
														 @"My Templates");
	NSMutableArray *userTemplatesArray = [groups objectForKey:userTemplatesGroupName];
	if (userTemplatesArray)
		[groups removeObjectForKey:userTemplatesGroupName];
	
	NSString *userTemplatesPath = [TEMPLATE_FOLDER stringByExpandingTildeInPath];
	[self addTemplatesFromPath:userTemplatesPath
					 groupName:userTemplatesGroupName];
	
	[self selectItemWithItemPath:selectedItemPath];
}

- (void) addTemplatesFromPath:(NSString *) path
					groupName:(NSString *) theGroupName
{
	NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
	NSString *imagesPath = [resourcePath
							stringByAppendingPathComponent:@"images"];
	
	NSError *error;
	NSArray *templateFilenames = [[NSFileManager defaultManager]
								  contentsOfDirectoryAtPath:path
								  error:&error];
	
	int templatesCount = [templateFilenames count];
	for (int i = 0; i < templatesCount; i++)
	{
		NSString *templateFilename = [templateFilenames objectAtIndex:i];
		NSString *templatePath = [path stringByAppendingPathComponent:templateFilename];
		string templatePathString = string([templatePath UTF8String]);
		OEParser parser(templatePathString);
		if (!parser.isOpen())
			continue;
		
		NSString *label = [templateFilename stringByDeletingPathExtension];
		
		OEDMLInfo *dmlInfo = parser.getDMLInfo();
		NSString *imageName = [NSString stringWithUTF8String:dmlInfo->image.c_str()];
		NSString *description = [NSString stringWithUTF8String:dmlInfo->image.c_str()];
		NSString *groupName = [NSString stringWithUTF8String:dmlInfo->group.c_str()];
		
		if (theGroupName)
			groupName = theGroupName;
		
		if (![groups objectForKey:groupName])
		{
			NSMutableArray *group = [[[NSMutableArray alloc] init] autorelease];
			[groups setObject:group forKey:groupName];
		}
		NSString *imagePath = [imagesPath stringByAppendingPathComponent:imageName];
		ChooserItem *item = [[ChooserItem alloc] initWithItem:templatePath
														label:label
													imagePath:imagePath
												  description:description];
		if (item)
			[item autorelease];
		
		[[groups objectForKey:groupName] addObject:item];
	}
	
	if (groupNames)
		[groupNames release];
	
	groupNames = [NSMutableArray arrayWithArray:[[groups allKeys]
												 sortedArrayUsingSelector:
												 @selector(compare:)]];
	[groupNames retain];
}

- (id) outlineView:(NSOutlineView *) outlineView child:(NSInteger) index ofItem:(id) item
{
	if (!item)
		return [groupNames objectAtIndex:index];
	
	return nil;
}

- (BOOL) outlineView:(NSOutlineView *) outlineView isItemExpandable:(id) item
{
	return NO;
}

- (NSInteger) outlineView:(NSOutlineView *) outlineView numberOfChildrenOfItem:(id) item
{
	if (!item)
		return [groupNames count];
	
	return 0;
}

- (id)outlineView: (NSOutlineView *) outlineView
objectValueForTableColumn:(NSTableColumn *) tableColumn
		   byItem:(id) item
{
	return item;
}

- (void) outlineViewSelectionDidChange:(NSNotification *) notification
{
	if (selectedGroup)
	{
		[selectedGroup release];
		selectedGroup = nil;
	}
	
	int row = [fOutlineView selectedRow];
	if (row != -1)
	{
		selectedGroup = [groupNames objectAtIndex:row];
		[selectedGroup retain];
	}
	
	[fImageBrowserView reloadData];
	[fImageBrowserView setSelectionIndexes:[NSIndexSet indexSetWithIndex:0]
					  byExtendingSelection:NO];
}

- (NSUInteger) numberOfItemsInImageBrowser:(IKImageBrowserView *) aBrowser
{
	if (!selectedGroup)
		return 0;
	
	return [[groups objectForKey:selectedGroup] count];
}

- (id) imageBrowser:(IKImageBrowserView *) aBrowser itemAtIndex:(NSUInteger) index
{
	if (!selectedGroup)
		return nil;
	
	return [[groups objectForKey:selectedGroup] objectAtIndex:index];
}

- (void) imageBrowser:(IKImageBrowserView *) aBrowser
cellWasDoubleClickedAtIndex:(NSUInteger) index
{
	if ([chooserDelegate respondsToSelector:
		 @selector(chooserWasDoubleClicked:)])
		[chooserDelegate chooserWasDoubleClicked:self];
}

- (void) selectItemWithItemPath:(NSString *) itemPath
{
	int groupIndex = 0;
	int chooserIndex = 0;
	
	int groupsCount = [groupNames count];
	for (int i = 0; i < groupsCount; i++)
	{
		NSString *groupName = [groupNames objectAtIndex:i];
		NSArray *templates = [groups objectForKey:groupName];
		
		int templatesCount = [templates count]; 
		for (int j = 0; j < templatesCount; j++)
		{
			ChooserItem *item = [templates objectAtIndex:j];
			if ([[item itemPath] compare:itemPath] == NSOrderedSame)
			{
				groupIndex = i;
				chooserIndex = j;
			}
		}
	}
	
	[fOutlineView selectRowIndexes:[NSIndexSet indexSet]
			  byExtendingSelection:NO];
	[fOutlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:groupIndex]
			  byExtendingSelection:NO];
	
	[fImageBrowserView reloadData];
	[fImageBrowserView setSelectionIndexes:[NSIndexSet indexSetWithIndex:chooserIndex]
				 byExtendingSelection:NO];
	[fImageBrowserView scrollIndexToVisible:chooserIndex];
}

- (NSString *) selectedItemPath
{
	int index = [[fImageBrowserView selectionIndexes] firstIndex];
	if (index == NSNotFound)
		return nil;
	
	ChooserItem *item = [self imageBrowser:fImageBrowserView
							   itemAtIndex:index];
	
	return [[[item itemPath] copy] autorelease];
}

@end