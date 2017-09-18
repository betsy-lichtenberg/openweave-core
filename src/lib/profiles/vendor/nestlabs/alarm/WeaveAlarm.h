/* -*- Mode: C++; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil -*-
 *
 *    Copyright (c) 2013-2016 Nest Labs, Inc.
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
 *      This file defines constants, classes, and messages used by the Weave Alarm Profile.
 *
 */

#ifndef _WEAVE_ALARM_PROFILE_H
#define _WEAVE_ALARM_PROFILE_H


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

#include <Weave/Support/NLDLLUtil.h>

#include <Weave/Core/WeaveCore.h>
#include <Weave/Core/WeaveServerBase.h>
#include <Weave/Profiles/ProfileCommon.h>
#include <Weave/Support/crypto/HMAC.h>

/**
 * @name Alarm state definitions
 *
 * Alarm conditions are composed of the alarm source and alarm state.
 * Alarm state occupies the lower four bits of the alarm condition.
 * The following states are defined at this time:
 *
 * @{
 */

/**
 * @def  WEAVE_ALARM_STATE_STANDBY
 *
 * @brief
 *   Alarm state indicating that the alarm is in the "all clear" state.
 *
 */

#define WEAVE_ALARM_STATE_STANDBY     0x00
/**
 * @def  WEAVE_ALARM_STATE_HEADS_UP_1
 *
 * @brief
 *   Pre-alarm state: the first heads-up threshold was exceeded
 *
 */

#define WEAVE_ALARM_STATE_HEADS_UP_1  0x01

/**
 * @def  WEAVE_ALARM_STATE_HEADS_UP_2
 *
 * @brief
 *   Pre-alarm state: the second heads-up threshold was exceeded
 *
 */

#define WEAVE_ALARM_STATE_HEADS_UP_2  0x02

/**
 * @def  WEAVE_ALARM_STATE_HU_HUSH
 *
 * @brief
 *   Pre-alarm state: the alarm was hushed either in heads up 1 or
 *   heads up 2 state.
 *
 */

#define WEAVE_ALARM_STATE_HU_HUSH     0x03

/**
 * @def  WEAVE_ALARM_STATE_ALARM_HUSHABLE
 *
 * @brief
 *   Alarm state indicating that the alarm is active but may be
 *   hushed.
 *
 */

#define WEAVE_ALARM_STATE_ALARM_HUSHABLE 0x04

/**
 * @def  WEAVE_ALARM_STATE_ALARM_NONHUSHABLE
 *
 * @brief
 *   Alarm state indicating that the alarm is active but may be
 *   not hushed.
 *
 */

#define WEAVE_ALARM_STATE_ALARM_NONHUSHABLE 0x05

/**
 * @def  WEAVE_ALARM_STATE_ALARM_GLOBAL_HUSH
 *
 * @brief
 *   Alarm state indicating that all the participating nodes
 *   (both the originating node and the remote nodes) are hushed.
 *
 */

#define WEAVE_ALARM_STATE_ALARM_GLOBAL_HUSH 0x06

/**
 * @def  WEAVE_ALARM_STATE_ALARM_REMOTE_HUSH
 *
 * @brief
 *   Alarm state indicating that the originating node is alarming
 *   and the remote nodes are hushed.
 *
 */

#define WEAVE_ALARM_STATE_ALARM_REMOTE_HUSH 0x07

/**
 * @def  WEAVE_ALARM_STATE_SELFTEST
 *
 * @brief
 *   Alarm state indicating self test state.
 *
 */

#define WEAVE_ALARM_STATE_SELFTEST    0x08

/**
 * @def  WEAVE_ALARM_ANNOUNCE_HEADS_UP_1
 *
 * @brief
 *   Alarm state indicating heads-up state 1.
 *
 */

#define WEAVE_ALARM_ANNOUNCE_HEADS_UP_1  0x09

/**
 * @def  WEAVE_ALARM_ANNOUNCE_HEADS_UP_2
 *
 * @brief
 *   Alarm state indicating heads-up state 2.
 *
 */

#define WEAVE_ALARM_ANNOUNCE_HEADS_UP_2  0x0A

/**
 * @def  WEAVE_ALARM_STATE_INVALID
 *
 * @brief
 *   Alarm state indicating invalid state.
 *
 */

#define WEAVE_ALARM_STATE_INVALID        0xFF

/**
 * @}
 */

