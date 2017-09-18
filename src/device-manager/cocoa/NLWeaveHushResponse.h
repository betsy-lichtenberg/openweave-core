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
 *      This file declares NLWeaveHushResponse and NLWeaveAlarmCondition, wrappers for C++ objects.
 *      The reason we need wrappers is iOS app team doesn't want to directly include C++ headers in application code
 *
 */

#import <Foundation/Foundation.h>

#import "NLWeaveDeviceManagerTypes.h"

/** @Class wrapper for a single alarm condition, with source and state/severity
 *
 */
@interface NLWeaveAlarmCondition : NSObject

/// the source of this alarm condition
@property (nonatomic) NLWeaveAlarmSource source;

/// the current state/severity of this alarm condition
@property (nonatomic) NLWeaveAlarmState state;

@end

/** @Class wrapper for a WeaveAlarmHushResponse message
 *
 */
@interface NLWeaveHushResponse : NSObject

/// hush result: 0 for success
@property (nonatomic) NLWeaveHushResult hushResult;

/// array of NLWeaveAlarmCondition
@property (nonatomic) NSArray * conditions;

@end
