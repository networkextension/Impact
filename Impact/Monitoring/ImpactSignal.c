//
//  ImpactSignal.c
//  Impact
//
//  Created by Matt Massicotte on 2019-09-18.
//  Copyright © 2019 Chime Systems Inc. All rights reserved.
//

#include "ImpactSignal.h"
#include "ImpactLog.h"
#include "ImpactCrashHandler.h"
#include "ImpactUtility.h"

#include <signal.h>
#include <stdbool.h>
#include <sys/errno.h>
#include <unistd.h>

static const int ImpactHandledSignals[ImpactSignalCount] = {
    SIGBUS,
    SIGABRT,
    SIGILL,
    SIGSEGV,
    SIGSYS
};

static void ImpactSignalHandler(int signal, siginfo_t* info, ucontext_t* uap);

ImpactResult ImpactSignalInitialize(ImpactState* state) {
    atomic_store(&state->mutableState.signalCount, 0);

    sigset_t set = {0};

    sigemptyset(&set);

    // We have to carefully consider what happens when a subsequent
    // signal is devliered while our handler is running. Including a
    // signal caused by the handler itself.
    //
    // SA_RESETHAND ensures that a second identical signal will
    // not invoke our handler.
    //
    // SA_NODEFER ensures that the signal we are handling can
    // be raised during our handler.
    const struct sigaction action = {
        (void*)ImpactSignalHandler,
        set,
        SA_SIGINFO | SA_RESETHAND | SA_NODEFER
    };

    bool someFailed = false;

    for (uint32_t i = 0; i < ImpactSignalCount; ++i) {
        const int signal = ImpactHandledSignals[i];
        struct sigaction* preexistingAction = &state->constantState.preexistingActions[i];

        const int result = sigaction(signal, &action, preexistingAction);
        if (result != 0) {
            someFailed = true;
        }
    }

    return someFailed ? ImpactResultFailure : ImpactResultSuccess;
}

static ImpactResult ImpactSignalInstallDefaultHandlers() {
    bool someFailed = false;

    struct sigaction action = {0};

    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_DFL;
    
    for (uint32_t i = 0; i < ImpactSignalCount; ++i) {
        const int signal = ImpactHandledSignals[i];

        int result = sigaction(signal, &action, NULL);
        if (result != 0) {
            ImpactDebugLog("[Log:%s] unable to install default signal handler %d %d\n", __func__, signal, result);
            someFailed = true;
        }
    }

    return someFailed ? ImpactResultFailure : ImpactResultSuccess;
}

static ImpactResult ImpactSignalRestorePreexisting(const ImpactState* state) {
    if (ImpactInvalidPtr(state)) {
        return ImpactResultArgumentInvalid;
    }

    bool someFailed = false;

    for (uint32_t i = 0; i < ImpactSignalCount; ++i) {
        const int signal = ImpactHandledSignals[i];
        const struct sigaction action = state->constantState.preexistingActions[i];

        if (action.sa_handler == SIG_DFL) {
            continue;
        }

        int result = sigaction(signal, &action, NULL);
        if (result != 0) {
            ImpactDebugLog("[Log:%s] unable to restore signal handler %d %d\n", __func__, signal, result);
            someFailed = true;
        }
    }

    return someFailed ? ImpactResultFailure : ImpactResultSuccess;
}

ImpactResult ImpactSignalUninstallHandlers(const ImpactState* state) {
    ImpactResult result = ImpactSignalRestorePreexisting(state);
    if (result == ImpactResultSuccess) {
        return result;
    }

    ImpactDebugLog("[Log:%s] failed to restore preexisting handlers %d\n", __func__, result);

    // ok, fall back to removing all handlers
    return ImpactSignalInstallDefaultHandlers();
}

static ImpactResult ImpactSignalLog(ImpactState* state, siginfo_t* info) {
    ImpactLogger* log = &state->constantState.log;

    ImpactLogWriteString(log, "hello from the signal handler: ");
    ImpactLogWriteInteger(log, atomic_load(&state->mutableState.signalCount));
    ImpactLogWriteString(log, "\n");

    return ImpactResultSuccess;
}

static void ImpactSignalHandlerEntranceAdjustState(ImpactState* state) {
    if (ImpactInvalidPtr(state)) {
        return;
    }

    ImpactCrashState currentState = atomic_load(&state->mutableState.crashState);
    switch (currentState) {
        case ImpactCrashStateInitialized:
            ImpactStateTransition(state, currentState, ImpactCrashStateFirstSignal);
            break;
        case ImpactCrashStateFirstMachExceptionReplied:
            ImpactStateTransition(state, currentState, ImpactCrashStateFirstSignalAfterMachExceptionReplied);
            break;
        default:
            ImpactStateInvalid(currentState);
            break;
    }
}

static void ImpactSignalHandlerExitAdjustState(ImpactState* state) {
    if (ImpactInvalidPtr(state)) {
        return;
    }

    ImpactCrashState currentState = atomic_load(&state->mutableState.crashState);
    switch (currentState) {
        case ImpactCrashStateFirstSignal:
            ImpactStateTransition(state, currentState, ImpactCrashStateFirstSignalHandled);
            break;
        case ImpactCrashStateFirstSignalAfterMachExceptionReplied:
            ImpactStateTransition(state, currentState, ImpactCrashStateFirstSignalHandledAfterMachExceptionReplied);
            break;
        default:
            ImpactStateInvalid(currentState);
            break;
    }
}

static void ImpactSignalHandler(int signal, siginfo_t* info, ucontext_t* uap) {
    // save the current thread's errno before we make any calls that might
    // have the side-effect of changing it
    int savedErrno = errno;
    ImpactState* state = GlobalImpactState;

    // If our state has got bad, what can we reasonably do?
    if (ImpactInvalidPtr(state)) {
        return;
    }

    ImpactSignalHandlerEntranceAdjustState(state);

    // Attempt to put back whatever handlers were in place when we started. It is
    // important to give previous handlers a chance to run, particularly if we fail
    // at some point. This is why we do this early.

//    ImpactResult result = ImpactSignalUninstallHandlers(state);
//    if (result != ImpactResultSuccess) {
//        ImpactDebugLog("[Log:%s] failed to uninstall handlers %d\n", __func__, result);
//    }

    // At this point, any signal, including the current, could be
    // reraised and our handler would not be invoked. This protects us
    // from a crash loop within the handler.

    // Has something gone wrong with the function invocation? For example,
    // was our handler reinstalled without SA_SIGINFO?
    if (ImpactInvalidPtr(info) || ImpactInvalidPtr(uap)) {
        return;
    }

    ImpactSignalLog(state, info);
    ImpactCrashHandler(state, uap->uc_mcontext);

    ImpactSignalHandlerExitAdjustState(state);

    // restore errno before allowing signal to be re-raised
    errno = savedErrno;
}