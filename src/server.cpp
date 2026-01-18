#include <dotenv.h>
#include <grpcpp/grpcpp.h>
#include <mutex>
#include <print>
#include <deque>

#include "chatroom.grpc.pb.h"

using chatroom::ChatMessage;
using chatroom::ChatRequest;
using chatroom::ChatService;
using chatroom::ChatSnapshot;
using chatroom::Empty;
using chatroom::PingResponse;
using chatroom::SnapshotRequest;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

constinit int MAX_MESSAGES_LOAD = 50;
constinit int MAX_MESSAGE_CHAR_LENGTH = 100;

class ChatServiceImpl final : public ChatService::Service
{
  public:
    ChatServiceImpl(int messagesLimit) : messagesLimit(messagesLimit)
    {
    }

    Status SendMessage(ServerContext *context, const ChatRequest *request, Empty *response) override
    {
        ChatMessage msg = request->message();
        std::string text = msg.text();

        if (text.empty() || text.find_first_not_of(" \t\n\v\f\r") == std::string::npos)
        {
            return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, "Empty message.");
        }

        if (text.size() > MAX_MESSAGE_CHAR_LENGTH)
        {
            msg.set_text(text.substr(0, MAX_MESSAGE_CHAR_LENGTH) + "...");
        }

        std::lock_guard<std::mutex> guard(messages_mutex);
        messages.push_back(msg);

        if (messages.size() > messagesLimit)
        {
            messages.pop_front();
        }

        std::println(R"({{"Event":"MessageReceived","ts":{},"username":"{}","text":"{}"}})", msg.timestamp(),
                     msg.username(), msg.text());

        return Status::OK;
    }

    Status GetSnapshot(ServerContext *context, const SnapshotRequest *request, ChatSnapshot *response) override
    {
        int64_t startTime = request->starttime();

        std::vector<ChatMessage> result;

        std::copy_if(messages.begin(), messages.end(), std::back_inserter(result),
                     [startTime](const ChatMessage &m) { return m.timestamp() > startTime; });

        auto *repeated = response->mutable_messages();
        repeated->Reserve(result.size());

        for (const auto &msg : result)
        {
            *repeated->Add() = msg;
        }

        return Status::OK;
    }

    Status Ping(ServerContext *context, const Empty *request, PingResponse *response) override
    {
        response->set_message("Ping received.");
        return Status::OK;
    }

  private:
    ChatServiceImpl() = delete;

    std::mutex messages_mutex;
    std::deque<ChatMessage> messages;

    const int messagesLimit;
};

int main()
{
    dotenv::init(dotenv::Preserve, "config/server.env");

    std::string serverListenAddr;

    const char *envVar = std::getenv("CHATROOM_LISTEN_ADDRESS");
    if (envVar)
    {
        serverListenAddr = std::string(envVar);
    }
    else
    {
        std::println("Failed to parse CHATROOM_LISTEN_ADDRESS environment variable. Please specify an address.");
        return EXIT_FAILURE;
    }

    ChatServiceImpl service(MAX_MESSAGES_LOAD);

    ServerBuilder builder;
    builder.AddListeningPort(serverListenAddr, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());

    std::println("Server listening on {}", serverListenAddr);
    server->Wait();
}
