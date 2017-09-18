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

#ifndef MOCKALARMREMOTE_H_
#define MOCKALARMREMOTE_H_

#include <Weave/Profiles/vendor/nestlabs/alarm/WeaveAlarm.h>

class MockAlarmRemote
{
public:
    MockAlarmRemote();

    WEAVE_ERROR Init(nl::Weave::WeaveExchangeManager *exchangeMgr,
        const uint64_t peerNodeId,
        const uint32_t proximityerVificationCode,
        const uint8_t encryptionType = nl::Weave::kWeaveEncryptionType_None,
        const uint16_t keyId = nl::Weave::WeaveKeyId::kNone);

    WEAVE_ERROR Shutdown(void);

    void SetPeerNodeId(const uint64_t node_id);

private:

    nl::Weave::WeaveExchangeManager * mExchangeMgr;
    uint8_t mEncryptionType;
    uint16_t mKeyId;
    nl::Weave::ExchangeContext * mExchangeContext_hush;

    WEAVE_ERROR HushAlarm(const uint64_t peerNodeId, const uint32_t proximityerVificationCode);
    void OnHushCompleted(const nl::Weave::Profiles::AlarmHushResponse * const alarmHushResponse, const nl::Weave::Profiles::StatusReporting::StatusReport * const statusReport = NULL);

    //static void HandleDelayedHush(nl::Inet::InetLayer *inetLayer, void *appState, INET_ERROR aErr);
    static void HandleResponseTimeout(nl::Weave::ExchangeContext *ec);
    static void HandleHushResponse(nl::Weave::ExchangeContext *ec, const nl::Inet::IPPacketInfo *pktInfo, const nl::Weave:: WeaveMessageInfo *msgInfo, uint32_t profileId, uint8_t msgType, nl::Weave::System::PacketBuffer *payload);
};

#endif /* MOCKALARMREMOTE_H_ */
