/* -*- mode: c++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*- */

/*
 *
 *    Copyright (c) 2013-2017 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    This document is the property of Nest. It is considered
 *    confidential and proprietary information.
 *
 *    This document may not be reproduced or transmitted in any form,
 *    in whole or in part, without the express written permission of
 *    Nest.
 *
 */

/**
 *    @file
 *      This file implements Weave Alarm Profile supporting
 *      interconnected alarm functionality.
 *
 */

// __STDC_FORMAT_MACROS must be defined for PRIX64 to be defined for pre-C++11 clib
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif // __STDC_FORMAT_MACROS

// __STDC_LIMIT_MACROS must be defined for UINT8_MAX and INT32_MAX to be defined for pre-C++11 clib
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif // __STDC_LIMIT_MACROS

// __STDC_CONSTANT_MACROS must be defined for INT64_C and UINT64_C to be defined for pre-C++11 clib
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif // __STDC_CONSTANT_MACROS

// it is important for this first inclusion of inttypes.h to have all the right switches turned ON
#include <inttypes.h>

#include <Weave/Core/WeaveCore.h>
#include <Weave/Core/WeaveEncoding.h>
#include <Weave/Core/WeaveMessageLayer.h>
#include <Weave/Core/WeaveServerBase.h>

#include <Weave/Profiles/ProfileCommon.h>
#include <Weave/Profiles/common/WeaveMessage.h>
#include <Weave/Profiles/vendor/nestlabs/alarm/WeaveAlarm.h>

#include <Weave/Support/CodeUtils.h>
#include <Weave/Support/ProfileStringSupport.hpp>
#include <Weave/Support/WeaveFaultInjection.h>
#include <Weave/Support/crypto/WeaveCrypto.h>
#include <Weave/Support/logging/WeaveLogging.h>

namespace nl {
namespace Weave {
namespace Profiles {

using namespace ::nl::Weave;
using namespace ::nl::Weave::Profiles;
using namespace ::nl::Weave::Encoding;

#ifndef WEAVE_ALARM_DETAIL_LOGGING
#define WEAVE_ALARM_DETAIL_LOGGING  1
#endif // WEAVE_ALARM_DETAIL_LOGGING

#if WEAVE_ALARM_DETAIL_LOGGING && WEAVE_DETAIL_LOGGING
static void AlarmLogDetail(Alarm & aAlarm);
#else
#define AlarmLogDetail(ALARM)
#define WeaveLogDetail(MOD, MSG, ...)
#endif // WEAVE_ALARM_DETAIL_LOGGING && WEAVE_DETAIL_LOGGING

#define GetLeastSignificantByte(v) (uint8_t(v))

#define kWeave_VendorNameString_Nest "Nest"
#define kWeave_ProfileNameString_Alarm kWeave_VendorNameString_Nest ":Alarm"

// Forward Declarations

static const char *GetAlarmMessageName(uint32_t inProfileId, uint8_t inMsgType);
static const char *GetAlarmProfileName(uint32_t inProfileId);
#if !WEAVE_CONFIG_SHORT_ERROR_STR
static const char *GetAlarmStatusReportFormatString(uint32_t inProfileId, uint16_t inStatusCode);
#else
#define GetAlarmStatusReportFormatString NULL
#endif // #if !WEAVE_CONFIG_SHORT_ERROR_STR

static void _AlarmProfileStringInit(void) __attribute__((constructor));
static void _AlarmProfileStringDestroy(void) __attribute__((destructor));

// Globals

/**
 *  This structure provides storage for callbacks associated for
 *  returning human-readable support strings associated with the
 *  profile.
 */
static const Weave::Support::ProfileStringInfo sAlarmProfileStringInfo = {
    kWeaveProfile_Alarm,

    GetAlarmMessageName,
    GetAlarmProfileName,
    GetAlarmStatusReportFormatString
};

/**
 *  Context for registering and deregistering callbacks associated
 *  with for returning human-readable support strings associated with
 *  the profile.
 */
static Weave::Support::ProfileStringContext sAlarmProfileStringContext = {
    NULL,
    sAlarmProfileStringInfo
};

/**
 *  One time, yet reentrant, initializer for registering Weave Alarm
 *  profile callbacks for returning human-readable support strings
 *  associated with the profile.
 */
static void _AlarmProfileStringInit(void)
{
    (void)Weave::Support::RegisterProfileStringInfo(sAlarmProfileStringContext);
}

/**
 *  One time, yet reentrant, deinitializer for unregistering Weave Alarm
 *  profile callbacks for returning human-readable support strings
 *  associated with the profile.
 */
static void _AlarmProfileStringDestroy(void)
{
    (void)Weave::Support::UnregisterProfileStringInfo(sAlarmProfileStringContext);
}

/**
 *  @brief
 *    Callback function that returns a human-readable NULL-terminated
 *    C string describing the status code associated with this profile.
 *
 *  This callback, when registered, is invoked when a human-readable
 *  NULL-terminated C string is needed to describe the status code
 *  associated with this profile.
 *
 *  @param[in]  inProfileId   The profile identifier associated with the
 *                            specified status code.
 *
 *  @param[in]  inStatusCode  The status code for which a human-readable
 *                            descriptive string is sought.
 *
 *  @return a pointer to the NULL-terminated C string if a match is
 *  found; otherwise, NULL.
 *
 */
#if !WEAVE_CONFIG_SHORT_ERROR_STR
static const char *GetAlarmStatusReportFormatString(uint32_t inProfileId, uint16_t inStatusCode)
{
    const char *result = NULL;

    switch (inProfileId)
    {

    case kWeaveProfile_Alarm:
        switch (inStatusCode)
        {
        case kAlarmUpdateStatus_Success:
            result = "[ " kWeave_ProfileNameString_Alarm "(%08" PRIX32 "):%" PRIu16 " ] Success";
            break;

        case kAlarmUpdateStatus_Rejected:
            result = "[ " kWeave_ProfileNameString_Alarm "(%08" PRIX32 "):%" PRIu16 " ] Rejected";
            break;

        case kAlarmUpdateStatus_Invalid:
            result = "[ " kWeave_ProfileNameString_Alarm "(%08" PRIX32 "):%" PRIu16 " ] Invalid";
            break;

        case kAlarmUpdateStatus_Timeout:
            result = "[ " kWeave_ProfileNameString_Alarm "(%08" PRIX32 "):%" PRIu16 " ] Timeout";
            break;

        case kAlarmStatusReport_HushRejected_ProximityValidationFailure:
            result = "[ " kWeave_ProfileNameString_Alarm "(%08" PRIX32 "):%" PRIu16 " ] Hush rejected because proximity verification failed";
            break;

        case kAlarmStatusReport_HushRejected_SignatureValidationFailure:
            result = "[ " kWeave_ProfileNameString_Alarm "(%08" PRIX32 "):%" PRIu16 " ] Hush rejected because signature verification failed";
            break;

        default:
            result = "[ " kWeave_ProfileNameString_Alarm "(%08" PRIX32 "):%" PRIu16 " ]";
            break;

        }
        break;
    }

    return (result);
}
#endif // #if !WEAVE_CONFIG_SHORT_ERROR_STR

/**
 *  @brief
 *    Callback function that returns a human-readable NULL-terminated
 *    C string describing the message type associated with this
 *    profile.
 *
 *  This callback, when registered, is invoked when a human-readable
 *  NULL-terminated C string is needed to describe the message type
 *  associated with this profile.
 *
 *  @param[in]  inProfileId  The profile identifier associated with the
 *                           specified message type.
 *
 *  @param[in]  inMsgType    The message type for which a human-readable
 *                           descriptive string is sought.
 *
 *  @return a pointer to the NULL-terminated C string if a match is
 *  found; otherwise, NULL.
 *
 */
static const char *GetAlarmMessageName(uint32_t inProfileId, uint8_t inMsgType)
{
    const char *result = NULL;

    switch (inProfileId) {

    case kWeaveProfile_Alarm:
        switch (inMsgType) {

        case kAlarmMessageType_Alarm:
            result = "Alarm";
            break;

        case kAlarmMessageType_AlarmUpdate:
            result = "AlarmUpdate";
            break;

        case kAlarmMessageType_AlarmAck:
            result = "AlarmAck";
            break;

        case kAlarmMessageType_AlarmHushRequest:
            result = "AlarmHushRequest";
            break;

        case kAlarmMessageType_AlarmHushResponse:
            result = "AlarmHushResponse";
            break;

        }
        break;
    }

    return (result);
}

/**
 *  @brief
 *    Callback function that returns a human-readable NULL-terminated
 *    C string describing the profile with this profile.
 *
 *  This callback, when registered, is invoked when a human-readable
 *  NULL-terminated C string is needed to describe this profile.
 *
 *  @param[in]  inProfileId  The profile identifier for which a human-readable
 *                           descriptive string is sought.
 *
 *  @return a pointer to the NULL-terminated C string if a match is
 *  found; otherwise, NULL.
 *
 */
static const char *GetAlarmProfileName(uint32_t inProfileId)
{
    const char *result = NULL;

    switch (inProfileId)
    {
    case kWeaveProfile_Alarm:
        result = kWeave_ProfileNameString_Alarm;
        break;
    }

    return (result);
}

/**
 * @brief
 *   Default constructor for `Alarm` message.  The message is ready for
 *   use immediately upon completion of the constructor.
 *
 */
Alarm::Alarm(void)
{
    init();
}

/**
 * @brief
 *   Initialize the `Alarm` message object with default values.
 *
 * @return #WEAVE_NO_ERROR Unconditionally.
 */

WEAVE_ERROR Alarm::init(void)
{
    mLength = 0;
    mAlarmCtr = 0;
    mWhere = 0;
    mIsSessionIdAndExtEvtSnValid = false;
    mSessionId = 0;
    mExtEvtSeqNum = 0;
    memset(mConditions, 0, sizeof(mConditions));
    return WEAVE_NO_ERROR;
}

/**
 * @brief
 *   Equality comparison between this `Alarm` message and another `Alarm`
 *   message
 *
 * @param[in] aAlarm   A read-only reference to the other alarm to
 *                     compare against.
 *
 * @return `true` if the alarms reflect the same alarm conditions
 *         `false` otherwise.  The equality comparison considers the
 *         alarm "where" and the list of conditions only.  The alarm
 *         counter is ignored for the purposes of this comparison.
 */
bool Alarm::operator ==(const Alarm &aAlarm) const
{
    if (mWhere != aAlarm.mWhere)
    {
        return false;
    }

    for (int t = 0; t < kMaxNumAlarmConditions; t++)
    {
        if (mConditions[t] != aAlarm.mConditions[t])
            return false;
    }

    return true;
}

/**
 * @brief
 *   Retrieve a single alarm state from the list of alarms.
 *
 * @param[in] i   The index of the condition to retrieve.
 *
 * @return The alarm state at the index `i` if the index points to a valid
 *         condition index; otherwise, 0.
 */
uint8_t Alarm::GetAlarmState(uint8_t i) const
{
    if ((i >= mLength) || (i >= kMaxNumAlarmConditions))
        return 0;
    return mConditions[i] & 0x0f;
}

/**
 * @brief
 *   Retrieve a single alarm condition (source) from the list of
 *   alarms.
 *
 * @param[in] i   The index of the condition to retrieve.
 *
 * @return The alarm condition (source) at the index `i` if the index
 *         points to a valid condition index; otherwise, 0.
 */
uint8_t Alarm::GetAlarmCondition(uint8_t i) const
{
    if ((i >= mLength) || (i >= kMaxNumAlarmConditions))
        return 0;
    return mConditions[i] & 0xf0;
}


/**
 * @brief
 *   Retrieve a single alarm (source and state) from the list of
 *   alarms.
 *
 * @param[in] i   The index of the condition to retrieve.
 *
 * @return The alarm (source and state) at the index `i` if the index
 *         points to a valid condition index; otherwise, 0.
 */
uint8_t Alarm::GetAlarm(uint8_t i) const
{
    if ((i >= mLength) || (i >= kMaxNumAlarmConditions))
        return 0;
    return mConditions[i];
}


/**
 * @brief
 *   Set the alarm state for an existing alarm in the list.
 *
 * @note Parameter `i` must be a valid index into the alarm list. If `i`
 *       is outside of the valid alarm list, no action is taken.  The
 *       length of the list is unchanged by this operation.
 *
 * @param[in] i    The index of the alarm to update.
 * @param[in] val  The alarm state to set.
 */
void Alarm::SetAlarmState(uint8_t i, uint8_t val)
{
    if ((i >= mLength) || (i >= kMaxNumAlarmConditions))
        return;
    mConditions[i]= (mConditions[i] & 0xf0) | (val & 0x0f);
}

/**
 * @brief
 *   Set the alarm condition (source) for an existing alarm in the
 *   list.
 *
 * @note Parameter `i` must be a valid index into the alarm list. If `i`
 *       is outside of the valid alarm list, no action is taken.  The
 *       length of the list is unchanged by this operation.
 *
 * @param[in] i    The index of the alarm to update.
 * @param[in] val  The alarm condition (source) to set.
 */
void Alarm::SetAlarmCondition(uint8_t i, uint8_t val)
{
    if ((i >= mLength) || (i >= kMaxNumAlarmConditions))
        return;
    mConditions[i]= (mConditions[i] & 0x0f) | (val & 0xf0);
}


/**
 * @brief
 *   Determine whether the severity of the alarm merits
 *   rebroadcasting.
 *
 * @return `true` if the alarm should be rebroadcast; otherwise, `false`.
 */
bool Alarm::KeepReBroadcasting(void) const
{
    int i;
    for (i = 0; i < mLength; i++) {
        uint8_t state = GetAlarmState(i);

        // For all states OTHER then the below, keep rebroadcasting
        if (state != WEAVE_ALARM_STATE_STANDBY &&
            state != WEAVE_ALARM_STATE_SELFTEST &&
            state != WEAVE_ALARM_ANNOUNCE_HEADS_UP_1 &&
            state != WEAVE_ALARM_ANNOUNCE_HEADS_UP_2)
            return true;
    }
    return false;
}

/**
 * @brief
 *   Replace an existing alarm in the list with a new alarm (both
 *   source and state).
 *
 * @note Parameter `i` must be a valid index into the alarm list. If
 *       `i` is outside of the valid alarm list, no action is taken.
 *       The length of the list is unchanged by this operation.
 *
 * @param[in] i     The index of the alarm to update.
 * @param[in] val   The alarm value (state and source) to set.
 */
void Alarm::SetAlarm(uint8_t i, uint8_t val)
{
    if ((i >= mLength) || (i >= kMaxNumAlarmConditions))
        return;
    mConditions[i] = val;
}

/**
 * @brief
 *   Add a new alarm(source and state) to the existing list of the
 *   alarms.
 *
 * @note When the alarm list exceeds allocated storage, the method
 *       does not alter the existing list.  No error code is returned
 *       at this time.
 *
 * @param[in] val   The alarm value (source and state) to add to the
 *                  alarm list.
 */
void Alarm::AddAlarm(uint8_t val)
{
    if ((mLength + 1) < kMaxNumAlarmConditions)
        mConditions[mLength++] = val;
}


/**
 * @brief
 *   Serialize the current alarm message into the provided `PacketBuffer`.
 *
 * @param[out] aPacket   A pointer to an  `PacketBuffer` used to store the
 *                       serialized message.
 *
 * @retval #WEAVE_NO_ERROR               On success.
 *
 * @retval #WEAVE_ERROR_BUFFER_TOO_SMALL If aPacket was NULL or
 *         contained insufficient space to serialize the message.
 *
 * @retval #WEAVE_ERROR_INCORRECT_STATE if the alarm state is
 *         incorrect i.e. it contains invalid length, session ID or
 *         sequence number.
 */
WEAVE_ERROR Alarm::pack(PacketBuffer * aPacket)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    uint8_t *cursor = NULL;
    // $mLength number of bytes for condition
    const size_t necessaryDataLength = kPayloadSizeWithoutConditions_V2 + mLength;

