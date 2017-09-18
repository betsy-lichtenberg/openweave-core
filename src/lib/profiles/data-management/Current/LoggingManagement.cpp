/*
 *
 *    Copyright (c) 2015-2017 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */
// __STDC_FORMAT_MACROS must be defined for PRIX64 to be defined for pre-C++11 clib
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif // __STDC_FORMAT_MACROS

// it is important for this first inclusion of inttypes.h to have all the right switches turned ON
#include <inttypes.h>

#define WEAVE_CONFIG_BDX_NAMESPACE kWeaveManagedNamespace_Development

#include <Weave/Profiles/data-management/Current/WdmManagedNamespace.h>
#include <Weave/Profiles/data-management/DataManagement.h>

#include <Weave/Profiles/bulk-data-transfer/Development/BulkDataTransfer.h>
#include <Weave/Profiles/bulk-data-transfer/Development/BDXMessages.h>

#include <SystemLayer/SystemTimer.h>

#if HAVE_NEW
#include <new>
#else
inline void * operator new     (size_t, void * p) throw() { return p; }
inline void * operator new[]   (size_t, void * p) throw() { return p; }
#endif // HAVE_NEW

using namespace nl::Weave::TLV;

namespace nl {
namespace Weave {
namespace Profiles {
namespace WeaveMakeManagedNamespaceIdentifier(DataManagement, kWeaveManagedNamespaceDesignation_Current) {

// Static instance: embedded platforms not always implement a proper
// C++ runtime; instead, the instance is initialized via placement new
// in CreateLoggingManangement.

static LoggingManagement sInstance;

LoggingManagement & LoggingManagement::GetInstance(void)
{
    return sInstance;
}

struct ReclaimEventCtx
{
    CircularEventBuffer *mEventBuffer;
    size_t mSpaceNeededForEvent;
};

WEAVE_ERROR LoggingManagement::AlwaysFail(nl::Weave::TLV::WeaveCircularTLVBuffer &inBuffer, void * inAppData, nl::Weave::TLV::TLVReader & inReader)
{
    return WEAVE_ERROR_NO_MEMORY;
}

WEAVE_ERROR LoggingManagement::CopyToNextBuffer(CircularEventBuffer *inEventBuffer)
{
    CircularTLVWriter writer;
    CircularTLVReader reader;
    WeaveCircularTLVBuffer checkpoint = inEventBuffer->mNext->mBuffer;
    WeaveCircularTLVBuffer *nextBuffer = &(inEventBuffer->mNext->mBuffer);
    WEAVE_ERROR err;

    // Set up the next buffer s.t. it fails if needs to evict an element
    nextBuffer->mProcessEvictedElement = AlwaysFail;

    writer.Init(nextBuffer);

    // Set up the reader s.t. it is positioned to read the head event
    reader.Init(&(inEventBuffer->mBuffer));

    err = reader.Next();
    SuccessOrExit(err);

    err = writer.CopyElement(reader);
    SuccessOrExit(err);

    err = writer.Finalize();
    SuccessOrExit(err);

exit:
    if (err != WEAVE_NO_ERROR)
    {
        inEventBuffer->mNext->mBuffer = checkpoint;
    }
    return err;
}

WEAVE_ERROR LoggingManagement::EnsureSpace(size_t inRequiredSpace)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    size_t requiredSpace = inRequiredSpace;
    CircularEventBuffer *eventBuffer = mEventBuffer;
    WeaveCircularTLVBuffer *circularBuffer;
    ReclaimEventCtx ctx;

    // check whether we actually need to do anything, exit if we don't
    VerifyOrExit(requiredSpace > eventBuffer->mBuffer.AvailableDataLength(), err = WEAVE_NO_ERROR);

    while (true)
    {
        circularBuffer = &(eventBuffer->mBuffer);
        // check that the request can ultimately be satisfied.
        VerifyOrExit(requiredSpace <= circularBuffer->GetQueueSize(), err = WEAVE_ERROR_BUFFER_TOO_SMALL);

        if (requiredSpace > circularBuffer->AvailableDataLength())
        {
            ctx.mEventBuffer = eventBuffer;
            ctx.mSpaceNeededForEvent = 0;

            circularBuffer->mProcessEvictedElement = EvictEvent;
            circularBuffer->mAppData = &ctx;
            err = circularBuffer->EvictHead();

            // one of two things happened: either the element was evicted,
            // or we figured out how much space we need to evict it into
            // the next buffer

            if (err != WEAVE_NO_ERROR)
            {
                VerifyOrExit(ctx.mSpaceNeededForEvent != 0, /* no-op, return err */);
                if (ctx.mSpaceNeededForEvent <= eventBuffer->mNext->mBuffer.AvailableDataLength())
                {
                    // we can copy the event outright.  copy event and
                    // subsequently evict head s.t. evicting the head
                    // element always succeeds.
                    // Since we're calling CopyElement and we've checked
                    // that there is space in the next buffer, we don't expect
                    // this to fail.
                    err = CopyToNextBuffer(eventBuffer);
                    SuccessOrExit(err);

                    // success; evict head unconditionally
                    circularBuffer->mProcessEvictedElement = NULL;
                    err = circularBuffer->EvictHead();
                    // if unconditional eviction failed, this
                    // means that we have no way of further
                    // clearing the buffer.  fail out and let the
                    // caller know that we could not honor the
                    // request
                    SuccessOrExit(err);
                    continue;
                }
                // we cannot copy event outright. We remember the
                // current required space in mAppData, we note the
                // space requirements for the event in the current
                // buffer and make that space in the next buffer.
                circularBuffer->mAppData = reinterpret_cast<void*>(requiredSpace);
                eventBuffer = eventBuffer->mNext;

                // Sanity check: Die here on null event buffer.  If
                // eventBuffer->mNext were null, then the `EvictBuffer`
                // in line 130 would have succeeded -- the event was
                // already in the final buffer.
                VerifyOrDie(eventBuffer != NULL);

                requiredSpace = ctx.mSpaceNeededForEvent;
            }
        }
        else
        {
            if (eventBuffer == mEventBuffer)
                break;
            eventBuffer = eventBuffer->mPrev;
            requiredSpace = reinterpret_cast<size_t>(eventBuffer->mBuffer.mAppData);
            err = WEAVE_NO_ERROR;
        }
    }

