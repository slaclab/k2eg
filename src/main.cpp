#include <k2eg/k2eg.h>
#include <k2eg/service/epics/EpicsChannel.h>

#include <cstdlib>
#include <csignal>
#include <iostream>
#include <csignal>
#include <iostream>

// Place the macro code here:
#if defined(__SANITIZE_ADDRESS__)
    #define K2EG_USE_ASAN
#elif defined(__has_feature)
    #if __has_feature(address_sanitizer)
        #define K2EG_USE_ASAN
    #endif
#endif

k2eg::K2EG g;

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

int main(int argc, const char* argv[])
{
// Install a signal handler via sigaction
auto install_handler = [](int sig, void (*handler)(int)) {
    struct sigaction sa{};
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    return sigaction(sig, &sa, nullptr) == 0;
};
// Register handlers
#ifdef K2EG_USE_ASAN
    install_handler(SIGUSR1, handle_signal);
#endif
    if (!install_handler(SIGINT, event_handler)) {
        std::cerr << "SIGINT handler registration error" << '\n';
        return EXIT_FAILURE;
    }
    if (!install_handler(SIGTERM, event_handler)) {
        std::cerr << "SIGTERM handler registration error" << '\n';
        return EXIT_FAILURE;
    }
    // RAII guard for EPICS init/deinit
    struct EpicsGuard {
        EpicsGuard() { k2eg::service::epics_impl::EpicsChannel::init(); }
        ~EpicsGuard() { k2eg::service::epics_impl::EpicsChannel::deinit(); }
    } epics;
    int ret = g.run(argc, argv);
    return ret;
}