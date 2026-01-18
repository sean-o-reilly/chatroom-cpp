#include <dotenv.h>
#include <ftxui/component/component.hpp>          // for Input, Renderer, Vertical
#include <ftxui/component/screen_interactive.hpp> // for Component, ScreenInteractive
#include <grpcpp/grpcpp.h>
#include <print>
#include <yaml-cpp/yaml.h>

#include "chatroom.grpc.pb.h"

using namespace ftxui;

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

class ChatClient
{
  public:
    ChatClient(std::shared_ptr<Channel> channel) : stub_(ChatService::NewStub(channel))
    {
        using namespace std::chrono;
        timeOfLastReceivedMessage_ =
            duration_cast<milliseconds>(system_clock::now().time_since_epoch() - (10min)).count();
        // request at most last 10 minutes of chat history when starting chat
    }

    void SendMessage(const std::string &username, const std::string &text)
    {
        ChatMessage msg;
        msg.set_username(username);
        msg.set_text(text);

        {
            using namespace std::chrono;
            msg.set_timestamp(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
        }

        ChatRequest request;
        *request.mutable_message() = msg;

        Empty response;
        grpc::ClientContext context;

        grpc::Status status = stub_->SendMessage(&context, request, &response);

        if (!status.ok())
        {
            throw std::runtime_error(status.error_message());
        }
    }

    std::vector<ChatMessage> GetSnapshot()
    {
        grpc::ClientContext context;
        SnapshotRequest request;
        request.set_starttime(timeOfLastReceivedMessage_);

        ChatSnapshot response;
        grpc::Status status = stub_->GetSnapshot(&context, request, &response);

        if (!status.ok())
        {
            throw std::runtime_error(status.error_message());
        }

        if (response.messages_size())
        {
            timeOfLastReceivedMessage_ =
                std::max(timeOfLastReceivedMessage_,
                         (*(response.messages().rbegin())).timestamp()); // take max of most recent message
        }

        return std::vector(response.messages().begin(), response.messages().end());
    }

    bool PingServer()
    {
        Empty request;
        PingResponse response;
        grpc::ClientContext context;

        grpc::Status status = stub_->Ping(&context, request, &response);
        if (!status.ok())
        {
            std::println("Failed to connect to server: {}", status.error_message());
            return false;
        }

        return true;
    }

  private:
    ChatClient() = delete;

    const std::unique_ptr<ChatService::Stub> stub_;

    int64_t timeOfLastReceivedMessage_;
};

class Feed
{
  public:
    Feed(int size) : feedSizeLimit_(size)
    {
    }

    void Update(std::vector<ChatMessage> &messages)
    {
        std::lock_guard<std::mutex> lg(feedMutex_);
        for (const ChatMessage &msg : messages)
        {
            std::string str = "[" + msg.username() + "] " + msg.text();
            feed_.push_back(str);

            if (feed_.size() > feedSizeLimit_)
            {
                feed_.pop_front();
            }
        }
    }

    std::vector<std::string> Get() const
    {
        return std::vector(feed_.begin(), feed_.end());
    }

    std::vector<std::string> Get(const int n) const
    {
        if (feed_.size() < n)
        {
            return Get();
        }
        return std::vector(feed_.end() - n, feed_.end());
    }

  private:
    Feed() = delete;

    std::mutex feedMutex_;
    std::deque<std::string> feed_;

    const int feedSizeLimit_;
};

class ToastManager
{
  public:
    ToastManager() = default;

    void AddToast(const std::string &message, std::chrono::milliseconds ttl = 3s)
    {
        std::unique_lock<std::mutex> lock(mtx);

        cv.wait(lock, [this] { return !toastActive; });

        currentToast = message;
        toastActive = true;

        std::thread([this, ttl] {
            std::this_thread::sleep_for(ttl);
            {
                std::lock_guard<std::mutex> lock(mtx);
                currentToast.clear();
                toastActive = false;
            }
            cv.notify_all();
        }).detach();
    }

    std::string GetCurrentToast()
    {
        return currentToast;
    }