    // On exit, configure the top-level s.t. it will always fail to evict an element
    mEventBuffer->mBuffer.mProcessEvictedElement = AlwaysFail;
    mEventBuffer->mBuffer.mAppData = NULL;

exit:
    return err;
}


/**
 * @brief Helper function for writing event header and data according to event
 *   logging protocol.
 *
 * @param aContext[inout]   EventLoadOutContext, initialized with stateful
 *                          information for the buffer. State is updated
 *                          and preserved by BlitEvent using this context.
 *
 * @param inSchema[in]      Schema defining importance, profile ID, and
 *                          structure type of this event.
 *
 * @param inEventWriter[in] The callback to invoke to serialize the event data.
 *
 * @param inAppData[in]     Application context for the callback.
 *
 * @param inOptions[in]     EventOptions describing timestamp and other tags
 *                          relevant to this event.
 *
 */
WEAVE_ERROR LoggingManagement::BlitEvent(EventLoadOutContext *aContext,
                                         const EventSchema &inSchema,
                                         EventWriterFunct inEventWriter,
                                         void *inAppData,
                                         const EventOptions *inOptions)
{

    WEAVE_ERROR err = WEAVE_NO_ERROR;
    TLVWriter checkpoint = aContext->mWriter;
    TLVType containerType;

    VerifyOrExit(aContext->mCurrentEventID >= aContext->mStartingEventID,
                 /* no-op: don't write event, but advance current event ID */);

    VerifyOrExit(inOptions != NULL, err = WEAVE_ERROR_INVALID_ARGUMENT);
    VerifyOrExit(inOptions->timestampType != kTimestampType_Invalid,
                 err = WEAVE_ERROR_INVALID_ARGUMENT);

    err = aContext->mWriter.StartContainer(AnonymousTag, kTLVType_Structure, containerType);
    SuccessOrExit(err);

    // Event metadata

    // Importance
    err = aContext->mWriter.Put(ContextTag(kTag_EventImportance), static_cast<uint16_t>(inSchema.mImportance));
    SuccessOrExit(err);

    // If mFirst, record event ID
    if (aContext->mFirst)
    {
        err = aContext->mWriter.Put(ContextTag(kTag_EventID), aContext->mCurrentEventID);
        SuccessOrExit(err);
    }

    // Related Event processing
    if (inOptions->relatedEventID != 0)
    {
        err = aContext->mWriter.Put(ContextTag(kTag_RelatedEventImportance), static_cast<uint16_t>(inOptions->relatedImportance));
        SuccessOrExit(err);

        err = aContext->mWriter.Put(ContextTag(kTag_RelatedEventID), inOptions->relatedEventID);
        SuccessOrExit(err);
    }

    // if mFirst, record absolute time
    if (aContext->mFirst)
    {
#if WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
        if (inOptions->timestampType == kTimestampType_UTC)
        {
            err = aContext->mWriter.Put(ContextTag(kTag_EventUTCTimestamp),
                               inOptions->timestamp.utcTimestamp);
            SuccessOrExit(err);
        }
        else
#endif // WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
        {
            err = aContext->mWriter.Put(ContextTag(kTag_EventSystemTimestamp),
                               inOptions->timestamp.systemTimestamp);
            SuccessOrExit(err);
        }
    }
    // else record delta
    else
    {
#if WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
        if (inOptions->timestampType == kTimestampType_UTC)
        {
            int64_t deltatime = inOptions->timestamp.utcTimestamp - aContext->mCurrentUTCTime;
            err = aContext->mWriter.Put(ContextTag(kTag_EventDeltaUTCTime), deltatime);
            SuccessOrExit(err);
        }
        else
#endif // WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
        {
            int32_t deltatime = inOptions->timestamp.systemTimestamp - aContext->mCurrentTime;
            err = aContext->mWriter.Put(ContextTag(kTag_EventDeltaSystemTime), deltatime);
            SuccessOrExit(err);
        }
    }

    // Event Trait Profile ID
    if (inSchema.mMinCompatibleDataSchemaVersion != 1 || inSchema.mDataSchemaVersion != 1) {
        TLV::TLVType type;

        err = aContext->mWriter.StartContainer(ContextTag(kTag_EventTraitProfileID), kTLVType_Array, type);
        SuccessOrExit(err);

        err = aContext->mWriter.Put(TLV::AnonymousTag, inSchema.mProfileId);
        SuccessOrExit(err);

        if (inSchema.mDataSchemaVersion != 1) {
            err = aContext->mWriter.Put(TLV::AnonymousTag, inSchema.mDataSchemaVersion);
            SuccessOrExit(err);
        }

        if (inSchema.mMinCompatibleDataSchemaVersion != 1) {
            err = aContext->mWriter.Put(TLV::AnonymousTag, inSchema.mMinCompatibleDataSchemaVersion);
            SuccessOrExit(err);
        }

        err = aContext->mWriter.EndContainer(type);
        SuccessOrExit(err);
    }
    else {
        err = aContext->mWriter.Put(ContextTag(kTag_EventTraitProfileID), inSchema.mProfileId);
        SuccessOrExit(err);
    }

    // Event resource
    if (inOptions->eventSource != NULL)
    {
        if (mExchangeMgr != NULL)
        {
            if (inOptions->eventSource->ResourceID != mExchangeMgr->FabricState->LocalNodeId)
            {
                err = aContext->mWriter.Put(ContextTag(kTag_EventResourceID),
                                 inOptions->eventSource->ResourceID);
                SuccessOrExit(err);
            }

            err = aContext->mWriter.Put(ContextTag(kTag_EventTraitInstanceID),
                             inOptions->eventSource->TraitInstanceID);
            SuccessOrExit(err);
        }
    }

    // Event Type (aka Event Message ID)
    err = aContext->mWriter.Put(ContextTag(kTag_EventType), inSchema.mStructureType);
    SuccessOrExit(err);

    // Callback to write the EventData
    err = inEventWriter(aContext->mWriter, kTag_EventData, inAppData);
    SuccessOrExit(err);

    err = aContext->mWriter.EndContainer(containerType);
    SuccessOrExit(err);

    err = aContext->mWriter.Finalize();
    SuccessOrExit(err);

    // only update mFirst if an event was successfully written.
    if (aContext->mFirst)
    {
        aContext->mFirst = false;
    }

exit:
    if (err != WEAVE_NO_ERROR)
    {
        aContext->mWriter = checkpoint;
    }
    else
    {
        // update these variables since BlitEvent can be used to track the
        // state of a set of events over multiple calls.
        aContext->mCurrentEventID++;
#if WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
        if (inOptions->timestampType == kTimestampType_UTC)
        {
            aContext->mCurrentUTCTime = inOptions->timestamp.utcTimestamp;
        }
        else
#endif // WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
        {
            aContext->mCurrentTime = inOptions->timestamp.systemTimestamp;
        }
    }
    return err;
}

/**
 * @brief Create and initialize the logging management buffers. Must
 *   be called prior to the logging being used.
 *
 * @param inMgr[in]         WeaveExchangeManager to be used with this logging subsystem
 *
 * @param inNumBuffers[in]  Number of buffers to use for event storage
 *
 * @param inBufferLengths[in] Description of inBufferLengths
 *
 * @param inBuffers[in]     The buffers to use for actual event logging.
 */
void LoggingManagement::CreateLoggingManagement(nl::Weave::WeaveExchangeManager *inMgr,
                                                size_t inNumBuffers, size_t *inBufferLengths, void **inBuffers,
                                                nl::Weave::Platform::PersistedStorage::Key *inCounterKeys,
                                                const uint32_t *inCounterEpochs, nl::Weave::PersistedCounter **inCounterStorage)
{
    new (&sInstance)LoggingManagement(inMgr, inNumBuffers, inBufferLengths, inBuffers, inCounterKeys, inCounterEpochs, inCounterStorage);
}

/**
 * @brief Create and initialize the logging management buffers. Must
 *   be called prior to the logging being used.
 *
 * @param inMgr[in]         WeaveExchangeManager to be used with this logging subsystem.
 *
 * @param inNumBuffers[in]  Number of buffers to use for event storage.
 *
 * @param inBufferLengths[in] Description of inBufferLengths.
 *
 * @param inBuffers[in]     The buffers to use for actual event logging.
 *
 * @param nWeaveCounter[in] The array of counter pointers must contain the initialized counters, and has to contain inNumBuffers of counters.
 */
void LoggingManagement::CreateLoggingManagement(nl::Weave::WeaveExchangeManager *inMgr,
                                                size_t inNumBuffers, size_t *inBufferLengths, void **inBuffers,
                                                nl::Weave::MonotonicallyIncreasingCounter **nWeaveCounter
                                                )
{
    new (&sInstance)LoggingManagement(inMgr, inNumBuffers, inBufferLengths, inBuffers, nWeaveCounter);
}

/**
 * @brief Perform any actions we need to on shutdown.
 */
void LoggingManagement::DestroyLoggingManagement(void)
{
    Platform::CriticalSectionEnter();
    sInstance.mState = kLoggingManagementState_Shutdown;
    sInstance.mEventBuffer = NULL;
    Platform::CriticalSectionExit();
}

/**
 * @brief Set the WeaveExchangeManager to be used with this logging subsystem.  On some
 *   platforms, this may need to happen separately from CreateLoggingManagement() above.
 *
 * @param inMgr[in]         WeaveExchangeManager to be used with this logging subsystem
 */
WEAVE_ERROR LoggingManagement::SetExchangeManager(nl::Weave::WeaveExchangeManager *inMgr)
{
    mExchangeMgr = inMgr;
    return WEAVE_NO_ERROR;
}

/**
 * @brief
 *   LoggingManagement constructor
 *
 * For prioritization to work correctly, inBuffers must be incrementally
 * increasing in priority.
 *
 * @param inMgr[in]         WeaveExchangeManager to be used with this logging subsystem
 *
 * @param inNumBuffers[in]  Number of buffers to use for event storage
 *
 * @param inBufferLengths[in] Description of inBufferLengths
 *
 * @paran inBuffers[in]     The buffers to use for actual event logging.
 *
 * @return LoggingManagement
 */
LoggingManagement::LoggingManagement(WeaveExchangeManager *inMgr,
                                     size_t inNumBuffers,
                                     size_t *inBufferLengths,
                                     void **inBuffers,
                                     nl::Weave::Platform::PersistedStorage::Key *inCounterKeys,
                                     const uint32_t *inCounterEpochs,
                                     nl::Weave::PersistedCounter **inCounterStorage)
{
    CircularEventBuffer *current = NULL;
    CircularEventBuffer *prev = NULL;
    CircularEventBuffer *next = NULL;
    size_t i;

    mThrottled = 0;
    mExchangeMgr = inMgr;

    for (i = 0; i < inNumBuffers; i++)
    {
        next = ((i+1) < inNumBuffers) ?
            static_cast<CircularEventBuffer *>(inBuffers[i+1]) :
            NULL;

        new (inBuffers[i]) CircularEventBuffer (static_cast<uint8_t*>(inBuffers[i]) + sizeof(CircularEventBuffer),
                                                inBufferLengths[i] - sizeof(CircularEventBuffer),
                                                prev,
                                                next);

        current = prev = static_cast<CircularEventBuffer *>(inBuffers[i]);
        current->mBuffer.mProcessEvictedElement = AlwaysFail;
        current->mBuffer.mAppData = NULL;
        current->mImportance = static_cast<ImportanceType> (inNumBuffers - i);

        if ((inCounterStorage != NULL) && (inCounterStorage[i] != NULL))
        {
            // We have been provided storage for a counter for this importance level.
            new (inCounterStorage[i]) PersistedCounter();
            WEAVE_ERROR err = inCounterStorage[i]->Init(inCounterKeys[i], inCounterEpochs[i]);
            if (err != WEAVE_NO_ERROR)
            {
                WeaveLogError(EventLogging, "%s inCounterStorage[%d]->Init() failed with %d", __FUNCTION__, i, err);
            }
            current->mEventIdCounter = inCounterStorage[i];
        }
        else
        {
            // No counter has been provided, so we'll use our
            // "built-in" non-persisted counter.
            current->mEventIdCounter = &(current->mNonPersistedCounter);
        }

        current->mFirstEventID = current->mEventIdCounter->GetValue();
    }
    mEventBuffer = static_cast<CircularEventBuffer *> (inBuffers[0]);

    mState = kLoggingManagementState_Idle;
    mBDXUploader = NULL;
    mBytesWritten = 0;
    mUploadRequested = false;
    mMaxImportanceBuffer = static_cast<ImportanceType>(inNumBuffers);
}

