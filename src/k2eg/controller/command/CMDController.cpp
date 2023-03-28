#include <k2eg/common/utility.h>
#include <k2eg/controller/command/CMDController.h>
#include <k2eg/service/ServiceResolver.h>

#include <boost/json.hpp>

using namespace k2eg::service;
using namespace k2eg::controller::command;
using namespace k2eg::service::log;
using namespace k2eg::service::pubsub;

namespace bj = boost::json;

CMDController::CMDController(ConstCMDControllerConfigUPtr configuration, CMDControllerCommandHandler cmd_handler)
    : configuration(std::move(configuration))
    , cmd_handler(cmd_handler)
    , logger(ServiceResolver<ILogger>::resolve())
    , subscriber(ServiceResolver<ISubscriber>::resolve()) {
    start();
}

CMDController::~CMDController() { stop(); }

void CMDController::consume() {
    SubscriberInterfaceElementVector received_message;
    while (run) {
        // fetch message
        subscriber->getMsg(received_message, configuration->max_message_to_fetch, configuration->fetch_time_out);
        if (received_message.size()) {
            CommandConstShrdPtrVec result_vec;
            std::for_each(
                received_message.begin(),
                received_message.end(),
                [&logger = logger, &result_vec = result_vec](auto message) {
                    if (!message->data_len) return;
                    bj::error_code ec;
                    bj::object command_description;
                    bj::string_view value_str = bj::string_view(message->data.get(), message->data_len);
                    try {
                        command_description = bj::parse(value_str, ec).as_object();

                        if (ec) {
                            logger->logMessage(
                                STRING_FORMAT("Error: '%1%' parsing command: %2%",
                                              ec.message() % std::string(message->data.get(), message->data_len)),
                                LogLevel::ERROR);
                            return;
                        }
                        // parse the command and call the handler
                        if (auto v = MapToCommand::parse(command_description)) {
                            result_vec.push_back(v);
                        }
                    } catch (std::exception& ex) {
                        logger->logMessage(
                            STRING_FORMAT("Error: '%1%' parsing command: %2%",
                                          std::string(ex.what()) % std::string(message->data.get(), message->data_len)),
                            LogLevel::ERROR);
                    }
                });
            try {
                // dispatch the received command
                if (result_vec.size()) {
                    cmd_handler(result_vec);
                }
                // at this point we can commit, in sync mode,  the offset becaus all
                // mesage has been maaged
                subscriber->commit();
            } catch (...) {
                logger->logMessage("Error occured during command processing", LogLevel::ERROR);
            }
            received_message.clear();
        }
        // wait
        std::this_thread::sleep_for(std::chrono::microseconds(250));
    }
}

void CMDController::start() {
    logger->logMessage("Starting command controller");
    if(configuration->topic_in.empty()) {
        throw std::runtime_error("The message queue is mandatory");
    }
    logger->logMessage("Receive command message from: " + configuration->topic_in);
    subscriber->setQueue({configuration->topic_in});
    run = true;
    t_subscriber = std::thread(&CMDController::consume, this);
}

void CMDController::stop() {
    if (t_subscriber.joinable()) {
        run = false;
        t_subscriber.join();
    }
    logger->logMessage("Stopping command controller");
}