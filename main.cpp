#include <fstream>
#include <queue>
#include <rxcpp/rx.hpp>
#include <immer/map.hpp>
#include <curses.h>
#include "crt-expr.hpp"
#include "crt-context.hpp"
#include "crt-algorithm.hpp"




//=============================================================================
#define KEY_DELETE 127
#define KEY_RETURN 10
#define KEY_TAB 9
#define KEY_CONTROL(c) (c) - 96
#define PAIR_SUCCESS 1
#define PAIR_ERROR 2
#define PAIR_SELECTED 3
#define PAIR_SELECTED_FOCUS 4
#define COMPONENT_CONSOLE 0
#define COMPONENT_LIST 1




//=============================================================================
struct Action
{
    Action(int key) : key(key) {}
    Action(crt::context products) : products(products) {}

    int key = -1;
    crt::context products;
};
    



//=============================================================================
struct State
{


    //=========================================================================
    static State load(std::string fname)
    {
        auto state = State();
        auto ifs = std::ifstream(fname);
        auto ser = std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
        state.rules = crt::context::parse(ser);
        return state;
    }

    void dump(std::string fname) const
    {
        auto doc = crt::cont_t().transient();

        for (const auto& item : rules)
        {
            doc.push_back(item.second.keyed(item.first));
        }

        auto outf = std::ofstream(fname);
        outf << crt::expression(doc.persistent()).unparse();
    }


    //=========================================================================
    int cursor = 0;
    int selected_kernel_row = 0;
    int focus_component = 0;
    bool success = true;
    std::string text;
    std::string message;
    crt::context rules;
    crt::context products;
    crt::context products_prev;
};




//=============================================================================
struct Screen
{
    Screen()
    {
        reset();
    }

    ~Screen()
    {
        delete_windows();
    }

    Screen(Screen&& other)
    {
        consoleView = other.consoleView;
        messageView = other.messageView;
        kernelView  = other.kernelView;
        other.consoleView = nullptr;
        other.messageView = nullptr;
        other.kernelView  = nullptr;
    }

    Screen(const Screen& other) = delete;
    Screen& operator=(const Screen& other) = delete;

    void delete_windows()
    {
        if (consoleView)
        {
            delwin(consoleView);
            consoleView = nullptr;
        }
        if (kernelView)
        {
            delwin(kernelView);
            kernelView = nullptr;
        }
        if (messageView)
        {
            delwin(messageView);
            messageView = nullptr;
        }
        endwin();
    }

    void reset()
    {
        int lines, cols;

        delete_windows();

        initscr();
        cbreak();
        noecho();
        raw();

        mousemask(ALL_MOUSE_EVENTS, NULL);
        mouseinterval(0);
        getmaxyx(stdscr, lines, cols);

        assert(has_colors());
        start_color();
        init_pair(PAIR_ERROR, COLOR_RED, COLOR_BLACK);
        init_pair(PAIR_SUCCESS, COLOR_GREEN, COLOR_BLACK);
        init_pair(PAIR_SELECTED, COLOR_BLUE, COLOR_GREEN);
        init_pair(PAIR_SELECTED_FOCUS, COLOR_BLUE, COLOR_WHITE);

        consoleView = derwin(stdscr, 3, cols, lines - 3, 0);
        messageView = derwin(stdscr, 3, cols, lines - 6, 0);
        kernelView  = derwin(stdscr, lines - 6, cols, 0, 0);

        nodelay(consoleView, TRUE);
        keypad(consoleView, TRUE);
        render(lastState);
    }

    void render(const State& state)
    {
        lastState = state;

        clear();
        draw_console(consoleView, state);
        draw_kernel_view(kernelView, state);
        draw_message_view(messageView, state);
        wrefresh(kernelView);
        wrefresh(messageView);

        switch (state.focus_component)
        {
            case COMPONENT_CONSOLE:
                curs_set(1);
                break;
            case COMPONENT_LIST:
                curs_set(0);
                break;
        }
    }

    int get_character()
    {
        return wgetch(consoleView);
    }


private:


    //=============================================================================
    static void draw_console(WINDOW *win, State state)
    {
        wborder(win, 0, 0, 0, 0, 0, 0, 0, 0);
        wmove(win, 1, 1);
        waddch(win, ':');
        waddch(win, '>');
        waddch(win, ' ');

        for (auto c : state.text)
        {
            waddch(win, c);
        }
        wmove(win, 1, state.cursor + 4);
    }

