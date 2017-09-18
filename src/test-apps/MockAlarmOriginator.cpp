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

WeaveAlarmServer gAlarmServer;

AlarmHushRequest hushChallenge;

void LogAlarmCondition(const uint8_t alarm_condition);

MockAlarmOriginator::MockAlarmOriginator() :
    mAlarmClient(NULL)
{
}

WEAVE_ERROR MockAlarmOriginator::Init(WeaveExchangeManager *exchangeMgr,
    const bool shouldAlarm,
    const uint8_t encryptionType,
    const uint16_t keyId)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;
    Alarm alarm;

    err = gAlarmServer.Init(exchangeMgr, this);
    SuccessOrExit(err);

    gAlarmServer.mInterfaceId = INET_NULL_INTERFACEID;
    gAlarmServer.mAlarmRebroadcastPeriodMsec = 4000;         // ms
    gAlarmServer.mAlarmRefreshPeriodMsec = 30000;            // ms
    gAlarmServer.mAlarmRebroadcastThreshold = 6;

    err = hushChallenge.init();
    SuccessOrExit(err);

    {
        // challenge code shall be sent over through BLE advertising, in the hope of saving one round trip
        const uint32_t challengeCode = 0xABCD0123;
        const uint16_t hushKeyId = 0x0002;
        const uint8_t hushKey [16] = { 1 };
        hushChallenge.mProximityVerificationCode = 0xDEADBEEF;
        err = hushChallenge.sign(challengeCode, hushKeyId, hushKey, int16_t(sizeof(hushKey)));
        SuccessOrExit(err);
        WeaveLogProgress(Alarm, "Hush challenge: 0x%X keyId: 0x%X, key is %d-byte starting with %d",
            challengeCode, hushChallenge.mSignature.mKeyId, sizeof(hushKey), hushKey[0]);
        WeaveLogProgress(Alarm, "Hush mSignature part 1: %02X%02X%02X%02X-%02X%02X%02X%02X",
            hushChallenge.mSignature.mKeyedHash[0], hushChallenge.mSignature.mKeyedHash[1],
            hushChallenge.mSignature.mKeyedHash[2], hushChallenge.mSignature.mKeyedHash[3],
            hushChallenge.mSignature.mKeyedHash[4], hushChallenge.mSignature.mKeyedHash[5],
            hushChallenge.mSignature.mKeyedHash[6], hushChallenge.mSignature.mKeyedHash[7]);
        WeaveLogProgress(Alarm, "Hush mSignature part 2: %02X%02X%02X%02X-%02X%02X%02X%02X",
            hushChallenge.mSignature.mKeyedHash[8], hushChallenge.mSignature.mKeyedHash[9],
            hushChallenge.mSignature.mKeyedHash[10], hushChallenge.mSignature.mKeyedHash[11],
            hushChallenge.mSignature.mKeyedHash[12], hushChallenge.mSignature.mKeyedHash[13],
            hushChallenge.mSignature.mKeyedHash[14], hushChallenge.mSignature.mKeyedHash[15]);
        WeaveLogProgress(Alarm, "Hush mSignature part 3: %02X%02X%02X%02X",
            hushChallenge.mSignature.mKeyedHash[16], hushChallenge.mSignature.mKeyedHash[17],
            hushChallenge.mSignature.mKeyedHash[18], hushChallenge.mSignature.mKeyedHash[19]);
    }

    gAlarmServer.SetAlarmDelegate(this);

    mAlarmClient = gAlarmServer.NewClient(kAnyNodeId, encryptionType, keyId);
    VerifyOrExit(NULL != mAlarmClient, err = WEAVE_ERROR_NO_MEMORY);

    if (shouldAlarm)
    {
        alarm.AddAlarm(WEAVE_ALARM_CO | WEAVE_ALARM_STATE_HEADS_UP_1);
        alarm.AddAlarm(WEAVE_ALARM_SMOKE | WEAVE_ALARM_STATE_ALARM_NONHUSHABLE);

        err = mAlarmClient->SendAlarm(&alarm);
        SuccessOrExit(err);
    }

    {
        // just a simple test to demo how random number generation utility routines work
        uint32_t testRandom = 0;
        WeaveAlarmClient::GenerateHushChallenge(&testRandom);
        WeaveLogProgress(Alarm, "Random number generated for hush challenge code: 0x%X", testRandom);
        WeaveAlarmClient::GenerateProximityVerificationCode(&testRandom);
        WeaveLogProgress(Alarm, "Random number generated for proximity verification code: 0x%X", testRandom);
    }