        /**
 * @brief
 *   LoggingManagement constructor
 *
 * For prioritization to work correctly, inBuffers must be incrementally
 * increasing in priority.
 *
 * @param inMgr[in]         WeaveExchangeManager to be used with this logging subsystem.
 *
 * @param inNumBuffers[in]  Number of buffers to use for event storage.
 *
 * @param inBufferLengths[in] Description of inBufferLengths.
 *
 * @paran inBuffers[in]     The buffers to use for actual event logging.
 *
 * @param nWeaveCounter[in] The array of counter pointers must contain the initialized counters, and has to contain inNumBuffers of counters.
 * @return LoggingManagement
 */
LoggingManagement::LoggingManagement(WeaveExchangeManager *inMgr,
                                     size_t inNumBuffers,
                                     size_t *inBufferLengths,
                                     void **inBuffers,
                                     nl::Weave::MonotonicallyIncreasingCounter **nWeaveCounter)
{
    CircularEventBuffer *current = NULL;
    CircularEventBuffer *prev = NULL;
    CircularEventBuffer *next = NULL;
    size_t i;

    mThrottled = 0;
    mExchangeMgr = inMgr;

    for (i = 0; i < inNumBuffers; i++)
    {
        next = ((i+1) < inNumBuffers) ?
               static_cast<CircularEventBuffer *>(inBuffers[i+1]) :
               NULL;

        new (inBuffers[i]) CircularEventBuffer (static_cast<uint8_t*>(inBuffers[i]) + sizeof(CircularEventBuffer),
                                                inBufferLengths[i] - sizeof(CircularEventBuffer),
                                                prev,
                                                next);

        current = prev = static_cast<CircularEventBuffer *>(inBuffers[i]);
        current->mBuffer.mProcessEvictedElement = AlwaysFail;
        current->mBuffer.mAppData = NULL;
        current->mImportance = static_cast<ImportanceType> (inNumBuffers - i);
        current->mEventIdCounter = nWeaveCounter[i];
        current->mFirstEventID = current->mEventIdCounter->GetValue();
    }

    mEventBuffer = static_cast<CircularEventBuffer *> (inBuffers[0]);

    mState = kLoggingManagementState_Idle;
    mBDXUploader = NULL;
    mBytesWritten = 0;
    mUploadRequested = false;
    mMaxImportanceBuffer = static_cast<ImportanceType>(inNumBuffers);
}
/**
 * @brief
 *   LoggingManagement default constructor. Provided primarily to make the compiler happy.
 *

 * @return LoggingManagement
 */
LoggingManagement::LoggingManagement(void):
    mEventBuffer(NULL),
    mExchangeMgr(NULL),
    mState(kLoggingManagementState_Idle),
    mBDXUploader(NULL),
    mBytesWritten(0),
    mThrottled(0),
    mMaxImportanceBuffer(kImportanceType_Invalid),
    mUploadRequested(false)
{
}

/**
 * @brief
 *   A function to get the current importance of a profile.
 *
 * The function returns the current importance of a profile as
 * currently configured in the #LoggingConfiguration trait.  When
 * per-profile importance is supported, it is used; otherwise only
 * global importance is supported.  When the log is throttled, we only
 * record the Production events
 *
 * @param profileId Profile against which the event is being logged
 *
 * @return Importance of the current profile based on the config
 */
ImportanceType LoggingManagement::GetCurrentImportance(uint32_t profileId)
{
    const LoggingConfiguration &config = LoggingConfiguration::GetInstance();
    ImportanceType retval;

    if (mThrottled != 0)
    {
        retval = Production;
    }
    else if (config.SupportsPerProfileImportance())
    {
        retval = config.GetProfileImportance(profileId);
    }
    else
    {
        retval = config.mGlobalImportance;
    }
    return (retval < mMaxImportanceBuffer ? retval : mMaxImportanceBuffer);
}

/**
 * @brief
 *   Get the max available importance of the system.
 *
 *  This function returns the max importance stored by logging management,
 *  as defined by both the global importance and number of buffers available
 *
 * @return ImportanceType Max importance type currently stored.
 */
ImportanceType LoggingManagement::GetMaxImportance(void)
{
    const LoggingConfiguration &config = LoggingConfiguration::GetInstance();
    return (config.mGlobalImportance < mMaxImportanceBuffer ? config.mGlobalImportance : mMaxImportanceBuffer);
}


/**
 * @brief
 *   Allocate a new event ID based on the event importance, and advance the counter
 *   if we have one.
 *
 * @param inImportance Event importance
 *
 * @return event_id_t Event ID for this importance.
 */
event_id_t CircularEventBuffer::VendEventID(void)
{
    event_id_t retval = 0;
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    // Assign event ID to the buffer's counter's value.
    retval = mEventIdCounter->GetValue();
    mLastEventID = static_cast<event_id_t>(retval);

    // Now advance the counter.
    err = mEventIdCounter->Advance();
    if (err != WEAVE_NO_ERROR)
    {
        WeaveLogError(EventLogging, "%s Advance() for importance %d failed with %d", __FUNCTION__, mImportance, err);
    }

    return retval;
}

/**
 * @brief
 *   Fetch the most recently vended ID for a particular importance level
 *
 * @param inImportance Importance level
 *
 * @return event_id_t most recently vended event ID for that event importance
 */
event_id_t LoggingManagement::GetLastEventID(ImportanceType inImportance)
{
    return GetImportanceBuffer(inImportance)->mLastEventID;
}

/**
 * @brief
 *   Fetch the most recently vended ID for a particular importance level
 *
 * @param inImportance Importance level
 *
 * @return event_id_t most recently vended event ID for that event importance
 */
event_id_t LoggingManagement::GetFirstEventID(ImportanceType inImportance)
{
    return GetImportanceBuffer(inImportance)->mFirstEventID;
}

CircularEventBuffer *LoggingManagement::GetImportanceBuffer(ImportanceType inImportance) const
{
    CircularEventBuffer *buf = mEventBuffer;
    while (!buf->IsFinalDestinationForImportance(inImportance))
    {
        buf = buf->mNext;
    }
    return buf;
}

/**
 * @brief
 *   The public API for registering a set of externally stored events.
 *
 * Register a callback of form #FetchExternalEventsFunct. This API requires
 * the platform to know the number of events on registration. The internal
 * workings also require this number to be constant. Since this API
 * does not allow the platform to register specific event IDs, this prevents
 * the platform from persisting storage of events (at least with unique event
 * IDs).
 *
 * The callback will be called whenever a subscriber attempts to fetch event IDs
 * within the range any number of times until it is unregistered.
 *
 * This variant of the function should be used when the external
 * provider does not care to be notified when the external events have
 * been delivered.
 *
 * The pointer to the ExternalEvents struct will be NULL on failure, otherwise
 * will be populated with start and end event IDs assigned to the callback.
 * This pointer should be used to unregister the set of events.
 *
 * See the documentation for #FetchExternalEventsFunct for details on what
 * the callback must implement.
 *
 * @param inImportance          Importance level
 *
 * @param inCallback            Callback to register to fetch external events
 *
 * @param inNumEvents           Number of events in this set
 *
 * @param aExternalEventsPtr    Pointer to ExternalEvents struct.
 *
 * @return WEAVE_ERROR_NO_MEMORY        If no more callback slots are available.
 *
 * @return WEAVE_ERROR_INVALID_ARGUMENT Null function callback or no events to register.
 *
 * @return WEAVE_NO_ERROR               On success.
 */
WEAVE_ERROR LoggingManagement::RegisterEventCallbackForImportance(ImportanceType inImportance,
        FetchExternalEventsFunct inCallback,
        size_t inNumEvents,
        ExternalEvents **aExternalEventsPtr)
{
    return GetImportanceBuffer(inImportance)->RegisterExternalEventsCallback(inCallback, NULL, inNumEvents, aExternalEventsPtr);
}

/**
 * @brief
 *   The public API for registering a set of externally stored events.
 *
 * Register a callback of form #FetchExternalEventsFunct. This API requires
 * the platform to know the number of events on registration. The internal
 * workings also require this number to be constant. Since this API
 * does not allow the platform to register specific event IDs, this prevents
 * the platform from persisting storage of events (at least with unique event
 * IDs).
 *
 * The callback will be called whenever a subscriber attempts to fetch event IDs
 * within the range any number of times until it is unregistered.
 *
 * This variant of the function should be used when the external
 * provider wants to be notified when the events have been delivered
 * to a subscriber.  When the events are delivered, the external
 * provider is notified that about the node ID of the recipient and
 * last event delivered to that recipient.  Note that the external
 * provider may be notified several times for the same event ID.
 * There are no specific restrictions on the handler, in particular,
 * the handler may unregister the external event IDs.
 *
 * The pointer to the ExternalEvents struct will be NULL on failure, otherwise
 * will be populated with start and end event IDs assigned to the callback.
 * This pointer should be used to unregister the set of events.
 *
 * See the documentation for #FetchExternalEventsFunct for details on what
 * the callback must implement.
 *
 * @param inImportance          Importance level
 *
 * @param inFetchCallback       Callback to register to fetch external events
 *
 * @param inNotifyCallback      Callback to register for delivery notification
 *
 * @param inNumEvents           Number of events in this set
 *
 * @param aExternalEventsPtr    Pointer to ExternalEvents struct.
 *
 * @return WEAVE_ERROR_NO_MEMORY        If no more callback slots are available.
 *
 * @return WEAVE_ERROR_INVALID_ARGUMENT Null function callback or no events to register.
 *
 * @return WEAVE_NO_ERROR               On success.
 */
WEAVE_ERROR LoggingManagement::RegisterEventCallbackForImportance(ImportanceType inImportance,
        FetchExternalEventsFunct inFetchCallback,
        NotifyExternalEventsDeliveredFunct inNotifyCallback,
        size_t inNumEvents,
        ExternalEvents **aExternalEventsPtr)
{
    return GetImportanceBuffer(inImportance)->RegisterExternalEventsCallback(inFetchCallback, inNotifyCallback, inNumEvents, aExternalEventsPtr);
}


/**
 * @brief
 *   The public API for unregistering a set of externally stored events.
 *
 * Unregistering the callback will prevent LoggingManagement from calling
 * the callback for a set of events. LoggingManagement will no longer send
 * those event IDs to subscribers.
 *
 * The intent is for one function to serve a set of events at a time. If
 * a new set of events need to be registered using the same function, the
 * callback should first be unregistered, then registered again. This
 * means the original set of events can no longer be fetched.
 *
 * This function succeeds unconditionally. If the callback was never
 * registered or was already unregistered, it's a no-op.
 *
 * @param inImportance          Importance level
 * @param inPtr                 Pointer to ExternalEvents struct to unregister.
 */
void LoggingManagement::UnregisterEventCallbackForImportance(ImportanceType inImportance,
        ExternalEvents *inPtr)
{
    GetImportanceBuffer(inImportance)->UnregisterExternalEventsCallback(inPtr);
}

// Internal API used in copying an event out of the event buffers

WEAVE_ERROR LoggingManagement :: CopyAndAdjustDeltaTime(const TLVReader &aReader, size_t aDepth, void *aContext)
{
    WEAVE_ERROR err;
    CopyAndAdjustDeltaTimeContext *ctx = static_cast<CopyAndAdjustDeltaTimeContext *>(aContext);
    TLVReader reader(aReader);

    if (aReader.GetTag() == nl::Weave::TLV::ContextTag(kTag_EventDeltaSystemTime))
    {
        if (ctx->mContext->mFirst) // First event gets a timestamp, subsequent ones get a delta T
        {
            err = ctx->mWriter->Put(nl::Weave::TLV::ContextTag(kTag_EventSystemTimestamp), ctx->mContext->mCurrentTime);
        }
        else
        {
            err = ctx->mWriter->CopyElement(reader);
        }
    }
#if WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
    else if (aReader.GetTag() == ContextTag(kTag_EventDeltaUTCTime))
    {
        if (ctx->mContext->mFirstUtc)
        {
            err = ctx->mWriter->Put(ContextTag(kTag_EventUTCTimestamp), ctx->mContext->mCurrentUTCTime);
            ctx->mContext->mFirstUtc = false;
        }
        else
        {
            err = ctx->mWriter->CopyElement(reader);
        }
    }
#endif // WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
    else
    {
        err = ctx->mWriter->CopyElement(reader);
    }

    // First event in the sequence gets a eventID neatly packaged
    // right after the importance to keep tags ordered
    if (aReader.GetTag() == nl::Weave::TLV::ContextTag(kTag_EventImportance))
    {
        if (ctx->mContext->mFirst)
        {
            err = ctx->mWriter->Put(nl::Weave::TLV::ContextTag(kTag_EventID), ctx->mContext->mCurrentEventID);
        }
    }

    return err;
}

/**
 * @brief
 *   Log an event via a callback, with options.
 *
 * The function logs an event represented as an ::EventWriterFunct and
 * an app-specific `appData` context.  The function writes the event
 * metadata and calls the `inEventWriter` with an ::TLVWriter
 * reference and `inAppData` context so that the user code can emit
 * the event data directly into the event log.  This form of event
 * logging minimizes memory consumption, as event data is serialized
 * directly into the target buffer.  The event data MUST contain
 * context tags to be interpreted within the schema identified by
 * `inProfileID` and `inEventType`. The tag of the first element will be
 * ignored; the event logging system will replace it with the
 * eventData tag.
 *
 * The event is logged if the schema importance exceeds the logging
 * threshold specified in the LoggingConfiguration.  If the event's
 * importance does not meet the current threshold, it is dropped and
 * the function returns a `0` as the resulting event ID.
 *
 * This variant of the invocation permits the caller to set any
 * combination of `EventOptions`:
 * - timestamp, when 0 defaults to the current time at the point of
 *   the call,
 * - "root" section of the event source (event source and trait ID);
 *   if NULL, it defaults to the current device. the event is marked as
 *   relating to the device that is making the call,
 * - a related event ID for grouping event IDs; when the related event
 *   ID is 0, the event is marked as not relating to any other events,
 * - urgency; by default non-urgent.
 *
 * @param[in] inSchema     Schema defining importance, profile ID, and
 *                         structure type of this event.
 *
 * @param[in] inEventWriter The callback to invoke to actually
 *                         serialize the event data
 *
 * @param[in] inAppData    Application context for the callback.
 *
 * @param[in] inOptions    The options for the event metadata. May be NULL.
 *
 * @return event_id_t      The event ID if the event was written to the
 *                         log, 0 otherwise.
 */
event_id_t LoggingManagement :: LogEvent(const EventSchema &inSchema, EventWriterFunct inEventWriter, void *inAppData, const EventOptions *inOptions)
{
    event_id_t event_id = 0;

    Platform::CriticalSectionEnter();

    // Make sure we're alive.
    VerifyOrExit(mState != kLoggingManagementState_Shutdown, /* no-op */);

    event_id = LogEventPrivate(inSchema, inEventWriter, inAppData, inOptions);

exit:
    Platform::CriticalSectionExit();
    return event_id;
}


// Note: the function below must be called with the critical section
// locked, and only when the logger is not shutting down

inline event_id_t LoggingManagement :: LogEventPrivate(const EventSchema &inSchema, EventWriterFunct inEventWriter, void *inAppData, const EventOptions *inOptions)
{
    event_id_t event_id = 0;
    CircularTLVWriter writer;
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    size_t requestSize = WEAVE_CONFIG_EVENT_SIZE_RESERVE;
    bool didWriteEvent = false;
#if WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
    int32_t ev_opts_deltatime = 0;
#endif // WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
    WeaveCircularTLVBuffer checkpoint = mEventBuffer->mBuffer;
    EventLoadOutContext ctxt = EventLoadOutContext(writer,
         inSchema.mImportance, GetImportanceBuffer(inSchema.mImportance)->mLastEventID);
    EventOptions opts = EventOptions(static_cast<timestamp_t>(System::Timer::GetCurrentEpoch()));

    // check whether the entry is to be logged or discarded silently
    VerifyOrExit(inSchema.mImportance <= GetCurrentImportance(inSchema.mProfileId), /* no-op */);

    // Create all event specific data
    // Timestamp; encoded as a delta time
    if ((inOptions != NULL) && (inOptions->timestampType == kTimestampType_System))
    {
#if WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
        ev_opts_deltatime = inOptions->timestamp.systemTimestamp - opts.timestamp.systemTimestamp;
#endif
        opts.timestamp.systemTimestamp = inOptions->timestamp.systemTimestamp;
    }

    if (GetImportanceBuffer(inSchema.mImportance)->mFirstEventTimestamp == 0)
    {
        GetImportanceBuffer(inSchema.mImportance)->AddEvent(opts.timestamp.systemTimestamp);
    }


#if WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
    // UTC timestamp; encoded as a delta time
    if ((inOptions != NULL) && (inOptions->timestampType == kTimestampType_UTC))
    {
        opts.timestamp.utcTimestamp = inOptions->timestamp.utcTimestamp;
        opts.timestampType = kTimestampType_UTC;
    }
    else
    {
        // temporary until time APIs return unsigned values
        Profiles::Time::timesync_t utc_tmp;
        err = nl::Weave::Platform::Time::GetSystemTimeMs(&utc_tmp);
        if ((err == WEAVE_NO_ERROR) && (utc_tmp != 0))
        {
            opts.timestamp.utcTimestamp = static_cast<utc_timestamp_t>(static_cast<int64_t>(utc_tmp) + ev_opts_deltatime);
            opts.timestampType = kTimestampType_UTC;
        }
    }

    if ((opts.timestampType == kTimestampType_UTC) &&
            (GetImportanceBuffer(inSchema.mImportance)->mFirstEventUTCTimestamp == 0))
    {
        GetImportanceBuffer(inSchema.mImportance)->AddEventUTC(opts.timestamp.utcTimestamp);
    }
#endif // WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS

    if (inOptions != NULL)
    {
        opts.eventSource = inOptions->eventSource;
        opts.relatedEventID = inOptions->relatedEventID;
        opts.relatedImportance = inOptions->relatedImportance;
    }

    ctxt.mFirst = false;
    ctxt.mCurrentEventID = GetImportanceBuffer(inSchema.mImportance)->mLastEventID;
    ctxt.mCurrentTime = GetImportanceBuffer(inSchema.mImportance)->mLastEventTimestamp;
#if WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
    ctxt.mCurrentUTCTime = GetImportanceBuffer(inSchema.mImportance)->mLastEventUTCTimestamp;
#endif // WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS

    // Begin writing
    while (!didWriteEvent)
    {
        // Ensure we have space in the in-memory logging queues
        err = EnsureSpace(requestSize);
        SuccessOrExit(err);

        // save a checkpoint for the underlying buffer.  Note that with
        // the current event buffering scheme, only the mEventBuffer will
        // be affected by the writes to the `writer` below, and thus
        // that's the only thing we need to checkpoint.
        checkpoint = mEventBuffer->mBuffer;

        // Start the event container (anonymous structure) in the circular buffer
        writer.Init(&(mEventBuffer->mBuffer));

        err = BlitEvent(&ctxt, inSchema, inEventWriter, inAppData, &opts);

        if (err == WEAVE_ERROR_NO_MEMORY)
        {
            // try again
            err = WEAVE_NO_ERROR;
            requestSize += WEAVE_CONFIG_EVENT_SIZE_INCREMENT;
            mEventBuffer->mBuffer = checkpoint;
            continue;
        }

        didWriteEvent = true;
    }

    mBytesWritten += writer.GetLengthWritten();

exit:

    if (err != WEAVE_NO_ERROR)
    {
        mEventBuffer->mBuffer = checkpoint;
    }
    else if (inSchema.mImportance <= GetCurrentImportance(inSchema.mProfileId))
    {
        event_id = GetImportanceBuffer(inSchema.mImportance)->VendEventID();

#if WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
        if (opts.timestampType == kTimestampType_UTC)
        {
            GetImportanceBuffer(inSchema.mImportance)->AddEventUTC(opts.timestamp.utcTimestamp);

#if WEAVE_CONFIG_EVENT_LOGGING_VERBOSE_DEBUG_LOGS
            WeaveLogDetail(EventLogging, "LogEvent event id: %u importance: %u profile id: 0x%x structure id: 0x%x utc timestamp: 0x%" PRIx64 , event_id, inSchema.mImportance, inSchema.mProfileId, inSchema.mStructureType, opts.timestamp.utcTimestamp);
#endif // WEAVE_CONFIG_EVENT_LOGGING_VERBOSE_DEBUG_LOGS
        }
        else
#endif // WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
        {
            GetImportanceBuffer(inSchema.mImportance)->AddEvent(opts.timestamp.systemTimestamp);

#if WEAVE_CONFIG_EVENT_LOGGING_VERBOSE_DEBUG_LOGS
            WeaveLogDetail(EventLogging, "LogEvent event id: %u importance: %u profile id: 0x%x structure id: 0x%x sys timestamp: 0x%" PRIx32 , event_id, inSchema.mImportance, inSchema.mProfileId, inSchema.mStructureType, opts.timestamp.systemTimestamp);
#endif // WEAVE_CONFIG_EVENT_LOGGING_VERBOSE_DEBUG_LOGS
        }

        ScheduleFlushIfNeeded(inOptions == NULL ? false : inOptions->urgent);
    }

    return event_id;
}

/**
 * @brief
 *   ThrottleLogger elevates the effective logging level to the Production level.
 *
 */
void LoggingManagement::ThrottleLogger(void)
{
    WeaveLogProgress(EventLogging, "LogThrottle on");

    __sync_add_and_fetch(&mThrottled, 1);
}

/**
 * @brief
 *   UnthrottleLogger restores the effective logging level to the configured logging level.
 *
 */
void LoggingManagement::UnthrottleLogger(void)
{
    uint32_t throttled = __sync_sub_and_fetch(&mThrottled, 1);

    if (throttled == 0)
    {
        WeaveLogProgress(EventLogging, "LogThrottle off");
    }
}

// internal API, used to copy events to external buffers
WEAVE_ERROR LoggingManagement :: CopyEvent(const TLVReader & aReader, TLVWriter & aWriter, EventLoadOutContext *aContext)
{
    TLVReader reader;
    TLVType containerType;
    CopyAndAdjustDeltaTimeContext context(&aWriter, aContext);
    const bool recurse = false;
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    reader.Init(aReader);
    err = reader.EnterContainer(containerType);
    SuccessOrExit(err);

    err = aWriter.StartContainer(AnonymousTag, kTLVType_Structure, containerType);
    SuccessOrExit(err);

    err = nl::Weave::TLV::Utilities::Iterate(reader, CopyAndAdjustDeltaTime, &context, recurse);
    VerifyOrExit(err == WEAVE_NO_ERROR || err == WEAVE_END_OF_TLV, );

    err = aWriter.EndContainer(containerType);
    SuccessOrExit(err);

    err = aWriter.Finalize();

exit:
    return err;
}

/**
 * @brief
 *   Internal API used to implement #FetchEventsSince
 *
 * Iterator function to used to copy an event from the log into a
 * TLVWriter. The included aContext contains the context of the copy
 * operation, including the TLVWriter that will hold the copy of an
 * event.  If event cannot be written as a whole, the TLVWriter will
 * be rolled back to event boundary.
 *
 * @retval #WEAVE_END_OF_TLV function reached the end of the event
 *
 * @retval #WEAVE_ERROR_NO_MEMORY Function could not write a portion of
 *                          the event to the TLVWriter.
 *
 * @retval #WEAVE_ERROR_BUFFER_TOO_SMALL Function could not write a
 *                          portion of the event to the TLVWriter.
 */
WEAVE_ERROR LoggingManagement :: CopyEventsSince(const TLVReader & aReader, size_t aDepth, void *aContext)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    const bool recurse = false;
    TLVReader innerReader;
    TLVType tlvType;
    EventEnvelopeContext event;
    EventLoadOutContext *loadOutContext = static_cast<EventLoadOutContext *>(aContext);
    nl::Weave::TLV::TLVWriter   checkpoint;

#if WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS
    // TODO: CopyEventsSince using the LoggingManagement singleton is less than
    // ideal. Consider a future refactor of the Iterate call in FetchEventsSince.
    while (LoggingManagement::GetInstance().IsEventExternal(loadOutContext->mImportance, loadOutContext->mCurrentEventID))
    {
        if (loadOutContext->mCurrentEventID >= loadOutContext->mStartingEventID)
        {
            // the next event is externally handled. return to FetchEventsSince.
            err = WEAVE_END_OF_TLV;
            break;
        }
        else
        {
            // skip over
            loadOutContext->mCurrentEventID = LoggingManagement::GetInstance().GetEndOfExternalEventRange(loadOutContext->mImportance, loadOutContext->mCurrentEventID) + 1;
        }
    }
    SuccessOrExit(err);
#endif // WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS

    innerReader.Init(aReader);
    err = innerReader.EnterContainer(tlvType);
    SuccessOrExit(err);

    err = nl::Weave::TLV::Utilities::Iterate(innerReader, FetchEventParameters, &event, recurse);
    VerifyOrExit(event.mNumFieldsToRead == 0, err = WEAVE_NO_ERROR);

    err = WEAVE_NO_ERROR;

    if (event.mImportance == loadOutContext->mImportance)
    {
        loadOutContext->mCurrentTime += event.mDeltaTime;
#if WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
        loadOutContext->mCurrentUTCTime += event.mDeltaUtc;
#endif // WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
        if (loadOutContext->mCurrentEventID >= loadOutContext->mStartingEventID)
        {
            // checkpoint the writer
            checkpoint = loadOutContext->mWriter;

            err = CopyEvent(aReader, loadOutContext->mWriter, loadOutContext);

            // WEAVE_NO_ERROR and WEAVE_END_OF_TLV signify a
            // successful copy.  In all other cases, roll back the
            // writer state back to the checkpoint, i.e., the state
            // before we began the copy operation.
            VerifyOrExit((err == WEAVE_NO_ERROR) || (err == WEAVE_END_OF_TLV),
                loadOutContext->mWriter = checkpoint);

            loadOutContext->mCurrentTime = 0;
            loadOutContext->mFirst = false;
        }

        loadOutContext->mCurrentEventID++;
    }

exit:
    return err;
}

#if WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS
/**
 * @brief
 *   IsEventExternal checks whether inEventID is stored by the platform.
 *
 * @retval True if the event is stored externally.
 * @retval False otherwise.
 */
inline bool LoggingManagement::IsEventExternal(ImportanceType inImportance, event_id_t inEventID) const
{
    return (GetImportanceBuffer(inImportance)->GetExternalEventsFromEventID(inEventID) != NULL);
}

/**
 * @brief
 *   For an event ID known to be stored externally, return the last event ID in that set.
 *
 * @retval The last event ID of a group of event IDs.
 */
inline event_id_t LoggingManagement::GetEndOfExternalEventRange(ImportanceType inImportance, event_id_t inEventID) const
{
    ExternalEvents *ev = GetImportanceBuffer(inImportance)->GetExternalEventsFromEventID(inEventID);
    return ev->mLastEventID;
}
#endif // WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS

/**
 * @brief
 *   A function to retrieve events of specified importance since a specified event ID.
 *
 * Given a ::TLVWriter, an importance type, and an event ID, the
 * function will fetch events of specified importance since the
 * specified event.  The function will continue fetching events until
 * it runs out of space in the ::TLVWriter or in the log. The function
 * will terminate the event writing on event boundary.
 *
 * @param ioWriter[in]     The writer to use for event storage
 *
 * @param inImportance[in] The importance of events to be fetched
 *
 * @param ioEventID[inout] On input, the ID of the event immediately
 *                         prior to the one we're fetching.  On
 *                         completion, the ID of the last event
 *                         fetched.
 *
 * @retval #WEAVE_END_OF_TLV The function has reached the end of the
 *                         available log entries at the specified
 *                         importance level
 *
 * @retval #WEAVE_ERROR_NO_MEMORY The function ran out of space in the
 *                         ioWriter, more events in the log are
 *                         available.
 *
 * @retval #WEAVE_ERROR_BUFFER_TOO_SMALL The function ran out of space
 *                         in the ioWriter, more events in the log are
 *                         available.
 *
 */
WEAVE_ERROR LoggingManagement :: FetchEventsSince(TLVWriter &ioWriter, ImportanceType inImportance, event_id_t &ioEventID)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    const bool recurse = false;
    TLVReader reader;
    EventLoadOutContext aContext(ioWriter, inImportance, ioEventID);
    CircularEventBuffer *buf = mEventBuffer;