    static void draw_message_view(WINDOW *win, State state)
    {
        wattron(win, COLOR_PAIR(state.success ? PAIR_SUCCESS : PAIR_ERROR));
        wborder(win, 0, 0, 0, 0, 0, 0, 0, 0);
        wmove(win, 1, 1);
        wprintw(win, "%s", state.message.data());
        wattroff(win, COLOR_PAIR(state.success ? PAIR_SUCCESS : PAIR_ERROR));
    }

    static void draw_kernel_view(WINDOW *win, State state)
    {
        wborder(win, 0, 0, 0, 0, 0, 0, 0, 0);

        int row = 0;

        for (const auto& item : state.rules)
        {
            if (state.selected_kernel_row == row)
            {
                if (state.focus_component == COMPONENT_LIST)
                    wattron(win, COLOR_PAIR(PAIR_SELECTED_FOCUS));
                else
                    wattron(win, COLOR_PAIR(PAIR_SELECTED));
            }

            wmove(win, row + 1, 1);
            wprintw(win, "%-20s", item.first.data());

            wattroff(win, COLOR_PAIR(PAIR_SELECTED_FOCUS));
            wattroff(win, COLOR_PAIR(PAIR_SELECTED));

            wmove(win, row + 1, 24);
            wprintw(win, "%s", item.second.keyed("").unparse().data());

            wmove(win, row + 1, 80);

            try {
                wprintw(win, "%s", state.products.at(item.first).keyed("").unparse().data());
            }
            catch (const std::out_of_range& e)
            {
                try {
                    wprintw(win, "%s", (state.products_prev.at(item.first).keyed("").unparse() + " <pending>").data());
                }
                catch (const std::out_of_range& e)
                {
                    wprintw(win, "%s", "<not cached>");                
                }
            }
            ++row;
        }
    }

    WINDOW* consoleView = nullptr;
    WINDOW* messageView = nullptr;
    WINDOW* kernelView  = nullptr;

    State lastState;
};




//=============================================================================
State decrement_selected_row(State state)
{
    if (state.selected_kernel_row > 0)
        state.selected_kernel_row--;
    return state;
}

State increment_selected_row(State state)
{
    if (state.selected_kernel_row < state.rules.size() - 1)
        state.selected_kernel_row++;
    return state;
}

State decrement_text_cursor(State state)
{
    if (state.cursor > 0)
        state.cursor--;
    return state;
}

State increment_text_cursor(State state)
{
    if (state.cursor < state.text.size())
        state.cursor++;
    return state;
}

State insert_rule_from_text(State state)
{
    auto e = crt::parse(state.text);
    state.rules = state.rules.insert(e);
    state.products = state.products.erase(state.rules.referencing(e.key()));
    return state;
}

State remove_selected_rule(State state)
{
    auto s = state;
    auto key = s.rules.nth_key(s.selected_kernel_row);
    s.rules = state.rules.erase(key);
    s.products = state.products.erase(state.rules.referencing(key));
    return s;
}

State ensure_selected_rule_in_range(State state)
{
    if (state.selected_kernel_row >= state.rules.size())
        state.selected_kernel_row = state.rules.size() - 1;
    if (state.selected_kernel_row < 0)
        state.selected_kernel_row = 0;
    return state;
}

State delete_text_backward(State state)
{
    if (state.cursor > 0)
    {
        const auto& t = state.text;
        const auto& c = state.cursor;
        state.text = t.substr(0, c - 1) + t.substr(c);
        state.cursor--;
    }
    return state;
}

State delete_text_forward(State state)
{
    if (state.cursor < state.text.size())
    {
        const auto& t = state.text;
        const auto& c = state.cursor;
        state.text = t.substr(0, c) + t.substr(c + 1);
    }
    return state;
}

State delete_text_to_end(State state)
{
    state.text = state.text.substr(0, state.cursor);
    return state;
}

State append_char_to_entry(State state, char c)
{
    if (std::isalnum(c) || std::ispunct(c) || std::isspace(c))
    {
        state.text = state.text.substr(0, state.cursor)
        + std::string(1, c)
        + state.text.substr(state.cursor);
        state.cursor++;
    }
    return state;
}

State cursor_to_start(State state)
{
    state.cursor = 0;
    return state;
}

State cursor_to_end(State state)
{
    state.cursor = state.text.size();
    return state;
}

State resolve_products(State state)
{
    state.products = crt::resolve_full(state.rules, state.products);
    return state;
}