exit:
    WeaveLogFunctError(err);

    return err;
}

WEAVE_ERROR MockAlarmOriginator::Shutdown(void)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    err = gAlarmServer.Shutdown();
    SuccessOrExit(err);

exit:
    WeaveLogFunctError(err);

    return err;
}

WEAVE_ERROR MockAlarmOriginator::OnHushRequest(nl::Weave::ExchangeContext *ec, uint32_t proximityVerificationCode, const AlarmHushSignature & signature)
{
    WEAVE_ERROR err = WEAVE_NO_ERROR;

    // Application code shall verify the content/signature of the hush request and decide if
    // this request needs to be further processed
    // hush requests coming from channels other than BLE can be ignored

    WeaveLogProgress(Alarm, "Req key id: 0x%X", signature.mKeyId);

    WeaveLogProgress(Alarm, "Req signature part 1: %02X%02X%02X%02X-%02X%02X%02X%02X",
        signature.mKeyedHash[0], signature.mKeyedHash[1],
        signature.mKeyedHash[2], signature.mKeyedHash[3],
        signature.mKeyedHash[4], signature.mKeyedHash[5],
        signature.mKeyedHash[6], signature.mKeyedHash[7]);
    WeaveLogProgress(Alarm, "Req signature part 2: %02X%02X%02X%02X-%02X%02X%02X%02X",
        signature.mKeyedHash[8], signature.mKeyedHash[9],
        signature.mKeyedHash[10], signature.mKeyedHash[11],
        signature.mKeyedHash[12], signature.mKeyedHash[13],
        signature.mKeyedHash[14], signature.mKeyedHash[15]);
    WeaveLogProgress(Alarm, "Req signature part 3: %02X%02X%02X%02X",
        signature.mKeyedHash[16], signature.mKeyedHash[17],
        signature.mKeyedHash[18], signature.mKeyedHash[19]);

    if (!signature.mIsSigned)
    {
        WeaveLogProgress(Alarm, "Hush: not signed");

        // the request is not signed
        err = gAlarmServer.SendStatusReport(kWeaveProfile_Alarm,
            kAlarmStatusReport_HushRejected_SignatureValidationFailure);
    }
    else if (hushChallenge.mSignature != signature)
    {
        WeaveLogProgress(Alarm, "Hush: not signed or signature doesn't match");

        // since we use signature comparison, the request is either not signed, or doesn't match with ours
        err = gAlarmServer.SendStatusReport(kWeaveProfile_Alarm,
            kAlarmStatusReport_HushRejected_SignatureValidationFailure);
    }
    else if (NULL == mAlarmClient)
    {
        WeaveLogProgress(Alarm, "Hush: no alarm");

        // there is no Alarm at all to hush, but we should still respond with a 'success',
        err = gAlarmServer.SendHushResponse(kAlarmHushResult_Success);
        SuccessOrExit(err);
    }
    else
    {
        WeaveLogProgress(Alarm, "Hush: incoming proximity verification code 0x%X", proximityVerificationCode);

        if (hushChallenge.mProximityVerificationCode != proximityVerificationCode)
        {
            WeaveLogProgress(Alarm, "Hush: expected proximity verification code 0x%X, doesn't match", proximityVerificationCode);

            err = gAlarmServer.SendStatusReport(kWeaveProfile_Alarm,
                kAlarmStatusReport_HushRejected_ProximityValidationFailure);
            SuccessOrExit(err);
        }
        else
        {
            // apply hush to each of condition we have
            // application code might have other logic to execute for hushing and doesn't have to be
            // bound by this particular Alarm object
            Alarm & currentAlarm(mAlarmClient->mCurrentAlarm);

            WeaveLogProgress(Alarm, "BEFORE Hush: counter [%d] with [%d] conditions",
                currentAlarm.mAlarmCtr,
                currentAlarm.mLength);

            for (int i = 0; i < currentAlarm.mLength; ++i)
            {
                LogAlarmCondition(currentAlarm.mConditions[i]);
            }

            for (int i = 0; i < currentAlarm.mLength; ++i)
            {
                if (currentAlarm.GetAlarmState(i) <= WEAVE_ALARM_STATE_ALARM_HUSHABLE)
                {
                    currentAlarm.SetAlarmState(i, WEAVE_ALARM_STATE_ALARM_REMOTE_HUSH);
                }
            }

            WeaveLogProgress(Alarm, "AFTER Hush: counter [%d] with [%d] conditions",
                currentAlarm.mAlarmCtr,
                currentAlarm.mLength);

            for (int i = 0; i < currentAlarm.mLength; ++i)
            {
                LogAlarmCondition(currentAlarm.mConditions[i]);
            }

            // this call 'kills' the alarm and makes the pointer useless
            mAlarmClient->Close();
            mAlarmClient = NULL;

            err = gAlarmServer.SendHushResponse(kAlarmHushResult_Success, currentAlarm.mLength, currentAlarm.mConditions);
            SuccessOrExit(err);
        }
    }

exit:
    WeaveLogFunctError(err);

    return err;
}