/**
 * @name Alarm source
 *
 * Alarm conditions are composed of the alarm source and alarm state.
 * Alarm source occupies the upper 4 bits of the alarm condition.  The
 * following alarm sources are defined at this time:
 *
 * @{
 */

/**
 * @def WEAVE_ALARM_SMOKE
 *
 * @brief
 *   Alarm is triggered by the smoke sensor.
 */

#define WEAVE_ALARM_SMOKE      0x10

/**
 * @def WEAVE_ALARM_TEMP
 *
 * @brief
 *   Alarm is triggered by the temperature sensor.
 */

#define WEAVE_ALARM_TEMP       0x20

/**
 * @def WEAVE_ALARM_CO
 *
 * @brief
 *   Alarm is triggered by the carbon monoxide sensor.
 */

#define WEAVE_ALARM_CO         0x30


/**
 * @def WEAVE_ALARM_CH4
 *
 * @brief
 *   Alarm is triggered by the natural gas sensor.
 */

#define WEAVE_ALARM_CH4        0x40

/**
 * @def WEAVE_ALARM_HUMIDITY
 *
 * @brief
 *   Alarm is triggered by the humidity sensor.
 */

#define WEAVE_ALARM_HUMIDITY   0x50

/**
 * @def WEAVE_ALARM_OTHER
 *
 * @brief
 *   Alarm is triggered by a sensor not captured in the previous
 *   definitions.
 */

#define WEAVE_ALARM_OTHER      0xf0

/**
 * @def WEAVE_ALARM_INVALID
 *
 * @brief
 *   Invalid alarm source.
 */

#define WEAVE_ALARM_INVALID    0xff

/**
 * @}
 */

/**
 * @name Parameters that govern the runtime of the alarm source.
 *
 * The following constants guide the runtime characteristics of Weave
 * Alarm rebroadcasts.
 *
 * @{
 */

/**
 * @def WEAVE_ALARM_DEFAULT_REBROADCAST_PERIOD_MSEC
 *
 * @brief
 *   Defines the default interval, in milliseconds, of rebroadcasting
 *   the alarm message.
 *
 * This constant defines the time interval that guides the rebroadcast
 * interval of the current message.  Each node participating in the
 * alarm protocol will resend a message at most once within that time
 * interval.  The constant is analogous to `T` in the Trickle algorithm
 * (<a href="https://tools.ietf.org/html/rfc6206">RFC 6206</a>).
 */

#define WEAVE_ALARM_DEFAULT_REBROADCAST_PERIOD_MSEC 3000   // in ms

/**
 * @def WEAVE_ALARM_DEFAULT_REBROADCAST_THRESH
 *
 * @brief
 *   Number of received broadcasts required to suppress retransmission
 *
 * The constant is analogous to redundancy constant `k` in Trickle
 * algorithm (<a href="https://tools.ietf.org/html/rfc6206">RFC
 * 6206</a>) description.  Its current value is an artifact of the
 * behaviors observed in the 802.15.4 stack used, where each link
 * local multicast was automatically resent by the firmware three
 * times.
 *
 */

#define WEAVE_ALARM_DEFAULT_REBROADCAST_THRESH 6

/**
 * @def WEAVE_ALARM_DEFAULT_REBROADCAST_TIMEOUT_MSEC
 *
 * @brief
 *   Defines the default interval, in milliseconds, of refreshing the
 *   alarm message.
 *
 * The constant defines the how frequently the alarm should be
 * refreshed before it is considered stale.  Stale alarms cease to be
 * rebroadcast.  The constant should be several lengths of the
 * #WEAVE_ALARM_DEFAULT_REBROADCAST_PERIOD_MSEC.  After an alarm
 * becomes stale, the associated alarm session enters the grace
 * period.
 *
 */

#define WEAVE_ALARM_DEFAULT_REBROADCAST_TIMEOUT_MSEC 30000

/**
 * @def WEAVE_ALARM_MAX_NUM_HOPS
 *
 * @brief
 *   The max number of hops an alarm event is expected to reach
 *
 * The constant is used to drive timeouts dependent on the alarm
 * propagation delay across the network.
 *
 */

#define WEAVE_ALARM_MAX_NUM_HOPS 4