//=============================================================================
State reduce(State state, int key)
{
    switch (key)
    {
        case KEY_CONTROL('d'): return delete_text_backward  (state);
        case KEY_CONTROL('k'): return delete_text_to_end    (state);
        case KEY_CONTROL('a'): return cursor_to_start       (state);
        case KEY_CONTROL('e'): return cursor_to_end         (state);
        case KEY_LEFT:         return decrement_text_cursor (state);
        case KEY_RIGHT:        return increment_text_cursor (state);
        case KEY_UP:           return decrement_selected_row(state);
        case KEY_DOWN:         return increment_selected_row(state);
        case KEY_RETURN:       return delete_text_to_end(cursor_to_start(insert_rule_from_text(state)));
        case KEY_MOUSE:        return state;
        case KEY_TAB: state.focus_component = (state.focus_component + 1) % 2; return state;
        case KEY_DELETE:
            return state.focus_component == COMPONENT_LIST
            ? ensure_selected_rule_in_range(remove_selected_rule(state))
            : delete_text_backward(state);
        default:
            return state.focus_component == COMPONENT_LIST
            ? state
            : append_char_to_entry(state, key);
    }
    return state;
}

State main_reducer(State state, Action action)
{
    auto c = action.key;

    if (c == -1)
    {
        if (action.products.empty())
        {
            state.products_prev = state.products;
        }
        else
        {
            state.products = action.products;
        }
        return state;
    }

    try {
        state.message = "last event: " + std::to_string(c);            
        state.success = true;
        state = reduce(state, c);
    }
    catch (const std::exception& e)
    {
        state.message = e.what();
        state.success = false;
    }
    return state;
}




//=============================================================================
template<typename T>
struct single_item_queue
{
public:

    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m);
        return is_empty;
    }

    void push(const T& new_value)
    {
        std::lock_guard<std::mutex> lock(m);
        value = new_value;
        is_empty = false;
    }

    T pop()
    {
        std::lock_guard<std::mutex> lock(m);

        if (is_empty)
        {
            throw std::out_of_range("pop called on empty single_item_queue");
        }
        is_empty = true;
        return value;
    }

private:
    T value;
    bool is_empty = true;
    mutable std::mutex m;
};




//=============================================================================
int main()
{
    using namespace rxcpp;


    // Declare action and state queues, initialize screen
    //=========================================================================
    single_item_queue<crt::context> prods_queue;
    single_item_queue<State> state_queue;
    auto screen = Screen();
    auto state = resolve_products(State::load("out.crt"));


    // Declare the event bus
    //=========================================================================
    auto event_subject = subjects::subject<Action>();
    auto event_bus = event_subject.get_subscriber();
    auto event_stream = event_subject.get_observable();


    // Declare event pipelines
    //=========================================================================
    auto state_stream = event_stream.scan(state, main_reducer);
    auto prods_stream = state_stream.map([] (auto state)
    {
        return observable<>::create<crt::context>(crt::resolution_of(state.rules, state.products, 300))
        .subscribe_on(observe_on_event_loop())
        .concat(observable<>::just(crt::context()));
    })
    .switch_on_next()
    .start_with(state.products);

    prods_stream.subscribe([&prods_queue] (auto p) { prods_queue.push(p); });
    state_stream.subscribe([&state_queue] (auto s) { state_queue.push(s); });


    // Helpers to interpret event codes
    //=========================================================================
    auto is_user_event = [] (int c)
    {
        return c != ERR;
    };

    auto requires_screen_reset = [] (int c)
    {
        return c == 410 || c == -1 || c == KEY_CONTROL('r');
    };

    auto is_exit_event = [] (int c, State state)
    {
        return c == KEY_CONTROL('c') || (c == KEY_CONTROL('d') && state.text.empty());
    };


    // Main loop
    //=========================================================================
    while (true)
    {
        int c = screen.get_character();

        if (is_user_event(c))
        {
            if (is_exit_event(c, state))
            {
                break;
            }
            else if (requires_screen_reset(c))
            {
                screen.reset();
            }
            else
            {
                event_bus.on_next(c);
            }
        }
        else if (! prods_queue.empty())
        {
            event_bus.on_next(prods_queue.pop());
        }
        else if (! state_queue.empty())
        {
            screen.render(state = state_queue.pop());
        }
    }

    event_bus.on_completed();
    state.dump("out.crt");

    return 0;
}
