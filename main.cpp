#include <curses.h>
#include "immer/map.hpp"
#include "crt-expr.hpp"




//=============================================================================
#define KEY_DELETE 127
#define KEY_RETURN 10
#define KEY_CONTROL(c) (c) - 96
#define PAIR_SUCCESS 1
#define PAIR_ERROR 2
using kernel_t = immer::map<std::string, crt::expression>;




//=============================================================================
struct State
{
    int cursor = 0;
    bool success = true;
    std::string text;
    std::string message;
    kernel_t kernel;
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
        getmaxyx(stdscr, lines, cols);

        assert(has_colors());
        start_color();
        init_pair(PAIR_ERROR, COLOR_RED, COLOR_BLACK);
        init_pair(PAIR_SUCCESS, COLOR_GREEN, COLOR_BLACK);

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

        int row = 1;

        for (const auto& item : state.kernel)
        {
            wmove(win, row, 1);
            wprintw(win, "%s", item.first.data());

            wmove(win, row, 20);
            wprintw(win, "%s", item.second.unparse().data());

            ++row;
        }
    }

    WINDOW* consoleView = nullptr;
    WINDOW* messageView = nullptr;
    WINDOW* kernelView  = nullptr;

    State lastState;
};




//=============================================================================
std::tuple<kernel_t, std::string, std::string, bool>
reduce_add_kernel_definition(kernel_t kernel, std::string entry)
{
    try {
        auto e = crt::parse(entry);

        if (e.key().empty())
        {
            throw std::runtime_error("empty assignment!");
        }
        return std::make_tuple(kernel.set(e.key(), e.keyed("")), "inserted " + e.key(), std::string(), true);
    }
    catch (const std::exception& e)
    {
        return std::make_tuple(kernel, e.what(), entry, false);
    }
}




//=============================================================================
State reduce(State state, int action)
{
    switch (action)
    {
        case KEY_LEFT:
            if (state.cursor > 0)
                state.cursor--;
            break;
        case KEY_RIGHT:
            if (state.cursor < state.text.size())
                state.cursor++;
            break;
        case KEY_RETURN:
            std::tie(state.kernel, state.message, state.text, state.success)
            = reduce_add_kernel_definition(state.kernel, state.text);
            state.cursor = state.text.size();
            break;
        case KEY_DELETE:
            if (state.cursor > 0)
            {
                state.text = state.text.substr(0, state.text.size() - 1);
                state.cursor--;
            }
            break;
        case KEY_CONTROL('a'):
            state.cursor = 0;
            break;
        case KEY_CONTROL('k'):
            state.text = state.text.substr(0, state.cursor);
            break;
        case KEY_CONTROL('e'):
            state.cursor = state.text.size();
            break;
        default:
            state.text = state.text.substr(0, state.cursor) + std::string(1, action) + state.text.substr(state.cursor);
            state.cursor++;
            break;
    }
    return state;
}




//=============================================================================
int main()
{
    Screen screen;
    State state;

    while (true)
    {
        screen.render(state);
        int c = screen.get_character();

        if (c == KEY_CONTROL('d'))
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
    return 0;
}
