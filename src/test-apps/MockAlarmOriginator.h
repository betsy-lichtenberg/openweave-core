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

#ifndef MOCKALARMORIGINATOR_H_
#define MOCKALARMORIGINATOR_H_

#include <Weave/Profiles/vendor/nestlabs/alarm/WeaveAlarm.h>

class MockAlarmOriginator: public nl::Weave::Profiles::AlarmDelegate
{
public:
    MockAlarmOriginator();

    WEAVE_ERROR Init(nl::Weave::WeaveExchangeManager *exchangeMgr,
        const bool shouldAlarm,
        const uint8_t encryptionType = nl::Weave::kWeaveEncryptionType_None,
        const uint16_t keyId = nl::Weave::WeaveKeyId::kNone);

    WEAVE_ERROR Shutdown(void);

    WEAVE_ERROR OnHushRequest(nl::Weave::ExchangeContext *ec, uint32_t proximityVerificationCode, const nl::Weave::Profiles::AlarmHushSignature & signature);

    void OnAlarmClientStateChange(nl::Weave::Profiles::WeaveAlarmClient * const client);

    void OnNewRemoteAlarmDropped(const nl::Weave::Profiles::Alarm & aAlarm);

    int CompareSeverity(const nl::Weave::Profiles::Alarm & aAlarm1, const nl::Weave::Profiles::Alarm & aAlarm2);

private:

    nl::Weave::Profiles::WeaveAlarmClient * mAlarmClient;

    static void LogAlarmEvent(nl::Weave::Profiles::WeaveAlarmClient *client, nl::Weave::Profiles::Alarm *payload,
        void *appState);
};

#endif /* MOCKALARMORIGINATOR_H_ */
