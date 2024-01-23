#include <k2eg/k2eg.h>
#include <k2eg/service/epics/EpicsChannel.h>

#include <cstdlib>
#include <iomanip>
#include <iostream>

k2eg::K2EGateway g;
void event_handler(int signum) {
    if ((signum == SIGABRT) || (signum == SIGSEGV)) {
        std::cerr << "INTERNAL ERROR, please provide log, Catch SIGNAL: " << signum;
    } else {
        g.stop();
    }
}

int main(int argc, char* argv[]) {
    if (signal((int)SIGINT, event_handler) == SIG_ERR) {
        std::cerr << "SIGINT Signal handler registration error";
        return EXIT_FAILURE;
    }

    if (signal((int)SIGTERM, event_handler) == SIG_ERR) {
        std::cerr << "SIGTERM Signal handler registration error";
        return EXIT_FAILURE;
    }
    // init ca epics support
    k2eg::service::epics_impl::EpicsChannel::init();
    int ret = g.run(argc, const_cast<const char**>(argv));
    // init deinit epics support
    k2eg::service::epics_impl::EpicsChannel::deinit();
    return ret;
}