/**
 * @def WEAVE_ALARM_DEFAULT_GRACE_PERIOD_MSEC
 *
 * @brief
 *   Defines the default duration, in milliseconds, of the grace
 *   period.
 *
 * After the alarm becomes stale, the associated session enters the
 * grace period.  During the grace period, the node remains awake,
 * listening for the any delayed alarm notifications. The constant
 * reflects the maximum propagation delay for alarm messages across
 * the mesh network for a given level of reliable delivery.
 *
 */

#define WEAVE_ALARM_DEFAULT_GRACE_PERIOD_MSEC 30000

/**
 * @}
 */

/**
 *  @name Constants driving the size of state occupied by the Weave Alarm
 *
 * @{
 */

/**
 * @def MAX_CONCURRENT_ALERTS
 *
 * @brief
 *   Defines the number of concurrent alarms in the network.
 *
 * The constant drives the pool size of objects allocated to track
 * individual alarm sessions.  Any node in the Weave fabric can
 * originate at most a single alarm session at a time, though the
 * specifics of the alarm condition will vary over the lifetime of the
 * session. For example, a single alarm session might start with a
 * pre-alarm carbon monoxide warning, evolve into a non-hushable smoke
 * alarm, and ultimately return to all clear.  The constant should
 * generally be sized the number of devices in the network.  If some
 * alarm aggregation fidelity loss is permitted, the number may
 * alternatively be sized to the number of distinct UX actions taken
 * by the system; e.g. if the alarm system UI speaks different
 * sentences for up to 3 alarm locations, and lumps all other
 * locations into a single sentence, the concurrent alerts could be
 * sized to 5 (1 alarm session for the current node, and 4 for the
 * distinct UX treatments)
 *
 */

#define MAX_CONCURRENT_ALERTS 10

/**
 * @def MAX_CONCURRENT_MESSAGES
 *
 * @brief
 *   Number of concurrent alarm messages allocated by Weave Alarm
 *
 * The number of messages required is either
 * - one message per node in the network, or
 * - one message per concurrent alarm (the alarm session holds onto
 *   the messages for retransmission purposes) plus at least one
 *   message so we can determine whether the incoming alarm message
 *   from a new client is more urgent than any alarm sessions in
 *   displayed in the current UX.
 *
 */

#define MAX_CONCURRENT_MESSAGES ((MAX_CONCURRENT_ALERTS) + 1)

/**
 * @def WEAVE_ALARM_MAX_INCOMING_ALARM_MESSAGE_SIZE
 *
 * @brief
 *   Maximum message size to allocate for Weave Alarm payloads
 *
 * Weave Alarm retains a number of different messages for
 * rebroadcasts.  The standard PacketBuffer objects are sized for
 * maximum sized IP packets, and the PacketBuffer API contains no
 * provisions for allocating smaller messages.  To ease the memory
 * pressure, Weave Alarm maintains its own pool of small messages.
 * The maximum value for the constant below is the size of the small
 * message - alignment offset - #WEAVE_HEADER_RESERVE_SIZE.  For small
 * messages of 128 bytes, the maximum value would be 76.
 *
 */

#define WEAVE_ALARM_MAX_INCOMING_ALARM_MESSAGE_SIZE 64
/**
 * @}
 */

/**
 * @name Other constants
 */

/**
 * @def WEAVE_ALARM_FORWARD_COUNTER_DISTANCE_LIMIT
 *
 * @brief
 *   Maximum valid counter distance
 *
 * Counter is implemented as a `uint8_t` value that rolls over.  The constant
 * defines as the "distance" the next alarm counter can be to be
 * considered as a successor to the current one.  It shall be no more
 * than the half MAX value of the counter (in whatever size is
 * subsequently chosen).
 */

#define WEAVE_ALARM_FORWARD_COUNTER_DISTANCE_LIMIT (UINT8_MAX/2)

// TODO: constant currently unused.  Can we remove it?
// TODO: Replace this once we have the new key id scheme defined in Weave
// WEAVE-232 has been created for this item
/**
 * @def WEAVE_ALARM_HUSH_KEY_ID
 *
 * @brief
 *   ID of a key used to secure the hush request.
 *
 * This constant is intended to be used by application code, during
 * the process of filling and verifying the hush request.  Alarm
 * profile code just packs the id value in and uses the given key to
 * sign the request.
 *
 */

#define WEAVE_ALARM_HUSH_KEY_ID (0x2)

/**
 *  @def ALARM_MULTICAST_ADDR
 *
 *  @brief
 *    If defined, all multicasts related to Alarm messages go to this
 *    address, no matter what the peer node ID implies. Otherwise an
 *    IPv6 link-local multicast address would be picked, as the
 *    default value implies.
 */