    VerifyOrExit(aPacket != NULL, err = WEAVE_ERROR_BUFFER_TOO_SMALL);
    cursor = aPacket->Start();

    VerifyOrExit(aPacket->AvailableDataLength() >= necessaryDataLength, err = WEAVE_ERROR_BUFFER_TOO_SMALL);

    // this shall not happen, as we should always generate the session ID and
    // extended sequence number when we compose any alarm messages (instead of just duplicating)
    VerifyOrExit(mIsSessionIdAndExtEvtSnValid, err = WEAVE_ERROR_INCORRECT_STATE);

    // this shall not happen, as the original parser implementation could go wild
    // if received more than 8 alarm conditions, and the alarm class cannot hold more than that
    VerifyOrExit(mLength <= kMaxNumAlarmConditions, err = WEAVE_ERROR_INCORRECT_STATE);

    Write8(cursor, mAlarmCtr);
    Write8(cursor, mLength);
    for (int i = 0; i < mLength; ++i)
    {
        Write8(cursor, mConditions[i]);
    }

    Write8(cursor, mWhere);
    LittleEndian::Write32(cursor, mSessionId);
    LittleEndian::Write32(cursor, mExtEvtSeqNum);

    // NOTE: if this packet is going to be extended, we need a new value for necessaryDataLength

    // calculate the message length again
    aPacket->SetDataLength(cursor - aPacket->Start());

exit:
    return err;
}

/**
 * @brief
 *   Deserialize the message in the packet into an `Alarm` message.
 *
 * @param[in] aPacket   A read-only pointer to an `PacketBuffer`
 *                      containing data to be deserialized
 *
 * @param[out] aResult  A pointer to an `Alarm` message that will contain
 *                      the deserialized alarm message.  If the return
 *                      code indicates any failure, the result is
 *                      unchanged. Must not be NULL.
 *
 * @retval #WEAVE_NO_ERROR On success.
 *
 * @retval #WEAVE_ERROR_MESSAGE_INCOMPLETE The message was NULL or did
 *         not contain enough data to deserialize the message.
 *
 * @retval #WEAVE_ERROR_INVALID_MESSAGE_LENGTH The message was too
 *         long to be deserialized on this device.
 *
 * @retval #WEAVE_ERROR_INVALID_ARGUMENT The pointer to store results
 *          was NULL.
 */
WEAVE_ERROR Alarm::parse(const PacketBuffer *aPacket, Alarm *aResult)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    uint8_t numAlarmConditions = 0;
    const uint8_t *cursor = NULL;
    // start with the assumption that we're dealing with a V1 alarm message
    size_t expectedDataLength = kPayloadSizeWithoutConditions_V1;

    VerifyOrExit(aPacket != NULL, err = WEAVE_ERROR_MESSAGE_INCOMPLETE);
    VerifyOrExit(aResult != NULL, err = WEAVE_ERROR_INVALID_ARGUMENT);

    cursor = aPacket->Start();

    VerifyOrExit(aPacket->DataLength() >= expectedDataLength, err = WEAVE_ERROR_MESSAGE_INCOMPLETE);

    aResult->mAlarmCtr = Read8(cursor);
    numAlarmConditions = Read8(cursor);

    // $mLength number of bytes for condition
    expectedDataLength += size_t(numAlarmConditions);
    VerifyOrExit(aPacket->DataLength() >= expectedDataLength, err = WEAVE_ERROR_MESSAGE_INCOMPLETE);

    // fail if the message contains more than 8 conditions
    // the original implementation would parse incorrectly if there are more than 8, so
    // we'd better fail here. (this is more like a design phase issue)
    VerifyOrExit(numAlarmConditions <= kMaxNumAlarmConditions, err = WEAVE_ERROR_INVALID_MESSAGE_LENGTH);
    aResult->mLength = numAlarmConditions;

    for (uint8_t i = 0; i < numAlarmConditions; ++i)
    {
        aResult->mConditions[i] = Read8(cursor);
    }

    aResult->mWhere = Read8(cursor);

    // 4 bytes for session ID
    // 4 bytes for extended event sequence number
    expectedDataLength += (kPayloadSizeWithoutConditions_V2 - kPayloadSizeWithoutConditions_V1);

    if (aPacket->DataLength() < expectedDataLength)
    {
        //this is not an error, as maybe it's a V1
    }
    else
    {
        aResult->mSessionId = LittleEndian::Read32(cursor);
        aResult->mExtEvtSeqNum = LittleEndian::Read32(cursor);
        aResult->mIsSessionIdAndExtEvtSnValid = true;

        if (aResult->mAlarmCtr != GetLeastSignificantByte(aResult->mExtEvtSeqNum))
        {
            WeaveLogError(Alarm, "ctr <> evtSN");
        }
    }

exit:
    return err;
}

#if WEAVE_ALARM_DETAIL_LOGGING && WEAVE_DETAIL_LOGGING

static const char *AlarmSourceToString(uint8_t aCondition)
{
    switch (aCondition & 0xf0)
    {
    case WEAVE_ALARM_SMOKE:
        return "smoke";
    case WEAVE_ALARM_TEMP:
        return "temperature";
    case WEAVE_ALARM_CO:
        return "carbon monoxide";
    case WEAVE_ALARM_CH4:
        return "gas";
    case WEAVE_ALARM_HUMIDITY:
        return "humidity";
    case WEAVE_ALARM_OTHER:
        return "other";
    default:
        return "unknown";
    }
}

static const char *AlarmStateToString(uint8_t aCondition)
{
    switch (aCondition & 0x0f)
    {
    case WEAVE_ALARM_STATE_STANDBY:
        return "standby";
    case WEAVE_ALARM_STATE_HEADS_UP_1:
        return "heads up 1";
    case WEAVE_ALARM_STATE_HEADS_UP_2:
        return "heads up 2";
    case WEAVE_ALARM_STATE_HU_HUSH:
        return "heads up hush";
    case WEAVE_ALARM_STATE_ALARM_HUSHABLE:
        return "ALARM, hushable";
    case WEAVE_ALARM_STATE_ALARM_NONHUSHABLE:
        return "ALARM, NONHUSHABLE";
    case WEAVE_ALARM_STATE_ALARM_GLOBAL_HUSH:
        return "global hush";
    case WEAVE_ALARM_STATE_ALARM_REMOTE_HUSH:
        return "remote hush";
    case WEAVE_ALARM_STATE_SELFTEST:
        return "selftest";
    case WEAVE_ALARM_ANNOUNCE_HEADS_UP_1:
        return "announce heads up 1";
    case WEAVE_ALARM_ANNOUNCE_HEADS_UP_2:
        return "announce heads up 2";
    default:
        return "unknown";
    }
}

static void AlarmLogDetail(Alarm & aAlarm)
{
    for (uint8_t i = 0; i < aAlarm.mLength; ++i)
    {
        WeaveLogDetail(Alarm, "Alarm %u [%2d] 0x%02X %s: %s",
            aAlarm.mIsSessionIdAndExtEvtSnValid ? aAlarm.mExtEvtSeqNum : aAlarm.mAlarmCtr,
            i, aAlarm.GetAlarm(i),
            AlarmSourceToString(aAlarm.GetAlarm(i)), AlarmStateToString(aAlarm.GetAlarm(i)));
    }
}

#endif // WEAVE_ALARM_DETAIL_LOGGING && WEAVE_DETAIL_LOGGING

/**
 * @brief
 *   Default constructor for the `AlarmHushSignature`.
 */
AlarmHushSignature::AlarmHushSignature(void) :
    mIsSigned(false)
{
}

/**
 * @brief
 *   Equality operator for comparing two instances of `AlarmHushSignature`.
 *
 * @note The equality comparison is only meaningful for signed
 *       instances of `AlarmHushSignature`; in case where either of
 *       the operands is unsigned returns false.  As a result, the
 *       operator does not strictly meet the requirements for equality
 *       relationship.  For signed instances, function returns the
 *       results of comparison for both the key IDs and the
 *       signatures.
 *
 * param[in] rhs  A read-only reference to the `AlarmHushSignature`
 *                instance to compare against.
 *
 * @return `false` if any of the signatures are not signed, the IDs
 *         don't match, or the content doesn't match
 */
bool AlarmHushSignature::operator == (const AlarmHushSignature & rhs) const
{
    if (mIsSigned && rhs.mIsSigned && (mKeyId == rhs.mKeyId))
    {
        return (0 == memcmp(mKeyedHash, rhs.mKeyedHash, sizeof(mKeyedHash)));
    }

    // note that the comparison of two not signed signatures is always false
    // this forces the application layer to be aware of if a request has been signed or not
    return false;
}

/**
 * @brief
 *   Not-equal operator for comparing two instances of `AlarmHushSignature`.
 *
 * @note The operator performs the equality testing and reverses the
 *       resulting value.  The equality comparison is only meaningful
 *       for signed instances of `AlarmHushSignature`s; consequently
 *       any two unsigned Alarm Hush Signatures are unequal to one
 *       another.  For signed instances, function returns true if
 *       either the key IDs or the signatures do not match.
 *
 * param[in] rhs  A read-only reference to the `AlarmHushSignature`
 *                instance to compare against.
 *
 * @return negated result of operator ==
 */
bool AlarmHushSignature::operator != (const AlarmHushSignature & rhs) const
{
    return !((*this) == rhs);
}

/**
 * @brief
 *   Default constructor for `AlarmHushRequest`.  Object must be
 *   initialized via `AlarmHushRequest::init()` prior to use.
 *
 */
AlarmHushRequest::AlarmHushRequest(void)
{
}


/**
 * @brief
 *   Initialize the `AlarmHushRequest`.
 *
 * @retval #WEAVE_NO_ERROR unconditionally.
 */
WEAVE_ERROR AlarmHushRequest::init(void)
{
    mProximityVerificationCode = 0;

    return WEAVE_NO_ERROR;
}

/**
 * @brief
 *   Deserialize the message in the packet into an `AlarmHushRequest` message.
 *
 * @param[in] aPacket   A read-only pointer to `PacketBuffer` containing
 *                      data to be deserialized.
 *
 * @param[out] aResult  A pointer to an `AlarmHushRequest` message that
 *                      will contain the deserialized alarm hush
 *                      request message.  If the return code indicates
 *                      any failure, the result is unchanged. Must not
 *                      be NULL.
 *
 * @retval #WEAVE_NO_ERROR                 On success.
 *
 * @retval #WEAVE_ERROR_MESSAGE_INCOMPLETE The message was NULL or did
 *         not contain enough data to deserialize the message.
 *
 * @retval #WEAVE_ERROR_INVALID_ARGUMENT   The pointer to store results
 *          was NULL.
 */
WEAVE_ERROR AlarmHushRequest::parse(const PacketBuffer* aPacket, AlarmHushRequest *aResult)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    // cannot use AlarmHushRequest::mProximityVerificationCode as clang compiler on OSX doesn't like it
    const size_t expectedDataLength = sizeof(aResult->mProximityVerificationCode) + sizeof(aResult->mSignature.mKeyId) + sizeof(aResult->mSignature.mKeyedHash);

    VerifyOrExit(aPacket != NULL, err = WEAVE_ERROR_MESSAGE_INCOMPLETE);
    VerifyOrExit(aResult != NULL, err = WEAVE_ERROR_INVALID_ARGUMENT);

    VerifyOrExit(aPacket->DataLength() >= expectedDataLength, err = WEAVE_ERROR_MESSAGE_INCOMPLETE);

    {
        const uint8_t *cursor = aPacket->Start();
        aResult->mProximityVerificationCode = LittleEndian::Read32(cursor);
        aResult->mSignature.mKeyId = LittleEndian::Read16(cursor);
        memcpy(aResult->mSignature.mKeyedHash, cursor, sizeof(aResult->mSignature.mKeyedHash));
        // Note: cursor should be updated before further extension can be made into the parser
        // cursor += sizeof(aResult->mSignature.mKeyedHash);
    }

    aResult->mSignature.mIsSigned = true;

exit:
    return err;
}


/**
 * @brief
 *   Serialize the current alarm hush request message into the
 *        provided `PacketBuffer`
 *
 * @param[out] aPacket   A pointer to an `PacketBuffer` used to store the
 *                       serialized message.
 *
 * @retval #WEAVE_NO_ERROR               On success.
 *
 * @retval #WEAVE_ERROR_BUFFER_TOO_SMALL If aPacket was NULL or
 *         contained insufficient space to serialize the message.
 *
 * @retval #WEAVE_ERROR_INCORRECT_STATE  If the alarm hush request
 *         message is incorrect, e.g. it is not signed.
 */

