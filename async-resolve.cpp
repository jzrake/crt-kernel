#include <queue>
#include "crt-expr.hpp"
#include "crt-workers.hpp"
#include "crt-context.hpp"




//=============================================================================
struct Message
{
    enum class Type
    {
        None,
        TaskStarting,
        TaskCanceled,
        TaskFinished,
    };

    Message() {}
    Message(Type type, std::string message, crt::expression value)
    : type(type)
    , message(message)
    , value(value)
    {
    }

    operator bool() const
    {
        return type != Type::None;
    }

    Type type = Type::None;
    std::string message;
    crt::expression value;
};




//=============================================================================
class MessageQueue : public crt::worker_pool::listener_t
{
public:

    void task_starting(int worker, std::string name) override
    {
        char message[1024];
        std::snprintf(message, 1024, "task '%s' starting on worker %d", name.data(), worker);

        std::lock_guard<std::mutex> lock(mutex);
        messages.push({Message::Type::TaskStarting, message, {}});
    }

    void task_canceled(int worker, std::string name) override
    {
        char message[1024];
        std::snprintf(message, 1024, "task '%s' canceled on worker %d", name.data(), worker);

        std::lock_guard<std::mutex> lock(mutex);
        messages.push({Message::Type::TaskCanceled, message, {}});
    }

    void task_finished(int worker, std::string name, crt::worker_pool::product_t result) override
    {
        char message[1024];
        std::snprintf(message, 1024, "task '%s' finished on worker %d: %s",
            name.data(), worker, result.unparse().data());

        // std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::lock_guard<std::mutex> lock(mutex);
        messages.push({Message::Type::TaskFinished, message, result});
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return messages.empty();
    }

    Message next()
    {
        std::lock_guard<std::mutex> lock(mutex);

        if (messages.empty())
        {
            return Message();
        }
        auto message = messages.front();
        messages.pop();
        return message;
    }

private:
    std::queue<Message> messages;
    mutable std::mutex mutex;
};




//=============================================================================
int resolve(std::string source)
{
    MessageQueue messenger;
    crt::worker_pool workers(4, &messenger);

    auto rules = crt::context::parse(source);
    auto products = rules.resolve(workers);


    while (products != rules.resolve())
    {
        while (auto message = messenger.next())
        {
            // std::printf("%s\n", message.message.data());

            if (message.value)
            {
                products = rules.resolve(workers, products.insert(message.value));
            }
        }
    }

    return 0;
}




//=============================================================================
int main(int argc, const char* argv[])
{
    for (int n = 0; n < (argc > 1 ? std::atoi(argv[1]) : 1); ++n)
    {
        resolve("(a=b b=c c=d d=e e=f f=g g=h h=i i=j j=1)");
        resolve("(a=(b c)    b=(d e) c=(f g)    d=(h i) e=(j k) f=(l m) g=(n o)    h=1 i=2 j=3 k=4 l=5 m=6 n=7 o=8)");
    }
    return 0;
}
