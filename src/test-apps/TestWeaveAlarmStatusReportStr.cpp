/*
 *
 *    Copyright (c) 2016-2017 Nest Labs, Inc.
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

/**
 *    @file
 *      This file tests status report strings for the Nest Weave Alarm
 *      profile.
 *
 */

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <Weave/Profiles/vendor/nestlabs/alarm/WeaveAlarm.h>
#include <Weave/Support/ErrorStr.h>

#include <nltest.h>

using namespace nl::Weave::Profiles;

static const size_t kStatusCodesMax = 20;

struct ProfileStatus
{
    uint32_t       mProfileId;
    const char *   mFormat;
    size_t         mStatusCodeCount;
    uint16_t       mStatusCodes[kStatusCodesMax];
};

static struct ProfileStatus sContext[] = {
    {
        kWeaveProfile_Alarm,
        "[ Nest:Alarm(%08" PRIX32 "):%" PRIu16 " ]",
        6,
        {
            kAlarmUpdateStatus_Success,
            kAlarmUpdateStatus_Rejected,
            kAlarmUpdateStatus_Invalid,
            kAlarmUpdateStatus_Timeout,
            kAlarmStatusReport_HushRejected_ProximityValidationFailure,
            kAlarmStatusReport_HushRejected_SignatureValidationFailure,
        }
    }
};

static const size_t kTestElements = sizeof(sContext) / sizeof(sContext[0]);

static void WeaveAlarmCheckStatusReportStr(nlTestSuite *inSuite, void *inContext)
{
    char statusStr[1024];

    for (size_t ith = 0; ith < kTestElements; ith++)
    {
        for (uint16_t jth = 0; jth < sContext[ith].mStatusCodeCount; jth++)
        {
           snprintf(statusStr, sizeof(statusStr), sContext[ith].mFormat, sContext[ith].mProfileId, sContext[ith].mStatusCodes[jth]);
           NL_TEST_ASSERT(inSuite, (strcmp(nl::StatusReportStr(sContext[ith].mProfileId, sContext[ith].mStatusCodes[jth]), statusStr) != 0));
        }
    }
}

static const nlTest sTests[] = {
    NL_TEST_DEF("WeaveAlarmStatusReportStr", WeaveAlarmCheckStatusReportStr),

    NL_TEST_SENTINEL()
};

int main(void)
{
    nlTestSuite theSuite = {
        "weave-alarm-status-report-strings",
        &sTests[0],
        NULL,
        NULL
    };

    // Generate machine-readable, comma-separated value (CSV) output.
    nl_test_set_output_style(OUTPUT_CSV);

    // Run test suit againt one context.
    nlTestRunner(&theSuite, &sContext);

    return nlTestRunnerStats(&theSuite);
}
