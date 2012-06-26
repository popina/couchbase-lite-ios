//
//  TDQuery.h
//  TouchDB
//
//  Created by Jens Alfke on 6/18/12.
//  Copyright (c) 2012 Couchbase, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@class TDView, TouchDatabase, TouchDocument;
@class TouchLiveQuery, TouchQueryEnumerator, TouchQueryRow;


/** Options for TDQuery.stale property, to allow out-of-date results to be returned. */
typedef enum {
    kTDStaleNever,           /**< Never return stale view results (default) */
    kTDStaleOK,              /**< Return stale results as long as view is already populated */
    kTDStaleUpdateAfter      /**< Return stale results, then update view afterwards */
} TDStaleness;


/** Represents a TouchDB 'view', or a view-like resource like _all_documents. */
@interface TouchQuery : NSObject
{
    @private
    TouchDatabase* _database;
    TDView* _view;              // nil for _all_docs query
    NSUInteger _limit, _skip;
    id _startKey, _endKey;
    NSString* _startKeyDocID;
    NSString* _endKeyDocID;
    TDStaleness _stale;
    BOOL _descending, _prefetch, _sequences;
    NSArray *_keys;
    NSUInteger _groupLevel;
    SInt64 _lastSequence;
}

/** The database that contains this view. */
@property (readonly) TouchDatabase* database;

/** The maximum number of rows to return. Default value is 0, meaning 'unlimited'. */
@property NSUInteger limit;

/** The number of initial rows to skip. Default value is 0.
    Should only be used with small values. For efficient paging, use startkey and limit.*/
@property NSUInteger skip;

/** Should the rows be returned in descending key order? Default value is NO. */
@property BOOL descending;

/** If non-nil, the key value to start at. */
@property (copy) id startKey;

/** If non-nil, the key value to end after. */
@property (copy) id endKey;

/** If non-nil, the document ID to start at. 
    (Useful if the view contains multiple identical keys, making .startKey ambiguous.) */
@property (copy) NSString* startKeyDocID;

/** If non-nil, the document ID to end at. 
    (Useful if the view contains multiple identical keys, making .endKey ambiguous.) */
@property (copy) NSString* endKeyDocID;

/** If set, allows faster results at the expense of returning possibly out-of-date data. */
@property TDStaleness stale;

/** If non-nil, the query will fetch only the rows with the given keys. */
@property (copy) NSArray* keys;

/** If non-zero, enables grouping of results, in views that have reduce functions. */
@property NSUInteger groupLevel;

/** If set to YES, the results will include the entire document contents of the associated rows.
    These can be accessed via TDQueryRow's -documentContents property. */
@property BOOL prefetch;

@property BOOL sequences;


/** Sends the query to the server and returns an enumerator over the result rows (Synchronous). */
- (TouchQueryEnumerator*) rows;

/** Same as -rows, except returns nil if the query results have not changed since the last time it was evaluated (Synchronous). */
- (TouchQueryEnumerator*) rowsIfChanged;


/** Returns a live query with the same parameters. */
- (TouchLiveQuery*) asLiveQuery;

@end


/** A TDQuery subclass that automatically refreshes the result rows every time the database changes.
    All you need to do is watch for changes to the .rows property. */
@interface TouchLiveQuery : TouchQuery
{
    @private
    BOOL _observing, _updating;
    TouchQueryEnumerator* _rows;
}

/** In TDLiveQuery the -rows accessor is now a non-blocking property that can be observed using KVO. Its value will be nil until the initial query finishes. */
@property (readonly, retain) TouchQueryEnumerator* rows;

@end


/** Enumerator on a TDQuery's result rows.
    The objects returned are instances of TDQueryRow. */
@interface TouchQueryEnumerator : NSEnumerator <NSCopying>
{
    @private
    TouchDatabase* _database;
    NSArray* _rows;
    NSUInteger _totalCount;
    NSUInteger _nextRow;
    NSUInteger _sequenceNumber;
}

/** The number of rows returned in this enumerator */
@property (readonly) NSUInteger count;

/** The total number of rows in the query (excluding options like limit, skip, etc.) */
@property (readonly) NSUInteger totalCount;

/** The database's current sequenceNumber at the time the view was generated. */
@property (readonly) NSUInteger sequenceNumber;

/** The next result row. This is the same as -nextObject but with a checked return type. */
- (TouchQueryRow*) nextRow;

/** Random access to a row in the result */
- (TouchQueryRow*) rowAtIndex: (NSUInteger)index;

@end


/** A result row from a TouchDB view query. */
@interface TouchQueryRow : NSObject
{
    @private
    TouchDatabase* _database;
    id _result;
}

@property (readonly) id key;
@property (readonly) id value;

/** The ID of the document described by this view row.
    (This is not necessarily the same as the document that caused this row to be emitted; see the discussion of the .sourceDocumentID property for details.) */
@property (readonly) NSString* documentID;

/** The ID of the document that caused this view row to be emitted.
    This is the value of the "id" property of the JSON view row.
    It will be the same as the .documentID property, unless the map function caused a related document to be linked by adding an "_id" key to the emitted value; in this case .documentID will refer to the linked document, while sourceDocumentID always refers to the original document. */
@property (readonly) NSString* sourceDocumentID;

/** The revision ID of the document this row was mapped from. */
@property (readonly) NSString* documentRevision;

/** The document this row was mapped from.
    This will be nil if a grouping was enabled in the query, because then the result rows don't correspond to individual documents. */
@property (readonly) TouchDocument* document;

/** The properties of the document this row was mapped from.
    To get this, you must have set the -prefetch property on the query; else this will be nil. */
@property (readonly) NSDictionary* documentProperties;

/** If this row's key is an array, returns the item at that index in the array.
    If the key is not an array, index=0 will return the key itself.
    If the index is out of range, returns nil. */
- (id) keyAtIndex: (NSUInteger)index;

/** Convenience for use in keypaths. Returns the key at the given index. */
@property (readonly) id key0, key1, key2, key3;

/** The local sequence number of the associated doc/revision.
    Valid only if the 'sequences' and 'prefetch' properties were set in the query; otherwise returns 0. */
@property (readonly) UInt64 localSequence;
@end