WEAVE_ERROR AlarmHushRequest::pack(PacketBuffer *aPacket)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    const size_t necessaryDataLength = sizeof(mProximityVerificationCode) + sizeof(mSignature.mKeyId) + sizeof(mSignature.mKeyedHash);
    VerifyOrExit(aPacket != NULL, err = WEAVE_ERROR_BUFFER_TOO_SMALL);
    VerifyOrExit(aPacket->AvailableDataLength() >= necessaryDataLength, err = WEAVE_ERROR_BUFFER_TOO_SMALL);
    VerifyOrExit(mSignature.mIsSigned, err = WEAVE_ERROR_INCORRECT_STATE);

    {
        uint8_t *cursor = aPacket->Start();

        LittleEndian::Write32(cursor, mProximityVerificationCode);
        LittleEndian::Write16(cursor, mSignature.mKeyId);
        memcpy(cursor, mSignature.mKeyedHash, sizeof(mSignature.mKeyedHash));
        cursor += sizeof(mSignature.mKeyedHash);

        // calculate the message length again
        aPacket->SetDataLength(cursor - aPacket->Start());
    }

exit:
    return err;
}

/**
 * @brief
 *   Sign the hush request with information provided here, and fill
 *   the request with signature
 *
 * @param[in] aChallenge A random 32-bit challenge delivered
 *                       out-of-band from Weave.
 *
 * @param[in] aKeyId     A 16-bit key ID of the key used to sign this
 *                       request. The definition of this is TBD at
 *                       this time; until further notice, users of
 *                       this API should fill-in this value with
 *                       #WEAVE_ALARM_HUSH_KEY_ID.
 *
 * @param[in]  aKey      The actual key used to sign the request.
 *
 * @param[in] aKeySize   The number of bytes to be used from the
 *                       key. The key must be at least
 *                       #kAlarmHush_MinKeySize bytes long.
 *
 * @retval #WEAVE_NO_ERROR                On success.
 *
 * @retval #WEAVE_ERROR_INVALID_ARGUMENT  If the key size if
 *                                        insufficiently large
 */
WEAVE_ERROR AlarmHushRequest::sign(uint32_t aChallenge, uint16_t aKeyId, const uint8_t aKey[], uint16_t aKeySize)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    // make sure the key used to sign the message is long enough
    VerifyOrExit(aKeySize >= kAlarmHush_MinKeySize, err = WEAVE_ERROR_INVALID_ARGUMENT);

    {
        Crypto::HMACSHA1 hmac;
        hmac.Begin(aKey, aKeySize);
        hmac.AddData(reinterpret_cast<uint8_t *>(&aChallenge), sizeof(aChallenge));
        hmac.AddData(reinterpret_cast<uint8_t *>(&mProximityVerificationCode), sizeof(mProximityVerificationCode));
        hmac.Finish(mSignature.mKeyedHash);
    }

    mSignature.mIsSigned = true;
    mSignature.mKeyId = aKeyId;

exit:
    return err;
}


/**
 * @brief
 *   Default constructor for `AlarmHushResponse`.  Object must be
 *   initialized via `AlarmHushResponse::init()` prior to use.
 */
AlarmHushResponse::AlarmHushResponse(void)
{
    mHushResult = kAlarmUpdateStatus_Success;
    mLength = 0;
    memset(mConditions, 0, sizeof(mConditions));
}

/**
 * @brief
 *   Initialize `AlarmHushResponse` with the latest status stored in an
 *   `Alarm` object, with the given status code.
 *
 * @param[in] aResult         The result status code to be returned in
 *                            this `AlarmHushResponse` message (check the
 *                            kAlarmHushResult_XXX family of
 *                            constants)
 *
 * @param[in] aNumEntry       The number of valid entries in `aAlarmCondition`
 *
 * @param[in] aAlarmCondition A read-only pointer to an array of alarm
 *                            conditions. When `aNumEntry` is zero, this
 *                            pointer could be NULL
 *
 * @retval #WEAVE_NO_ERROR               On success.
 *
 * @retval #WEAVE_ERROR_BUFFER_TOO_SMALL When the number of conditions
 *         passed in exceeds the capacity of this `AlarmHushResponse`
 *         message.
 */
WEAVE_ERROR AlarmHushResponse::init(uint8_t aResult, uint8_t aNumEntry, const uint8_t * aAlarmCondition)
{
    if (aNumEntry > sizeof(mConditions)/sizeof(mConditions[0]))
    {
        return WEAVE_ERROR_BUFFER_TOO_SMALL;
    }

    mHushResult = aResult;
    mLength = aNumEntry;
    if (mLength > 0)
    {
        memcpy(mConditions, aAlarmCondition, mLength * sizeof(mConditions[0]));
    }

    return WEAVE_NO_ERROR;
}

/**
 * @brief
 *   Deserialize the message in the packet into an `AlarmHushResponse`
 *   message
 *
 * @param[in]  aPacket A pointer to an `PacketBuffer` containing data to
 *                     be deserialized
 *
 * @param[out] aResult A pointer to an `AlarmHushResponse` message
 *                     that will contain the deserialized alarm hush
 *                     response message.  If the return code indicates
 *                     any failure, the result is unchanged. Must not
 *                     be NULL.
 *
 * @retval #WEAVE_NO_ERROR                 On success.
 *
 * @retval #WEAVE_ERROR_MESSAGE_INCOMPLETE      The message was NULL
 *         or did not contain enough data to deserialize the message.
 *
 * @retval #WEAVE_ERROR_INVALID_MESSAGE_LENGTH  The message was too
 *         long to be deserialized on this device.
 *
 * @retval #WEAVE_ERROR_INVALID_ARGUMENT        The pointer to store
 *         results was NULL.
 */

WEAVE_ERROR AlarmHushResponse::parse(const PacketBuffer *aPacket, AlarmHushResponse *aResult)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    const uint8_t *cursor;

    size_t expectedDataLength = kPayloadSizeWithoutConditions;
    VerifyOrExit(aPacket != NULL, err = WEAVE_ERROR_MESSAGE_INCOMPLETE);
    VerifyOrExit(aPacket->DataLength() >= expectedDataLength, err = WEAVE_ERROR_MESSAGE_INCOMPLETE);
    VerifyOrExit(aResult != NULL, err = WEAVE_ERROR_INVALID_ARGUMENT);

    cursor = aPacket->Start();
    aResult->mHushResult = Read8(cursor);
    aResult->mLength = Read8(cursor);

    // we cannot handle any extra alarm conditions
    // since the original alarm implementation would go wild if we have more than 8 conditions,
    // and alarm class can only hold 8 conditions, it doesn't make much sense to support beyond that
    VerifyOrExit(aResult->mLength <= Alarm::kMaxNumAlarmConditions, err = WEAVE_ERROR_INVALID_MESSAGE_LENGTH);

    // $mLength number of bytes for condition
    expectedDataLength += aResult->mLength;
    VerifyOrExit(aPacket->DataLength() >= expectedDataLength, err = WEAVE_ERROR_MESSAGE_INCOMPLETE);

    for (uint8_t i = 0; i < aResult->mLength; ++i)
    {
        aResult->mConditions[i] = Read8(cursor);
    }

exit:
    return err;
}


/**
 * @brief
 *   Serialize the current alarm message into the provided `PacketBuffer`.
 *
 * @param[out] aPacket   A pointer to an  `PacketBuffer` used to store the
 *                       serialized message.
 *
 * @retval #WEAVE_NO_ERROR               On success.
 *
 * @retval #WEAVE_ERROR_BUFFER_TOO_SMALL If aPacket was NULL or
 *         contained insufficient space to serialize the message.
 *
 * @retval #WEAVE_ERROR_INCORRECT_STATE If the alarm hush response is
 *         incorrect e.g. it contains invalid length
 */

WEAVE_ERROR AlarmHushResponse::pack(PacketBuffer *aPacket)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    uint8_t *cursor = NULL;

    // $mLength number of bytes for condition
    const size_t necessaryDataLength = kPayloadSizeWithoutConditions + mLength;

    VerifyOrExit(aPacket != NULL, err = WEAVE_ERROR_BUFFER_TOO_SMALL);
    VerifyOrExit(aPacket->AvailableDataLength() >= necessaryDataLength, err = WEAVE_ERROR_BUFFER_TOO_SMALL);
    // we cannot handle any extra alarm conditions
    // since the original alarm implementation would go wild if we have more than 8 conditions,
    // and alarm class can only hold 8 conditions, it doesn't make much sense to support beyond that
    VerifyOrExit(mLength <= Alarm::kMaxNumAlarmConditions, err = WEAVE_ERROR_INCORRECT_STATE);

    cursor = aPacket->Start();

    Write8(cursor, mHushResult);
    Write8(cursor, mLength);
    for (int i = 0; i < mLength; ++i)
    {
        Write8(cursor, mConditions[i]);
    }

    // calculate the message length again
    aPacket->SetDataLength(cursor - aPacket->Start());

exit:
    return err;
}

// Alarm client


/**
 * @brief
 *   Converts the ClientState enum into a human readable string
 *
 * @param[in] aState ClientState enum
 *
 * @return Human-readable representation of the client state, suitable
 *         for printing.  When the parameter does not correspond to
 *         any known state, string "UNKNOWN" is returned.
 */
const char * WeaveAlarmClient::GetClientStateName(const ClientState aState)
{
    switch (aState)
    {
    case kAlarmClientState_Closed:      return "CLOSED";
    case kAlarmClientState_Active:      return "ACTIVE";
    case kAlarmClientState_GracePeriod: return "GRACE";
    case kAlarmClientState_Linger:      return "LINGER";
    default: return "UNKNOWN";
    }
}


/**
 * @brief
 *   Default constructor for `WeaveAlarmClient`.
 *
 * @note Application code must not instantiate `WeaveAlarmClient`
 *       directly.  Instances of `WeaveAlarmClient` must be obtained
 *       via calls to `WeaveAlarmServer::NewClient()` to ensure
 *       correct functionality.
 *
 */
WeaveAlarmClient::WeaveAlarmClient(void)
{
}

/**
 * @brief
 *   Send the alarm to all nodes in the network
 *
 * The main entry point to sending the `Alarm` messages to the network.
 * The function expects that this object has been initialized and has
 * an active `ExchangeContext`.  The payload passed in is copied by
 * value and becomes the current payload.  The function allocates an
 * `PacketBuffer` to serialize the `Alarm` into, and initiates a Trickle
 * session to disseminate that alarm message throughout the network.
 *
 * @param[in] aPayload A read-only pointer to an alarm message to
 *                     propagate to the network.
 *
 * @return #WEAVE_NO_ERROR on success, or an error code from
 *         `ExchangeContext::SendMessage()`
 *
 * @retval #WEAVE_ERROR_INCORRECT_STATE if the `WeaveAlarmClient`
 *         doesn't represent a local alarm or if it has not been
 *         initialized properly.
 *
 * @retval #WEAVE_ERROR_NO_MEMORY if the client was unable to allocate
 *         `PacketBuffer` for actually transmitting the message.

 */
WEAVE_ERROR WeaveAlarmClient::SendAlarm(const Alarm *aPayload)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    uint32_t preservedSessionId;
    uint32_t preservedExtEvtSeqNum;
    PacketBuffer *msgBuf = NULL;

    VerifyOrExit(IsLocalAlarm(), err = WEAVE_ERROR_INCORRECT_STATE);
    VerifyOrExit(NULL != mEc, err = WEAVE_ERROR_INCORRECT_STATE);

    // this is local alarm, so we must have the session ID and extended event sequence number valid
    VerifyOrExit(mCurrentAlarm.mIsSessionIdAndExtEvtSnValid, err = WEAVE_ERROR_INCORRECT_STATE);

    if (mCurrentAlarm.mExtEvtSeqNum >= UINT32_MAX)
    {
        // this shall not happen in practice
        WeaveLogError(Alarm, "[%d] WARNING evtSN wrap around", FindClientIndex());
        RegenerateSessionId();
    }

    preservedSessionId = mCurrentAlarm.mSessionId;
    preservedExtEvtSeqNum = mCurrentAlarm.mExtEvtSeqNum;

    msgBuf = mServer->NewPacketBuffer();
    WEAVE_FAULT_INJECT(FaultInjection::kFault_SendAlarm, if (msgBuf) { PacketBuffer::Free(msgBuf); msgBuf = NULL; });
    VerifyOrExit(NULL != msgBuf, err = WEAVE_ERROR_NO_MEMORY);

    // increase event sequence number
    ++preservedExtEvtSeqNum;
    if (0 == GetLeastSignificantByte(preservedExtEvtSeqNum))
    {
        // make sure the lowest byte (the alarm counter) is never 0
        // 0 is not anything bad, but normally alarm counter starts from 1
        ++preservedExtEvtSeqNum;
        WeaveLogError(Alarm, "[%d] WARNING ctr wrap around", FindClientIndex());
    }

    mCurrentAlarm = *aPayload;
    mCurrentAlarm.mIsSessionIdAndExtEvtSnValid = true;
    mCurrentAlarm.mSessionId = preservedSessionId;
    mCurrentAlarm.mExtEvtSeqNum = preservedExtEvtSeqNum;
    // the alarm counter is now just the lowest byte of extended event sequence number
    mCurrentAlarm.mAlarmCtr = GetLeastSignificantByte(preservedExtEvtSeqNum);
    mCurrentAlarm.pack(msgBuf);
    mEc-> PeerIntf = mServer->mInterfaceId;

    // Note that InterfaceId is typdef to be a pointer with LwIP, but an unsigned int otherwise.
    // Casting it to uint64_t should get us reasonable results on all platforms.
    WeaveLogDetail(Alarm, "[%d] SendAlarm. session:0x%x, ctr:%u, evtSN:%u, i/f 0x%" PRIx64, FindClientIndex(),
        mCurrentAlarm.mSessionId, mCurrentAlarm.mAlarmCtr, mCurrentAlarm.mExtEvtSeqNum, (uint64_t)mServer->mInterfaceId);
    AlarmLogDetail(mCurrentAlarm);

    // note that this setup has to preceed SendMessage, as the settings change made in this function affects the behavior of SendMessage
    err = mEc->SetupTrickleRetransmit(
        mServer->mAlarmRebroadcastPeriodMsec,
        mServer->mAlarmRebroadcastThreshold,
        mServer->mAlarmRefreshPeriodMsec);
    SuccessOrExit(err);

    // note that adding ExchangeContext::kSendFlag_DelaySen adds a random delay, depending on the trickle mechanism, before it's sent out
    err = mEc->SendMessage(kWeaveProfile_Alarm, kAlarmMessageType_Alarm, msgBuf, ExchangeContext::kSendFlag_RetransmissionTrickle);
    msgBuf = NULL;
    SuccessOrExit(err);

    SetClientState_Active();