    Platform::CriticalSectionEnter();

    while (!buf->IsFinalDestinationForImportance(inImportance))
    {
        buf = buf->mNext;
    }

    aContext.mCurrentTime = buf->mFirstEventTimestamp;
#if WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
    aContext.mCurrentUTCTime = buf->mFirstEventUTCTimestamp;
#endif // WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
    aContext.mCurrentEventID = buf->mFirstEventID;
    err = GetEventReader(reader, inImportance);
    SuccessOrExit(err);

#if WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS
    if (IsEventExternal(inImportance, ioEventID))
    {
        ExternalEvents *ev = buf->GetExternalEventsFromEventID(ioEventID);
        aContext.mCurrentEventID = ev->mFirstEventID;

        if (ev->mFetchEventsFunct)
        {
            err = ev->mFetchEventsFunct(&aContext);
        }
        else
        {
            aContext.mCurrentEventID = ev->mLastEventID + 1;
            err = WEAVE_END_OF_TLV;
        }
    }
    else
#endif // WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS
    {
        err =  nl::Weave::TLV::Utilities::Iterate(reader, CopyEventsSince, &aContext, recurse);
    }

exit:
    ioEventID = aContext.mCurrentEventID;

    Platform::CriticalSectionExit();
    return err;
}

/**
 * @brief
 *   A helper method useful for examining the in-memory log buffers
 *
 * @param ioReader[inout] A reference to the reader that will be
 *                        initialized with the backing storage from
 *                        the event log
 *
 * @param inImportance[in] The starting importance for the reader.
 *                         Note that in this case the starting
 *                         importance is somewhat counter intuitive:
 *                         more important events share the buffers
 *                         with less important events, in addition to
 *                         their dedicated buffers.  As a result, the
 *                         reader will traverse the least data when
 *                         the Debug importance is passed in.
 *
 * @retval #WEAVE_NO_ERROR Unconditionally.
 */
WEAVE_ERROR LoggingManagement :: GetEventReader(TLVReader &ioReader, ImportanceType inImportance)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    CircularEventBuffer *buffer;
    CircularEventReader reader;
    for (buffer = mEventBuffer;
         buffer != NULL && !buffer->IsFinalDestinationForImportance(inImportance);
         buffer= buffer->mNext);
    VerifyOrExit(buffer != NULL, err = WEAVE_ERROR_INVALID_ARGUMENT);

