extern "C" {
#include "postgres.h"
#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/proc.h"
#include "storage/lwlock.h"
#include "utils/guc.h"
#include "pgstat.h"
}

#include "worker.hpp"
#include "processor.hpp"
#include "state.h"   // get_shared_state(), PostfgaShmemState

namespace {
    volatile sig_atomic_t shutdown_requested = false;
    volatile sig_atomic_t reload_requested   = false;

    /* ---- signal handler ---- */
    void bgw_sigterm_handler(SIGNAL_ARGS) {
        int save_errno = errno;
        shutdown_requested = true;
        if (MyLatch)
            SetLatch(MyLatch);
        errno = save_errno;
    }

    void bgw_sighup_handler(SIGNAL_ARGS) {
        int save_errno = errno;
        reload_requested = true;
        if (MyLatch)
            SetLatch(MyLatch);
        errno = save_errno;
    }

} // anonymous namespace

namespace postfga::bgw {

Worker::Worker(PostfgaShmemState *state)
    : state_(state)
{
    Assert(state_ != nullptr);
}

void Worker::run()
{
    initialize();

    /* shared state 안에 latch 등록 */
    LWLockAcquire(state_->lock, LW_EXCLUSIVE);
    state_->bgw_latch = MyLatch;
    LWLockRelease(state_->lock);

    /* 메인 루프 진입 */
    process();

    /* 종료 시 latch 등록 해제 */
    LWLockAcquire(state_->lock, LW_EXCLUSIVE);
    state_->bgw_latch = NULL;
    LWLockRelease(state_->lock);
}

void Worker::initialize()
{
    /* PostgreSQL 스타일 시그널 핸들러 등록 */
    pqsignal(SIGTERM, bgw_sigterm_handler);
    pqsignal(SIGHUP, bgw_sighup_handler);

    /* block 되어 있던 signal 해제 */
    BackgroundWorkerUnblockSignals();

    /* 필요시 DB 연결 (지금은 주석 처리, 나중에 원하면 사용)
     * BackgroundWorkerInitializeConnection(NULL, NULL, 0);
     */
}

void Worker::process()
{
    Processor processor;

    while (!shutdown_requested)
    {
        int rc = WaitLatch(MyLatch,
                           WL_LATCH_SET | WL_EXIT_ON_PM_DEATH,
                           -1,
                           PG_WAIT_EXTENSION);

        /* latch reset */
        ResetLatch(MyLatch);

        /* postmaster 죽음 감지 시 곧바로 종료 */
        if (rc & WL_POSTMASTER_DEATH)
            proc_exit(1);

        /* SIGHUP 처리: config reload */
        if (reload_requested)
        {
            reload_requested = false;
            ProcessConfigFile(PGC_SIGHUP);
        }

        /* 큐에 쌓인 gRPC 요청 처리 */
        processor.execute();

        /* pg_stat_activity 보고 (큰 의미는 없지만 관례상) */
        pgstat_report_activity(STATE_IDLE, NULL);
    }
}

} // namespace postfga::bgw