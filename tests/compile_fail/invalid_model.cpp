#include <vosp/workflow.hpp>

struct InvalidModel
{
};

int main()
{
    vosp::ProcessRunner<InvalidModel> runner;
    static_cast<void>(runner);
}