    reader.Init(buffer);

    ioReader.Init(reader);
exit:
    return err;
}

//internal API
WEAVE_ERROR LoggingManagement :: FetchEventParameters(const TLVReader & aReader, size_t aDepth, void *aContext)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    EventEnvelopeContext * envelope = static_cast<EventEnvelopeContext *>(aContext);
    TLVReader reader;
    uint16_t extImportance; // Note: the type here matches the type case in LoggingManagement::LogEvent, importance section
    reader.Init(aReader);

    VerifyOrExit(envelope->mNumFieldsToRead > 0, err = WEAVE_END_OF_TLV);

    if (reader.GetTag() == nl::Weave::TLV::ContextTag(kTag_EventImportance))
    {
        err = reader.Get(extImportance);
        SuccessOrExit(err);
        envelope->mImportance = static_cast<ImportanceType>(extImportance);

        envelope->mNumFieldsToRead --;
    }

    if (reader.GetTag() == nl::Weave::TLV::ContextTag(kTag_EventDeltaSystemTime))
    {
        err = reader.Get(envelope->mDeltaTime);
        SuccessOrExit(err);

        envelope->mNumFieldsToRead --;
    }

#if WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
    if (reader.GetTag() == nl::Weave::TLV::ContextTag(kTag_EventDeltaUTCTime))
    {
        err = reader.Get(envelope->mDeltaUtc);
        SuccessOrExit(err);

        envelope->mNumFieldsToRead --;
    }
#endif // WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS

exit:
    return err;
}

// internal API: determine importance of an event, and the space the event requires

WEAVE_ERROR LoggingManagement :: EvictEvent(WeaveCircularTLVBuffer &inBuffer, void * inAppData, TLVReader & inReader)
{
    ReclaimEventCtx *ctx = static_cast<ReclaimEventCtx *>(inAppData);
    CircularEventBuffer *eventBuffer = ctx->mEventBuffer;
    TLVType containerType;
    EventEnvelopeContext context;
    const bool recurse = false;
    WEAVE_ERROR err;
    ImportanceType imp = kImportanceType_Invalid;

    // pull out the delta time, pull out the importance

    err = inReader.Next();
    SuccessOrExit(err);

    err = inReader.EnterContainer(containerType);
    SuccessOrExit(err);

    nl::Weave::TLV::Utilities::Iterate(inReader, FetchEventParameters, &context, recurse);

    err = inReader.ExitContainer(containerType);

    SuccessOrExit(err);

    imp = static_cast<ImportanceType>(context.mImportance);

    if (eventBuffer->IsFinalDestinationForImportance(imp))
    {
        // event is getting dropped.  Increase the eventid and first timestamp.
        eventBuffer->RemoveEvent();
        eventBuffer->mFirstEventTimestamp += context.mDeltaTime;
#if WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
        eventBuffer->mFirstEventUTCTimestamp += context.mDeltaUtc;
#endif // WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
        ctx->mSpaceNeededForEvent = 0;
    }
    else
    {
        // event is not getting dropped. Note how much space it requires, and return.
        ctx->mSpaceNeededForEvent = inReader.GetLengthRead();
        err = WEAVE_END_OF_TLV;
    }

exit:
    return err;
}

// Notes: called as a result of the timer expiration.  Main job:
// figure out whether trigger still applies, if it does, then kick off
// the upload.  If it does not, perform the appropriate backoff.

void LoggingManagement::LoggingFlushHandler(System::Layer *systemLayer, void *appState, INET_ERROR err)
{
    LoggingManagement *logger = static_cast<LoggingManagement *>(appState);
    logger->FlushHandler(systemLayer, err);
}

// FlushHandler is only called by the Weave thread. As such, guard variables
// do not need to be atomically set or checked.
void LoggingManagement::FlushHandler(System::Layer *inSystemLayer, INET_ERROR inErr)
{
#if WEAVE_CONFIG_EVENT_LOGGING_BDX_OFFLOAD
    const LoggingConfiguration &config = LoggingConfiguration::GetInstance();
#endif //WEAVE_CONFIG_EVENT_LOGGING_BDX_OFFLOAD

    switch (mState)
    {
    case kLoggingManagementState_Idle:
    {
#if WEAVE_CONFIG_EVENT_LOGGING_BDX_OFFLOAD
        // Nothing prevents a flush.  If the configuration supports
        // it, transition into "in progress" state, and kick off the
        // offload process.  If no valid upload location exists,
        // schedule an upload at the maximum upload interval
        if ((mBDXUploader != NULL) && (config.GetDestNodeId() != kAnyNodeId))
        {
            WEAVE_ERROR err;
            mState = kLoggingManagementState_InProgress;
            err = mBDXUploader->StartUpload(config.GetDestNodeId(), config.GetDestNodeIPAddress());
            if (err != WEAVE_NO_ERROR)
                WeaveLogError(EventLogging, "Failed to start BDX (err: %d)", err);
        }
        else
        {
            if (mExchangeMgr != NULL)
            {
                mExchangeMgr->MessageLayer->SystemLayer->StartTimer(config.mMaximumLogUploadInterval, LoggingFlushHandler, this);
            }
        }
#endif //WEAVE_CONFIG_EVENT_LOGGING_BDX_OFFLOAD

#if WEAVE_CONFIG_EVENT_LOGGING_WDM_OFFLOAD
        if (mExchangeMgr != NULL)
        {
            nl::Weave::Profiles::DataManagement::SubscriptionEngine::GetInstance()->GetNotificationEngine()->Run();
            mUploadRequested = false;
        }
#endif //WEAVE_CONFIG_EVENT_LOGGING_WDM_OFFLOAD

        break;
    }
    case kLoggingManagementState_Holdoff:
    {
#if WEAVE_CONFIG_EVENT_LOGGING_BDX_OFFLOAD
        mState = kLoggingManagementState_Idle;
        mUploadRequested = false;
        ScheduleFlushIfNeeded(false);
        if (mUploadRequested == false)
        {
            if (mExchangeMgr != NULL)
            {
                mExchangeMgr->MessageLayer->SystemLayer->StartTimer(config.mMaximumLogUploadInterval, LoggingFlushHandler, this);
            }
        }
#endif //WEAVE_CONFIG_EVENT_LOGGING_BDX_OFFLOAD
        break;
    }

    case kLoggingManagementState_InProgress:
    case kLoggingManagementState_Shutdown:
    {
        // should never end in these states in this function
        break;
    }

    }
}