void LogAlarmCondition(const uint8_t alarm_condition)
{
    const char * state;
    const char * source;

    switch (alarm_condition & 0xf0)
    {
    case WEAVE_ALARM_SMOKE:
        source = "smoke";
        break;
    case WEAVE_ALARM_TEMP:
        source = "temperature";
        break;
    case WEAVE_ALARM_CO:
        source = "carbon monoxide";
        break;
    case WEAVE_ALARM_CH4:
        source = "gas";
        break;
    case WEAVE_ALARM_HUMIDITY:
        source = "humidity";
        break;
    case WEAVE_ALARM_OTHER:
        source = "other";
        break;
    default:
        source = "unknown";
        break;
    }

    switch (alarm_condition & 0x0f)
    {
    case WEAVE_ALARM_STATE_STANDBY:
        state = "standby";
        break;
    case WEAVE_ALARM_STATE_HEADS_UP_1:
        state = "heads up 1";
        break;
    case WEAVE_ALARM_STATE_HEADS_UP_2:
        state = "heads up 2";
        break;
    case WEAVE_ALARM_STATE_HU_HUSH:
        state = "heads up hush";
        break;
    case WEAVE_ALARM_STATE_ALARM_HUSHABLE:
        state = "ALARM, hushable";
        break;
    case WEAVE_ALARM_STATE_ALARM_NONHUSHABLE:
        state = "ALARM, NONHUSHABLE";
        break;
    case WEAVE_ALARM_STATE_ALARM_GLOBAL_HUSH:
        state = "global hush";
        break;
    case WEAVE_ALARM_STATE_ALARM_REMOTE_HUSH:
        state = "remote hush";
        break;
    case WEAVE_ALARM_STATE_SELFTEST:
        state = "selftest";
        break;
    default:
        state = "unknown";
        break;
    }

    WeaveLogProgress(Alarm, "[%s]: [%s]", state, source);
}

void MockAlarmOriginator::OnAlarmClientStateChange(WeaveAlarmClient * const aClient)
{
    WeaveLogProgress(Alarm, "Alarm from 0x%" PRIx64 " changed", aClient->GetOriginator());

    for (int i = 0; i < aClient->mCurrentAlarm.mLength; ++i)
    {
        LogAlarmCondition(aClient->mCurrentAlarm.GetAlarm(i));
    }
}