#ifndef ALARM_MULTICAST_ADDR
#define ALARM_MULTICAST_ADDR IPAddress::MakeIPv6Multicast(kIPv6MulticastScope_Link, kIPV6MulticastGroup_AllNodes)
#endif // ALARM_MULTICAST_ADDR

/**
 * @}
 */


namespace nl {
namespace Weave {
namespace Profiles {

enum {
    /**
     * Alarm message type.  Alarm messages ultimately originate at
     * the alarm originator, and are sent to all nodes within the
     * network
     */
    kAlarmMessageType_Alarm             = 1,
    /**
     * @deprecated
     * Alarm update message type. Alarm updates are sent from remote
     * nodes to the originator to request state change
     */
    kAlarmMessageType_AlarmUpdate       = 2,
    /**
     * @deprecated
     * Alarm ack message type.  Alarm acks messages are sent from the
     * originator to the remote requesting update, and are used as a
     * reliability layer for the Alarm Update messages.
     */
    kAlarmMessageType_AlarmAck          = 3,
    /**
     * Alarm hush message type.
     */
    kAlarmMessageType_AlarmHushRequest  = 4,
    /**
     * Alarm hush response message type.
     */
    kAlarmMessageType_AlarmHushResponse = 5,
};

/**
 *
 * @brief
 *   Profile-specific status code used in StatusReport as response to
 *   any request.
 *
 * @detail
 *   Common status codes are listed in CommonProfile, like
 *   kStatus_BadRequest and kStatus_Busy.
 *
 */
enum {
    /**
     *  Alarm update succeeded.
     */
    kAlarmUpdateStatus_Success = 0,
    /**
     * @deprecated
     * Alarm update was rejected by the application layer as not
     * applicable under current policy, e.g. requesting a hush for an
     * unhushable alarm.
     */
    kAlarmUpdateStatus_Rejected = 1,
    /**
     * @deprecated
     * Alarm update was rejected as invalid, e.g. it pertained to
     * stale state or specified an invalid transition.
     */
    kAlarmUpdateStatus_Invalid = 2,
    /**
     * @deprecated
     * Alarm update could not be used in a timely manner.
     */
    kAlarmUpdateStatus_Timeout = 3,
    /**
     * Hush was rejected because proximity validation failed.
     */
    kAlarmStatusReport_HushRejected_ProximityValidationFailure  = 4,
    /**
     * Hush was rejected because signature validation failed.
     */
    kAlarmStatusReport_HushRejected_SignatureValidationFailure  = 5,
};

/**
 * @brief
 *   Possible results in an AlarmHushResponse message.
 *
 * @note
 *   The response to a AlarmHushRequest could also be a StatusReport
 *   message, especially when the response is generated within
 *   AlarmServer without contacting the application layer.  The
 *   AlarmHushResponse carries both result code and (updated) alarm
 *   condition.  If no alarm condition is needed in the response,
 *   application layer should consider using a StatusReport message.
 *
 */
enum {
    kAlarmHushResult_Success                                    = 0,    ///< Hush was successful, and the the condition list is valid
};


/**
 * @class Alarm
 *
 * @brief
 *   Message class to represent the Weave Alarm payloads
 *
 * Alarm class contains the values from the Alarm message along with
 * the methods to serialize and deserialize the message, a number of
 * accessor functions, and convenience functions for manipulating
 * alarms.
 */
class NL_DLL_EXPORT Alarm {
    friend class WeaveAlarmClient;
    public:

    enum
    {
        /// Max number of alarm conditions, for both V1 and V2
        kMaxNumAlarmConditions = 8,

        /// Size of payload in V1 alarm messages, without the actual conditions
        /// 1 byte for alarm counter
        /// 1 byte for length
        /// 1 byte for where
        kPayloadSizeWithoutConditions_V1 = 3,

        /// Size of payload in V2 alarm messages, without the actual conditions
        /// everything in V1, plus
        /// 4 bytes for session id
        /// 4 bytes for extended event sequence number
        kPayloadSizeWithoutConditions_V2 = kPayloadSizeWithoutConditions_V1 + 8,
    };

    Alarm(void);
    WEAVE_ERROR init(void);

    bool operator == (const Alarm &) const;

    uint8_t GetAlarmState(uint8_t) const;
    uint8_t GetAlarmCondition(uint8_t) const;
    uint8_t GetAlarm(uint8_t) const;