void LoggingManagement::SignalUploadDone(void)
{
#if WEAVE_CONFIG_EVENT_LOGGING_BDX_OFFLOAD
    const LoggingConfiguration &config = LoggingConfiguration::GetInstance();
    if (mState == kLoggingManagementState_InProgress)
    {
        mState = kLoggingManagementState_Holdoff;
        if (mExchangeMgr != NULL)
        {
            mExchangeMgr->MessageLayer->SystemLayer->StartTimer(config.mMinimumLogUploadInterval, LoggingFlushHandler, this);
        }
    }
#endif //WEAVE_CONFIG_EVENT_LOGGING_BDX_OFFLOAD
}

/**
 * @brief
 *  Schedule a log offload task.
 *
 * The function decides whether to schedule a task offload process,
 * and if so, it schedules the `LoggingFlushHandler` to be run
 * asynchronously on the Weave thread.
 *
 * The decision to schedule a flush is dependent on three factors:
 *
 * -- an explicit request to flush the buffer
 *
 * -- the state of the event buffer and the amount of data not yet
 *    synchronized with the event consumers
 *
 * -- whether there is an already pending request flush request event.
 *
 * The explicit request to schedule a flush is passed via an input
 * parameter.
 *
 * The automatic flush is typically scheduled when the event buffers
 * contain enough data to merit starting a new offload.  Additional
 * triggers -- such as minimum and maximum time between offloads --
 * may also be taken into account depending on the offload strategy.
 *
 * The pending state of the event log is indicated by
 * `mUploadRequested` variable. Since this function can be called by
 * multiple threads, `mUploadRequested` must be atomically read and
 * set, to avoid scheduling a redundant `LoggingFlushHandler` before
 * the notification has been sent.
 *
 * @param inRequestFlush A boolean value indicating whether the flush
 *                       should be scheduled regardless of internal
 *                       buffer management policy.
 *
 * @retval #WEAVE_ERROR_INCORRECT_STATE LoggingManagement module was not initialized fully.
 *
 * @retval #WEAVE_NO_ERROR On success.
 */
WEAVE_ERROR LoggingManagement::ScheduleFlushIfNeeded(bool inRequestFlush)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

#if WEAVE_CONFIG_EVENT_LOGGING_BDX_OFFLOAD
    inRequestFlush |= CheckShouldRunBDX();
#endif // WEAVE_CONFIG_EVENT_LOGGING_BDX_OFFLOAD
#if WEAVE_CONFIG_EVENT_LOGGING_WDM_OFFLOAD
    inRequestFlush |= CheckShouldRunWDM();
#endif // WEAVE_CONFIG_EVENT_LOGGING_WDM_OFFLOAD

    if (inRequestFlush && __sync_bool_compare_and_swap(&mUploadRequested, false, true))
    {
        if ((mExchangeMgr != NULL) &&
            (mExchangeMgr->MessageLayer != NULL) &&
            (mExchangeMgr->MessageLayer->SystemLayer != NULL))
        {
            mExchangeMgr->MessageLayer->SystemLayer->ScheduleWork(LoggingFlushHandler, this);
        }
        else
        {
            err = WEAVE_ERROR_INCORRECT_STATE;
            mUploadRequested = false;
        }
    }

    return err;
}

#if WEAVE_CONFIG_EVENT_LOGGING_BDX_OFFLOAD
bool LoggingManagement::CheckShouldRunBDX(void)
{
    const LoggingConfiguration &config = LoggingConfiguration::GetInstance();
    return ((mBDXUploader != NULL) && ((mBytesWritten - mBDXUploader->GetUploadPosition()) > config.mUploadThreshold));
}
#endif // WEAVE_CONFIG_EVENT_LOGGING_BDX_OFFLOAD

#if WEAVE_CONFIG_EVENT_LOGGING_WDM_OFFLOAD
/**
 * @brief
 *  Decide whether to offload events based on the number of bytes in event buffers unscheduled for upload.
 *
 * The behavior of the function is controlled via
 * #WEAVE_CONFIG_EVENT_LOGGING_BYTE_THRESHOLD constant.  If the system
 * wrote more than that number of bytes since the last time a WDM
 * Notification was sent, the function will indicate it is time to
 * trigger the NotificationEngine.
 *
 * @returns true  events should be offloaded
 * @returns false otherwise
 */
bool LoggingManagement::CheckShouldRunWDM(void)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    size_t minimalBytesOffloaded = mBytesWritten;
    bool ret = false;

    // Get the minimal log position (in bytes) across all subscribers
    err = nl::Weave::Profiles::DataManagement::SubscriptionEngine::GetInstance()->GetMinEventLogPosition(minimalBytesOffloaded);
    SuccessOrExit(err);

    // return true if we can offload more than the threshold bytes to a subscription
    ret = ((minimalBytesOffloaded + WEAVE_CONFIG_EVENT_LOGGING_BYTE_THRESHOLD) < mBytesWritten);

exit:
    return ret;
}

#endif // WEAVE_CONFIG_EVENT_LOGGING_WDM_OFFLOAD

WEAVE_ERROR LoggingManagement::SetLoggingEndpoint(event_id_t *inEventEndpoints, size_t inNumImportanceLevels, size_t &outBytesOffloaded)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    CircularEventBuffer *eventBuffer = mEventBuffer;

    Platform::CriticalSectionEnter();

    outBytesOffloaded = mBytesWritten;

    while (eventBuffer != NULL && inNumImportanceLevels > 0)
    {
      if ((eventBuffer->mImportance >= kImportanceType_First) &&
          ((static_cast<size_t>(eventBuffer->mImportance - kImportanceType_First)) < inNumImportanceLevels))
        {
            inEventEndpoints[eventBuffer->mImportance - kImportanceType_First] = eventBuffer->mLastEventID;
        }
        eventBuffer = eventBuffer->mNext;
    }

    Platform::CriticalSectionExit();
    return err;
}

/**
 * @brief
 *   Get the total number of bytes written (across all event
 *   importances) to this log since its instantiation
 *
 * @returns The number of bytes written to the log.
 */
uint32_t LoggingManagement::GetBytesWritten(void) const
{
    return mBytesWritten;
}

void LoggingManagement::NotifyEventsDelivered(ImportanceType inImportance, event_id_t inLastDeliveredEventID, uint64_t inRecipientNodeID) const
{
#if WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS

    for (size_t i = 0; i < WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS; i++)
    {
        ExternalEvents *externalEvents = &(GetImportanceBuffer(inImportance)->mExternalEventsList[i]);
        if ((externalEvents->mFetchEventsFunct != NULL) && (externalEvents->mNotifyEventsDeliveredFunct != NULL))
        {
            if (inLastDeliveredEventID >= externalEvents->mFirstEventID)
            {
                event_id_t eventId = inLastDeliveredEventID;
                if (eventId > externalEvents->mLastEventID)
                {
                    eventId = externalEvents->mLastEventID;
                }

                externalEvents->mNotifyEventsDeliveredFunct(externalEvents, eventId, inRecipientNodeID);
            }
        }
    }
#endif
}

void LoggingManagement::SetBDXUploader(LogBDXUpload *inUploader)
{
    if (mBDXUploader == NULL)
    {
        mBDXUploader = inUploader;
    }
    else
    {
        WeaveLogError(EventLogging, "mBDXUploader already set");
    }
}

/**
 * @brief
 *   A constructor for the CircularEventBuffer (internal API).
 *
 * @param inBuffer[in]       The actual storage to use for event storage.
 *
 * @param inBufferLength[in] The length of the #inBuffer in bytes.
 *
 * @param inPrev[in]         The pointer to CircularEventBuffer storing
 *                           events of lesser priority.
 *
 * @param inNext[in]         The pointer to CircularEventBuffer storing
 *                           events of greater priority.
 *
 * @return CircularEventBuffer
 */
CircularEventBuffer::CircularEventBuffer(uint8_t *inBuffer, size_t inBufferLength, CircularEventBuffer *inPrev, CircularEventBuffer *inNext) :
    mBuffer(inBuffer, inBufferLength),
    mPrev(inPrev),
    mNext(inNext),
    mImportance(kImportanceType_First),
    mFirstEventID(1),
    mLastEventID(0),
    mFirstEventTimestamp(0),
    mLastEventTimestamp(0),
