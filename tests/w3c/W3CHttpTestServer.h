#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace httplib {
class Server;
class Request;
class Response;
}  // namespace httplib

namespace RSM {
namespace W3C {

class W3CHttpTestServer {
public:
    using EventCallback = std::function<void(const std::string &eventName, const std::string &data)>;

    W3CHttpTestServer(int port, const std::string &path = "/test");
    ~W3CHttpTestServer();

    bool start();
    void stop();

    void setEventCallback(const EventCallback &callback) {
        eventCallback_ = callback;
    }

    bool isRunning() const {
        return running_.load();
    }

    int getPort() const {
        return port_;
    }

    std::string getPath() const {
        return path_;
    }

private:
    void handlePost(const httplib::Request &req, httplib::Response &res);

    int port_;
    std::string path_;
    std::unique_ptr<httplib::Server> server_;
    std::thread serverThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> shutdownRequested_{false};
    EventCallback eventCallback_;
};

}  // namespace W3C
}  // namespace RSM