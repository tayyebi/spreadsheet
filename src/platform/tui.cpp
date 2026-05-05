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
#include "app.h"        // App

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
#  include <sys/ioctl.h>    // ioctl, TIOCGWINSZ
#  include <sys/select.h>   // select() — timeout-based stdin probe
#endif

#include <cstdio>           // fwrite, fflush, snprintf, fputs
#include <cstring>          // std::strlen
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

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

    // Screen dimensions (terminal character cells).  Updated on each frame.
    int scr_w_ = 80;
    int scr_h_ = 24;

    // Two flat character grids: current frame and previously displayed frame.
    std::vector<ScreenCell> buf_;
    std::vector<ScreenCell> prev_;
    bool first_frame_ = true;

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

    // Query the actual terminal dimensions.  Returns 80×24 on failure.
    static void queryTermSize(int& w, int& h) {
        struct winsize ws{};
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 &&
            ws.ws_col > 0 && ws.ws_row > 0) {
            w = ws.ws_col;
            h = ws.ws_row;
        } else {
            w = 80; h = 24;
        }
    }

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

        // Enable mouse button events (X10) + SGR extended coordinates.
        // SGR mode (\033[?1006h) handles terminals wider than 223 columns and
        // embeds modifier keys in the button parameter (shift=4, ctrl=16).
        const char* mouseOn = "\033[?1000h\033[?1006h";
        tuiWrite(mouseOn, std::strlen(mouseOn));
        std::fflush(stdout);

        raw_mode_ = true;
    }

    void disableRawMode() {
        if (raw_mode_) {
            // Disable mouse tracking before restoring terminal mode.
            const char* mouseOff = "\033[?1006l\033[?1000l";
            tuiWrite(mouseOff, std::strlen(mouseOff));
            std::fflush(stdout);
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

    // Parse an SGR mouse event that started with ESC [ < …
    // Returns a valid MouseEvent or sets event.button=0 on parse failure.
    MouseEvent parseSgrMouse() {
        // Accumulate digits and semicolons until 'M' (press) or 'm' (release).
        std::string seq;
        for (int i = 0; i < 32; ++i) {
            int b = readByte(100);
            if (b < 0) break;
            seq += (char)b;
            if (b == 'M' || b == 'm') break;
        }
        if (seq.empty()) return {};

        // Parse: Pb;Px;Py{M|m}
        int pb = 0, px = 0, py = 0;
        char terminator = 0;
        // Simple manual parse — no sscanf to stay dependency-free.
        size_t i = 0;
        while (i < seq.size() && seq[i] >= '0' && seq[i] <= '9')
            pb = pb * 10 + (seq[i++] - '0');
        if (i < seq.size() && seq[i] == ';') ++i;
        while (i < seq.size() && seq[i] >= '0' && seq[i] <= '9')
            px = px * 10 + (seq[i++] - '0');
        if (i < seq.size() && seq[i] == ';') ++i;
        while (i < seq.size() && seq[i] >= '0' && seq[i] <= '9')
            py = py * 10 + (seq[i++] - '0');
        if (i < seq.size()) terminator = seq[i];

        // Button encoding: bit0-1 = button (0=left,1=mid,2=right),
        // bit2 = shift, bit3 = meta, bit4 = ctrl, bit5 = motion,
        // bit6 = wheel (64=up, 65=down).
        MouseEvent me{};
        me.x       = px - 1;   // convert from 1-based to 0-based
        me.y       = py - 1;
        me.pressed = (terminator == 'M');
        me.shift   = (pb & 4) != 0;

        int btn = pb & 3;
        if (pb & 64) {
            // Wheel event
            me.button  = (pb & 1) ? 5 : 4;  // 64=up→4, 65=down→5
            me.pressed = true;
        } else {
            me.button = btn + 1;  // 1=left, 2=middle, 3=right
        }
        return me;
    }

    // Parse a legacy X10 mouse event that started with ESC [ M
    MouseEvent parseLegacyMouse() {
        int b  = readByte(100);
        int px = readByte(100);
        int py = readByte(100);
        if (b < 0 || px < 0 || py < 0) return {};

        MouseEvent me{};
        // Legacy encoding: button byte = button_code + 32 + modifiers
        // button_code: 0=left, 1=mid, 2=right, 64=wheel-up, 65=wheel-down
        int pb   = b - 32;
        me.x     = px - 33;  // 33 = 32 (offset) + 1 (1-based)
        me.y     = py - 33;
        me.shift = (pb & 4) != 0;
        if (pb & 64) {
            me.button  = (pb & 1) ? 5 : 4;
            me.pressed = true;
        } else {
            int btn    = pb & 3;
            me.button  = (btn == 3) ? 0 : btn + 1;  // btn==3 means release in X10
            me.pressed = (btn != 3);
        }
        return me;
    }

    // Parse one KeyEvent from the POSIX raw byte stream.
    // Handles ANSI escape sequences, Ctrl+letter, and ASCII printable chars.
    // When a mouse event is detected, fires mcb_ and returns a null KeyEvent.
    // Returns a synthetic Ctrl+Q on EOF or Ctrl+D / Ctrl+Q input.
    KeyEvent posixReadKey(int timeout_ms = -1) {
        int c = readByte(timeout_ms);
        KeyEvent ke{};

        // Timed poll with no input: return a null event.
        if (c < 0 && timeout_ms >= 0) {
            return {};
        }

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
                } else if (c3 == '<') {
                    // SGR mouse event: ESC [ < Pb ; Px ; Py {M|m}
                    MouseEvent me = parseSgrMouse();
                    if (me.button > 0 && mcb_) mcb_(me);
                    return {};  // null KeyEvent — will be skipped by run()
                } else if (c3 == 'M') {
                    // Legacy X10 mouse event: ESC [ M b px py
                    MouseEvent me = parseLegacyMouse();
                    if (me.button > 0 && mcb_) mcb_(me);
                    return {};
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

    static constexpr const char* ANSI_CLEAR_SCREEN = "\033[2J";

    // Write one character + colour into the back-buffer (bounds-checked).
    void bufWrite(int col, int row, char ch, Color fg, Color bg) {
        if (row < 0 || row >= scr_h_ || col < 0 || col >= scr_w_) return;
        buf_[row * scr_w_ + col] = {ch, fg, bg};
    }

    // Resize the back-buffer to match the current terminal dimensions.
    void resizeBuf(int w, int h) {
        scr_w_  = w;
        scr_h_  = h;
        buf_.assign(static_cast<std::size_t>(w * h), ScreenCell{});
        prev_.assign(static_cast<std::size_t>(w * h), ScreenCell{});
        first_frame_ = true;
    }

public:
    // -----------------------------------------------------------------------
    // Constructor / destructor
    // -----------------------------------------------------------------------
    TuiWindow() {
        enableRawMode();

        // Initialise the back-buffer to the current terminal size.
#ifdef _WIN32
        {
            CONSOLE_SCREEN_BUFFER_INFO csbi{};
            if (GetConsoleScreenBufferInfo(hOut_, &csbi)) {
                int w = csbi.srWindow.Right  - csbi.srWindow.Left + 1;
                int h = csbi.srWindow.Bottom - csbi.srWindow.Top  + 1;
                if (w > 0 && h > 0) { scr_w_ = w; scr_h_ = h; }
            }
        }
#else
        queryTermSize(scr_w_, scr_h_);
#endif
        resizeBuf(scr_w_, scr_h_);

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
    // IWindow interface
    // -----------------------------------------------------------------------

    void getTermSize(int& rows, int& cols) const override {
        rows = scr_h_;
        cols = scr_w_;
    }

    // Write text at (x, y), preserving per-cell background already in buf_.
    void drawText(int x, int y, const std::string& t, Color fg) override {
        for (int i = 0; i < (int)t.size(); ++i) {
            int cx = x + i;
            int idx = y * scr_w_ + cx;
            Color bg = (y >= 0 && y < scr_h_ && cx >= 0 && cx < scr_w_)
                       ? buf_[static_cast<std::size_t>(idx)].bg
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
    // Also checks for a terminal resize (handled on the next render cycle).
    void updateDisplay() override {
        std::string out;
        out.reserve(4096);

        // Track current terminal colour to avoid redundant escape sequences.
        Color cur_fg{255, 0, 255};   // deliberately invalid initial value
        Color cur_bg{255, 0, 255};

        for (int r = 0; r < scr_h_; ++r) {
            for (int c = 0; c < scr_w_; ++c) {
                std::size_t idx = static_cast<std::size_t>(r * scr_w_ + c);
                const ScreenCell& cell = buf_[idx];

                // Skip cells that haven't changed since the last frame.
                if (!first_frame_ && cell == prev_[idx]) continue;

                appendMoveTo(out, c, r);

                if (cell.fg.r != cur_fg.r || cell.fg.g != cur_fg.g ||
                    cell.fg.b != cur_fg.b || cell.bg.r != cur_bg.r ||
                    cell.bg.g != cur_bg.g || cell.bg.b != cur_bg.b) {
                    appendColor(out, cell.fg, cell.bg);
                    cur_fg = cell.fg;
                    cur_bg = cell.bg;
                }

                out += cell.ch;
                prev_[idx] = cell;
            }
        }

        // Reset attributes and park cursor below the grid.
        out += "\033[0m";
        appendMoveTo(out, 0, scr_h_);

        if (!out.empty()) {
            tuiWrite(out.data(), out.size());
            std::fflush(stdout);
        }

        first_frame_ = false;

        // Check if the terminal was resized; update buffers for the next frame.
#ifndef _WIN32
        {
            int nw = 80, nh = 24;
            queryTermSize(nw, nh);
            if (nw != scr_w_ || nh != scr_h_) {
                resizeBuf(nw, nh);
                // Clear screen so the next full repaint covers the new size.
                tuiWrite(ANSI_CLEAR_SCREEN, std::strlen(ANSI_CLEAR_SCREEN));
                std::fflush(stdout);
            }
        }
#endif
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
    // Reads one event at a time and fires the registered callback.
    // Mouse events are fired from within posixReadKey / winReadKey.
    // Ctrl+Q or Ctrl+D exits the loop.
    // -----------------------------------------------------------------------
    void run() override {
        for (;;) {
#ifdef _WIN32
            KeyEvent ke = winReadKey();
            // Ctrl+Q or Ctrl+D exits.
            if (ke.ctrl && (ke.ch == 'q' || ke.ch == 'd')) break;
#else
            constexpr int RESIZE_POLL_TIMEOUT_MS = 100;
            KeyEvent ke = posixReadKey(RESIZE_POLL_TIMEOUT_MS);
            // Keep the UI responsive to terminal window resize even when idle.
            int nw, nh;
            queryTermSize(nw, nh);
            if (nw != scr_w_ || nh != scr_h_) {
                resizeBuf(nw, nh);
                tuiWrite(ANSI_CLEAR_SCREEN, std::strlen(ANSI_CLEAR_SCREEN));
                std::fflush(stdout);
                if (kcb_) kcb_(KeyEvent{});
            }
            // posixReadKey() returns ctrl+q as the quit sentinel,
            // or a null event (key==0, ch==0) for mouse events.
            if (ke.ctrl && ke.ch == 'q') break;
            if (ke.key == 0 && ke.ch == 0) continue;  // mouse event — already fired
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
