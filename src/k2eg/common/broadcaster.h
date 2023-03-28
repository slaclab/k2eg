#ifndef k2eg_COMMON_BROADCASTER_H_
#define k2eg_COMMON_BROADCASTER_H_

#include <functional>
#include <memory>
namespace k2eg::common {

/**
 * Class that abstract a brodacaster(https://stackoverflow.com/questions/34397819/using-stdfunction-and-stdbind-to-store-callback-and-handle-object-deletion/34400239#34400239)
 * the base concept is that untile the token is live the handler receive the messagess. 
 * All the handler in the array are weak_ptr taht can be locked untile the token still
 * alive.
*/
using BroadcastToken = std::shared_ptr<void>;
template <class... Args> struct broadcaster {
    using target = std::function<void(Args...)>;
    using wp_target = std::weak_ptr<target>;
    using sp_target = std::shared_ptr<target>;
    static sp_target wrap_target(target t) { return std::make_shared<target>(std::move(t)); };

    BroadcastToken registerHandler(target f) {
        auto t = wrap_target(std::move(f));
        targets.push_back(t);
        return t;
    }

    void purge() {
        targets.erase(std::remove_if(targets.begin(), targets.end(), [&](wp_target t) -> bool { return t.expired(); }),
                      targets.end());
    }

    void broadcast(Args... args) {
        purge();
        // auto targets_copy = targets; // in case targets is modified by listeners
        for (auto wp: targets) {
            if (auto sp = wp.lock()) {
                (*sp)(args...);
            }
        }
    }
    std::vector<wp_target> targets;
};
} // namespace k2eg::common

#endif // k2eg_COMMON_BROADCASTER_H_