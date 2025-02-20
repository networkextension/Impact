//
//  ImpactLog.h
//  Impact
//
//  Created by Matt Massicotte on 2019-09-18.
//  Copyright © 2019 Chime Systems Inc. All rights reserved.
//

#ifndef ImpactLog_h
#define ImpactLog_h

#include "ImpactResult.h"
#include "ImpactState.h"

#include <sys/types.h>
#include <stdbool.h>

#if __OBJC__
#import <Foundation/Foundation.h>
#endif

_Pragma("clang assume_nonnull begin")
__BEGIN_DECLS

ImpactResult ImpactLogInitialize(ImpactState* state, const char* path);
ImpactResult ImpactLogDeinitialize(ImpactLogger* log);
bool ImpactLogIsValid(const ImpactLogger* log);

ImpactResult ImpactLog(const char * __restrict format, ...) __printflike(1, 2);

ImpactResult ImpactLogWriteData(const ImpactLogger* log, const char* data, size_t length);
ImpactResult ImpactLogWriteString(const ImpactLogger* log, const char* string);
ImpactResult ImpactLogWriteInteger(const ImpactLogger* log, uintptr_t number);

ImpactResult ImpactLogWriteKeyInteger(const ImpactLogger* log, const char* key, uintptr_t number, bool last);
ImpactResult ImpactLogWriteKeyPointer(const ImpactLogger* log, const char* key, const void* _Nullable ptr, bool last);
ImpactResult ImpactLogWriteKeyString(const ImpactLogger* log, const char* key, const char* string, bool last);

#if __OBJC__
ImpactResult ImpactLogWriteKeyStringObject(const ImpactLogger* log, const char* key, NSString* string, bool last);
#endif

ImpactResult ImpactLogWriteKeyHexData(const ImpactLogger* log, const char* key, const uint8_t* _Nullable data, size_t length, bool last);

__END_DECLS
_Pragma("clang assume_nonnull end")

#endif /* ImpactLog_h */
