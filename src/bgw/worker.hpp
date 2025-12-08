#pragma once

struct FgaState;

namespace fga::bgw
{

    class Worker
    {
      public:
        explicit Worker(FgaState* state);
        void run();

      private:
        void initialize();
        void process();

        FgaState* state_ = nullptr;
    };

} // namespace fga::bgw
