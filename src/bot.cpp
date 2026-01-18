#include "chatroom.grpc.pb.h"
#include <grpcpp/grpcpp.h>

#include <chrono>
#include <print>
#include <thread>
#include <dotenv.h>

using chatroom::ChatMessage;
using chatroom::ChatRequest;
using chatroom::ChatService;
using chatroom::ChatSnapshot;
using chatroom::Empty;
using chatroom::PingResponse;
using chatroom::SnapshotRequest;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using namespace std::chrono_literals;

int main()
{
    dotenv::init(dotenv::Preserve, "config/bot.env");

    std::string serverAddr;

    const char *envVar = std::getenv("CHATROOM_SERVER_ADDRESS");
    if (envVar)
    {
        serverAddr = std::string(envVar);
    }
    else
    {
        std::println("Failed to parse CHATROOM_SERVER_ADDRESS environment variable. Please specify a server address.");
        return EXIT_FAILURE;
    }

    std::shared_ptr<Channel> channel = grpc::CreateChannel(serverAddr, grpc::InsecureChannelCredentials());
    std::unique_ptr<ChatService::Stub> stub(ChatService::NewStub(channel));

    ChatMessage msg;
    msg.set_username("Bot");
    msg.set_text("Hello");

    auto TestConnect = [&stub]() {
        Empty request;
        PingResponse response;
        grpc::ClientContext context;

        grpc::Status status = stub->Ping(&context, request, &response);
        if (!status.ok())
        {
            std::println("Bot failed to connect to server: {}", status.error_message());
            return false;
        }

        return true;
    };

    while (!TestConnect())
    {
        std::this_thread::sleep_for(3s);
    }

    std::println("Bot connected to server.");

    while (true)
    {
        {
            using namespace std::chrono;
            msg.set_timestamp(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
        }

        ChatRequest request;
        *request.mutable_message() = msg;

        Empty response;
        grpc::ClientContext context;

        grpc::Status status = stub->SendMessage(&context, request, &response);

        std::this_thread::sleep_for(2s);
    }

    return 0;
}