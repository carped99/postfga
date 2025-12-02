

#include <string>

#include "openfga/v1/openfga_service.grpc.pb.h"
#include "payload.h"
#include "request_variant.hpp"

namespace postfga::client
{
    namespace
    {

        inline std::string make_user(const FgaTuple& r)
        {
            std::string user;
            user.reserve(64);
            user.append(r.subject_type).append(":").append(r.subject_id);
            return user;
        }

        inline std::string make_object(const FgaTuple& r)
        {
            std::string obj;
            obj.reserve(64);
            obj.append(r.object_type).append(":").append(r.object_id);
            return obj;
        }

        openfga::v1::CheckRequest make_check_request(const FgaCheckTupleRequest& in)
        {
            // auto object = make_object(in);
            // auto user   = make_user(in);

            openfga::v1::CheckRequest out;
            // auto* tuple_key = out.mutable_tuple_key();
            // tuple_key->set_object(object);
            // tuple_key->set_user(user);
            // tuple_key->set_relation(in.relation);
            return out;
        }

    } // anonymous namespace
} // namespace postfga::client
