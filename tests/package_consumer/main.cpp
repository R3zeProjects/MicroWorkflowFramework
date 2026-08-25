#include <vosp/workflow.hpp>

int main()
{
    const auto capabilities = vosp::capabilities();
    return capabilities.wall_time_limit ? 0 : 1;
}
