//
//  ImpactThread.c
//  Impact
//
//  Created by Matt Massicotte on 2019-09-27.
//  Copyright © 2019 Chime Systems Inc. All rights reserved.
//

#include "ImpactThread.h"
#include "ImpactUtility.h"
#include "ImpactLog.h"

#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/vm_map.h>
#include <mach/thread_act.h>

ImpactResult ImpactThreadListInitialize(ImpactThreadList* list) {
    const kern_return_t kr = task_threads(mach_task_self(), &list->threads, &list->count);
    if (kr != KERN_SUCCESS) {
        ImpactDebugLog("[Log:%s] unable to get threads %d\n", __func__, kr);
        return ImpactResultFailure;
    }

    list->threadSelf = mach_thread_self();

    return ImpactResultSuccess;
}

ImpactResult ImpactThreadListDeinitialize(ImpactThreadList* list) {
    const task_t task = mach_task_self();
    
    for (mach_msg_type_number_t i = 0; i < list->count; ++i) {
        const kern_return_t kr = mach_port_deallocate(task, list->threads[i]);
        if (kr != KERN_SUCCESS) {
            ImpactDebugLog("[Log:%s] unable to dealloc thread port %d\n", __func__, kr);
        }

        list->threads[i] = MACH_PORT_NULL;
    }

    const vm_size_t size = sizeof(thread_t) * list->count;

    kern_return_t kr = vm_deallocate(task, (vm_address_t)list->threads, size);
    if (kr != KERN_SUCCESS) {
        ImpactDebugLog("[Log:%s] unable to dealloc thread storage %d\n", __func__, kr);
    }

    kr = mach_port_deallocate(task, list->threadSelf);
    if (kr != KERN_SUCCESS) {
        ImpactDebugLog("[Log:%s] unable to dealloc self thread port %d\n", __func__, kr);
    }

    return ImpactResultSuccess;
}

static ImpactResult ImpactThreadListSuspendAllExceptForCurrent(const ImpactThreadList* list) {
    for (mach_msg_type_number_t i = 0; i < list->count; ++i) {
        const thread_act_t thread = list->threads[i];

        if (thread == list->threadSelf) {
            continue;
        }

        const kern_return_t kr = thread_suspend(thread);
        if (kr != KERN_SUCCESS) {
            ImpactDebugLog("[Log:%s] failed to suspend thread %d\n", __func__, kr);
        }
    }

    return ImpactResultSuccess;
}

static ImpactResult ImpactThreadListResumeAllExceptForCurrent(const ImpactThreadList* list) {
    for (mach_msg_type_number_t i = 0; i < list->count; ++i) {
        const thread_act_t thread = list->threads[i];

        if (thread == list->threadSelf) {
            continue;
        }

        const kern_return_t kr = thread_resume(thread);
        if (kr != KERN_SUCCESS) {
            ImpactDebugLog("[Log:%s] failed to suspend thread %d\n", __func__, kr);
        }
    }

    return ImpactResultSuccess;
}

ImpactResult ImpactThreadLog(ImpactState* state, thread_act_t thread) {
    ImpactLogger* log = &state->constantState.log;

    ImpactLogWriteString(log, "hello from the thread logger\n");

    return ImpactResultSuccess;
}

ImpactResult ImpactThreadListLog(ImpactState* state, const ImpactThreadList* list) {
    ImpactResult result = ImpactThreadListSuspendAllExceptForCurrent(list);

    for (mach_msg_type_number_t i = 0; i < list->count; ++i) {
        const thread_act_t thread = list->threads[i];

        if (thread == list->threadSelf) {
            // what's a good strategy for logging the current thread?
            continue;
        }

        result = ImpactThreadLog(state, thread);
        if (result != ImpactResultSuccess) {
            ImpactDebugLog("[Log:%s] failed to log thread %d %d\n", __func__, i, result);
        }
    }

    result = ImpactThreadListResumeAllExceptForCurrent(list);

    return ImpactResultSuccess;
}