exit:
    WeaveLogFunctError(err);

    if (msgBuf != NULL)
    {
        // This will never be executed with the current version of the code
        PacketBuffer::Free(msgBuf);
    }

    // set client state to linger on error, which also cancels trickle timer and notifies app
    if ((NULL != mEc) && (WEAVE_NO_ERROR != err))
    {
        // put local alarm client into linger
        SetClientState_Linger();
    }

    return err;
}

/**
 * @brief
 *   Handle the expiration of the timer dedicated to the refresh
 *   of the alarm freshness.
 *
 * For local alarms, we decide whether we should keep alarming.  If
 * so, the current alarm message is refreshed with a new counter and
 * new session ID, as needed.  If the alarm was not refreshed, the
 * `WeaveAlarmClient` will transition into the linger state.
 *
 * @param[in] aEc A pointer to the `ExchangeContext` associated with
 *                this `WeaveAlarmClient`.
 */
void WeaveAlarmClient::HandleRetransmissionTimeout(ExchangeContext *aEc)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    WeaveAlarmClient *client = (WeaveAlarmClient *) aEc->AppState;

    const int clientIndex = client->FindClientIndex();
    static const char strRetransmissionTimeout[] = "Retransmission timeout.";

    IgnoreUnusedVariable(clientIndex);
    IgnoreUnusedVariable(strRetransmissionTimeout);

    if (client->IsLocalAlarm())
    {
        if (client->mCurrentAlarm.KeepReBroadcasting())
        {
            WeaveLogDetail(Alarm, "[%d] %s Refresh", clientIndex, strRetransmissionTimeout);
            err = client->SendAlarm(&(client->mCurrentAlarm));
            if (WEAVE_NO_ERROR != err)
            {
                WeaveLogError(Alarm, "[%d] Refresh failed", clientIndex);
            }
        }
        else
        {
            WeaveLogDetail(Alarm, "[%d] %s Nothing to report", clientIndex, strRetransmissionTimeout);
            AlarmLogDetail(client->mCurrentAlarm);

            // put local alarm client into linger
            client->SetClientState_Linger();
        }
    }
    else
    {
        WeaveLogDetail(Alarm, "[%d] %s Stop", clientIndex, strRetransmissionTimeout);

        // put client into grace period
        client->SetClientState_GracePeriod();
    }

    WeaveLogFunctError(err);
}


/**
 * @brief
 *   Initialize the current `WeaveAlarmClient` to default values
 *
 * @param[in] aAlarmServer A read-only pointer to the alarm server
 *                         object holding the pool from which this
 *                         alarm client originated.
 */
void WeaveAlarmClient::initAlarmClient(WeaveAlarmServer * const aAlarmServer)
{
    mEc = NULL;
    mCurrentAlarm.init();
    mClientState = kAlarmClientState_Closed;
    mServer = aAlarmServer;
    mIsLocalAlarm = false;
}

/**
 * @brief
 *   Close this alarm client.
 *
 * The method attempts to set the client state to closed via
 * `WeaveAlarmClient::SetClientState_Closed()` without issuing
 * notifications to the higher layers.
 *
 * @note The object is considered terminated at this point.  The
 *       object's storage is available for use by its associated
 *       `WeaveAlarmServer`.  Consequently, this object must not be
 *       used by the application code past this call.  The object may
 *       be reanimated (logically, a new object is created in the same
 *       storage) via `WeaveAlarmServer::NewClient()`.
 *
 */
void WeaveAlarmClient::Close(void)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    // there is no need to notify the app layer about state change, as we are closed by upper layer
    err = SetClientState_Closed (false);

    WeaveLogFunctError(err);
}


/**
 * @brief
 *   C-to-C++ trampoline for handling linger timeout, actual
 *   handling done in `WeaveAlarmClient::HandleLingerTimeout()`
 *
 * @param[in] aSystemLayer  A pointer to an `SystemLayer` object that
 *                          generated the timeout (ignored)
 * @param[in] aAppState     A pointer to a context object associated with
 *                          the timeout
 * @param[in] aError        An error code from the timer (ignored)
 */
void WeaveAlarmClient::_HandleLingerTimeout(System::Layer* aSystemLayer, void* aAppState, System::Error aError)
{
    WeaveAlarmClient* client = reinterpret_cast<WeaveAlarmClient*>(aAppState);
    client->HandleLingerTimeout();
}

/**
 * @brief
 *   Handle the end of the linger state by closing the associated
 *   client and cleaning up the underlying resources.
 */
void WeaveAlarmClient::HandleLingerTimeout(void)
{
    // as a precaution, for timers could fire at any time
    if (NULL != mServer)
    {
        WeaveLogDetail(Alarm, "[%d] Linger ended. Closing", FindClientIndex());
    }
    SetClientState_Closed();
}


/**
 * @brief
 *   C-to-C++ trampoline for handling grace period timeout, actual
 *   handling done in `WeaveAlarmClient::HandleGracePeriodTimeout()`.
 *
 * @param[in] inet      A pointer to an `SystemLayer` object that
 *                      generated the timeout (ignored)
 * @param[in] aAppState A pointer to a context object associated with
 *                      the timeout
 * @param[in] err       An error code from the timer (ignored)
 */
void WeaveAlarmClient::_HandleGracePeriodTimeout(System::Layer* aSystemLayer, void* aAppState, System::Error aError)
{
    WeaveAlarmClient* client = reinterpret_cast<WeaveAlarmClient*>(aAppState);
    client->HandleGracePeriodTimeout();
}


/**
 * @brief
 *   Handle the end of grace period by transitioning into the linger
 *   state.
 *
 */
void WeaveAlarmClient::HandleGracePeriodTimeout(void)
{
    // as a precaution, for timers could fire at any time
    if (NULL != mServer)
    {
        WeaveLogDetail(Alarm, "[%d] Grace period ended, moving to linger", FindClientIndex());
        SetClientState_Linger();
    }
}

/**
 * @brief
 *   Find the index of this instance within the owning 'WeaveAlarmServer'
 *
 * @return -1 on failure; otherwise, the index of this instance in the pool
 *   of 'WeaveAlarmClient' instances
 */
int WeaveAlarmClient::FindClientIndex(void) const
{
    int result = -1;
    if (NULL != mServer)
    {
        result = mServer->GetClientIndex(this);
    }

    return result;
}

/**
 * @brief
 *   Set the state of `WeaveAlarmClient`
 *
 * @note The method only acts as a setter, and does not implement the
 * overall state machine.  Applications should use `SetClientState_*`
 * APIs whenever possible instead.
 *
 * @param[in] aNewState New state.
 */
void WeaveAlarmClient::ForceClientStateChangeNoError(ClientState aNewState)
{
    WeaveLogDetail(Alarm, "[%d] ClientState: %s -> %s", FindClientIndex(), GetClientStateName(mClientState), GetClientStateName(aNewState));
    mClientState = aNewState;
}


/**
 * @brief
 *   Cancels the timers governing the timers related to the grace
 *   period and linger states.
 *
 */
void WeaveAlarmClient::CancelAllTimersExceptForTrickle(void)
{
    if (NULL != mServer)
    {
        // Note that the general practice in Weave code is to assume
        // all pointers to singleton global objects have been properly initialized
        mServer->ExchangeMgr->MessageLayer->SystemLayer->CancelTimer(_HandleLingerTimeout, this);
        mServer->ExchangeMgr->MessageLayer->SystemLayer->CancelTimer(_HandleGracePeriodTimeout, this);
    }
}


/**
 * @brief
 *   Attempts to set the state of the `WeaveAlarmClient` to
 *   #kAlarmClientState_Active.
 *
 * The function examines the state of the current `WeaveAlarmClient`.
 * If state transition is permitted (based the current state of the
 * object), then object transitions into #kAlarmClientState_Active
 * state.  Appropriate actions are executed along the way.  If
 * transition is not permitted, the `WeaveAlarmClient` remains
 * unchanged and the error code is returned.
 *
 * @retval #WEAVE_NO_ERROR              On success.
 *
 * @retval #WEAVE_ERROR_INCORRECT_STATE If the `WeaveAlarmClient` is
 *         in a state that does not permit the transition to
 *         #kAlarmClientState_Active.
 */
WEAVE_ERROR WeaveAlarmClient::SetClientState_Active(void)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    switch (mClientState)
    {
    case kAlarmClientState_Linger:
    case kAlarmClientState_GracePeriod:
    case kAlarmClientState_Active:
    case kAlarmClientState_Closed:
        break;
    default:
        ExitNow(err = WEAVE_ERROR_INCORRECT_STATE);
    }

    // cancel all timers so we have a cleaner slate
    CancelAllTimersExceptForTrickle();

    ForceClientStateChangeNoError(kAlarmClientState_Active);

exit:
    WeaveLogFunctError(err);

    // reclaim all resources on error, as this is a catastrophic error
    if (WEAVE_NO_ERROR != err)
    {
        WeaveLogError(Alarm, "[%d] Error setting to ACTIVE", FindClientIndex());

        if (IsLocalAlarm())
        {
            // do not close local alarm client, but at least we can try to cancel trickling
            // Canceling retransmission at this point would prevent the trickle timer from firing.
            mEc->CancelRetrans();
        }
        else
        {
            // close if this is a remote client
            (void)SetClientState_Closed();
        }
    }

    mServer->OnAlarmClientStateChange(this);
    return err;
}


/**
 * @brief
 *   Attempts to set the state of the `WeaveAlarmClient` to
 *   #kAlarmClientState_Closed.
 *
 * The function examines the state of the current `WeaveAlarmClient`.
 * If state transition is permitted (based the current state of the
 * object), then object transitions into #kAlarmClientState_Closed
 * state.  Appropriate actions are executed along the way.  If
 * transition is not permitted, the `WeaveAlarmClient` remains unchanged
 * and the error code is returned.
 *
 * @param[in] aNotifyAppLayer a flag dictating whether the application
 *            should be notified about the state change via
 *            `WeaveAlarmServer::OnAlarmClientStateChange()` callback.
 *
 * @retval #WEAVE_NO_ERROR              On success.
 *
 * @retval #WEAVE_ERROR_INCORRECT_STATE If the `WeaveAlarmClient` is in
 *         a state that does not permit the transition to
 *         #kAlarmClientState_Closed.
 */
WEAVE_ERROR WeaveAlarmClient::SetClientState_Closed(bool aNotifyAppLayer)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    switch (mClientState)
    {
    case kAlarmClientState_Closed:
        // do nothing if we're already closed
        ExitNow();
        break;
    case kAlarmClientState_Active:
    case kAlarmClientState_Linger:
    case kAlarmClientState_GracePeriod:
        break;
    default:
        ExitNow(err = WEAVE_ERROR_INCORRECT_STATE);
    }

    // reclaim all resources, be careful about partially initialized states

    if (NULL != mServer)
    {
        // cancel all timers so we have a cleaner slate
        CancelAllTimersExceptForTrickle();
    }

    ForceClientStateChangeNoError(kAlarmClientState_Closed);

exit:
    WeaveLogFunctError(err);

    if (WEAVE_NO_ERROR != err)
    {
        WeaveLogError(Alarm, "[%d] Error setting to CLOSED", FindClientIndex());
    }

    if (aNotifyAppLayer && (NULL != mServer))
    {
        // notify the application layer that state of some alarm client has changed
        // note that the "IsLocalAlarm" status is not reset yet, so application layer has a chance to evaluate what happened
        mServer->OnAlarmClientStateChange(this);
    }

    mIsLocalAlarm = false;

    // close exchange context, which cancels the trickle implicitly
    // we put it after OnAlarmClientStateChange callback so application layer can still access the source node id
    if (mEc != NULL) {
        mEc->Close();
        mEc = NULL;
    }

    // we've tried our best reclaiming all resource, so no further error handling
    return err;
}


/**
 * @brief
 *   Attempts to set the state of the `WeaveAlarmClient` to
 *   #kAlarmClientState_GracePeriod.
 *
 * The function examines the state of the current `WeaveAlarmClient`.
 * If state transition is permitted (based the current state of the
 * object), then object transitions into #kAlarmClientState_GracePeriod
 * state.  Appropriate actions are executed along the way.  If
 * transition is not permitted, the `WeaveAlarmClient` remains unchanged
 * and the error code is returned.
 *
 * @retval #WEAVE_NO_ERROR              On success.
 *
 * @retval #WEAVE_ERROR_INCORRECT_STATE If the `WeaveAlarmClient` is not
 *         in a #kAlarmClientState_Active state or if the
 *         `WeaveAlarmClient` does not pertain a local alarm.
 */

WEAVE_ERROR WeaveAlarmClient::SetClientState_GracePeriod(void)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    // Usage error
    // local alarm shall never enter this state
    VerifyOrExit(!IsLocalAlarm(), err = WEAVE_ERROR_INCORRECT_STATE);

    switch (mClientState)
    {
    case kAlarmClientState_Active:
        break;
    default:
        ExitNow(err = WEAVE_ERROR_INCORRECT_STATE);
    }

    WeaveLogDetail(Alarm, "[%d] Stop trickle", FindClientIndex());

    // make sure trickle is stopped
    mEc->CancelRetrans();

    // cancel all timers so we have a cleaner slate
    CancelAllTimersExceptForTrickle ();

    // start timer for grace period
    SuccessOrExit(err = mServer->ExchangeMgr->MessageLayer->SystemLayer->StartTimer(mServer->mAlarmGracePeriodMsec, _HandleGracePeriodTimeout, this));

    ForceClientStateChangeNoError(kAlarmClientState_GracePeriod);

