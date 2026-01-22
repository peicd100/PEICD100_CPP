#include <chrono>
#include <deque>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif

struct Pos { int x, y; };

static bool operator==(const Pos& a, const Pos& b) { return a.x == b.x && a.y == b.y; }

enum class Dir { Up, Down, Left, Right };

static bool is_opposite(Dir a, Dir b) {
    return (a == Dir::Up && b == Dir::Down) ||
        (a == Dir::Down && b == Dir::Up) ||
        (a == Dir::Left && b == Dir::Right) ||
        (a == Dir::Right && b == Dir::Left);
}

#ifdef _WIN32
static void set_cursor_home() {
    // ANSI escape: move cursor to home
    std::cout << "\x1b[H";
}
static void enable_ansi() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}
static bool key_hit() { return _kbhit() != 0; }
static int get_key() { return _getch(); }
#else
static termios orig_termios;
static void disable_raw_mode() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); }
static void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    std::atexit(disable_raw_mode);
    termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}
static bool key_hit() {
    unsigned char c;
    return read(STDIN_FILENO, &c, 1) == 1;
}
static int get_key() {
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1) return c;
    return -1;
}
static void set_cursor_home() { std::cout << "\x1b[H"; }
#endif

static void clear_screen() {
    std::cout << "\x1b[2J\x1b[H";
}

static void draw(int w, int h, const std::deque<Pos>& snake, const Pos& food, int score, bool gameOver) {
    set_cursor_home();

    // top border
    std::cout << '+';
    for (int x = 0; x < w; ++x) std::cout << '-';
    std::cout << "+\n";

    for (int y = 0; y < h; ++y) {
        std::cout << '|';
        for (int x = 0; x < w; ++x) {
            Pos p{ x, y };
            if (p == snake.front()) std::cout << 'O';
            else if (p == food) std::cout << '*';
            else {
                bool body = false;
                for (size_t i = 1; i < snake.size(); ++i) {
                    if (snake[i] == p) { body = true; break; }
                }
                std::cout << (body ? 'o' : ' ');
            }
        }
        std::cout << "|\n";
    }

    // bottom border
    std::cout << '+';
    for (int x = 0; x < w; ++x) std::cout << '-';
    std::cout << "+\n";

    std::cout << "Score: " << score << "   (WASD / Arrow keys)  Quit: Q";
    if (gameOver) std::cout << "   GAME OVER! Press R to restart.";
    std::cout << "\n";
    std::cout.flush();
}

static Pos next_head(Pos head, Dir d) {
    switch (d) {
    case Dir::Up:    return { head.x, head.y - 1 };
    case Dir::Down:  return { head.x, head.y + 1 };
    case Dir::Left:  return { head.x - 1, head.y };
    case Dir::Right: return { head.x + 1, head.y };
    }
    return head;
}

static bool contains(const std::deque<Pos>& snake, const Pos& p) {
    for (const auto& s : snake) if (s == p) return true;
    return false;
}

static Pos random_empty_cell(int w, int h, const std::deque<Pos>& snake) {
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_int_distribution<int> dx(0, w - 1), dy(0, h - 1);

    for (;;) {
        Pos p{ dx(rng), dy(rng) };
        if (!contains(snake, p)) return p;
    }
}

static void reset_game(int w, int h, std::deque<Pos>& snake, Dir& dir, Dir& pending, Pos& food, int& score, bool& gameOver) {
    snake.clear();
    snake.push_front({ w / 2, h / 2 });
    snake.push_back({ w / 2 - 1, h / 2 });
    dir = Dir::Right;
    pending = dir;
    score = 0;
    gameOver = false;
    food = random_empty_cell(w, h, snake);
}

int main() {
#ifdef _WIN32
    enable_ansi();
#else
    enable_raw_mode();
#endif

    const int W = 30;
    const int H = 20;
    const auto tick = std::chrono::milliseconds(120);

    std::deque<Pos> snake;
    Dir dir = Dir::Right;
    Dir pending = dir;
    Pos food{ 0, 0 };
    int score = 0;
    bool gameOver = false;

    reset_game(W, H, snake, dir, pending, food, score, gameOver);

    clear_screen();
    draw(W, H, snake, food, score, gameOver);

    for (;;) {
        // input (non-blocking)
        if (key_hit()) {
            int k = get_key();

#ifdef _WIN32
            // Arrow keys in Windows console: 224 then code
            if (k == 224) {
                int k2 = get_key();
                if (k2 == 72) pending = Dir::Up;
                if (k2 == 80) pending = Dir::Down;
                if (k2 == 75) pending = Dir::Left;
                if (k2 == 77) pending = Dir::Right;
            }
#endif
            // WASD
            if (k == 'w' || k == 'W') pending = Dir::Up;
            if (k == 's' || k == 'S') pending = Dir::Down;
            if (k == 'a' || k == 'A') pending = Dir::Left;
            if (k == 'd' || k == 'D') pending = Dir::Right;

            if (k == 'q' || k == 'Q') break;

            if (gameOver && (k == 'r' || k == 'R')) {
                reset_game(W, H, snake, dir, pending, food, score, gameOver);
                clear_screen();
            }
        }

        // update
        if (!gameOver) {
            if (!is_opposite(dir, pending)) dir = pending;

            Pos nh = next_head(snake.front(), dir);

            // wall collision
            if (nh.x < 0 || nh.x >= W || nh.y < 0 || nh.y >= H) {
                gameOver = true;
            }
            else {
                // move
                snake.push_front(nh);

                bool ate = (nh == food);
                if (ate) {
                    score += 1;
                    food = random_empty_cell(W, H, snake);
                }
                else {
                    snake.pop_back();
                }

                // self collision (check from 2nd cell)
                for (size_t i = 1; i < snake.size(); ++i) {
                    if (snake[i] == nh) { gameOver = true; break; }
                }
            }
        }

        draw(W, H, snake, food, score, gameOver);
        std::this_thread::sleep_for(tick);
    }

    std::cout << "\nBye.\n";
    return 0;
}