    void SetAlarmState(uint8_t, uint8_t);
    void SetAlarmCondition(uint8_t, uint8_t);
    void SetAlarm(uint8_t, uint8_t);

    void AddAlarm(uint8_t);
    bool KeepReBroadcasting(void) const;
    // parsing
    WEAVE_ERROR pack(PacketBuffer *);
    static WEAVE_ERROR parse(const PacketBuffer *, Alarm *);

    // data members

    /// Alarm counter used to determine the alarm freshness
    uint8_t mAlarmCtr;

    /// Length of the alarm condition list
    uint8_t mLength;

    //TODO: FIXME: more flexible alarm allocations
    /// List of alarm conditions (each condition contains both the alarm state and alarm source
    uint8_t mConditions[kMaxNumAlarmConditions];

    /// Location of the alarm originator.  The mWhere is populated
    /// with the spoken where ID in the namespace of the spoken
    /// locations.
    uint8_t mWhere;

    /// true if this alarm message comes with session ID,
    /// or if pack should encode session ID into the message buffer
    bool mIsSessionIdAndExtEvtSnValid;

    /// 4-byte session ID
    uint32_t mSessionId;

    /// 4-byte extended event sequence number
    uint32_t mExtEvtSeqNum;
    private:
};

/**
 * @class AlarmHushSignature
 *
 * @brief
 *   storage of a signature for AlarmHushRequest
 *
 * This class can be used to compare two signatures
 */
class AlarmHushSignature {
public:

    AlarmHushSignature(void);

    bool operator == (const AlarmHushSignature &rhs) const;
    bool operator != (const AlarmHushSignature &rhs) const;

    bool mIsSigned;
    uint16_t mKeyId;
    uint8_t mKeyedHash[Crypto::HMACSHA1::kDigestLength];
};

/**
 *
 * @class AlarmHush
 *
 * @brief
 *   pack and parse AlarmHush request message
 *
 * This class can pack as well as parse AlarmHush request message
 */
class AlarmHushRequest {
public:
    enum
    {
        // Note that HMAC is tricky if you use variable key sizes.
        // The best policy is to use a long, fixed-length key with a proper random number generator
        // One example for the trickiness: [1, 0, 0] is the same as [1] to HMAC with block size larger than 3
        kAlarmHush_MinKeySize = 16
    };

    AlarmHushRequest(void);

    WEAVE_ERROR init(void);
    WEAVE_ERROR pack(PacketBuffer *);
    WEAVE_ERROR sign(uint32_t aChallenge, uint16_t aKeyId, const uint8_t aKey[], uint16_t aKeySize);

    static WEAVE_ERROR parse(const PacketBuffer*, AlarmHushRequest*);

    uint32_t mProximityVerificationCode;
    AlarmHushSignature mSignature;
};

/**
 * @class AlarmHushResponse
 *
 * @brief
 *   pack and parse AlarmHushResponse request message
 *
 * This class can pack as well as parse AlarmHushResponse request message
 */
class AlarmHushResponse {
    public:

    enum
    {
        /// Size of payload in alarm hush response messages, without the actual conditions
        /// 1 byte for alarm counter
        /// 1 byte for length
        kPayloadSizeWithoutConditions = 2,
    };

    AlarmHushResponse(void);

    WEAVE_ERROR init(uint8_t aResult, uint8_t aNumEntry, const uint8_t * aAlarmCondition);
    WEAVE_ERROR pack(PacketBuffer *);

    static WEAVE_ERROR parse(const PacketBuffer*, AlarmHushResponse*);

    uint8_t mHushResult;
    uint8_t mLength;
    uint8_t mConditions[Alarm::kMaxNumAlarmConditions];
    AlarmHushSignature mSignature;
};

class WeaveAlarmServer;

/**
 * @class WeaveAlarmClient
 *
 * @brief
 *   A class to track an alarm session from a single originator
 *   (either self or remote)
 *
 * The core runtime class of the Weave Alarm protocol. This class
 * provides behaviors for both the originator and the remotes to track
 * protocol behaviors across a single alarm session.
 */

class NL_DLL_EXPORT WeaveAlarmClient {
    friend class WeaveAlarmServer;
    friend class WeaveAlarmClientIterator;
public:

    /**
     * @enum WeaveAlarmClient::ClientState
     *
     * @brief
     *   Enum describing the possible states of the WeaveAlarmClient
     */

