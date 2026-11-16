// Squelchmoth - macOS window helpers (compiled on Apple platforms only).
#import <Cocoa/Cocoa.h>

extern "C" void sqmMakeFloatingPanel (void* nsViewHandle, bool floating)
{
    if (nsViewHandle == nullptr) return;
    NSView* view = (NSView*) nsViewHandle;
    NSWindow* window = [view window];
    if (window == nil) return;

    // Like Pro Tools' floating windows (Transport, Memory Locations, plug-in windows...): always above the
    // Edit/Mix window while Squelchmoth is in front, hidden while another app is in front.
    [window setLevel: floating ? NSFloatingWindowLevel : NSNormalWindowLevel];
    [window setHidesOnDeactivate: floating ? YES : NO];
    if (floating)
        [window setCollectionBehavior: [window collectionBehavior] | NSWindowCollectionBehaviorFullScreenAuxiliary];
}

extern "C" void sqmActivateApp()
{
    [NSApp activateIgnoringOtherApps: YES];
}
