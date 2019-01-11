// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "UnrealPakViewer.h"
#include "HAL/ExceptionHandling.h"
#include "Mac/MacPlatformCrashContext.h"
#include "Mac/MacPlatformProcess.h"
#include "Mac/CocoaThread.h"
#include "HAL/PlatformApplicationMisc.h"

static FString GSavedCommandLine;

@interface UE4AppDelegate : NSObject <NSApplicationDelegate, NSFileManagerDelegate>
{
}

@end

@implementation UE4AppDelegate

//handler for the quit apple event used by the Dock menu
- (void)handleQuitEvent:(NSAppleEventDescriptor*)Event withReplyEvent:(NSAppleEventDescriptor*)ReplyEvent
{
	[self requestQuit:self];
}

- (IBAction)requestQuit:(id)Sender
{
	GIsRequestingExit = true;
}

- (void) runGameThread:(id)Arg
{
	RunUnrealPakViewer(*GSavedCommandLine);
	
	[NSApp terminate: self];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)Sender;
{
	if(!GIsRequestingExit || ([NSThread gameThread] && [NSThread gameThread] != [NSThread mainThread]))
	{
		[self requestQuit:self];
		return NSTerminateLater;
	}
	else
	{
		return NSTerminateNow;
	}
}

- (void)applicationDidFinishLaunching:(NSNotification *)Notification
{
	//install the custom quit event handler
	NSAppleEventManager* appleEventManager = [NSAppleEventManager sharedAppleEventManager];
	[appleEventManager setEventHandler:self andSelector:@selector(handleQuitEvent:withReplyEvent:) forEventClass:kCoreEventClass andEventID:kAEQuitApplication];
	
	FPlatformApplicationMisc::ActivateApplication();
	RunGameThread(self, @selector(runGameThread:));
}

@end

int main(int argc, char *argv[])
{
	for (int32 Option = 1; Option < argc; Option++)
	{
		GSavedCommandLine += TEXT(" ");
		FString Argument(ANSI_TO_TCHAR(argv[Option]));
		if (Argument.Contains(TEXT(" ")))
		{
			if (Argument.Contains(TEXT("=")))
			{
				FString ArgName;
				FString ArgValue;
				Argument.Split( TEXT("="), &ArgName, &ArgValue );
				Argument = FString::Printf( TEXT("%s=\"%s\""), *ArgName, *ArgValue );
			}
			else
			{
				Argument = FString::Printf(TEXT("\"%s\""), *Argument);
			}
		}

		GSavedCommandLine += Argument;
	}
	
	SCOPED_AUTORELEASE_POOL;
	[NSApplication sharedApplication];
	[NSApp setDelegate:[UE4AppDelegate new]];
	[NSApp run];
	return 0;
}