    enum ClientState
    {
        /**
         * Client is closed.  There is no active state associated with
         * the client object, and the object may be used to allocate a
         * new alarm client.  A new alarm message, via an unsolicited
         * message handler and appropriate allocations, results in a
         * transition to kAlarmClientState_Active.
         */
        kAlarmClientState_Closed = 0,
        /**
         * Client is active. The session is actively (re-)transmitting
         * alarm messages, and considers the alarm state to be fresh
         * and current.  Upon a timeout, the alarm client transitions
         * to kAlarmClientState_GracePeriod.
         */
        kAlarmClientState_Active,
        /**
         * Client in the grace period.  The alarm state is considered
         * stale, __but__ the alarm client is willing to accept new
         * messages; if the message is judged to contain fresh and
         * current alarm state (based on the AlarmCounter), the client
         * will transition back to kAlarmClientState_Active.
         */
        kAlarmClientState_GracePeriod,
        /**
         * Client is in linger state.  This state is analogous to the
         * TCP linger state, and prevents collisions between two
         * consecutive sessions started by the same originator.  Upon
         * exiting the linger state, the client transitions to
         * kAlarmClientState_Closed.
         */
        kAlarmClientState_Linger,
    };

    WeaveAlarmClient(void);

    WEAVE_ERROR SendAlarm(const Alarm *aPayload);
    uint64_t GetOriginator(void);
    Alarm GetCurrentAlarm(void);
    uint8_t GetOriginatorWhere(void);
    bool IsLocalAlarm(void) const;

    void Close(void);

    void HandleAlarm(const IPPacketInfo *aPktInfo, const WeaveMessageInfo *aMsgInfo, PacketBuffer *aPayload, const Alarm & aParsedAlarm);

    static WEAVE_ERROR GenerateProximityVerificationCode(uint32_t * const aResult);
    static WEAVE_ERROR GenerateHushChallenge(uint32_t * const aResult);

    const ClientState GetClientState(void) const;
    static const char * GetClientStateName(const ClientState aState);

    /// Call this when app layer thinks it's safe to do so
    WEAVE_ERROR RegenerateSessionId(void);
    WEAVE_ERROR TryAdvanceExtEvtSeqNum(void);

    /**
     * @var mCurrentAlarm
     *
     * @brief
     *   The object representing the "current" alarm message
     *   associated with the alarm client.
     *
     * The message object is used to determine whether the incoming
     * message is more current than the one we currently have and to
     * (re-)transmit the messages as appropriate.
     */
    Alarm mCurrentAlarm;
private:
    static void HandleMessage(ExchangeContext *aEc, const IPPacketInfo *aPktInfo, const WeaveMessageInfo *aMsgInfo,
        uint32_t aProfileId, uint8_t aMsgType, PacketBuffer *aPayload);
    static void HandleRetransmissionTimeout(ExchangeContext *aEc);
    void initAlarmClient(WeaveAlarmServer * const aAlarmServer);
    int FindClientIndex(void) const;

    void ForceClientStateChangeNoError(ClientState aNewState);
    WEAVE_ERROR SetClientState_Active(void);
    WEAVE_ERROR SetClientState_Closed(bool aNotifyAppLayer = true);
    WEAVE_ERROR SetClientState_GracePeriod(void);
    WEAVE_ERROR SetClientState_Linger(void);

    void CancelAllTimersExceptForTrickle(void);
    static void _HandleLingerTimeout(System::Layer* aSystemLayer, void* aAppState, System::Error aError);
    void HandleLingerTimeout(void);

    static void _HandleGracePeriodTimeout(System::Layer* aSystemLayer, void* aAppState, System::Error aError);
    void HandleGracePeriodTimeout(void);

