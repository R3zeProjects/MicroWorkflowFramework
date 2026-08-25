#include <vosp/workflow/version.hpp>

static_assert(vosp::workflow::version_major == 0);
static_assert(vosp::workflow::version_minor == 1);
static_assert(vosp::workflow::version_patch == 1);
static_assert(vosp::workflow::version == "0.1.1-beta");
int mwf_version_header_contract()
{
    return 0;
}
