// openfga.hpp
#pragma once

#include <functional>
#include <memory>
#include "request.h"
#include "response.h"

namespace postfga::client {

class Client {
public:
    using CheckHandler = std::function<void(const CheckResponse&)>;
    using WriteHandler = std::function<void(const WriteResponse&)>;
    //using DeleteHandler = std::function<void(const DeleteResponse&)>;

    virtual ~Client() = default;

    virtual bool is_healthy() const = 0;

    virtual void async_check(const Request& req, CheckHandler handler) = 0;

    virtual void async_write(const Request& req, WriteHandler handler) = 0;

    virtual void shutdown() = 0;
};

} // namespace postfga::client