exit:
    WeaveLogFunctError(err);

    if (WEAVE_NO_ERROR != err)
    {
        WeaveLogError(Alarm, "[%d] Error setting to GRACE PERIOD", FindClientIndex());

        if (!IsLocalAlarm())
        {
            // timer failure for remote alarm is okay, just close it and notify the application layer on closing
            (void)SetClientState_Closed();
        }
    }

    mServer->OnAlarmClientStateChange(this);
    return err;
}

/**
 * @brief
 *   Attempts to set the state of the `WeaveAlarmClient` to
 *   #kAlarmClientState_Linger.
 *
 * The function examines the state of the current `WeaveAlarmClient`.
 * If state transition is permitted (based the current state of the
 * object), then object transitions into #kAlarmClientState_Linger
 * state.  Appropriate actions are executed along the way.  If
 * transition is not permitted, the `WeaveAlarmClient` remains unchanged
 * and the error code is returned.
 *
 * @retval #WEAVE_NO_ERROR              On success.
 *
 * @retval #WEAVE_ERROR_INCORRECT_STATE If the `WeaveAlarmClient` is not
 *         in either a #kAlarmClientState_Active or
 *         #kAlarmClientState_GracePeriod state or if the
 *         `WeaveAlarmClient` does not pertain a local alarm.
 */

WEAVE_ERROR WeaveAlarmClient::SetClientState_Linger(void)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    // stop trickling
    mEc->CancelRetrans();

    switch (mClientState)
    {
    case kAlarmClientState_Active:
        if (!IsLocalAlarm())
        {
            // Usage error
            // remote alarm shall not enter LINGER directly from ACTIVE
            ExitNow(err = WEAVE_ERROR_INCORRECT_STATE);
        }
        break;
    case kAlarmClientState_GracePeriod:
        break;
    default:
        ExitNow(err = WEAVE_ERROR_INCORRECT_STATE);
    }

    // cancel all timers so we have a cleaner slate
    CancelAllTimersExceptForTrickle ();

    if (IsLocalAlarm())
    {
        // local alarms are never automatically closed. they linger until next SendAlarm
        WeaveLogDetail(Alarm, "[%d] Initiator alarm timed out", FindClientIndex());

        SuccessOrExit(err = TryAdvanceExtEvtSeqNum());

        ForceClientStateChangeNoError(kAlarmClientState_Linger);
    }
    else
    {
        uint32_t lingerTimeoutMsec = mServer->mAlarmRefreshPeriodMsec * WEAVE_ALARM_MAX_NUM_HOPS;

        if (lingerTimeoutMsec > mServer->mAlarmGracePeriodMsec)
        {
            lingerTimeoutMsec -= mServer->mAlarmGracePeriodMsec;

            WeaveLogDetail(Alarm, "[%d] Setup linger timer: %u msec", FindClientIndex(), lingerTimeoutMsec);

            // remote alarms will be closed when this timer fires
            SuccessOrExit(err = mServer->ExchangeMgr->MessageLayer->SystemLayer->StartTimer(lingerTimeoutMsec, _HandleLingerTimeout, this));

            ForceClientStateChangeNoError(kAlarmClientState_Linger);
        }
        else
        {
            // strange, but not impossible (if grace period is set to be equal or longer than linger time
            WeaveLogDetail(Alarm, "[%d] No linger is needed", FindClientIndex());

            // directly move to CLOSED, as we've already spent out time in grace period
            SuccessOrExit(err = SetClientState_Closed());
        }
    }

exit:
    WeaveLogFunctError(err);

    if (WEAVE_NO_ERROR != err)
    {
        WeaveLogError(Alarm, "[%d] Error setting to LINGER", FindClientIndex());

        if (!IsLocalAlarm())
        {
            // it's possible for us to call this twice in a row if there is any error from this function
            // it should be fine
            (void)SetClientState_Closed();
        }
    }

    mServer->OnAlarmClientStateChange(this);
    return err;
}


/**
 * @brief
 *   Accessor function for retrieving the current state of the
 *   `WeaveAlarmClient`.
 *
 * @return Current state of the object.
 */
const WeaveAlarmClient::ClientState WeaveAlarmClient::GetClientState(void) const
{
    return mClientState;
}


/**
 * @brief
 *   Regenerate the ID of this Alarm Client.
 *
 * Session IDs are chosen at random.  They discriminate between alarm
 * messages across the alarm counter rollover or unexpected node
 * reboot.
 *
 * @retval #WEAVE_NO_ERROR on success
 *
 * @retval #WEAVE_ERROR_RANDOM_DATA_UNAVAILABLE if the random number
 *         generator was unable to generate a random session IDs
 */
WEAVE_ERROR WeaveAlarmClient::RegenerateSessionId(void)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    mCurrentAlarm.mIsSessionIdAndExtEvtSnValid = false;
    mCurrentAlarm.mSessionId = 0;
    uint32_t sessionId = 0;
    SuccessOrExit(err = Platform::Security::GetSecureRandomData((uint8_t *)&sessionId, sizeof(sessionId)));

    mCurrentAlarm.mIsSessionIdAndExtEvtSnValid = true;
    mCurrentAlarm.mSessionId = sessionId;
    mCurrentAlarm.mExtEvtSeqNum = 0;
    mCurrentAlarm.mAlarmCtr = GetLeastSignificantByte(mCurrentAlarm.mExtEvtSeqNum);

exit:
    WeaveLogFunctError(err);

    return err;
}


/**
 * @brief
 *   Advance the sequence number and regenerate the session ID as
 *   needed.
 *
 * @return #WEAVE_NO_ERROR On success. Failures (and resultant error
 *         codes) are those of `WeaveAlarmClient::RegenerateSessionId()`
 */
WEAVE_ERROR WeaveAlarmClient::TryAdvanceExtEvtSeqNum(void)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    bool IsRegenerationNeeded = true;

    if (mCurrentAlarm.mIsSessionIdAndExtEvtSnValid)
    {
        const uint32_t next256AlignedEvtSn = ((mCurrentAlarm.mExtEvtSeqNum + UINT8_MAX) >> 8) << 8;

        if (next256AlignedEvtSn > mCurrentAlarm.mExtEvtSeqNum)
        {
            // no wrap around detected, so the new sequence number is a valid one
            mCurrentAlarm.mExtEvtSeqNum = next256AlignedEvtSn;
            mCurrentAlarm.mAlarmCtr = GetLeastSignificantByte(mCurrentAlarm.mExtEvtSeqNum);
            IsRegenerationNeeded = false;

            WeaveLogDetail(Alarm, "[%d] Advance ExtEvtSN: %u, Ctr: %u", FindClientIndex(), mCurrentAlarm.mExtEvtSeqNum, mCurrentAlarm.mAlarmCtr);
        }
    }

    if (IsRegenerationNeeded)
    {
        SuccessOrExit(err = RegenerateSessionId());

        WeaveLogDetail(Alarm, "[%d] Reset ExtEvtSN: %u, Ctr: %u", FindClientIndex(), mCurrentAlarm.mExtEvtSeqNum, mCurrentAlarm.mAlarmCtr);
    }

exit:
    WeaveLogFunctError(err);

    return err;
}


/**
 * @brief
 *   Determine if the current node generated the alarm associated with
 *   this `WeaveAlarmClient`.
 *
 * @return `true` if the current node is the originator of this alarm;
 *         otherwise `false`.
 */
bool WeaveAlarmClient::IsLocalAlarm(void) const
{
    return mIsLocalAlarm;
}


/**
 * @brief
 *   Return the ID of the alarm originator.
 *
 * @return The Weave node ID of the originator.  If this
 *         `WeaveAlarmClient` is inactive or otherwise malformed, the
 *         function returns a node ID of 0.  If the current node is
 *         the originator, the function returns the local node ID.
 */
uint64_t WeaveAlarmClient::GetOriginator(void)
{
    if (NULL != mEc)
    {
        if (mEc->PeerNodeId == kAnyNodeId)
        {
            return mEc->ExchangeMgr->FabricState->LocalNodeId;
        }
        return mEc->PeerNodeId;
    }
    else
    {
        // this is an usage error
        return uint64_t(0);
    }
}


/**
 * @brief
 *   Get the current alarm message handled by this `WeaveAlarmClient`.
 *
 * @return The alarm message handled by this `WeaveAlarmClient`.
 */
Alarm WeaveAlarmClient::GetCurrentAlarm(void)
{
    return mCurrentAlarm;
}


/**
 * @brief
 *   Get the spoken location (where) of the alarm originator.
 *
 * @return The ID of the spoken location (where) of the alarm originator
 */
uint8_t WeaveAlarmClient::GetOriginatorWhere(void)
{
    return mCurrentAlarm.mWhere;
}


/**
 * @brief
 *   Handle the incoming alarm messages.
 *
 * Primary entry point for handling the messages.
 *
 * @param[in] aPktInfo   A read-only pointer to the packet info of the
 *                       incoming packet
 *
 * @param[in] aMsgInfo A read-only pointer to the `WeaveMessageInfo`
 *                       of the incoming packet
 *
 * @param[in] aPayload   A pointer to an `PacketBuffer` containing the
 *                       payload of the incoming message.  The
 *                       incoming message has been processed by both
 *                       Weave Message layer and by Weave Exchange
 *                       Layer, so at this point, the payload points
 *                       to an application (profile)-level payload
 *
 * @param[in] aParsedAlarm A read-only reference to the parsed Alarm
 *                       message.
 */
void WeaveAlarmClient::HandleAlarm(const IPPacketInfo *aPktInfo, const WeaveMessageInfo *aMsgInfo, PacketBuffer *aPayload, const Alarm & aParsedAlarm)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    PacketBuffer *msgBuf = NULL;
    bool closeClientOnError = false;
    bool isNewAlarm = false;
    bool isCounterOrEvtSnEqual = false;

    // if the server has an assigned interface but the alarm message
    // came in on some unknown interface, drop it.
    if (INET_NULL_INTERFACEID != mServer->mInterfaceId)
    {
        if (aPktInfo->Interface != mServer->mInterfaceId) {
            WeaveLogDetail(Alarm, "[%d] unexpected i/f 0x%" PRIx64 ", expecting 0x%" PRIx64, FindClientIndex(),
                (uint64_t)aPktInfo->Interface, (uint64_t)mServer->mInterfaceId);
            ExitNow(err = WEAVE_ERROR_NO_ENDPOINT);
        }
    }

    if (kAlarmClientState_Closed == mClientState)
    {
        // this is the first message to create this remote alarm client
        WeaveLogDetail(Alarm, "[%d] New remote alarm", FindClientIndex());
        mCurrentAlarm.init();
        isNewAlarm = true;
    }


    // we're printing incoming alarm's session id, for they should be the same in most cases,
    // and in the case of a new remote alarm, mCurrentAlarm is largely invalid, anyway
    if (isNewAlarm)
    {
        if (aParsedAlarm.mIsSessionIdAndExtEvtSnValid)
        {
            WeaveLogDetail(Alarm, "[%d] Src:0x%" PRIx64 ", Session:0x%x, incoming evtSN:%u",
                FindClientIndex(), aMsgInfo->SourceNodeId,
                aParsedAlarm.mSessionId,
                aParsedAlarm.mExtEvtSeqNum);
        }
        else
        {
            WeaveLogDetail(Alarm, "[%d] Src:0x%" PRIx64 ", incoming ctr:%u",
                FindClientIndex(), aMsgInfo->SourceNodeId,
                aParsedAlarm.mAlarmCtr);
        }
    }
    else
    {
        if (aParsedAlarm.mIsSessionIdAndExtEvtSnValid)
        {
            WeaveLogDetail(Alarm, "[%d] Src:0x%" PRIx64 ", Session:0x%x, evtSN:%u, incoming:%u",
                FindClientIndex(), aMsgInfo->SourceNodeId,
                aParsedAlarm.mSessionId,
                mCurrentAlarm.mExtEvtSeqNum,
                aParsedAlarm.mExtEvtSeqNum);
        }
        else
        {
            WeaveLogDetail(Alarm, "[%d] Src:0x%" PRIx64 ", ctr:%u, incoming:%u",
                FindClientIndex(), aMsgInfo->SourceNodeId,
                mCurrentAlarm.mAlarmCtr,
                aParsedAlarm.mAlarmCtr);
        }
    }

    isCounterOrEvtSnEqual = ((!mCurrentAlarm.mIsSessionIdAndExtEvtSnValid) && (aParsedAlarm.mAlarmCtr == mCurrentAlarm.mAlarmCtr)) ||
        (mCurrentAlarm.mIsSessionIdAndExtEvtSnValid && (aParsedAlarm.mExtEvtSeqNum == mCurrentAlarm.mExtEvtSeqNum));

    if ((kAlarmClientState_Active == mClientState) && isCounterOrEvtSnEqual)
    {
        // only check for duplicates if we're active
        // exchange context for a lingering client is not trickling, so there is no need to register duplicates
        mEc->HandleTrickleMessage(aPktInfo, aMsgInfo);
    }
    else if (IsLocalAlarm())
    {
        // this is a local-originated alarm
        // we only use incoming messages to count re-transmissions for trickle throttling, which is handled in previous case
        // if this alarm message doesn't carry the same alarm counter, we should just discard it
        WeaveLogDetail(Alarm, "[%d] Discard diff counter", FindClientIndex());
    }
    else if (((kAlarmClientState_Linger == mClientState) || (kAlarmClientState_GracePeriod == mClientState)) &&
        isCounterOrEvtSnEqual)
    {
        // if we're in linger state, don't accept anything that carries the same alarm counter
        WeaveLogDetail(Alarm, "[%d] Discard same counter", FindClientIndex());
    }
    else
    {
        // from now on, close the client on all errors
        // many operations here leads to unrecoverable state, so the only choice is to close everything
        closeClientOnError = true;

        if (!isNewAlarm)
        {
            // only check the event sequence number or alarm counter if this is not a new alarm
            if (mCurrentAlarm.mIsSessionIdAndExtEvtSnValid)
            {
                // both alarms are from newer version. We'd never have wrap around issue,
                // because a new session id would be generated when that happens

                if (aParsedAlarm.mExtEvtSeqNum < mCurrentAlarm.mExtEvtSeqNum)
                {
                    WeaveLogDetail(Alarm, "[%d] stale: %u < %u",
                        FindClientIndex(),
                        aParsedAlarm.mExtEvtSeqNum,
                        mCurrentAlarm.mExtEvtSeqNum);
                    ExitNow();
                }
            }
            else
            {
                // okay, both alarms are from older version
                // check alarm counter, and allow some space for wrap around
                const uint8_t distance = aParsedAlarm.mAlarmCtr - mCurrentAlarm.mAlarmCtr;

                if (distance > WEAVE_ALARM_FORWARD_COUNTER_DISTANCE_LIMIT)
                {
                    // this is considered as suspicious and rejected
                    WeaveLogDetail(Alarm, "[%d] ctr: %u - %u = %u > %u",
                        FindClientIndex(), aParsedAlarm.mAlarmCtr, mCurrentAlarm.mAlarmCtr,
                        distance, WEAVE_ALARM_FORWARD_COUNTER_DISTANCE_LIMIT);
                    ExitNow();
                }
            }
        }

        // make a copy of the incoming alarm
        mCurrentAlarm = aParsedAlarm;

        WeaveLogDetail(Alarm, "[%d] Overwrite", FindClientIndex());
        AlarmLogDetail(mCurrentAlarm);

        msgBuf = mServer->NewPacketBuffer();
        WEAVE_FAULT_INJECT(FaultInjection::kFault_HandleAlarm, if (msgBuf) { PacketBuffer::Free(msgBuf); msgBuf = NULL; });
        if (NULL == msgBuf)
        {
            WeaveLogError(Alarm, "[%d] no inetbuf", FindClientIndex());
            ExitNow(err = WEAVE_ERROR_NO_MEMORY);
        }

        if (aPayload->DataLength() > WEAVE_ALARM_MAX_INCOMING_ALARM_MESSAGE_SIZE)
        {
            WeaveLogDetail(Alarm, "[%d] msg too long (%d, %d)", FindClientIndex(), aPayload->DataLength(), msgBuf->DataLength());
            ExitNow(err = WEAVE_ERROR_MESSAGE_TOO_LONG);
        }

        memcpy(msgBuf->Start(), aPayload->Start(), aPayload->DataLength());
        msgBuf->SetDataLength(aPayload->DataLength());

        // make a copy of the Weave message information
        WeaveMessageInfo msgInfoCopy(*aMsgInfo);

        // note that this setup has to preceed SendMessage, as the settings change made in this function affects the behavior of SendMessage
        // note the timing for remote alarms is a little bit different from local alarm
        // this is not necessary, but kept so the behavior is as close to V1 as possible
        err = mEc->SetupTrickleRetransmit(
            mServer->mAlarmRebroadcastPeriodMsec,
            mServer->mAlarmRebroadcastThreshold,
            mServer->mAlarmRefreshPeriodMsec + mServer->mAlarmRebroadcastPeriodMsec);
        SuccessOrExit(err);

        // push copy of this incoming message into ec
        // Note that the flag kSendFlag_FromInitiator is actually not necessary in this version, because all ExchangeContext instances are
        // "initiators". This is left here to mark what it was in the original implementation.
        // In the original implementation, the ExchangeContext instances for remote alarm clients are actually "responders", but they use
        // this flag to fake their ExchangeHeader so they look like an "initiator" when a re-multicast is sent out.
        // They are "responders" because they are generated from Unsolicited Message Handler, and this makes the ExchangeContext matching
        // process in ExchangeMgr keep sending messages from other "initiators" into its message handler directly, skipping the UMH.
        // In this version, all ExchangeContext instances are "initiators", so ExchangeMgr would always dispatch into UMH.
        // This is true even when there are older implementations around, because they anyway pretend to be "initiators" when they send
        // messages out.
        err = mEc->SendMessage(kWeaveProfile_Alarm, kAlarmMessageType_Alarm, msgBuf,
            ExchangeContext::kSendFlag_RetransmissionTrickle | ExchangeContext::kSendFlag_DelaySend |
            ExchangeContext::kSendFlag_ReuseMessageId | ExchangeContext::kSendFlag_ReuseSourceId | ExchangeContext::kSendFlag_FromInitiator,
            &msgInfoCopy);
        msgBuf = NULL;
        SuccessOrExit(err);

        SetClientState_Active();
    }