#if WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
    mFirstEventUTCTimestamp(0),
    mLastEventUTCTimestamp(0),
    mUTCInitialized(false),
#endif // WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
    mEventIdCounter(NULL)
{
    // TODO: hook up the platform-specific persistent event ID.
#if WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS
    for (int i = 0; i < WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS; i++)
    {
        mExternalEventsList[i] = ExternalEvents();
    }
#endif // WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS
}

/**
 * @brief
 *   A helper function that determines whether the event of
 *   specified importance is dropped from this buffer.
 *
 * @param[in]   inImportance   Importance of the event.
 *
 * @returns True if the event will be dropped from this buffer as
 * a result of queue overflow.  False if the event will be instead
 * bumped to the next queue.
 */
bool CircularEventBuffer :: IsFinalDestinationForImportance(ImportanceType inImportance) const
{
    return !((mNext != NULL) && (mNext->mImportance >= inImportance));
}

/**
 * @brief
 *   Given a timestamp of an event, compute the delta time to store in the log
 *
 * @param inEventTimestamp The event timestamp.
 *
 * @return int32_t         Time delta to encode for the event.
 */
void CircularEventBuffer :: AddEvent(timestamp_t inEventTimestamp)
{
    if (mFirstEventTimestamp == 0)
    {
        mFirstEventTimestamp = inEventTimestamp;
        mLastEventTimestamp = inEventTimestamp;
    }
    mLastEventTimestamp = inEventTimestamp;
}


#if WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
/**
 * @brief
 *   Given a timestamp of an event, compute the delta utc time to store in the log
 *
 * @param inEventTimestamp The event's utc timestamp
 *
 * @return int64_t         Time delta to encode for the event.
 */
void CircularEventBuffer :: AddEventUTC(utc_timestamp_t inEventTimestamp)
{
    if (mUTCInitialized == false)
    {
        mFirstEventUTCTimestamp = inEventTimestamp;
        mUTCInitialized = true;
    }
    mLastEventUTCTimestamp = inEventTimestamp;
}
#endif // WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS

void CircularEventBuffer :: RemoveEvent(void)
{
#if WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS
    ExternalEvents *ev;
    while ((ev = GetExternalEventsFromEventID(mFirstEventID)) != NULL)
    {
        mFirstEventID = ev->mLastEventID + 1;
        ev->mFetchEventsFunct = NULL;
    }
#endif // WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS
    mFirstEventID++;
}

/**
 * @brief
 *   This function registers a set of event IDs and a function
 *   callback for externally stored events.
 *
 * @param aFetchCallback  The function callback for unloading events.
 * @param aNotifyCallback The function callback to notify about offload
 * @param aNumEvents      Number of events to register.
 *
 * @return WEAVE_ERROR_INVALID_ARGUMENT Null function callback or no events to register.
 * @return WEAVE_ERROR_NO_MEMORY If all callbacks are in use.
 * @return WEAVE_NO_ERROR Otherwise
 */
WEAVE_ERROR CircularEventBuffer :: RegisterExternalEventsCallback(
        FetchExternalEventsFunct aFetchCallback,
        NotifyExternalEventsDeliveredFunct aNotifyCallback,
        size_t aNumEvents,
        ExternalEvents **aExternalEventsPtr)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
#if WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS
    ExternalEvents *ev;

    Platform::CriticalSectionEnter();

    VerifyOrExit(aFetchCallback != NULL, err = WEAVE_ERROR_INVALID_ARGUMENT);
    VerifyOrExit(aNumEvents > 0, err = WEAVE_ERROR_INVALID_ARGUMENT);

    ev = GetNextAvailableExternalEvents();
    VerifyOrExit(ev != NULL, err = WEAVE_ERROR_NO_MEMORY);

    ev->mFirstEventID = VendEventID();
    // need to vend event IDs in a batch.
    for (size_t i = 1; i < aNumEvents; i++)
    {
        event_id_t tmp_id = VendEventID();
        IgnoreUnusedVariable(tmp_id);
    }

    ev->mLastEventID = mLastEventID;
    ev->mFetchEventsFunct = aFetchCallback;
    ev->mNotifyEventsDeliveredFunct = aNotifyCallback;
    if (aExternalEventsPtr != NULL)
    {
        *aExternalEventsPtr = ev;
    }

exit:
    Platform::CriticalSectionExit();
#else // WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS
    err = WEAVE_ERROR_NOT_IMPLEMENTED;
#endif // WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS

    return err;
}

/**
 * @brief
 *   This function unregisters a callback.
 *
 * Once the callback is unregistered, if the event IDs are fetched by
 * a subscriber, the event logging framework will return an empty buffer.
 * If the event IDs no longer need to be fetched, the ExternalEvents
 * callback can be reused.
 *
 * @param inPtr   Pointer to ExternalEvents struct to unregister.
 */
void CircularEventBuffer :: UnregisterExternalEventsCallback(
        ExternalEvents *inPtr)
{
#if WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS
    Platform::CriticalSectionEnter();

    if (inPtr != NULL)
    {
        inPtr->mFetchEventsFunct = NULL;
        inPtr->mNotifyEventsDeliveredFunct = NULL;
    }

    Platform::CriticalSectionExit();
#endif // WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS
}

#if WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS
/**
 * @brief
 *   A function to get a pointer to the ExternalEvent storing aEventID.
 *
 * @retval NULL if aEventID is not externally stored.
 */
ExternalEvents *CircularEventBuffer :: GetExternalEventsFromEventID(
        event_id_t aEventID)
{
    uint8_t idx;
    ExternalEvents *ev = NULL;

    for (idx = 0; idx < WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS; idx++)
    {
        ev = &mExternalEventsList[idx];

        if ((aEventID >= ev->mFirstEventID) &&
            (aEventID <= ev->mLastEventID) &&
            (mFirstEventID <= ev->mFirstEventID))
        {
            break;
        }
        ev = NULL;
    }

    return ev;
}

/**
 * @brief
 *   A function to get a pointer to the next available ExternalEvent.
 *
 * A NULL pointer assigned to an ExternalEvents.mFetchEventsFunct is an
 * indication that the callback slot is unused.
 *
 * @retval NULL if all callbacks are in use.
 */
ExternalEvents *CircularEventBuffer :: GetNextAvailableExternalEvents(void)
{
    uint8_t idx;
    ExternalEvents *ev = NULL;

    for (idx = 0; idx < WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS; idx++)
    {
        ev = &mExternalEventsList[idx];

        if ((ev->mFetchEventsFunct == NULL) &&
            (ev->mLastEventID <= mFirstEventID))
        {
            break;
        }
        ev = NULL;
    }

    return ev;
}
#endif // WEAVE_CONFIG_EVENT_LOGGING_NUM_EXTERNAL_CALLBACKS

/**
 * @brief
 *   Initializes a TLVReader object backed by CircularEventBuffer
 *
 * Reading begins in the CircularTLVBuffer belonging to this
 * CircularEventBuffer.  When the reader runs out of data, it begins
 * to read from the previous CircularEventBuffer.
 *
 * @param[in] inBuf A pointer to a fully initialized CircularEventBuffer
 *
 */
void CircularEventReader::Init(CircularEventBuffer *inBuf)
{
    CircularTLVReader reader;
    CircularEventBuffer *prev;
    reader.Init(&inBuf->mBuffer);
    TLVReader::Init(reader);
    mBufHandle =  (uintptr_t)inBuf;
    GetNextBuffer = CircularEventBuffer::GetNextBufferFunct;
    for (prev = inBuf->mPrev; prev != NULL; prev = prev->mPrev)
    {
        reader.Init(&prev->mBuffer);
        mMaxLen += reader.GetRemainingLength();
    }
}

WEAVE_ERROR CircularEventBuffer :: GetNextBufferFunct(TLVReader& ioReader, uintptr_t &inBufHandle, const uint8_t *&outBufStart, uint32_t &outBufLen)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    CircularEventBuffer *buf;

    VerifyOrExit(inBufHandle != 0, err = WEAVE_ERROR_INVALID_ARGUMENT);

    buf = static_cast<CircularEventBuffer *>((void *) inBufHandle);

    err = buf->mBuffer.GetNextBuffer(ioReader, outBufStart, outBufLen);
    SuccessOrExit(err);

    if ((outBufLen == 0) && (buf->mPrev != NULL))
    {
        inBufHandle = (uintptr_t) buf->mPrev;
        outBufStart = NULL;
        err  = GetNextBufferFunct(ioReader, inBufHandle, outBufStart, outBufLen);
    }

exit:
    return err;
}

CopyAndAdjustDeltaTimeContext::CopyAndAdjustDeltaTimeContext(TLVWriter *inWriter, EventLoadOutContext *inContext) :
    mWriter(inWriter),
    mContext(inContext)
{
}

EventEnvelopeContext::EventEnvelopeContext(void) :
    mNumFieldsToRead(2), // read out importance and either system or utc delta time. events do not store both deltas.
    mDeltaTime(0),
#if WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
    mDeltaUtc(0),
#endif // WEAVE_CONFIG_EVENT_LOGGING_UTC_TIMESTAMPS
    mImportance(kImportanceType_First)
{
}

} // WeaveMakeManagedNamespaceIdentifier(DataManagement, kWeaveManagedNamespaceDesignation_Current)
} // Profiles
} // Weave
} // nl
