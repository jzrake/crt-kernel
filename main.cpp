#include <fstream>
#include <curses.h>
#include "immer/map.hpp"
#include "crt-expr.hpp"
#include "crt-context.hpp"




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
struct State
{


    //=========================================================================
    static State load(std::string fname)
    {
        auto state = State();
        auto ifs = std::ifstream(fname);
        auto ser = std::string(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());

        for (auto e : crt::parse(ser))
        {
            if (! e->key().empty())
            {
                state.rules = std::move(state.rules).insert(e);
            }
        }
        state.products = state.rules.resolve();
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
                wprintw(win, "%s", "<not cached>");
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
auto reduce_insert_rule(crt::context rules, crt::context products, std::string entry)
{
    try {
        auto e = crt::parse(entry);
        auto new_rules = rules.insert(e);
        auto new_prods = products.erase(e.symbols());
        auto new_entry = std::string();
        auto message   = "inserted " + e.key();
        auto success   = true;

        if (e.key().empty())
        {
            throw std::runtime_error("empty assignment!");
        }
        return std::make_tuple(new_rules, new_prods, new_entry, message, success);
    }
    catch (const std::exception& e)
    {
        return std::make_tuple(rules, products, entry, std::string(e.what()), false);
    }
}

auto reduce_erase_nth_entry(crt::context rules, crt::context products, int index)
{
    auto k = rules.nth_key(index);
    return std::make_pair(rules.erase(k), products.erase(rules.referencing(k)));
}




//=============================================================================
State reduce(State state, int action)
{
    MEVENT event;

    switch (action)
    {
        case KEY_MOUSE:
        {
            if (getmouse(&event) == OK)
            {
                char message[1024];
                std::snprintf(message, 1024, "click %ld at (%d, %d)", event.bstate, event.x, event.y);
                state.message = message;
                state.success = true;
            }
            break;
        }
        case KEY_TAB:
        {
            state.focus_component = (state.focus_component + 1) % 2;
            break;
        }
        case KEY_LEFT:
        {
            if (state.cursor > 0)
                state.cursor--;
            break;
        }
        case KEY_RIGHT:
        {
            if (state.cursor < state.text.size())
                state.cursor++;
            break;
        }
        case KEY_UP:
        {
            if (state.selected_kernel_row > 0)
                state.selected_kernel_row--;
            break;
        }
        case KEY_DOWN:
        {
            if (state.selected_kernel_row < state.rules.size() - 1)
                state.selected_kernel_row++;
            break;
        }
        case KEY_RETURN:
        {
            std::tie(
                state.rules,
                state.products,
                state.text,
                state.message,
                state.success) = reduce_insert_rule(state.rules, state.products, state.text);

            state.products = state.rules.resolve(state.products);
            state.cursor = state.text.size();
            break;
        }
        case KEY_DELETE:
        {
            if (state.focus_component == COMPONENT_LIST)
            {
                std::tie(
                    state.rules,
                    state.products) = reduce_erase_nth_entry(
                    state.rules,
                    state.products,
                    state.selected_kernel_row);

                state.products = state.rules.resolve(state.products);

                if (state.selected_kernel_row > state.rules.size() - 1)
                    state.selected_kernel_row--;
            }
            else if (state.cursor > 0)
            {
                const auto& t = state.text;
                const auto& c = state.cursor;
                state.text = t.substr(0, c - 1) + t.substr(c);
                state.cursor--;
            }
            break;
        }
        case KEY_CONTROL('d'):
        {
            if (state.cursor < state.text.size())
            {
                const auto& t = state.text;
                const auto& c = state.cursor;
                state.text = t.substr(0, c) + t.substr(c + 1);
            }
            break;
        }
        case KEY_CONTROL('a'):
        {
            state.cursor = 0;
            break;
        }
        case KEY_CONTROL('k'):
        {
            state.text = state.text.substr(0, state.cursor);
            break;
        }
        case KEY_CONTROL('e'):
        {
            state.cursor = state.text.size();
            break;
        }
        default:
        {
            if (state.focus_component == COMPONENT_LIST)
            {
            }
            else if (std::isalnum(action) || std::ispunct(action) || std::isspace(action))
            {
                state.text = state.text.substr(0, state.cursor)
                + std::string(1, action)
                + state.text.substr(state.cursor);
                state.cursor++;
            }
            break;
        }
    }
    return state;
}




//=============================================================================
int main()
{
    auto screen = Screen();
    auto state = State::load("out.crt");

    while (true)
    {
        screen.render(state);
        int c = screen.get_character();

        if (c == KEY_CONTROL('c') || (c == KEY_CONTROL('d') && state.text.empty()))
        {
            break;
        }
        if (c == 410 || c == -1 || c == KEY_CONTROL('r'))
        {
            screen.reset();
            continue;
        }
        state.message = "last event: " + std::to_string(c);
        state = reduce(state, c);
    }
    state.dump("out.crt");

    return 0;
}
