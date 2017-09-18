/*
 *
 *    Copyright (c) 2014-2017 Nest Labs, Inc.
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

// __STDC_LIMIT_MACROS must be defined for UINT8_MAX and INT32_MAX to be defined for pre-C++11 clib
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif // __STDC_LIMIT_MACROS

// __STDC_CONSTANT_MACROS must be defined for INT64_C and UINT64_C to be defined for pre-C++11 clib
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif // __STDC_CONSTANT_MACROS

// it is important for this first inclusion of stdint.h to have all the right switches turned ON
#include <stdint.h>

// __STDC_FORMAT_MACROS must be defined for PRIX64 to be defined for pre-C++11 clib
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif // __STDC_FORMAT_MACROS

// it is important for this first inclusion of inttypes.h to have all the right switches turned ON
#include <inttypes.h>

#define WEAVE_CONFIG_ENABLE_LOG_FILE_LINE_FUNC_ON_ERROR 1

#include <Weave/Support/CodeUtils.h>
#include <Weave/Support/logging/WeaveLogging.h>
#include <Weave/Support/ErrorStr.h>

#include "MockAlarmOriginator.h"
#include "MockAlarmRemote.h"

using namespace nl::Weave;
using namespace nl::Weave::Profiles;

extern void LogAlarmCondition(const uint8_t alarm_condition);

MockAlarmRemote::MockAlarmRemote()
{
}

WEAVE_ERROR MockAlarmRemote::Init(WeaveExchangeManager *exchangeMgr,
    const uint64_t peerNodeId,
    const uint32_t proximityerVificationCode,
    const uint8_t encryptionType,
    const uint16_t keyId)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    Alarm alarm;

    mExchangeMgr = exchangeMgr;
    mEncryptionType = encryptionType;
    mKeyId = keyId;
    mExchangeContext_hush = NULL;

    err = HushAlarm(peerNodeId, proximityerVificationCode);
    SuccessOrExit(err);

exit:
    WeaveLogFunctError(err);

    return err;
}

WEAVE_ERROR MockAlarmRemote::Shutdown(void)
{
    if (NULL != mExchangeContext_hush)
    {
        mExchangeContext_hush->Close();
    }

    return WEAVE_NO_ERROR;
}

WEAVE_ERROR MockAlarmRemote::HushAlarm(const uint64_t peerNodeId, const uint32_t proximityerVificationCode)
{
    WEAVE_ERROR         err             = WEAVE_NO_ERROR;
    PacketBuffer*       msgBuf          = NULL;
    AlarmHushRequest    hushRequest;

    // Note that in a BLE hush case (which is actually the designed use case for hush),
    // we should already have an exchange context around a BLE connection ready, instead of creating
    // one at here
    mExchangeContext_hush = mExchangeMgr->NewContext(peerNodeId, this);
    if (NULL == mExchangeContext_hush)
    {
        ExitNow(err = WEAVE_ERROR_NO_MEMORY);
    }

    mExchangeContext_hush->EncryptionType = mEncryptionType;
    mExchangeContext_hush->KeyId = mKeyId;
    mExchangeContext_hush->AppState = this;
    mExchangeContext_hush->OnMessageReceived = HandleHushResponse;
    mExchangeContext_hush->OnResponseTimeout = HandleResponseTimeout;

    // timeout after 3000 msec
    mExchangeContext_hush->ResponseTimeout = 3000;

    // allocate buffer and then encode the response into it
    msgBuf = PacketBuffer::New();
    if (NULL == msgBuf)
    {
        ExitNow(err = WEAVE_ERROR_NO_MEMORY);
    }

    {
        hushRequest.mProximityVerificationCode = proximityerVificationCode;
        // challenge code shall be received over through BLE advertising, so we do not have to ask through Weave
        const uint32_t challengeCode = 0xABCD0123;
        const uint16_t hushKeyId = 0x0002;
        const uint8_t hushKey [16] = { 1 };
        err = hushRequest.sign(challengeCode, hushKeyId, hushKey, int16_t(sizeof(hushKey)));
        SuccessOrExit(err);
        WeaveLogProgress(Alarm, "Hush challenge: 0x%X, keyId: 0x%X, key is %d-byte starting with %d",
            challengeCode, hushRequest.mSignature.mKeyId, sizeof(hushKey), hushKey[0]);
    }

    err = hushRequest.pack(msgBuf);
    SuccessOrExit(err);

    // send out the request
    err = mExchangeContext_hush->SendMessage(kWeaveProfile_Alarm, kAlarmMessageType_AlarmHushRequest, msgBuf,
        ExchangeContext::kSendFlag_ExpectResponse);
    msgBuf = NULL;
    if (WEAVE_NO_ERROR == err)
    {
        // if nothing goes wrong, we should see either a response message or a timeout event
    }
    else
    {
        // failure at this stage is special, as we might fail to contact any node because of
        // any kind of network issues, and we won't hear from the response timeout
        SuccessOrExit(err);
    }

    exit:
    WeaveLogFunctError(err);

    if (NULL != msgBuf)
    {
        PacketBuffer::Free(msgBuf);
    }

    if ((WEAVE_NO_ERROR != err) && (NULL != mExchangeContext_hush))
    {
        mExchangeContext_hush->Close();
        mExchangeContext_hush = NULL;
    }

    return err;
}

void MockAlarmRemote::HandleResponseTimeout(ExchangeContext *ec)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    MockAlarmRemote * const alarm_remote = (MockAlarmRemote *) ec->AppState;
    if (NULL != alarm_remote)
    {
        alarm_remote->OnHushCompleted(NULL);
    }
    else
    {
        ExitNow(err = WEAVE_ERROR_INCORRECT_STATE);
    }

exit:
    WeaveLogFunctError(err);

    // note that ec shall be equal to alarm_remote->mExchangeContext_hush
    ec->Close();
    if (NULL != alarm_remote)
    {
        alarm_remote->mExchangeContext_hush = NULL;
    }
}

void MockAlarmRemote::HandleHushResponse(ExchangeContext *ec, const IPPacketInfo *pktInfo,
    const WeaveMessageInfo *msgInfo, uint32_t profileId, uint8_t msgType, PacketBuffer *payload)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    WeaveLogProgress(Alarm, "MockAlarmRemote::HandleHushResponse profile: 0x%X, message: %u", profileId, msgType);

    MockAlarmRemote * const alarm_remote = (MockAlarmRemote *) ec->AppState;
    if (NULL == alarm_remote)
    {
        ExitNow(err = WEAVE_ERROR_INCORRECT_STATE);
    }

    // only AlarmHushResponse is accepted as response in this conversation
    if ((kWeaveProfile_Alarm == profileId) && (kAlarmMessageType_AlarmHushResponse == msgType))
    {
        AlarmHushResponse alarmHushResponse;
        err = AlarmHushResponse::parse(payload, &alarmHushResponse);
        SuccessOrExit(err);

        alarm_remote->OnHushCompleted(&alarmHushResponse);
    }
    else if ((kWeaveProfile_Common == profileId) && (nl::Weave::Profiles::Common::kMsgType_StatusReport == msgType))
    {
        StatusReporting::StatusReport report;
        err = StatusReporting::StatusReport::parse(payload, report);
        SuccessOrExit(err);

        alarm_remote->OnHushCompleted(NULL, &report);
    }
    else
    {
        ExitNow(err = WEAVE_ERROR_NO_MESSAGE_HANDLER);
    }

exit:
    WeaveLogFunctError(err);

    PacketBuffer::Free(payload);

    // note that ec shall be equal to alarm_remote->mExchangeContext_hush
    ec->Close();
    if (NULL != alarm_remote)
    {
        alarm_remote->mExchangeContext_hush = NULL;
    }
}

void MockAlarmRemote::OnHushCompleted(const AlarmHushResponse * const alarmHushResponse, const nl::Weave::Profiles::StatusReporting::StatusReport * const statusReport)
{
    if ((NULL == alarmHushResponse) && (NULL == statusReport))
    {
        WeaveLogProgress(Alarm, "Hush response timed out");
    }
    else if (NULL == alarmHushResponse)
    {
        WeaveLogProgress(Alarm, "Error response: %s", nl::StatusReportStr(statusReport->mProfileId, statusReport->mStatusCode));
    }
    else
    {
        WeaveLogProgress(Alarm, "Hush status code [%d] with [%d] conditions",
            alarmHushResponse->mHushResult,
            alarmHushResponse->mLength);

        for (int i = 0; i < alarmHushResponse->mLength; ++i)
        {
            LogAlarmCondition(alarmHushResponse->mConditions[i]);
        }
    }
}
