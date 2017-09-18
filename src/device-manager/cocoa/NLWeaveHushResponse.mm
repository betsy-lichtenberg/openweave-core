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

/**
 *    @file
 *      This file implements NLWeaveHushResponse and NLWeaveAlarmCondition, wrappers for C++ objects.
 *      The reason we need wrappers is iOS app team doesn't want to directly include C++ headers in application code
 *
 */

#import "NLWeaveDeviceManagerTypes.h"
#import "NLWeaveHushResponse_Protected.h"
#import "NLLogging.h"

@implementation NLWeaveHushResponse

/**
 * initialize a NLWeaveHushResponse instance from parameters acquired from WeaveDeviceManager.
 * this is a 'protected' method which only appears in NLWeaveHushResponse_Protected.h
 *
 * @param[in] hushResult Result code for the hush operation
 * @param[in] length     Number of alarm conditions in the conditions array
 * @param[in] conditions Byte array of alarm conditions. each byte represents one alarm condition
 *
 * @return initialized instance On success. Nil on error
 */
+ (NLWeaveHushResponse *)createUsing:(NLWeaveHushResult)hushResult length:(int)length conditions:(const uint8_t*)conditions
{
    return [[NLWeaveHushResponse alloc] initWith:hushResult length:length conditions:conditions];
}

/**
 * initialize a NLWeaveHushResponse instance from parameters acquired from WeaveDeviceManager
 *
 * @param[in] hushResult Result code for the hush operation
 * @param[in] length     Number of alarm conditions in the conditions array
 * @param[in] conditions Byte array of alarm conditions. each byte represents one alarm condition
 *
 * @return initialized instance On success. Nil on error
 */
- (id)initWith:(NLWeaveHushResult)hushResult length:(int)length conditions:(const uint8_t*)conditions
{
    if (self = [super init])
    {
        WDM_LOG_DEBUG(@"Creating new NLWeaveHushResult");
        self.hushResult = hushResult;
        NSMutableArray * array = [[NSMutableArray alloc] initWithCapacity:length];
        for (int i = 0; i < length; ++i)
        {
            NLWeaveAlarmCondition * condition = [NLWeaveAlarmCondition new];
            condition.source = (NLWeaveAlarmSource)(conditions[i] & 0xF0);
            condition.state = (NLWeaveAlarmState)(conditions[i] & 0x0F);
            array[i] = condition;
        }
        self.conditions = [[NSArray alloc] initWithArray:array];
    }
    return self;
}

@end

@implementation NLWeaveAlarmCondition
@end
