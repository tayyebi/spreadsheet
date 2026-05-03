// =============================================================================
// tui.cpp  —  Terminal UI platform (POSIX + Windows, no external dependencies)
//
// Uses termios(3) on POSIX or the Windows Console API on Windows for raw-mode
// input, and ANSI/VT100 truecolor escape sequences for output.  The visible
// screen is modelled as a flat character grid; every draw call writes into a
// back-buffer and updateDisplay() flushes only the cells that differ from the
// previous frame to stdout.
//
// Entry point: main() at the bottom creates a TuiWindow and an App, renders
// the initial frame, and then enters the blocking key-input loop.
// =============================================================================

#include "platform.h"   // IWindow, Color, KeyEvent, MouseEvent
#include "app.h"        // App, Spreadsheet (for grid dimensions)

// ---------------------------------------------------------------------------
// Platform-specific includes
// ---------------------------------------------------------------------------
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <io.h>           // _isatty, _fileno
#else
#  include <termios.h>      // termios, tcgetattr, tcsetattr
#  include <unistd.h>       // read(), STDIN_FILENO, STDOUT_FILENO
#  include <sys/select.h>   // select() — timeout-based stdin probe
#endif

#include <cstdio>           // fwrite, fflush, snprintf, fputs
#include <cstring>          // std::strlen
#include <string>
#include <functional>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Screen dimensions  (in character cells)
// ---------------------------------------------------------------------------
static constexpr int SCR_W = App::HW + Spreadsheet::COLS * App::CW;
static constexpr int SCR_H = App::TB + App::FB + App::HH + Spreadsheet::ROWS * App::CH;

// ---------------------------------------------------------------------------
// ScreenCell  —  one character slot in the back-buffer
// ---------------------------------------------------------------------------
struct ScreenCell {
    char  ch = ' ';
    Color fg = {  0,   0,   0};
    Color bg = {255, 255, 255};

    bool operator==(const ScreenCell& o) const noexcept {
        return ch == o.ch
            && fg.r == o.fg.r && fg.g == o.fg.g && fg.b == o.fg.b
            && bg.r == o.bg.r && bg.g == o.bg.g && bg.b == o.bg.b;
    }
};

// ---------------------------------------------------------------------------
// tuiWrite  —  write n bytes to stdout, ignoring I/O errors
// (terminal output failures are unrecoverable and best silently ignored)
// ---------------------------------------------------------------------------
static void tuiWrite(const char* s, std::size_t n) {
    std::fwrite(s, 1, n, stdout);
}

// ---------------------------------------------------------------------------
// TuiWindow  —  concrete IWindow backed by an ANSI terminal
// ---------------------------------------------------------------------------
class TuiWindow : public IWindow {
    bool raw_mode_ = false;

    // Two character grids: current frame and previously displayed frame.
    ScreenCell buf_ [SCR_H][SCR_W];
    ScreenCell prev_[SCR_H][SCR_W];
    bool       first_frame_ = true;

    std::function<void(KeyEvent)>   kcb_;
    std::function<void(MouseEvent)> mcb_;

    // -----------------------------------------------------------------------
    // Platform-specific state and helpers
    // -----------------------------------------------------------------------
#ifdef _WIN32
    HANDLE hIn_          = INVALID_HANDLE_VALUE;
    HANDLE hOut_         = INVALID_HANDLE_VALUE;
    DWORD  savedInMode_  = 0;
    DWORD  savedOutMode_ = 0;

    void enableRawMode() {
        if (!_isatty(_fileno(stdin)))
            throw std::runtime_error(
                "spreadsheet requires a terminal (stdin is not a TTY)");

        hIn_  = GetStdHandle(STD_INPUT_HANDLE);
        hOut_ = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hIn_  == INVALID_HANDLE_VALUE ||
            hOut_ == INVALID_HANDLE_VALUE)
            throw std::runtime_error("GetStdHandle failed");

        if (!GetConsoleMode(hIn_,  &savedInMode_))
            throw std::runtime_error("GetConsoleMode(stdin) failed");
        if (!GetConsoleMode(hOut_, &savedOutMode_))
            throw std::runtime_error("GetConsoleMode(stdout) failed");