exit:
    WeaveLogFunctError(err);

    if (NULL != aPayload)
    {
        PacketBuffer::Free(aPayload);
        aPayload = NULL;
    }

    if (NULL != msgBuf)
    {
        PacketBuffer::Free(msgBuf);
        msgBuf = NULL;
    }

    // on error in critical stages, just close the (remote) alarm client
    if (closeClientOnError && (WEAVE_NO_ERROR != err))
    {
        // note that handler for a local alarm client would never set this flag closeClientOnError
        SetClientState_Closed();
    }
}

/**
 * @brief
 *   Generate 32-bit random number for the use in proximity
 *   verification code.
 *
 * @note The application layer is responsible for storing
 *       this result for later use.
 *
 * @param[out]  aResult A pointer to the output 32-bit random number.
 *
 * @retval #WEAVE_NO_ERROR                      On success.
 *
 * @retval #WEAVE_ERROR_INVALID_ARGUMENT        If a  NULL pointer was
 *         passed in for the aResult.
 *
 * @retval #WEAVE_ERROR_RANDOM_DATA_UNAVAILABLE If the random number
 *         generator was unable to provide a randomized verification
 *         code.
 */
WEAVE_ERROR WeaveAlarmClient::GenerateProximityVerificationCode(uint32_t * const aResult)
{
    if (aResult != NULL)
    {
        return Platform::Security::GetSecureRandomData((uint8_t *)aResult, sizeof(*aResult)/sizeof(uint8_t));
    }
    else
    {
        return WEAVE_ERROR_INVALID_ARGUMENT;
    }
}

/**
 * @brief
 *   Generate 32-bit random number for the use in proximity
 *   verification code.
 *
 * @note The application layer is responsible for storing
 *       this result for later use.
 *
 * @param[out]  aResult A pointer to the output 32-bit random number.
 *
 * @retval #WEAVE_NO_ERROR                      On success.
 *
 * @retval #WEAVE_ERROR_INVALID_ARGUMENT        If a  NULL pointer was
 *         passed in for the aResult.
 *
 * @retval #WEAVE_ERROR_RANDOM_DATA_UNAVAILABLE If the random number
 *         generator was unable to provide a randomized verification
 *         code.
 */
WEAVE_ERROR WeaveAlarmClient::GenerateHushChallenge(uint32_t * const aResult)
{
    return GenerateProximityVerificationCode(aResult);
}

// Alarm server

/**
 * @brief
 *   Default constructor for `WeaveAlarmServer`.
 *
 * All parameters that govern the runtime of the alarm protocol are
 * initialized to their default values.  Pointers to
 * `nl::Weave::ExchangeMgr and nl::Weave::FabricState` are initialized
 * to NULL.  The object is not usable until `WeaveAlarmServer::Init()`
 * has been called.
 */

WeaveAlarmServer::WeaveAlarmServer(void)
{
    ExchangeMgr = NULL;
    FabricState = NULL;
    mAlarmRefreshPeriodMsec = WEAVE_ALARM_DEFAULT_REBROADCAST_TIMEOUT_MSEC;
    mAlarmRebroadcastPeriodMsec = WEAVE_ALARM_DEFAULT_REBROADCAST_PERIOD_MSEC;
    mAlarmRebroadcastThreshold = WEAVE_ALARM_DEFAULT_REBROADCAST_THRESH;
    mAlarmGracePeriodMsec = WEAVE_ALARM_DEFAULT_GRACE_PERIOD_MSEC;
}

/**
 * @brief
 *   Given a pointer to the `nl::Weave::WeaveAlarmClient` from this
 *   server, return its index
 *
 * @param[in] aClient A read-only pointer to the client object. The
 *                    client object must have been allocated from
 *                    within the client pool of this server object
 *
 * @returns The index of the client in the client pool.
 */
int WeaveAlarmServer::GetClientIndex(const WeaveAlarmClient * aClient) const
{
    return (aClient - mClientPool);
}

void WeaveAlarmServer::OnAlarmClientStateChange(WeaveAlarmClient * const aClient)
{
    if (NULL != mAlarmDelegate)
    {
        mAlarmDelegate->OnAlarmClientStateChange(aClient);
    }
}

#if WEAVE_SYSTEM_CONFIG_USE_LWIP

// This gets called when we deallocate the buffer.  In our case
// deallocation means two things: setting the outerRef to NULL to
// indicate that the buffer is ready for allocation and decrementing
// the refcount on the outer reference s.t. the reference can be freed
// as needed.


static void TinyPBufFreeFunction(struct pbuf * pbuf)
{
    return;
}

WEAVE_ERROR WeaveAlarmServer::InitializeBackingStore(void)
{
#if WEAVE_SYSTEM_CONFIG_USE_LWIP
    memset(&mTinyPbufPool, 0, sizeof(mTinyPbufPool));

    for (int i = 0; i< MAX_CONCURRENT_MESSAGES; i++)
    {
        mTinyPbufPool[i].pc.custom_free_function = TinyPBufFreeFunction;
    }
#endif // WEAVE_SYSTEM_CONFIG_USE_LWIP

    return WEAVE_NO_ERROR;
}

void WeaveAlarmServer::ShutdownBackingStore(void)
{
}

PacketBuffer* WeaveAlarmServer::NewPacketBuffer(void)
{

    for (int i = 0; i< MAX_CONCURRENT_MESSAGES; i++)
    {

        // Note that these pbufs are not really accessed by LwIP through multiple threads, for they're only used
        // to retain the messages. When these messages need to be sent out, another copy into another pbuf from, device-wide pool,
        // is made in UDPEndPoint.
        if (mTinyPbufPool[i].pc.pbuf.ref == 0)
        {
            // the current tiny pbuf is unallocated, initialize and return
            struct tiny_custom_pbuf *tinypbuf = &(mTinyPbufPool[i]);
            struct pbuf * pbuf = pbuf_alloced_custom(PBUF_RAW,
                                                     sizeof(struct tiny_custom_pbuf) - LWIP_MEM_ALIGN_SIZE(sizeof(tiny_custom_pbuf)),
                                                     PBUF_POOL,
                                                     &(tinypbuf->pc),
                                                     &(tinypbuf->payload[0]),
                                                     sizeof(tinypbuf->payload));

            if (pbuf == NULL)
            {
                // Should never happen, but when it does fail fast
                // rather than retry the other pbufs
                goto error;
            }
            // reserve the space in the payload for Weave headers
            tinypbuf->pc.pbuf.payload = ((uint8_t*) tinypbuf->pc.pbuf.payload) + WEAVE_HEADER_RESERVE_SIZE;

            // make the pbufs follow the PacketBuffer protocol
            tinypbuf->pc.pbuf.len = tinypbuf->pc.pbuf.tot_len = 0;
            tinypbuf->pc.pbuf.next = NULL;

            //return the value
            return (PacketBuffer *) tinypbuf;
        }
    }
error:
    WeaveLogError(Alarm, "alarm-specific pbuf pool exhausted");
    return NULL;
}

#else // dummy implementation based on the existing PacketBuffers

PacketBuffer* WeaveAlarmServer::NewPacketBuffer(void)
{
    return PacketBuffer::New();
}

WEAVE_ERROR WeaveAlarmServer::InitializeBackingStore(void)
{
    return WEAVE_NO_ERROR;
}
void WeaveAlarmServer::ShutdownBackingStore(void)
{
    return;
}
#endif // WEAVE_SYSTEM_CONFIG_USE_LWIP

/**
 *  @brief
 *   Fully initializes the `WeaveAlarmServer` object.
 *
 *  @param[in] aExchangeMgr A pointer to the `WeaveExchangeManager` object
 *
 *  @param[in] aAppState    A pointer to the context object that will be
 *                          used with callbacks
 */

WEAVE_ERROR WeaveAlarmServer::Init(WeaveExchangeManager *aExchangeMgr, void *aAppState)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    mAlarmDelegate = NULL;
    mCurrentDelegateOp = NULL;
    mAppState = aAppState;

    if (ExchangeMgr != NULL)
    {
        return WEAVE_NO_ERROR;
    }

    err = InitializeBackingStore();
    if (err == WEAVE_NO_ERROR)
    {
        ExchangeMgr = aExchangeMgr;
        FabricState = aExchangeMgr->FabricState;
        ExchangeMgr->RegisterUnsolicitedMessageHandler(kWeaveProfile_Alarm, UnsolicitedMessageHandler, true, this);
    }

    for (int i = 0; i< MAX_CONCURRENT_ALERTS; i++)
    {
        mClientPool[i].initAlarmClient(this);
    }

    return err;
}


/**
 * @brief
 *   Shutdown the `WeaveAlarmServer`
 *
 * Closes all the existing `WeaveAlarmClient`s, shuts down any internal
 * buffering structures, and informs the `WeaveExchangeManager` to stop
 * accepting messages destined for Weave Alarm profile.
 *
 * @note This method logically terminates the object.  After calling
 *       `Shutdown`, the only permissible calls on the object are:
 *       - the object destructor
 *       - `WeaveAlarmServer::Init()` that can be used to reanimate the
 *       object.
 *
 * @retval #WEAVE_NO_ERROR unconditionally.
 */
WEAVE_ERROR WeaveAlarmServer::Shutdown(void)
{
    for (int i = 0; i < MAX_CONCURRENT_ALERTS; i++)
    {
        mClientPool[i].Close();
    }

    if (ExchangeMgr != NULL)
    {
        ExchangeMgr->UnregisterUnsolicitedMessageHandler(kWeaveProfile_Alarm, kAlarmMessageType_Alarm);
        ExchangeMgr = NULL;
    }

    ShutdownBackingStore();
    FabricState = NULL;
    return WEAVE_NO_ERROR;
}


/**
 * @brief
 *   A helper method that determines whether there are any
 *   available `WeaveAlarmClient`s.
 *
 * @return `true` if a call to allocate a `WeaveAlarmClient` for a remote
 *         alarm session can succeed immediately; otherwise `false`.
 */
bool WeaveAlarmServer::IsRemoteClientPoolFull(void) const
{
    bool result = true;
    for (int i = 0; i < MAX_CONCURRENT_ALERTS; i++)
    {
        if (mClientPool[i].GetClientState() == WeaveAlarmClient::kAlarmClientState_Closed)
        {
            if (!mClientPool[i].IsLocalAlarm())
            {
                // return false if there is a closed remote client
                result = false;
                break;
            }
        }
    }

    return result;
}