  private:
    std::string currentToast;
    std::condition_variable cv;
    std::mutex mtx;
    bool toastActive = false;
};

Color StringToFTXColor(const std::string &str)
{
    // clang-format off
        if (str == "Default") return Color::Default;
        if (str == "Black") return Color::Black;
        if (str == "GrayDark") return Color::GrayDark;
        if (str == "GrayLight") return Color::GrayLight;
        if (str == "White") return Color::White;
        if (str == "Blue") return Color::Blue;
        if (str == "BlueLight") return Color::BlueLight;
        if (str == "Cyan") return Color::Cyan;
        if (str == "CyanLight") return Color::CyanLight;
        if (str == "Green") return Color::Green;
        if (str == "GreenLight") return Color::GreenLight;
        if (str == "Magenta") return Color::Magenta;
        if (str == "MagentaLight") return Color::MagentaLight;
        if (str == "Red") return Color::Red;
        if (str == "RedLight") return Color::RedLight;
        if (str == "Yellow") return Color::Yellow;
        if (str == "YellowLight") return Color::YellowLight;
    // clang-format on

    return Color::Default;
}

int main()
{
    dotenv::init(dotenv::Preserve, "config/client.env");

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

    ChatClient client(channel);

    while (!client.PingServer())
    {
        std::this_thread::sleep_for(3s); // retry
    }

    // connected
    YAML::Node config = YAML::LoadFile("config/client.yaml");

    Color feedColor;
    Color inputColor;

    try
    {
        feedColor = StringToFTXColor(config["feedColor"].as<std::string>());
    }
    catch (...)
    {
        std::println("Failed to parse feedColor from client.yaml.");
    }

    try
    {
        inputColor = StringToFTXColor(config["inputColor"].as<std::string>());
    }
    catch (...)
    {
        std::println("Failed to parse inputColor from client.yaml.");
    }

    auto screen = ScreenInteractive::Fullscreen();

    constexpr int feedInternalMax = 50;
    Feed feed(feedInternalMax);

    std::string name;

    {
        std::string inputString;
        InputOption inputBoxOption;
        inputBoxOption.multiline = false;

        auto enterCallback = [&] {
            // trim / validate
            if (!inputString.empty() && inputString.find_first_not_of(" \t\n\v\f\r") != std::string::npos)
            {
                name = inputString;
                screen.Exit();
            }
        };

        inputBoxOption.on_enter = enterCallback;

        Component inputBox = Input(&inputString, inputBoxOption);
        auto component = Container::Vertical({inputBox});

        auto renderer = Renderer(component, [&] {
            return hbox({text("Enter your name:") | vcenter | border, inputBox->Render() | border}) | vcenter |
                   size(HEIGHT, EQUAL, 1);
        });

        screen.Loop(renderer);
    }

    if (name.empty())
    {
        return 0;
    }

    ToastManager errorToasts;

    std::jthread feedUpdater([&](std::stop_token stoken) {
        while (!stoken.stop_requested())
        {
            try
            {
                auto newMsgs = client.GetSnapshot();
                feed.Update(newMsgs);
            }
            catch (const std::exception &e)
            {
                errorToasts.AddToast("Error fetching messages from server: " + std::string(e.what()));
                std::this_thread::sleep_for(2s);
            }

            screen.PostEvent(Event::Custom); // PostEvent is thread safe
            std::this_thread::sleep_for(1s);
        }
    });

    // Chat loop
    {
        std::string inputString;

        InputOption inputBoxOption;
        inputBoxOption.multiline = false;

        auto enterCallback = [&] {
            try
            {
                client.SendMessage(name, inputString);
            }
            catch (std::exception &e)
            {
                std::string errMsg = e.what();
                std::thread([&errorToasts, errMsg] { // AddToast will block
                    errorToasts.AddToast("Failed to send message: " + errMsg);
                })
                    .detach();
            }

            inputString.clear();
            screen.PostEvent(Event::Custom);
        };

        inputBoxOption.on_enter = enterCallback;

        Component inputBox = Input(&inputString, inputBoxOption);

        auto component = Container::Vertical({inputBox});

        auto document = [&] {
            int terminalHeight = screen.dimy();
            int feedHeight = terminalHeight * 8 / 10;
            int inputHeight = terminalHeight * 2 / 10;

            Elements feedElements;
            for (const auto &line : feed.Get(feedHeight - 2)) // subtract 2 to account for border
            {
                feedElements.push_back(text(line));
            }

            return vbox({vbox(feedElements) | border | color(feedColor) | size(HEIGHT, EQUAL, feedHeight),
                         vbox(hbox({text(" > "), inputBox->Render()}) | vcenter | border | color(inputColor) |
                                  size(HEIGHT, EQUAL, inputHeight),
                              text(errorToasts.GetCurrentToast()) | color(Color::Red))});
        };

        auto renderer = Renderer(component, document);

        screen.Loop(renderer);
    }

    return 0;
}
