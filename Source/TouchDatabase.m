//
//  TouchDatabase.m
//  TouchDB
//
//  Created by Jens Alfke on 6/17/12.
//  Copyright (c) 2012 Couchbase, Inc. All rights reserved.
//

#import "TouchDBPrivate.h"
#import "TDCache.h"


#define kDocRetainLimit 50

NSString* const kTouchDatabaseChangeNotification = @"TouchDatabaseChange";


@implementation TouchDatabase


@synthesize tddb=_tddb, manager=_manager;


- (id) initWithManager: (TouchDatabaseManager*)manager
            TDDatabase: (TDDatabase*)tddb
{
    self = [super init];
    if (self) {
        _manager = [manager retain];
        _tddb = [tddb retain];
        [[NSNotificationCenter defaultCenter] addObserver: self
                                                 selector: @selector(tddbNotification:) name: nil object: tddb];
    }
    return self;
}


- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [_modelFactory release];
    _tddb.touchDatabase = nil;
    [_tddb release];
    [_manager release];
    [super dealloc];
}


- (void) tddbNotification: (NSNotification*)n {
    if ([n.name isEqualToString: TDDatabaseChangeNotification]) {
        TDRevision* rev = [n.userInfo objectForKey: @"rev"];
        NSURL* source = [n.userInfo objectForKey: @"source"];
        [[self cachedDocumentWithID: rev.docID] revisionAdded: rev source: source];

        // Post a database-changed notification, but only post one per runloop cycle by using
        // a notification queue. If the current notification has the "external" flag, make sure it
        // gets posted by clearing any pending instance of the notification that doesn't have the flag.
        NSDictionary* userInfo = source ? $dict({@"external", $true}) : nil;
        NSNotification* n = [NSNotification notificationWithName: kTouchDatabaseChangeNotification
                                                          object: self
                                                        userInfo: userInfo];
        NSNotificationQueue* queue = [NSNotificationQueue defaultQueue];
        if (source != nil)
            [queue dequeueNotificationsMatching: n coalesceMask: NSNotificationCoalescingOnSender];
        [queue enqueueNotification: n
                      postingStyle: NSPostASAP 
                      coalesceMask: NSNotificationCoalescingOnSender
                          forModes: [NSArray arrayWithObject: NSRunLoopCommonModes]];
    }
}


- (NSString*) name {
    return _tddb.name;
}


- (BOOL) inTransaction: (BOOL(^)(void))block {
    return 200 == [_tddb inTransaction: ^TDStatus {
        return block() ? 200 : 999;
    }];
}


- (BOOL) create: (NSError**)outError {
    return [_tddb open: outError];
}


#pragma mark - DOCUMENTS:


- (TouchDocument*) documentWithID: (NSString*)docID {
    TouchDocument* doc = (TouchDocument*) [_docCache resourceWithCacheKey: docID];
    if (!doc) {
        if (docID.length == 0)
            return nil;
        doc = [[[TouchDocument alloc] initWithDatabase: self documentID: docID] autorelease];
        if (!doc)
            return nil;
        if (!_docCache)
            _docCache = [[TDCache alloc] initWithRetainLimit: kDocRetainLimit];
        [_docCache addResource: doc];
    }
    return doc;
}


- (TouchDocument*) untitledDocument {
    return [self documentWithID: [TDDatabase generateDocumentID]];
}


- (TouchDocument*) cachedDocumentWithID: (NSString*)docID {
    return (TouchDocument*) [_docCache resourceWithCacheKey: docID];
}


- (void) clearDocumentCache {
    [_docCache forgetAllResources];
}


- (TouchQuery*) queryAllDocuments {
    return [[[TouchQuery alloc] initWithDatabase: self view: nil] autorelease];
}


#pragma mark - VIEWS:


- (TouchView*) viewNamed: (NSString*)name {
    TDView* view = [_tddb viewNamed: name];
    return view ? [[[TouchView alloc] initWithDatabase: self view: view] autorelease] : nil;
}


- (NSArray*) allViews {
    return [_tddb.allViews my_map:^id(TDView* view) {
        return [[[TouchView alloc] initWithDatabase: self view: view] autorelease];
    }];
}


#pragma mark - VALIDATION & FILTERS:


- (void) defineValidation: (NSString*)validationName asBlock: (TDValidationBlock)validationBlock {
    [_tddb defineValidation: validationName asBlock: validationBlock];
}


- (TDValidationBlock) validationNamed: (NSString*)validationName {
    return [_tddb validationNamed: validationName];
}


- (void) defineFilter: (NSString*)filterName asBlock: (TDFilterBlock)filterBlock {
    [_tddb defineFilter: filterName asBlock: filterBlock];
}


- (TDFilterBlock) filterNamed: (NSString*)filterName {
    return [_tddb filterNamed: filterName];
}


#pragma mark - REPLICATION:


- (TouchReplication*) pushToURL: (NSURL*)url {
    return [_manager replicationWithDatabase: self remote: url pull: NO create: YES];
}

- (TouchReplication*) pullFromURL: (NSURL*)url {
    return [_manager replicationWithDatabase: self remote: url pull: YES create: YES];
}

- (NSArray*) replicateWithURL: (NSURL*)otherDbURL exclusively: (bool)exclusively {
    return [_manager createReplicationsBetween: self and: otherDbURL exclusively: exclusively];
}


@end