    /// ExchangeContext object used to track the messages within the alarm session.
    ExchangeContext * mEc;
    /// The current state of the alarm client.
    ClientState mClientState;
    /// A flag used to distinguish whether the alarm is local (set to
    /// true on the originator of the alarm) or remote (set to false
    /// on all the remotes participating in the alarm session.
    bool mIsLocalAlarm;
    /// A server object responsible for initial allocation and
    /// lifecycle of the alarm client object.
    WeaveAlarmServer *mServer;
};

#if WEAVE_SYSTEM_CONFIG_USE_LWIP
#include "lwip/pbuf.h"
struct tiny_custom_pbuf {
    struct pbuf_custom pc;
    uint32_t payload[32];
};
#endif // WEAVE_SYSTEM_CONFIG_USE_LWIP

/**
 * @class AlarmDelegate
 *
 * @brief
 *   Interface for delegation for alarm-related application-level
 *   operations
 *
 */
class AlarmDelegate
{
public:
    /**
     * Called when an AlarmHushRequest is received when there is no
     * other hush operation in progress.
     *
     * Application logic shall use the exchange context to verify the
     * security settings of this message, and also to restrict the
     * connection type.
     *
     * Application logic shall send a response to this request using
     * either SendHushResponse or SendStatusReport, which also closes
     * the exchange context internally.
     *
     * @param ec A pointer to the exchange context created for this
     *                  incoming request
     *
     * @param proximityVerificationCode A random, 32-bit challenge,
     *                  originally created by the alarm originator.
     *                  The challenge proves the applicability of the
     *                  hush request to this alarm.
     *
     * @param signature A read-only reference to the signature that
     *                  comes with the request. Callee shall make a
     *                  copy of this object if the content is needed
     *                  after return.
     *
     * @return #WEAVE_NO_ERROR on success, otherwise AlarmServer would
     *                  close the exchange context
     */
    virtual WEAVE_ERROR OnHushRequest(ExchangeContext *aEc, uint32_t aProximityVerificationCode, const AlarmHushSignature & aSignature) = 0;

    virtual void OnAlarmClientStateChange(WeaveAlarmClient * const aClient) = 0;
    virtual void OnNewRemoteAlarmDropped(const Alarm & aAlarm) = 0;

    virtual int CompareSeverity(const Alarm & aAlarm1, const Alarm & aAlarm2) = 0;
};

/**
 * @class WeaveAlarmServer
 *
 * @brief
 *   A class to act as the alarm server: manage individual alarm
 *   clients and message pools.
 *
 * @detail
 *   There must be at most one instance of this class per Weave
 *   client.  Weave Alarms are not processed until this class is
 *   initialized, so it is critical that the instance of the class is
 *   initialized as soon as possible in the client's life cycle, and
 *   that the instance remains active through the lifetime of the
 *   Weave client.
 *
 */
class NL_DLL_EXPORT WeaveAlarmServer: public nl::Weave::WeaveServerBase {
    friend class WeaveAlarmClient;
    friend class WeaveAlarmClientIterator;
    public :
    WeaveAlarmServer(void);

    WEAVE_ERROR Init(WeaveExchangeManager *aExchangeMgr, void *aAppState) ;
    WEAVE_ERROR Shutdown(void);

    WeaveAlarmClient *NewClient(uint64_t aPeer, uint8_t aEncryptionType, uint16_t aKeyId);

    void UpdateAllClients(void);

    /**
     * @var mAlarmRebroadcastPeriodMsec
     *
     * @brief
     *   Specifies the interval, in milliseconds, between rounds
     *   of Trickle retransmission.
     *
     * This variable defines the time interval that guides the
     * rebroadcast interval of the current message.  Each node
     * participating in the alarm protocol will resend a message at
     * most once within that time interval.  The constant is analogous
     * to `T` in the Trickle algorithm (<a
     * href="https://tools.ietf.org/html/rfc6206">RFC 6206</a>).  The
     * value must be the same for all nodes in the network and it is
     * initially set to #WEAVE_ALARM_DEFAULT_REBROADCAST_PERIOD_MSEC.
     */
    uint32_t mAlarmRebroadcastPeriodMsec;

    /**
     * @var mAlarmRefreshPeriodMsec
     *
     * @brief
     *   Specifies the period, in milliseconds, in which the
     *   originator should refresh the alarm counter
     *
     * This variable defines the how frequently the alarm should be
     * refreshed before it is considered stale.  Stale alarms cease to
     * be rebroadcast.  The constant should be several lengths of the
     * mAlarmRebroadcastPeriodMsec.  After alarm becomes stale, the
     * associated alarm session enters the grace period.  The value
     * must be the same for all nodes in the network and it is
     * initially set to #WEAVE_ALARM_DEFAULT_REBROADCAST_TIMEOUT_MSEC.
     */
    uint32_t mAlarmRefreshPeriodMsec;