        // Disable echo, line buffering, and Ctrl-key processing.
        DWORD inMode = savedInMode_;
        inMode &= ~(DWORD)(ENABLE_ECHO_INPUT |
                            ENABLE_LINE_INPUT |
                            ENABLE_PROCESSED_INPUT);
        SetConsoleMode(hIn_, inMode);

        // Enable ANSI/VT100 escape sequences on the output console.
        DWORD outMode = savedOutMode_;
        outMode |= ENABLE_PROCESSED_OUTPUT |
                   ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut_, outMode);

        raw_mode_ = true;
    }

    void disableRawMode() {
        if (raw_mode_) {
            SetConsoleMode(hIn_,  savedInMode_);
            SetConsoleMode(hOut_, savedOutMode_);
            raw_mode_ = false;
        }
    }

    // Translate a Windows KEY_EVENT_RECORD to a platform-independent KeyEvent.
    // Returns false when the event should be skipped (key-up, unhandled key).
    static bool translateWinKey(const KEY_EVENT_RECORD& wke, KeyEvent& out) {
        if (!wke.bKeyDown) return false;

        out = {};
        out.ctrl  = (wke.dwControlKeyState &
                     (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
        out.shift = (wke.dwControlKeyState & SHIFT_PRESSED) != 0;
        out.alt   = (wke.dwControlKeyState &
                     (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;

        WORD vk = wke.wVirtualKeyCode;
        switch (vk) {
            case VK_UP:     out.key = IWindow::KEY_UP;        return true;
            case VK_DOWN:   out.key = IWindow::KEY_DOWN;      return true;
            case VK_LEFT:   out.key = IWindow::KEY_LEFT;      return true;
            case VK_RIGHT:  out.key = IWindow::KEY_RIGHT;     return true;
            case VK_HOME:   out.key = IWindow::KEY_HOME;      return true;
            case VK_END:    out.key = IWindow::KEY_END;       return true;
            case VK_PRIOR:  out.key = IWindow::KEY_PGUP;      return true;
            case VK_NEXT:   out.key = IWindow::KEY_PGDN;      return true;
            case VK_DELETE: out.key = IWindow::KEY_DELETE;    return true;
            case VK_F2:     out.key = IWindow::KEY_F2;        return true;
            case VK_F5:     out.key = IWindow::KEY_F5;        return true;
            case VK_RETURN: out.key = IWindow::KEY_ENTER;
                            out.ch  = '\r';                    return true;
            case VK_ESCAPE: out.key = IWindow::KEY_ESC;       return true;
            case VK_BACK:   out.key = IWindow::KEY_BACKSPACE; return true;
            case VK_TAB:    out.key = IWindow::KEY_TAB;
                            out.ch  = '\t';                    return true;
            default: break;
        }

        // Ctrl+letter: virtual key is the letter ('A'–'Z').
        if (out.ctrl && vk >= (WORD)'A' && vk <= (WORD)'Z') {
            out.ch  = char(vk - 'A' + 'a');   // normalize to lowercase
            out.key = out.ch;
            return true;
        }

        // Printable ASCII character.
        char ch = wke.uChar.AsciiChar;
        if (ch >= 32 && (unsigned char)ch < 127) {
            out.ch  = ch;
            out.key = ch;
            return true;
        }

        return false;  // skip unhandled keys
    }

    // Read one KeyEvent from the Windows console, blocking until ready.
    KeyEvent winReadKey() {
        INPUT_RECORD rec;
        DWORD        nRead;
        for (;;) {
            if (!ReadConsoleInputA(hIn_, &rec, 1, &nRead) || nRead == 0) {
                // I/O error or EOF — synthesise Ctrl+Q to exit cleanly.
                KeyEvent q{};
                q.ctrl = true; q.ch = 'q'; q.key = 'q';
                return q;
            }
            if (rec.EventType == KEY_EVENT) {
                KeyEvent ke{};
                if (translateWinKey(rec.Event.KeyEvent, ke))
                    return ke;
            }
        }
    }

#else   // POSIX ---------------------------------------------------------------

    struct termios saved_termios_{};

    void enableRawMode() {
        if (!isatty(STDIN_FILENO))
            throw std::runtime_error(
                "spreadsheet requires a terminal (stdin is not a TTY)");
        if (tcgetattr(STDIN_FILENO, &saved_termios_) != 0)
            throw std::runtime_error("tcgetattr failed");

        struct termios raw = saved_termios_;
        // Disable echo, canonical mode, signals, extended processing.
        raw.c_iflag &= ~(unsigned)(ICRNL | IXON);
        raw.c_lflag &= ~(unsigned)(ECHO | ICANON | ISIG | IEXTEN);
        // Read returns after every single byte, no timeout.
        raw.c_cc[VMIN]  = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        raw_mode_ = true;
    }

    void disableRawMode() {
        if (raw_mode_) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios_);
            raw_mode_ = false;
        }
    }

    // Read one raw byte from stdin.
    // If timeout_ms >= 0, wait at most that many milliseconds; return -1 on
    // timeout.  If timeout_ms < 0, block until a byte arrives.
    static int readByte(int timeout_ms = -1) {
        if (timeout_ms >= 0) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);
            struct timeval tv;
            tv.tv_sec  = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) <= 0)
                return -1;
        }
        unsigned char c = 0;
        if (read(STDIN_FILENO, &c, 1) != 1) return -1;
        return (int)c;
    }

    // Parse one KeyEvent from the POSIX raw byte stream.
    // Handles ANSI escape sequences, Ctrl+letter, and ASCII printable chars.
    // Returns a synthetic Ctrl+Q on EOF or Ctrl+D / Ctrl+Q input.
    KeyEvent posixReadKey() {
        int c = readByte();
        KeyEvent ke{};

        // EOF, Ctrl+D (4), or Ctrl+Q (17) → quit sentinel
        if (c < 0 || c == 4 || c == 17) {
            ke.ctrl = true; ke.ch = 'q'; ke.key = 'q';
            return ke;
        }

        if (c == '\033') {
            // Possibly the start of a multi-byte escape sequence.
            // Use a 100 ms timeout: a standalone ESC gets through quickly.
            int c2 = readByte(100);
            if (c2 < 0) {
                // No follow-on bytes — plain Escape keypress.
                ke.key = KEY_ESC;
            } else if (c2 == '[') {
                // CSI sequence: ESC [ ...
                int c3 = readByte(50);
                if (c3 < 0) {
                    ke.key = KEY_ESC;
                } else if (c3 >= '0' && c3 <= '9') {
                    // Numeric parameter, e.g. ESC[3~ or ESC[1;5A
                    int n  = c3 - '0';
                    int c4 = readByte(50);
                    while (c4 >= '0' && c4 <= '9') {
                        n  = n * 10 + (c4 - '0');
                        c4 = readByte(50);
                    }
                    if (c4 == '~') {
                        switch (n) {
                            case  1: ke.key = KEY_HOME;   break;
                            case  3: ke.key = KEY_DELETE; break;
                            case  4: ke.key = KEY_END;    break;
                            case  5: ke.key = KEY_PGUP;   break;
                            case  6: ke.key = KEY_PGDN;   break;
                            // ESC[11~ = Linux console F1, ESC[12~ = F2:
                            // both mapped to KEY_F2 (enter edit mode)
                            case 11: ke.key = KEY_F2;     break;
                            case 12: ke.key = KEY_F2;     break;
                            case 15: ke.key = KEY_F5;     break; // ESC[15~
                            default: break;
                        }
                    } else if (c4 == ';') {
                        // Modifier + key, e.g. ESC[1;5A
                        int mod = readByte(50);
                        int key = readByte(50);
                        if (mod == '5') ke.ctrl  = true;
                        if (mod == '2') ke.shift = true;
                        if (mod == '3') ke.alt   = true;
                        if (mod == '6') { ke.ctrl = true; ke.shift = true; }
                        switch (key) {
                            case 'A': ke.key = KEY_UP;    break;
                            case 'B': ke.key = KEY_DOWN;  break;
                            case 'C': ke.key = KEY_RIGHT; break;
                            case 'D': ke.key = KEY_LEFT;  break;
                            case 'H': ke.key = KEY_HOME;  break;
                            case 'F': ke.key = KEY_END;   break;
                            default:  break;
                        }
                    }
                } else {
                    // Single-letter CSI, e.g. ESC[A
                    switch (c3) {
                        case 'A': ke.key = KEY_UP;    break;
                        case 'B': ke.key = KEY_DOWN;  break;
                        case 'C': ke.key = KEY_RIGHT; break;
                        case 'D': ke.key = KEY_LEFT;  break;
                        case 'H': ke.key = KEY_HOME;  break;
                        case 'F': ke.key = KEY_END;   break;
                        case 'Z': ke.key = KEY_TAB; ke.shift = true;
                                  ke.ch  = '\t'; break; // Shift+Tab (ESC[Z)
                        default:  ke.key = c3;    break;
                    }
                }
            } else if (c2 == 'O') {
                // SS3 sequences: ESC O {key}
                int c3 = readByte(50);
                switch (c3) {
                    // ESC O P = F1 on xterm/VT220 — mapped to edit (F2)
                    case 'P': ke.key = KEY_F2;   break;
                    case 'Q': ke.key = KEY_F2;   break; // ESC O Q = F2
                    case 'T': ke.key = KEY_F5;   break; // ESC O T = F5
                    case 'H': ke.key = KEY_HOME; break;
                    case 'F': ke.key = KEY_END;  break;
                    default:  ke.key = c3;       break;
                }
            } else {
                // Alt+key: ESC followed by an ordinary character.
                ke.alt = true;
                ke.ch  = (char)c2;
                ke.key = c2;
            }
        } else if (c >= 1 && c <= 26) {
            // Ctrl+letter: byte 1–26 maps to Ctrl+A … Ctrl+Z.
            ke.ctrl = true;
            ke.ch   = char(c + 'a' - 1);
            ke.key  = c;
        } else if (c == '\r' || c == '\n') {
            ke.key = KEY_ENTER;
            ke.ch  = '\r';
        } else if (c == KEY_ESC) {
            ke.key = KEY_ESC;
        } else if (c == 127) {
            // DEL byte (0x7F) — sent by the Backspace key on most terminals.
            ke.key = KEY_BACKSPACE;
        } else if (c == KEY_BACKSPACE) {
            ke.key = KEY_BACKSPACE;
        } else if (c == '\t') {
            ke.key = KEY_TAB;
            ke.ch  = '\t';
        } else {
            ke.ch  = (char)c;
            ke.key = c;
        }

        return ke;
    }

