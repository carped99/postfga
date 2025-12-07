#pragma once

struct FgaState;

namespace postfga::bgw
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

} // namespace postfga::bgw