    /**
     * @var mAlarmRebroadcastThreshold
     *
     * @brief
     *   Specifies the number of message receptions required to
     *   suppress a retransmission
     *
     * This variable is analogous to redundancy constant `k` in
     * Trickle algorithm description (<a
     * href="https://tools.ietf.org/html/rfc6206">RFC 6206</a>).  Its
     * current value is an artifact of the behaviors observed in the
     * 802.15.4 stack used, where each link local multicast was
     * automatically resent by the firmware three times.  The value
     * must be the same for all nodes in the network and it is
     * initially set to #WEAVE_ALARM_DEFAULT_REBROADCAST_THRESH.
     */
    uint8_t mAlarmRebroadcastThreshold;

    /**
     * @var mAlarmGracePeriodMsec
     *
     * @brief
     *   Specifies the duration, in milliseconds, of the grace period.
     *
     * After the alarm becomes stale, the associated session enters
     * the grace period.  During the grace period, the node remains
     * awake, listening for the any delayed alarm notifications. The
     * value reflects the maximum propagation delay for alarm messages
     * across the mesh network for a given level of reliable delivery.
     * The value must be the same for all nodes in the network and it
     * is initially set to #WEAVE_ALARM_DEFAULT_GRACE_PERIOD_MSEC.
     *
     */
    uint32_t mAlarmGracePeriodMsec;

    /**
     *
     * @var mInterfaceId
     *
     * @brief
     *   An ID of the designated interface for accepting Weave Alarm
     *   packets, #INET_NULL_INTERFACEID for accepting packets on any
     *   interface.
     */
    InterfaceId mInterfaceId;

    /**
     * @var mAppState
     *
     * @brief
     *   A pointer to any application-specific state, passed in via
     *   `WeaveAlarmServer::Init()` method.
     *
     */
    void* mAppState;

    void SetAlarmDelegate(AlarmDelegate * aDelegate);

    WEAVE_ERROR SendHushResponse(uint8_t aHushResult, uint8_t aLength = 0, uint8_t *aConditions = NULL);
    WEAVE_ERROR SendStatusReport(uint32_t aStatusProfileId, uint16_t aStatusCode, WEAVE_ERROR aSysError = WEAVE_NO_ERROR);

    private:
    /// delegate for application level operations
    AlarmDelegate * mAlarmDelegate;
    /// exchange context for the current operation under processing in the delegate
    ExchangeContext * mCurrentDelegateOp;
    WeaveAlarmClient mClientPool[MAX_CONCURRENT_ALERTS];
#if WEAVE_SYSTEM_CONFIG_USE_LWIP
    struct tiny_custom_pbuf mTinyPbufPool[MAX_CONCURRENT_MESSAGES];
#endif // WEAVE_SYSTEM_CONFIG_USE_LWIP
    WeaveAlarmClient *AllocAlarmClientUsingExchangeContext(ExchangeContext *aEc, uint8_t aEncryptionType,
        uint16_t aKeyId, bool aRemoteOnly);
    void DispatchAlarmMessage(const IPPacketInfo *aPktInfo, const WeaveMessageInfo *aMsgInfo, PacketBuffer *aPayload);
    WEAVE_ERROR InitializeBackingStore(void);
    void ShutdownBackingStore(void);
    PacketBuffer * NewPacketBuffer(void);

    /// parse the hush request message and deliver to the delegate
    WEAVE_ERROR HandleHushRequest(PacketBuffer *aPayload);

    void OnAlarmClientStateChange(WeaveAlarmClient * const aClient);
    int GetClientIndex (const WeaveAlarmClient * aClient) const;
    bool IsRemoteClientPoolFull(void) const;
    WEAVE_ERROR CloseLessSevereAlarmClient(uint64_t aSrcNodeId, const Alarm & aAlarm);

    static void UnsolicitedMessageHandler(ExchangeContext *aEc, const IPPacketInfo *aPktInfo, const WeaveMessageInfo *aMsgInfo,
        uint32_t aProfileId, uint8_t aMsgType, PacketBuffer *aPayload);
};


/**
 * @class WeaveAlarmClientIterator
 *
 * @brief
 *   A helper class to iterate through a list of alarm clients.
 */
class WeaveAlarmClientIterator {
public:
    WeaveAlarmClientIterator(WeaveAlarmServer *);
    bool hasNext(void);
    WeaveAlarmClient * next(void);
private:
    WeaveAlarmServer *mServer;
    int mIndex;
};

}//ns Profiles
}//ns Weave
}//ns nl

#endif // _WEAVE_ALARM_PROFILE_H