#endif  // _WIN32

    // -----------------------------------------------------------------------
    // ANSI output helpers (shared by POSIX and Windows)
    // -----------------------------------------------------------------------
    static void appendMoveTo(std::string& out, int col, int row) {
        char buf[48];
        snprintf(buf, sizeof(buf), "\033[%d;%dH", row + 1, col + 1);
        out += buf;
    }

    static void appendColor(std::string& out, Color fg, Color bg) {
        char buf[56];
        snprintf(buf, sizeof(buf),
                 "\033[38;2;%d;%d;%dm\033[48;2;%d;%d;%dm",
                 fg.r, fg.g, fg.b,
                 bg.r, bg.g, bg.b);
        out += buf;
    }

    // Write one character + colour into the back-buffer (bounds-checked).
    void bufWrite(int col, int row, char ch, Color fg, Color bg) {
        if (row < 0 || row >= SCR_H || col < 0 || col >= SCR_W) return;
        buf_[row][col] = {ch, fg, bg};
    }

public:
    // -----------------------------------------------------------------------
    // Constructor / destructor
    // -----------------------------------------------------------------------
    TuiWindow() {
        enableRawMode();
        // Hide cursor and clear the screen before the first frame.
        const char* setup = "\033[?25l\033[2J";
        tuiWrite(setup, std::strlen(setup));
        std::fflush(stdout);
    }

    ~TuiWindow() {
        // Restore terminal: show cursor, reset attributes, clear and home.
        const char* restore = "\033[?25h\033[0m\033[2J\033[H";
        tuiWrite(restore, std::strlen(restore));
        std::fflush(stdout);
        disableRawMode();
    }

    // -----------------------------------------------------------------------
    // IWindow drawing interface
    // -----------------------------------------------------------------------

    // Write text at (x, y), preserving per-cell background already in buf_.
    void drawText(int x, int y, const std::string& t, Color fg) override {
        for (int i = 0; i < (int)t.size(); ++i) {
            int cx = x + i;
            Color bg = (y >= 0 && y < SCR_H && cx >= 0 && cx < SCR_W)
                       ? buf_[y][cx].bg
                       : Color{255, 255, 255};
            bufWrite(cx, y, t[i], fg, bg);
        }
    }

    // Fill a rectangle with spaces of the given background colour.
    void fillRect(int x, int y, int w, int h, Color bg) override {
        for (int dy = 0; dy < h; ++dy)
            for (int dx = 0; dx < w; ++dx)
                bufWrite(x + dx, y + dy, ' ', {0, 0, 0}, bg);
    }

    // Draw the outline of a rectangle using ASCII border characters.
    void drawRect(int x, int y, int w, int h, Color c) override {
        if (w <= 0 || h <= 0) return;
        constexpr Color bg = {255, 255, 255};
        if (h == 1) {
            bufWrite(x,         y, '[', c, bg);
            bufWrite(x + w - 1, y, ']', c, bg);
            for (int dx = 1; dx < w - 1; ++dx)
                bufWrite(x + dx, y, '-', c, bg);
        } else {
            for (int dx = 1; dx < w - 1; ++dx) {
                bufWrite(x + dx, y,         '-', c, bg);
                bufWrite(x + dx, y + h - 1, '-', c, bg);
            }
            for (int dy = 1; dy < h - 1; ++dy) {
                bufWrite(x,         y + dy, '|', c, bg);
                bufWrite(x + w - 1, y + dy, '|', c, bg);
            }
            bufWrite(x,         y,         '+', c, bg);
            bufWrite(x + w - 1, y,         '+', c, bg);
            bufWrite(x,         y + h - 1, '+', c, bg);
            bufWrite(x + w - 1, y + h - 1, '+', c, bg);
        }
    }

    // Blit the back-buffer to the terminal, writing only changed cells.
    void updateDisplay() override {
        std::string out;
        out.reserve(4096);

        // Track current terminal colour to avoid redundant escape sequences.
        Color cur_fg{255, 0, 255};   // deliberately invalid initial value
        Color cur_bg{255, 0, 255};

        for (int r = 0; r < SCR_H; ++r) {
            for (int c = 0; c < SCR_W; ++c) {
                const ScreenCell& cell = buf_[r][c];

                // Skip cells that haven't changed since the last frame.
                if (!first_frame_ && cell == prev_[r][c]) continue;

                appendMoveTo(out, c, r);

                if (cell.fg.r != cur_fg.r || cell.fg.g != cur_fg.g ||
                    cell.fg.b != cur_fg.b || cell.bg.r != cur_bg.r ||
                    cell.bg.g != cur_bg.g || cell.bg.b != cur_bg.b) {
                    appendColor(out, cell.fg, cell.bg);
                    cur_fg = cell.fg;
                    cur_bg = cell.bg;
                }

                out += cell.ch;
                prev_[r][c] = cell;
            }
        }

        // Reset attributes and park cursor at bottom-left.
        out += "\033[0m";
        appendMoveTo(out, 0, SCR_H);

        if (!out.empty()) {
            tuiWrite(out.data(), out.size());
            std::fflush(stdout);
        }

        first_frame_ = false;
    }

    void handleInput(std::function<void(KeyEvent)> f) override {
        kcb_ = std::move(f);
    }
    void handleMouse(std::function<void(MouseEvent)> f) override {
        mcb_ = std::move(f);
    }

    // -----------------------------------------------------------------------
    // run()  —  the terminal event loop
    //
    // Reads one key event at a time and fires the registered callback.
    // Ctrl+Q or Ctrl+D exits the loop.
    // -----------------------------------------------------------------------
    void run() override {
        for (;;) {
#ifdef _WIN32
            KeyEvent ke = winReadKey();
            // Ctrl+Q or Ctrl+D exits.
            if (ke.ctrl && (ke.ch == 'q' || ke.ch == 'd')) break;
#else
            KeyEvent ke = posixReadKey();
            // posixReadKey() returns ctrl+q as the quit sentinel.
            if (ke.ctrl && ke.ch == 'q') break;
#endif
            if (kcb_) kcb_(ke);
        }
    }
};

// ---------------------------------------------------------------------------
// main()  —  application entry point for TUI
// ---------------------------------------------------------------------------
int main() {
    try {
        TuiWindow win;
        App       app(win);

        app.render();   // paint the initial (empty) grid
        win.run();      // blocks until Ctrl+Q / Ctrl+D
    } catch (const std::exception& e) {
        std::fputs(e.what(), stderr);
        std::fputs("\n",     stderr);
        return 1;
    }
    return 0;
}
