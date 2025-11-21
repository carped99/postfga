
#pragma once

extern "C" {
#include "postgres.h"
#include "utils/guc.h"
// #include "client.h"  // GrpcClient, GrpcCheckRequest, GrpcWriteRequest, CheckResponse, WriteResponse, postfga_client_*
}

/*
 * ClientAdapter
 *
 * - gRPC 클라이언트 초기화/종료 관리
 * - GUC(postfga.endpoint)에서 endpoint 읽어서 초기화
 * - health 체크 및 poll 래핑
 * - async check/write 호출 래핑
 *
 * 이 클래스는 BGW 안에서만 사용한다고 가정.
 */
namespace postfga::client {

class ClientAdapter {
public:
    ClientAdapter() = default;
    ~ClientAdapter();

    // 클라이언트가 없으면 GUC에서 endpoint를 읽어 초기화한다.
    // 초기화 실패/health 실패 시 false 반환.
    bool ensure_initialized();

    // 단순 health 상태 확인 (초기화 안 되어 있으면 false)
    bool is_healthy() const;

    using CheckCallback = void (*)(const CheckResponse *, void *);
    using WriteCallback = void (*)(const WriteResponse *, void *);

    // async Check 요청
    // - ensure_initialized() 가 먼저 성공해야 함.
    bool check_async(const GrpcCheckRequest &req,
                     CheckCallback callback,
                     void *user_data);

    // async Write 요청
    bool write_async(const GrpcWriteRequest &req,
                     WriteCallback callback,
                     void *user_data);

    // 콜 완료 확인용 poll
    void poll(int timeout_ms);

    // 강제로 클라이언트 재생성하고 싶을 때 사용 (예: 치명적 에러 이후)
    void reset();

private:
    bool init_from_guc();

    // GrpcClient *client_ = nullptr;
};

} // namespace postfga::client
    