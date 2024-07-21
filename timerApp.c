#include <epicsStdio.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <epicsString.h>
#include <epicsAlarm.h>
#include <epicsMutex.h>
#include <dbScan.h>
#include <dbAccess.h>
#include <recGbl.h>
#include <recSup.h>
#include <devSup.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#include <boolinRecord.h>
#include <booloutRecord.h>
#include "timerRecord.h"

typedef struct {
    ELLNODE node;
    timerRecord *prec;
    epicsMutexId lock;
    epicsThreadId threadId;
    int running;
} timerDset;

static void timerThread(void *arg) {
    timerDset *pdset = (timerDset *) arg;
    timerRecord *prec = pdset->prec;
    epicsTimeStamp startTime;
    double setpoint;
    double elapsed;

    epicsMutexLock(pdset->lock);
    pdset->running = 1;
    epicsMutexUnlock(pdset->lock);

    epicsTimeGetCurrent(&startTime);
    setpoint = prec->set;

    while (pdset->running) {
        epicsThreadSleep(1.0);

        epicsMutexLock(pdset->lock);
        if (!pdset->running) {
            epicsMutexUnlock(pdset->lock);
            break;
        }

        elapsed = epicsTimeDiffInSeconds(&startTime, epicsTimeGetCurrent());
        if (elapsed >= setpoint) {
            prec->val = setpoint;
            prec->tse = (long)elapsed;
            prec->tme = (long)(elapsed / 60.0);
            prec->tte = (long)(elapsed + setpoint);
            recGblSetSevr(prec, TIME_ALARM, INVALID_ALARM);
            booloutRecordProcess(prec);
        } else {
            prec->val = elapsed;
            prec->tse = (long)elapsed;
            prec->tme = (long)(elapsed / 60.0);
            prec->tte = (long)(elapsed + setpoint);
            recGblSetSevr(prec, NO_ALARM, INVALID_ALARM);
        }
        epicsMutexUnlock(pdset->lock);
    }

    epicsMutexLock(pdset->lock);
    pdset->running = 0;
    epicsMutexUnlock(pdset->lock);
}

static long timerInit(timerDset *pdset) {
    pdset->lock = epicsMutexMustCreate();
    pdset->running = 0;
    pdset->threadId = epicsThreadMustCreate("timerThread",
        epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackMedium),
        (EPICSTHREADFUNC)timerThread, pdset);
    return 0;
}

static long timerStart(timerRecord *prec) {
    timerDset *pdset = (timerDset *)prec->dpvt;
    epicsMutexLock(pdset->lock);
    pdset->running = 1;
    epicsMutexUnlock(pdset->lock);
    epicsThreadStart(pdset->threadId);
    return 0;
}

static long timerStop(timerRecord *prec) {
    timerDset *pdset = (timerDset *)prec->dpvt;
    epicsMutexLock(pdset->lock);
    pdset->running = 0;
    epicsMutexUnlock(pdset->lock);
    return 0;
}

static long timerSet(timerRecord *prec) {
    prec->set = prec->rval;
    return 0;
}

static long timerEnable(timerRecord *prec) {
    if (prec->enbl) {
        timerStart(prec);
    } else {
        timerStop(prec);
    }
    return 0;
}

static long timerReset(timerRecord *prec) {
    timerStop(prec);
    prec->val = 0.0;
    prec->tse = 0;
    prec->tme = 0;
    prec->tte = 0;
    prec->set = 0.0;
    recGblSetSevr(prec, NO_ALARM, INVALID_ALARM);
    return 0;
}

static long timerAck(timerRecord *prec) {
    recGblSetSevr(prec, NO_ALARM, prec->ackS);
    return 0;
}

static long init_record(timerRecord *prec) {
    timerDset *pdset;

    prec->dpvt = callocMustSucceed(1, sizeof(timerDset), "timerDset");
    pdset = (timerDset *)prec->dpvt;
    ellAdd(&prec->dpvl, &pdset->node);

    timerInit(pdset);

    prec->set = prec->rval;
    prec->val = 0.0;
    prec->tse = 0;
    prec->tme = 0;
    prec->tte = 0;
    prec->pact = TRUE;
    prec->ackS = INVALID_ALARM;
    prec->ackT = INVALID_ALARM;
    prec->simm = TRUE;
    prec->enbl = FALSE;

    return 0;
}

static long process(timerRecord *prec) {
    if (prec->pact) {
        timerEnable(prec);
    }
    return 0;
}

static long special_linconv(DBLINK *pdbLink) {
    long status = 0;
    long *pval = (long *)pdbLink->value.pv;
    double val = (double)*pval / 100.0;
    epicsSnprintf(pdbLink->value.sval, sizeof(pdbLink->value.sval), "%ld:%02ld:%02.2f",
        (long)val / 60, (long)val % 60, val - (long)val);
    return status;
}

struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read_write;
    DEVSUPFUN special_linconv;
} devTimer = {
    6,
    NULL,
    NULL,
    init_record,
    NULL,
    NULL,
    special_linconv
};
epicsExportAddress(dset, devTimer);