void MockAlarmOriginator::OnNewRemoteAlarmDropped(const Alarm & aAlarm)
{
    WeaveLogProgress(Alarm, "Alarm at where id [%u] has been dropped", aAlarm.mWhere);

    for (int i = 0; i < aAlarm.mLength; ++i)
    {
        LogAlarmCondition(aAlarm.GetAlarm(i));
    }
}

int MockAlarmOriginator::CompareSeverity(const Alarm & aAlarm1, const Alarm & aAlarm2)
{
    enum
    {
        kResultSource_Smoke = 0,
        kResultSource_CO = 1,

        kResultAlarm1 = +1,
        kResultTie = 0,
        kResultAlarm2 = -1,

        kNumRecognizedAlarmSources = 2,
    };

    int resultTable[kNumRecognizedAlarmSources][2];
    resultTable[kResultSource_Smoke][0] = WEAVE_ALARM_STATE_INVALID;
    resultTable[kResultSource_Smoke][1] = kResultTie;
    resultTable[kResultSource_CO][0] = WEAVE_ALARM_STATE_INVALID;
    resultTable[kResultSource_CO][1] = kResultTie;

    for (int i = 0; i < aAlarm1.mLength; ++i)
    {
        switch (aAlarm1.GetAlarmCondition(i))
        {
        case WEAVE_ALARM_SMOKE:
            resultTable[kResultSource_Smoke][0] = aAlarm1.GetAlarmState(i);
            break;

        case WEAVE_ALARM_CO:
            resultTable[kResultSource_CO][0] = aAlarm1.GetAlarmState(i);
            break;

        default:
            WeaveLogError(Alarm, "Ignore unknown alarm source %u", aAlarm1.GetAlarmCondition(i));
        }
    }

    for (int i = 0; i < aAlarm2.mLength; ++i)
    {
        switch (aAlarm1.GetAlarmCondition(i))
        {
        case WEAVE_ALARM_SMOKE:
            if (WEAVE_ALARM_STATE_INVALID == resultTable[kResultSource_Smoke][0])
            {
                // alarm1 doesn't have this source, so alarm2 wins here
                resultTable[kResultSource_Smoke][0] = aAlarm2.GetAlarmState(i);
                resultTable[kResultSource_Smoke][1] = kResultAlarm2;
            }
            else
            {
                // both alarm1 and alarm2 have this source. let's compare them
                const int state1 = resultTable[kResultSource_Smoke][0];
                const int state2 = aAlarm2.GetAlarmState(i);
                if (state1 > state2)
                {
                    resultTable[kResultSource_Smoke][1] = kResultAlarm1;
                }
                else if (state1 == state2)
                {
                    resultTable[kResultSource_Smoke][1] = kResultTie;
                }
                else
                {
                    resultTable[kResultSource_Smoke][1] = kResultAlarm2;
                }
            }
            break;

        case WEAVE_ALARM_CO:
            if (WEAVE_ALARM_STATE_INVALID == resultTable[kResultSource_CO][0])
            {
                // alarm1 doesn't have this source, so alarm2 wins here
                resultTable[kResultSource_CO][0] = aAlarm2.GetAlarmState(i);
                resultTable[kResultSource_CO][1] = kResultAlarm2;
            }
            else
            {
                // both alarm1 and alarm2 have this source. let's compare them
                const int state1 = resultTable[kResultSource_CO][0];
                const int state2 = aAlarm2.GetAlarmState(i);
                if (state1 > state2)
                {
                    resultTable[kResultSource_CO][1] = kResultAlarm1;
                }
                else if (state1 == state2)
                {
                    resultTable[kResultSource_CO][1] = kResultTie;
                }
                else
                {
                    resultTable[kResultSource_CO][1] = kResultAlarm2;
                }
            }
            break;

        default:
            WeaveLogError(Alarm, "Ignore unknown alarm source %u", aAlarm2.GetAlarmCondition(i));
        }
    }

    if (resultTable[kResultSource_Smoke][1] != kResultTie)
    {
        // Smoke take precedence. Use the result of smoke if possible
        return resultTable[kResultSource_Smoke][1];
    }
    else
    {
        // otherwise, use CO
        return resultTable[kResultSource_CO][1];
    }
}