/**
 * @brief
 *   Find and close an alarm of lesser importance than the given alarm.
 *
 * The method is used in cases of extreme resource pressure to close
 * clients serving the alarms of lesser priority than the new,
 * incoming alarm.  It uses `AlarmDelegate::CompareSeverity()` to
 * compare the alarms to find any alarm that is less important.  If
 * such an alarm is found, it is closed, and is available for
 * subsequent allocation
 *
 * @param[in] aSrcNodeId   Node ID of the new alarm originator.
 *
 * @param[in] aAlarm       A read-only reference to the alarm message
 *                         containing the new alarm.
 *
 * @retval #WEAVE_NO_ERROR If an alarm of smaller severity was found and closed.
 *
 * @retval #WEAVE_ERROR_NO_MEMORY If the new alarm is of smaller
 *         importance than anything currently serviced.
 */
WEAVE_ERROR WeaveAlarmServer::CloseLessSevereAlarmClient(uint64_t aSrcNodeId, const Alarm & aAlarm)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    WeaveAlarmClient * client = NULL;

    VerifyOrExit(NULL != mAlarmDelegate, err = WEAVE_ERROR_INCORRECT_STATE);

    // pass 1: find less severe client to close
    for (int i = 0; i < MAX_CONCURRENT_ALERTS; i++)
    {
        if (mClientPool[i].IsLocalAlarm())
        {
            // skip all local alarms
            continue;
        }

        if (mAlarmDelegate->CompareSeverity (mClientPool[i].mCurrentAlarm, aAlarm) < 0)
        {
            // we found one client which is less severe than the incoming one, close it
            // application layer would be noticed before this function returns
            WeaveLogProgress(Alarm, "[%d] Closing less severe alarm from 0x%" PRIx64, i, mClientPool[i].GetOriginator());

            client = &mClientPool[i];
            ExitNow();
        }
    }

    // pass 2: find lowest src node id in all equally severe clients to close
    for (int i = 0; i < MAX_CONCURRENT_ALERTS; i++)
    {
        if (mClientPool[i].IsLocalAlarm())
        {
            // skip all local alarms
            continue;
        }

        if (mClientPool[i].GetOriginator() <= aSrcNodeId)
        {
            // skip all remote alarms that has equal or less src node id
            continue;
        }

        if (0 == mAlarmDelegate->CompareSeverity (mClientPool[i].mCurrentAlarm, aAlarm))
        {
            // we're targeting at the remote client that is
            // 1) equally severe, and
            // 2) has the largest src node id
            if (NULL == client)
            {
                client = &mClientPool[i];
            }
            else
            {
                if (client->GetOriginator() < mClientPool[i].GetOriginator())
                {
                    client = &mClientPool[i];
                }
            }
        }
    }

    if (NULL != client)
    {
        WeaveLogDetail(Alarm, "[%d] Closing equally severe alarm from 0x%" PRIx64, GetClientIndex(client), client->GetOriginator());
        ExitNow();
    }
    else
    {
        // note that we've filtered out all less severe remote clients,
        // and searched for all equally severe remote clients
        // so if client is still NULL, it means the incoming remote alarm is
        // 1) not very important, or
        // 2) has a larger node id
        ExitNow(err = WEAVE_ERROR_NO_MEMORY);
    }

exit:
    WeaveLogFunctError(err);

    if (NULL != client)
    {
        client->SetClientState_Closed();
    }

    return err;
}


/**
 * @brief
 *   Allocate and initialize a `WeaveAlarmClient` given an exchange
 *   context.
 *
 * The method finds an unused `WeaveAlarmClient` (client in a Closed
 * state), and allocates it with the given `ExchangeContext`.  If no
 * unused clients can be found, the `ExchangeContext` is closed.
 *
 * @param[in] aEc             A pointer to an `ExchangeContext` to
 *                            associate with the `WeaveAlarmClient`.
 *
 * @param[in] aEncryptionType Encryption type to use on the
 *                            `ExchangeContext`.
 *
 * @param[in] aKeyId         ID of the security key to use with the
 *                           `ExchangeContext`
 *
 * @param[in] aRemoteOnly    Disallow the allocation algorithm to
 *                           use `WeaveAlarmClient` that have been
 *                           marked for local use.
 *
 * @return A pointer to the `WeaveAlarmClient` on success; otherwise,
 *         NULL on failure.
 */
WeaveAlarmClient *WeaveAlarmServer::AllocAlarmClientUsingExchangeContext(
    ExchangeContext *aEc, uint8_t aEncryptionType, uint16_t aKeyId, bool aRemoteOnly)
{
    WeaveAlarmClient * client = NULL;
    for (int i = 0; i < MAX_CONCURRENT_ALERTS; i++)
    {
        if (mClientPool[i].GetClientState() == WeaveAlarmClient::kAlarmClientState_Closed)
        {
            client = &mClientPool[i];

            if (aRemoteOnly)
            {
                // skip any client which claims it's a local alarm
                if (client->IsLocalAlarm())
                {
                    WeaveLogError(Alarm, "[%d] ERROR: Local alarm is in CLOSED state", i);
                    continue;
                }
            }

            client->mEc = aEc;

            aEc->EncryptionType = aEncryptionType;
            aEc->KeyId = aKeyId;

            // enforce multicast address irrespective of any address the incoming message may imply
            aEc->PeerAddr = ALARM_MULTICAST_ADDR;

            // enable duplicate messages on this exchange
            aEc->AllowDuplicateMsgs = true;

            aEc->OnRetransmissionTimeout = WeaveAlarmClient::HandleRetransmissionTimeout;

            aEc->AppState = client;
            break;
        }
    }

    // close ec on allocation failure (otherwise it should already be held by the alarm client
    if (NULL == client)
    {
        aEc->Close();
        aEc = NULL;
    }
    return client;
}


/**
 * @brief
 *   Allocates and initializes a `WeaveAlarmClient` from the pool.
 *
 * @param[in] aPeer           Node ID of the peer that will receive
 *                            messages in this exchange.  In the
 *                            typical alarm usage, where the alarm is
 *                            disseminated to all nodes in the
 *                            network, the peer will be #kAnyNodeId.
 *
 * @param[in] aEncryptionType Encryption type to use for this client.
 *
 * @param[in] aKeyId          Key to use with this client.
 *
 * @return On success, a pointer to the allocated and initialized
 *         `WeaveAlarmClient`; otherwise, NULL if the allocation of
 *         either the `WeaveAlarmClient` or its `ExchangeContext`
 *         failed.
 */
WeaveAlarmClient * WeaveAlarmServer::NewClient(uint64_t aPeer, uint8_t aEncryptionType, uint16_t aKeyId)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    WeaveAlarmClient *client = NULL;
    ExchangeContext *ec = ExchangeMgr->NewContext(aPeer);

    VerifyOrExit(NULL != ec, err = WEAVE_ERROR_NO_MEMORY);

    // ec is either owned by the alarm client on success allocation, or closed on allocation failure
    client = AllocAlarmClientUsingExchangeContext(ec, aEncryptionType, aKeyId, false);
    VerifyOrExit(NULL != client, err = WEAVE_ERROR_NO_MEMORY);

    client->mIsLocalAlarm = true;

    // occupy this client slot (so it doesn't show as "closed" before application layer call "SendAlarm"
    // also enforce session ID check when any incoming message got accidentally delivered into that exchange context
    client->ForceClientStateChangeNoError(WeaveAlarmClient::kAlarmClientState_Linger);

    // create a new session every time a local client is created, mostly at boot up
    client->RegenerateSessionId();

exit:
    WeaveLogFunctError(err);

    return client;
}


/**
 * @brief
 *   Dispatch the incoming alarm message to an appropriate
 *   `WeaveAlarmClient`.
 *
 * The function parses out the incoming `PacketBuffer` into an `Alarm`
 * message.  If the message was parsed successfully, and passed a
 * number of other checks (such as coming in on the designated
 * interface), the function proceeds to find an appropriate
 * `WeaveAlarmClient`. First, the function checks whether the incoming
 * alarm message already belongs to an existing `WeaveAlarmClient`,
 * based on a session ID and source node ID.  If no matching client
 * has been found, the method attempts to allocate a new
 * `WeaveAlarmClient`.  If no such client has been found, the method
 * proceeds to discard the least severe alarm from the list; if the
 * incoming alarm has a smaller severity than the existing alarms, the
 * processing stops, and the incoming alarm is discarded.  Otherwise,
 * the method has obtained a valid `WeaveAlarmClient`, and uses it to
 * dispatch the incoming alarm message.
 *
 * @param[in] aPktInfo   A read-only pointer to a `IPPacketInfo`
 *                       structure containing the interface used to
 *                       receive the packet.
 *
 * @param[in] aMsgInfo A read-only pointer to a `WeaveMessageInfo`
 *                       containing the information about the incoming
 *                       packet.

 * @param[in] aPayload   A pointer to a `PacketBuffer` containing the
 *                       alarm payload.
 */
void WeaveAlarmServer::DispatchAlarmMessage(const IPPacketInfo *aPktInfo, const WeaveMessageInfo *aMsgInfo, PacketBuffer *aPayload)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    bool parseOk = false;
    Alarm parsedAlarm;

    SuccessOrExit(err = Alarm::parse(aPayload, &parsedAlarm));
    parseOk = true;

    if (parsedAlarm.mIsSessionIdAndExtEvtSnValid)
    {
        WeaveLogProgress(Alarm, "Incoming Src:0x%" PRIx64 ", session:0x%x, evtSN:%u",
            aMsgInfo->SourceNodeId,
            parsedAlarm.mSessionId,
            parsedAlarm.mExtEvtSeqNum);
    }
    else
    {
        WeaveLogProgress(Alarm, "Incoming Src:0x%" PRIx64 ", ctr:%u",
            aMsgInfo->SourceNodeId,
            parsedAlarm.mAlarmCtr);
    }

    // if the server has an assigned interface but the alarm message
    // came in on some unknown interface, drop it.
    if (INET_NULL_INTERFACEID != mInterfaceId)
    {
        if (aPktInfo->Interface != mInterfaceId)
        {
            // Note that InterfaceId is typdef to be a pointer with LwIP, but an unsigned int otherwise.
            // Casting it to uint64_t should get us reasonable results on all platforms.
            WeaveLogError(Alarm, "Reject: unexpected i/f 0x%" PRIx64 ", expecting 0x%" PRIx64, (uint64_t)aPktInfo->Interface, (uint64_t)mInterfaceId);
            ExitNow(err = WEAVE_ERROR_NO_ENDPOINT);
        }
    }

#if WEAVE_ALARM_DETAIL_LOGGING && WEAVE_DETAIL_LOGGING
    for (int i = 0; i < MAX_CONCURRENT_ALERTS; i++)
    {
        WeaveAlarmClient &client = mClientPool[i];
        if (client.GetClientState() == WeaveAlarmClient::kAlarmClientState_Closed)
        {
            WeaveLogDetail(Alarm, "[%d] [%6s]", i, WeaveAlarmClient::GetClientStateName(client.GetClientState()));
        }
        else
        {
            if (client.mCurrentAlarm.mIsSessionIdAndExtEvtSnValid)
            {
                WeaveLogDetail(Alarm, "[%d] [%6s] Src:0x%" PRIx64 ", Session:0x%" PRIx32 ", evtSN:%u",
                            i,
                            WeaveAlarmClient::GetClientStateName(client.GetClientState()),
                            client.GetOriginator(),
                            client.mCurrentAlarm.mSessionId,
                            client.mCurrentAlarm.mExtEvtSeqNum);
            }
            else
            {
                WeaveLogDetail(Alarm, "[%d] [%6s] Src:0x%" PRIx64 ", ctr:%u",
                            i,
                            WeaveAlarmClient::GetClientStateName(client.GetClientState()),
                            client.GetOriginator(),
                            client.mCurrentAlarm.mAlarmCtr);
            }
        }
    }
#endif // WEAVE_DETAIL_LOGGING

    // find if we already have a client handling alarm messages from
    // this source and session ID.  This occurs when we overhear one of our neighbors
    // rebroadcasting our own message; the ExchangeContext matching
    // logic in WeaveExchangeMgr will not pass the message into the
    // ExchangeContext associated with ourselves, because the
    // Initiator bit is set and that might indicate that someone else
    // is initiating an exchange.  We perform the check across the
    // board to further enforce the notion that all the alarms
    // originating at a single node and carry the same session ID should be handled within the same
    // WeaveAlarmClient.
    for (int i = 0; i < MAX_CONCURRENT_ALERTS; i++)
    {
        WeaveAlarmClient &client = mClientPool[i];
        // skip entries that are already closed
        // skip entries that have different node ID
        // skip entries that have different session ID settings (this is pretty suspicious)
        // (if I have one) skip any entry that doesn't have a matching session id with mine
        if ((WeaveAlarmClient::kAlarmClientState_Closed != client.GetClientState()) &&
            (client.GetOriginator() == aMsgInfo->SourceNodeId) &&
            (parsedAlarm.mIsSessionIdAndExtEvtSnValid == client.mCurrentAlarm.mIsSessionIdAndExtEvtSnValid) &&
            ((!parsedAlarm.mIsSessionIdAndExtEvtSnValid) || (parsedAlarm.mIsSessionIdAndExtEvtSnValid && (client.mCurrentAlarm.mSessionId == parsedAlarm.mSessionId))))
        {
            // dispatch to an existing alarm cient, with all the information we have at hand.
            // there is no need to pass profile id and message type, as we can handle one combination.
            // we still need to pass payload, as it's possible the client needs to update its retransmission cache
            client.HandleAlarm(aPktInfo, aMsgInfo, aPayload, parsedAlarm);
            // payload should have been freed in handler
            aPayload = NULL;
            ExitNow();
        }
    }

    // no match is found, is this one from our own past lives?
    if (FabricState->LocalNodeId == aMsgInfo->SourceNodeId)
    {
        // so, we have an incoming message from ourselves, but couldn't find a match, probably because we have retired that session
        // discard the message
        WeaveLogDetail(Alarm, "Drop: no active client");
        WeaveLogDetail(Alarm, "Src:0x%" PRIx64 ", Session:0x%x, ctr:%u, evtSN:%u",
            aMsgInfo->SourceNodeId,
            parsedAlarm.mIsSessionIdAndExtEvtSnValid ? parsedAlarm.mSessionId : 0,
            parsedAlarm.mAlarmCtr,
            parsedAlarm.mExtEvtSeqNum);
        // payload will be freed when we exit
        ExitNow ();
    }

    {
        // We didn't find an existing client, and it's not an echo, so it's time to create a new client
        WeaveLogProgress(Alarm, "Creating client");

        // first step, make sure we have some space for this new comer
        if (IsRemoteClientPoolFull())
        {
            // okay, the pool for remote clients is full
            // let's try to make some room
            // if we fail at here, we'd still close the payload and notify application layer when we exit
            SuccessOrExit(err = CloseLessSevereAlarmClient(aMsgInfo->SourceNodeId, parsedAlarm));
        }

        // create an exchange context, note that appstate is NULL initially, as we haven't had the client yet
        // note that a new exchange context has the "initiator" bit set by default, so no re-multicasts would be mapped into one by default
        ExchangeContext *newExchangeContext = ExchangeMgr->NewContext(aMsgInfo->SourceNodeId, aPktInfo->SrcAddress, aPktInfo->SrcPort, aPktInfo->Interface);
        VerifyOrExit (NULL != newExchangeContext, err = WEAVE_ERROR_NO_MEMORY);
        // newExchangeContext is either owned by the alarm client on success allocation, or closed on allocation failure
        WeaveAlarmClient *client = AllocAlarmClientUsingExchangeContext(newExchangeContext,
            aMsgInfo->EncryptionType, aMsgInfo->KeyId, true);
        VerifyOrExit(NULL != client, err = WEAVE_ERROR_NO_MEMORY);
        client->HandleAlarm(aPktInfo, aMsgInfo, aPayload, parsedAlarm);
        // payload should have been freed in handler
        aPayload = NULL;
    }

