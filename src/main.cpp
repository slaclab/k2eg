#include <k2eg/k2eg.h>
#include <k2eg/service/epics/EpicsChannel.h>

#include <cstdlib>
#include <iomanip>
#include <iostream>

// Place the macro code here:
#if defined(__SANITIZE_ADDRESS__)
    #define K2EG_USE_ASAN
#elif defined(__has_feature)
    #if __has_feature(address_sanitizer)
        #define K2EG_USE_ASAN
    #endif
#endif

k2eg::K2EGateway g;

#ifdef K2EG_USE_ASAN
extern "C" void __lsan_do_leak_check();

void handle_signal(int sig)
{
    if (sig == SIGUSR1)
    {
        std::cout << "[LSAN] Triggering leak check...\n";
        __lsan_do_leak_check();
    }
}
#endif

void event_handler(int signum)
{
    if ((signum == SIGABRT) || (signum == SIGSEGV))
    {
        std::cerr << "INTERNAL ERROR, please provide log, Catch SIGNAL: " << signum;
    }
    else
    {
        g.stop();
    }
}

int main(int argc, char* argv[])
{
#ifdef K2EG_USE_ASAN
    signal(SIGUSR1, handle_signal);
#endif
    if (signal((int)SIGINT, event_handler) == SIG_ERR)
    {
        std::cerr << "SIGINT Signal handler registration error";
        return EXIT_FAILURE;
    }

    if (signal((int)SIGTERM, event_handler) == SIG_ERR)
    {
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