exit:
    WeaveLogFunctError(err);

    if (NULL != aPayload)
    {
        PacketBuffer::Free(aPayload);
        aPayload = NULL;
    }

    if (WEAVE_NO_ERROR != err)
    {
        // oops! we couldn't handle this alarm message
        WeaveLogError(Alarm, "Drop msg");
        AlarmLogDetail(parsedAlarm);

        if (parseOk && (NULL != mAlarmDelegate))
        {
            mAlarmDelegate->OnNewRemoteAlarmDropped(parsedAlarm);
        }
    }
}


/**
 * @brief
 *   Assign the `AlarmDelegate` to the `WeaveAlarmServer`.
 *
 * @param[in] aDelegate  Application-layer delegate for handling alarm
 *                       semantics.  If a NULL is passed, the existing
 *                       delegate (if any) will be removed, and
 *                       further forwarding of alarm state to the
 *                       application layer will be suspended, but the
 *                       `WeaveAlarmServer` will continue to receive
 *                       and process messages according to the alarm
 *                       protocol specifications.
 */
void WeaveAlarmServer::SetAlarmDelegate(AlarmDelegate * aDelegate)
{
    mAlarmDelegate = aDelegate;
}

/**
 * @brief
 *   Pack and send an `AlarmHushResponse` with the given
 *   information. Current operation is considered closed after this
 *   call
 *
 * @param[in] hushResult A status code to be returned in this
 *                       `AlarmHushResponse` message (check the
 *                       kAlarmHushResult_XXX family of constants)
 *
 * @param[in] length     The number of valid entries in alarmCondition
 *
 * @param[in] conditions The pointer to the array of alarm conditions.
 *                       When the length parameter is zero, this
 *                       pointer may be NULL
 *
 * @retval #WEAVE_NO_ERROR              On success.
 *
 * @retval #WEAVE_ERROR_INCORRECT_STATE If there is no active hush
 *         request.
 *
 * @retval #WEAVE_ERROR_NO_MEMORY       If the system was unable to
 *         allocate memory for the hush response.
 *
 * @retval #WEAVE_ERROR Error codes returned by `SendMessage`.
 */
WEAVE_ERROR WeaveAlarmServer::SendHushResponse(uint8_t aHshResult, uint8_t aLength, uint8_t *aConditions)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    PacketBuffer *msgBuf = NULL;
    AlarmHushResponse alarmHushResponse;

    VerifyOrExit(mCurrentDelegateOp != NULL, err = WEAVE_ERROR_INCORRECT_STATE);

    err = alarmHushResponse.init(aHshResult, aLength, aConditions);
    SuccessOrExit(err);

    // allocate buffer and then encode the response into it
    msgBuf = PacketBuffer::New();
    VerifyOrExit(msgBuf != NULL, err = WEAVE_ERROR_NO_MEMORY);

    err = alarmHushResponse.pack(msgBuf);
    SuccessOrExit(err);

    // send out the response
    err = mCurrentDelegateOp->SendMessage(kWeaveProfile_Alarm, kAlarmMessageType_AlarmHushResponse, msgBuf);
    msgBuf = NULL;
    SuccessOrExit(err);

exit:
    if (msgBuf != NULL)
    {
        PacketBuffer::Free(msgBuf);
        msgBuf = NULL;
    }

    if (mCurrentDelegateOp != NULL)
    {
        mCurrentDelegateOp->Close();
        mCurrentDelegateOp = NULL;
    }

    return err;
}

/**
 * @brief
 *   Pack and send a StatusUpdate with the given information. Current
 *   operation is considered closed after this call
 *
 * @param[in] statusProfileId A profile id for the status code.
 *
 * @param[in] statusCode      A profile-specific status code.
 *
 * @param[in] sysError        Extra error information, if needed.
 *
 * @retval #WEAVE_NO_ERROR              On success.
 *
 * @retval #WEAVE_ERROR_INCORRECT_STATE If there is no active hush
 *         request.
 *
 * @retval #WEAVE_ERROR Error codes produced by `SendStatusReport()`
 */
WEAVE_ERROR WeaveAlarmServer::SendStatusReport(uint32_t aStatusProfileId, uint16_t aStatusCode, WEAVE_ERROR aSysError)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    VerifyOrExit(mCurrentDelegateOp != NULL, err = WEAVE_ERROR_INCORRECT_STATE);

    err = nl::Weave::WeaveServerBase::SendStatusReport(mCurrentDelegateOp, aStatusProfileId, aStatusCode, aSysError);
    SuccessOrExit(err);

exit:
    if (mCurrentDelegateOp != NULL)
    {
        mCurrentDelegateOp->Close();
        mCurrentDelegateOp = NULL;
    }

    return err;
}


/**
 * @brief
 *   Handle unsolicited alarm messages
 *
 * Alarm messages are handled differently than other message types: we
 * always close the incoming `ExchangeContext` and then create a new
 * one for new clients.  As a result, we receive all `Alarm` messages
 * through this `UnsolicitedMessageHandler`. The primary reason for
 * this design is the session ID mechanism: it can invalidate the
 * dispatching decision made in `WeaveExchangeManager`, so it is
 * better to do our own dispatch every time.  The Alarm message
 * processing flows from this method, through
 * `WeaveAlarmServer::DispatchAlarmMessage()` to
 * `WeaveAlarmClient::HandleAlarm()`.
 *
 * This method also handles Hush request processing; the handling of
 * the Hush messages is much closer to the canonical Weave message
 * dispatch.
 *
 * @param[in] aEc        A pointer to an `ExchangeContext`
 *                       instance created to handle the incoming
 *                       message
 *
 * @param[in] aPktInfo   A read-only pointer to a `IPPacketInfo`
 *                       structure containing the interface used to
 *                       receive the packet.
 *
 * @param[in] aMsgInfo A read-only pointer to a `WeaveMessageInfo`
 *                       containing the information about the incoming
 *                       packet.
 *
 * @param[in] aProfileId Profile ID extracted from the incoming
 *                       message (WeaveExchange layer header)
 *
 * @param[in] aMsgType   Message type extracted from the incoming
 *                       message (WeaveExchange layer header)
 *
 * @param[in] aPayload   A pointer to a `PacketBuffer` containing the
 *                       payload of one of the messages from Weave Alarm.

 */
void WeaveAlarmServer::UnsolicitedMessageHandler(ExchangeContext *aEc, const IPPacketInfo *aPktInfo,
    const WeaveMessageInfo *aMsgInfo, uint32_t aProfileId, uint8_t aMsgType, PacketBuffer *aPayload)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    WeaveAlarmServer *server = NULL;
    bool newOpCreated = false;

    VerifyOrExit(NULL != aEc, err = WEAVE_ERROR_INCORRECT_STATE);
    VerifyOrExit(NULL != aEc->AppState, err = WEAVE_ERROR_INCORRECT_STATE);
    VerifyOrExit(kWeaveProfile_Alarm == aProfileId, err = WEAVE_ERROR_INCORRECT_STATE);

    server = (WeaveAlarmServer *) aEc->AppState;

    // Alarm messages are handled differently than other message types
    // because they do not need to invoke the alarm delegate, nor use the stored
    // exchange context for delegated operations
    if (kAlarmMessageType_Alarm == aMsgType)
    {
        // always close the incoming Exchange Context and then create a new one for new clients, so we always
        // receive Alarm message through this UnsolicitedMessageHandler. The reason is the added session ID mechanism
        // invalidates the dispatching decision made in Exchange Manager, so we're better off doing the dispatching on
        // our own every time.
        // potentially speaking we can just flip the initiator bit in this ec instead of closing it, but closing it simplifies
        // the code paths for different situations and doesn't consume much more resource.
        aEc->Close();
        aEc = NULL;

        server->DispatchAlarmMessage(aPktInfo, aMsgInfo, aPayload);
        // payload should have been freed as part of alarm processing, so let's forget about it
        aPayload = NULL;
        ExitNow();
    }

    // Disallow simultaneous requests
    if (NULL != server->mCurrentDelegateOp)
    {
        err = WeaveServerBase::SendStatusReport(aEc, kWeaveProfile_Common, Common::kStatus_Busy, WEAVE_NO_ERROR);
        ExitNow();
    }

    // make sure we have a delegate to handle these requests
    if (NULL == server->mAlarmDelegate)
    {
        err = WeaveServerBase::SendStatusReport(aEc, kWeaveProfile_Common, Common::kStatus_UnexpectedMessage, WEAVE_NO_ERROR);
        ExitNow();
    }

    // transfer ownership of the ec to the current delegate operation
    // any thing goes wrong from here, the (newly gained) operation would be closed
    server->mCurrentDelegateOp = aEc;
    aEc = NULL;
    newOpCreated = true;

    // Decode and dispatch the message.
    switch (aMsgType)
    {
    case kAlarmMessageType_AlarmHushRequest:
        err = server->HandleHushRequest(aPayload);
        // payload should have been freed during processing, so let's forget about it
        aPayload = NULL;
        break;
    default:
        WeaveLogError(Alarm, "unsupported msg");
        err = server->SendStatusReport(kWeaveProfile_Common, Common::kStatus_UnsupportedMessage);
    }

exit:
    WeaveLogFunctError(err);

    if (NULL != aEc)
    {
        aEc->Close();
        aEc = NULL;
    }

    if (aPayload != NULL)
    {
        PacketBuffer::Free(aPayload);
        aPayload = NULL;
    }

    // close the current operation if there is any error
    if ((NULL != server) && (WEAVE_NO_ERROR != err))
    {
        if (newOpCreated && (NULL != server->mCurrentDelegateOp))
        {
            server->mCurrentDelegateOp->Close();
            server->mCurrentDelegateOp = NULL;
        }
    }
}

/**
 * @brief
 *   Process Hush request.
 *
 * Parses the hush request out of the incoming `PacketBuffer`, and
 * dispatches it to the `AlarmDelegate` for further processing. If the
 * message could not be parsed, a status report is sent to the peer
 * with a code #Common::kStatus_BadRequest.
 *
 * @param aPayload A pointer to an `PacketBuffer` containing the payload.
 *
 * @return #WEAVE_NO_ERROR on success; otherwise, on failure, return
 *         the error code from the application layer processing
 */
WEAVE_ERROR WeaveAlarmServer::HandleHushRequest(PacketBuffer *aPayload)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    AlarmHushRequest hushRequest;

    err = AlarmHushRequest::parse(aPayload, &hushRequest);
    VerifyOrExit(WEAVE_NO_ERROR == err, SendStatusReport(kWeaveProfile_Common, Common::kStatus_BadRequest));

    err = mAlarmDelegate->OnHushRequest(mCurrentDelegateOp, hushRequest.mProximityVerificationCode, hushRequest.mSignature);
    SuccessOrExit(err);

exit:
    WeaveLogFunctError(err);

    if (aPayload != NULL)
    {
        PacketBuffer::Free(aPayload);
        aPayload = NULL;
    }

    return err;
}


/**
 * @brief
 *   Default constructor for helper class for iterating through
 *   `WeaveAlarmClient`s.
 *
 * @param[in] aServer A read-only pointer to a `WeaveAlarmServer`
 *                    object that owns the client pool that will be
 *                    iterated over
 */
WeaveAlarmClientIterator::WeaveAlarmClientIterator(WeaveAlarmServer *aServer)
{
    mServer = aServer;

    // when we initialize, the "current" is undefined. User of this object must call "next" to get the head object
    mIndex = -1;
}

/**
 * @brief
 *   Iterator method, determine whether the iterator has a next element.
 *
 * @return `true` if there are more elements in the iterator; otherwise `false`.
 */
bool WeaveAlarmClientIterator::hasNext(void)
{
    return ((mIndex + 1) < MAX_CONCURRENT_ALERTS);
}

/**
 * @brief
 *   Fetch the next item in the iterator.
 *
 * @note This method performs no bounds checking.  Instead, it assumes
 *       that the user of the method has already ascertained that the
 *       iterator will return a valid pointer by calling
 *       `WeaveAlarmClientIterator::hasNext()`.
 *
 * @return Next element in the iterator, unconditionally.
 */
WeaveAlarmClient * WeaveAlarmClientIterator::next(void)
{
    mIndex++;
    return &(mServer->mClientPool[mIndex]);
}


}
}
}
