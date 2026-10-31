/*
    Zarqaa UI Engine - Single File Prototype
    Platform: Windows / Win32
    Language: C++17

    Build with MinGW:
        g++ zui_demo.cpp -std=c++17 -O2 -mwindows -lgdi32 -o ZarqaaZUI.exe

    Build with MSVC Developer Command Prompt:
        cl /std:c++17 /EHsc /O2 zui_demo.cpp user32.lib gdi32.lib /link /SUBSYSTEM:WINDOWS
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ZUI {

// ============================================================
// Basic types
// ============================================================

struct Color {
    uint8_t r { 0 };
    uint8_t g { 0 };
    uint8_t b { 0 };
    uint8_t a { 255 };

    constexpr Color() = default;

    constexpr Color(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255)
        : r(red), g(green), b(blue), a(alpha) {}

    COLORREF to_colorref() const
    {
        return RGB(r, g, b);
    }
};

namespace Colors {
    constexpr Color Transparent { 0, 0, 0, 0 };

    constexpr Color DesktopBackground { 13, 18, 30 };
    constexpr Color WindowBackground  { 24, 30, 45 };
    constexpr Color TitleBarBackground { 30, 39, 59 };
    constexpr Color TitleBarText { 240, 244, 255 };

    constexpr Color TextPrimary { 235, 240, 250 };
    constexpr Color TextSecondary { 160, 174, 192 };

    constexpr Color Accent { 42, 95, 255 };
    constexpr Color AccentHover { 74, 143, 255 };
    constexpr Color AccentPressed { 23, 63, 187 };

    constexpr Color Border { 54, 65, 88 };
    constexpr Color ButtonText { 255, 255, 255 };
    constexpr Color Success { 55, 190, 115 };
}

struct Point {
    int x { 0 };
    int y { 0 };
};

struct Size {
    int width { 0 };
    int height { 0 };
};

struct Rect {
    int x { 0 };
    int y { 0 };
    int width { 0 };
    int height { 0 };

    bool contains(Point point) const
    {
        return point.x >= x &&
               point.y >= y &&
               point.x < x + width &&
               point.y < y + height;
    }

    RECT to_win32() const
    {
        return RECT { x, y, x + width, y + height };
    }
};

enum class EventType {
    None,
    MouseMove,
    MouseDown,
    MouseUp,
    WindowResize
};

struct Event {
    EventType type { EventType::None };
    Point mouse_position {};
    Size window_size {};
};

// ============================================================
// Painter: GDI-backed software drawing layer
// ============================================================

class Painter {
public:
    explicit Painter(HDC hdc)
        : m_hdc(hdc)
    {
        SetBkMode(m_hdc, TRANSPARENT);
    }

    void fill_rect(const Rect& rect, Color color)
    {
        HBRUSH brush = CreateSolidBrush(color.to_colorref());
        RECT native_rect = rect.to_win32();
        FillRect(m_hdc, &native_rect, brush);
        DeleteObject(brush);
    }

    void draw_rect(const Rect& rect, Color color, int thickness = 1)
    {
        HPEN pen = CreatePen(PS_SOLID, thickness, color.to_colorref());
        HGDIOBJ old_pen = SelectObject(m_hdc, pen);
        HGDIOBJ old_brush = SelectObject(m_hdc, GetStockObject(HOLLOW_BRUSH));

        Rectangle(m_hdc, rect.x, rect.y, rect.x + rect.width, rect.y + rect.height);

        SelectObject(m_hdc, old_brush);
        SelectObject(m_hdc, old_pen);
        DeleteObject(pen);
    }

    void fill_rounded_rect(const Rect& rect, Color color, int radius)
    {
        HBRUSH brush = CreateSolidBrush(color.to_colorref());
        HGDIOBJ old_brush = SelectObject(m_hdc, brush);
        HGDIOBJ old_pen = SelectObject(m_hdc, GetStockObject(NULL_PEN));

        RoundRect(
            m_hdc,
            rect.x,
            rect.y,
            rect.x + rect.width,
            rect.y + rect.height,
            radius * 2,
            radius * 2
        );

        SelectObject(m_hdc, old_pen);
        SelectObject(m_hdc, old_brush);
        DeleteObject(brush);
    }

    void draw_rounded_rect(const Rect& rect, Color color, int radius, int thickness = 1)
    {
        HPEN pen = CreatePen(PS_SOLID, thickness, color.to_colorref());
        HGDIOBJ old_pen = SelectObject(m_hdc, pen);
        HGDIOBJ old_brush = SelectObject(m_hdc, GetStockObject(HOLLOW_BRUSH));

        RoundRect(
            m_hdc,
            rect.x,
            rect.y,
            rect.x + rect.width,
            rect.y + rect.height,
            radius * 2,
            radius * 2
        );

        SelectObject(m_hdc, old_brush);
        SelectObject(m_hdc, old_pen);
        DeleteObject(pen);
    }

    void draw_text(
        const Rect& rect,
        const std::wstring& text,
        Color color,
        int font_size = 16,
        bool centered = false,
        bool bold = false
    )
    {
        HFONT font = CreateFontW(
            -font_size,
            0,
            0,
            0,
            bold ? FW_BOLD : FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_OUTLINE_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            VARIABLE_PITCH,
            L"Segoe UI"
        );

        HGDIOBJ old_font = SelectObject(m_hdc, font);
        SetTextColor(m_hdc, color.to_colorref());

        RECT native_rect = rect.to_win32();

        UINT flags = DT_SINGLELINE | DT_VCENTER;
        flags |= centered ? DT_CENTER : DT_LEFT;

        DrawTextW(
            m_hdc,
            text.c_str(),
            static_cast<int>(text.size()),
            &native_rect,
            flags
        );

        SelectObject(m_hdc, old_font);
        DeleteObject(font);
    }

private:
    HDC m_hdc { nullptr };
};

// ============================================================
// Widget base class
// ============================================================

class Widget {
public:
    virtual ~Widget() = default;

    virtual void paint(Painter& painter)
    {
        for (auto& child : m_children) {
            if (child->visible()) {
                child->paint(painter);
            }
        }
    }

    virtual bool on_event(const Event& event)
    {
        for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
            Widget* child = it->get();

            if (child->visible() &&
                child->bounds().contains(event.mouse_position) &&
                child->on_event(event)) {
                return true;
            }
        }

        return false;
    }

    void set_bounds(const Rect& rect)
    {
        m_bounds = rect;
    }

    const Rect& bounds() const
    {
        return m_bounds;
    }

    void set_visible(bool visible)
    {
        m_visible = visible;
    }

    bool visible() const
    {
        return m_visible;
    }

    void add_child(std::unique_ptr<Widget> child)
    {
        m_children.push_back(std::move(child));
    }

protected:
    Rect m_bounds {};
    bool m_visible { true };
    std::vector<std::unique_ptr<Widget>> m_children;
};

// ============================================================
// Label widget
// ============================================================

class Label : public Widget {
public:
    explicit Label(const std::wstring& text)
        : m_text(text)
    {
    }

    void set_text(const std::wstring& text)
    {
        m_text = text;
    }

    void set_color(Color color)
    {
        m_color = color;
    }

    void set_font_size(int size)
    {
        m_font_size = size;
    }

    void set_bold(bool bold)
    {
        m_bold = bold;
    }

    void set_centered(bool centered)
    {
        m_centered = centered;
    }

    void paint(Painter& painter) override
    {
        painter.draw_text(
            m_bounds,
            m_text,
            m_color,
            m_font_size,
            m_centered,
            m_bold
        );

        Widget::paint(painter);
    }

private:
    std::wstring m_text;
    Color m_color { Colors::TextPrimary };
    int m_font_size { 16 };
    bool m_bold { false };
    bool m_centered { false };
};

// ============================================================
// Button widget
// ============================================================

class Button : public Widget {
public:
    explicit Button(const std::wstring& text)
        : m_text(text)
    {
    }

    void set_on_click(std::function<void()> callback)
    {
        m_on_click = std::move(callback);
    }

    void set_text(const std::wstring& text)
    {
        m_text = text;
    }

    void paint(Painter& painter) override
    {
        Color background = Colors::Accent;

        if (m_pressed) {
            background = Colors::AccentPressed;
        } else if (m_hovered) {
            background = Colors::AccentHover;
        }

        painter.fill_rounded_rect(m_bounds, background, 7);
        painter.draw_rounded_rect(m_bounds, Colors::Border, 7, 1);

        painter.draw_text(
            m_bounds,
            m_text,
            Colors::ButtonText,
            15,
            true,
            true
        );

        Widget::paint(painter);
    }

    bool on_event(const Event& event) override
    {
        if (event.type == EventType::MouseMove) {
            m_hovered = m_bounds.contains(event.mouse_position);
            return m_hovered;
        }

        if (event.type == EventType::MouseDown &&
            m_bounds.contains(event.mouse_position)) {
            m_pressed = true;
            return true;
        }

        if (event.type == EventType::MouseUp) {
            bool clicked = m_pressed && m_bounds.contains(event.mouse_position);
            m_pressed = false;

            if (clicked && m_on_click) {
                m_on_click();
            }

            return clicked;
        }

        return Widget::on_event(event);
    }

private:
    std::wstring m_text;
    std::function<void()> m_on_click;

    bool m_hovered { false };
    bool m_pressed { false };
};

// ============================================================
// Panel widget
// ============================================================

class Panel : public Widget {
public:
    explicit Panel(Color background)
        : m_background(background)
    {
    }

    void set_border(Color color, int thickness = 1)
    {
        m_border = color;
        m_border_thickness = thickness;
        m_has_border = true;
    }

    void paint(Painter& painter) override
    {
        painter.fill_rounded_rect(m_bounds, m_background, 10);

        if (m_has_border) {
            painter.draw_rounded_rect(
                m_bounds,
                m_border,
                10,
                m_border_thickness
            );
        }

        Widget::paint(painter);
    }

private:
    Color m_background;
    Color m_border { Colors::Border };
    int m_border_thickness { 1 };
    bool m_has_border { false };
};

// ============================================================
// ZUI Window widget
// ============================================================

class Window : public Widget {
public:
    explicit Window(const std::wstring& title)
        : m_title(title)
    {
    }

    void paint(Painter& painter) override
    {
        painter.fill_rounded_rect(m_bounds, Colors::WindowBackground, 12);
        painter.draw_rounded_rect(m_bounds, Colors::Border, 12, 1);

        Rect title_bar {
            m_bounds.x,
            m_bounds.y,
            m_bounds.width,
            m_title_bar_height
        };

        painter.fill_rounded_rect(title_bar, Colors::TitleBarBackground, 12);

        Rect title_text {
            m_bounds.x + 18,
            m_bounds.y,
            m_bounds.width - 70,
            m_title_bar_height
        };

        painter.draw_text(
            title_text,
            m_title,
            Colors::TitleBarText,
            16,
            false,
            true
        );

        Rect close_button {
            m_bounds.x + m_bounds.width - 42,
            m_bounds.y + 10,
            26,
            24
        };

        painter.fill_rounded_rect(close_button, Color(136, 50, 65), 5);
        painter.draw_text(close_button, L"×", Colors::TextPrimary, 18, true, true);

        Widget::paint(painter);
    }

    bool on_event(const Event& event) override
    {
        if (event.type == EventType::MouseDown) {
            Rect close_button {
                m_bounds.x + m_bounds.width - 42,
                m_bounds.y + 10,
                26,
                24
            };

            if (close_button.contains(event.mouse_position)) {
                m_closed = true;
                return true;
            }
        }

        if (m_closed) {
            return true;
        }

        return Widget::on_event(event);
    }

    bool closed() const
    {
        return m_closed;
    }

    int content_y() const
    {
        return m_bounds.y + m_title_bar_height;
    }

private:
    std::wstring m_title;
    int m_title_bar_height { 46 };
    bool m_closed { false };
};

// ============================================================
// Application class
// ============================================================

class Application {
public:
    static Application& instance()
    {
        static Application app;
        return app;
    }

    void initialize(HINSTANCE instance)
    {
        m_instance = instance;
    }

    void set_root_widget(std::unique_ptr<Widget> root)
    {
        m_root = std::move(root);
    }

    void request_repaint()
    {
        if (m_hwnd) {
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
    }

    int run(const wchar_t* title, int width, int height)
    {
        const wchar_t* class_name = L"ZarqaaZUIWindowClass";

        WNDCLASSW wc {};
        wc.lpfnWndProc = Application::window_proc;
        wc.hInstance = m_instance;
        wc.lpszClassName = class_name;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;

        RegisterClassW(&wc);

        DWORD style = WS_OVERLAPPEDWINDOW;
        RECT requested { 0, 0, width, height };
        AdjustWindowRect(&requested, style, FALSE);

        m_hwnd = CreateWindowExW(
            0,
            class_name,
            title,
            style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            requested.right - requested.left,
            requested.bottom - requested.top,
            nullptr,
            nullptr,
            m_instance,
            this
        );

        if (!m_hwnd) {
            return -1;
        }

        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);

        MSG message {};

        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        return static_cast<int>(message.wParam);
    }

private:
    static LRESULT CALLBACK window_proc(
        HWND hwnd,
        UINT message,
        WPARAM wparam,
        LPARAM lparam
    )
    {
        Application* app = reinterpret_cast<Application*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA)
        );

        if (message == WM_NCCREATE) {
            CREATESTRUCTW* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            app = static_cast<Application*>(create->lpCreateParams);

            SetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(app)
            );
        }

        if (app) {
            return app->handle_message(hwnd, message, wparam, lparam);
        }

        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    LRESULT handle_message(
        HWND hwnd,
        UINT message,
        WPARAM wparam,
        LPARAM lparam
    )
    {
        switch (message) {
        case WM_PAINT:
            paint(hwnd);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE:
            dispatch_mouse_event(EventType::MouseMove, lparam);
            return 0;

        case WM_LBUTTONDOWN:
            SetCapture(hwnd);
            dispatch_mouse_event(EventType::MouseDown, lparam);
            return 0;

        case WM_LBUTTONUP:
            ReleaseCapture();
            dispatch_mouse_event(EventType::MouseUp, lparam);
            return 0;

        case WM_SIZE:
            dispatch_resize_event(
                LOWORD(lparam),
                HIWORD(lparam)
            );
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    void paint(HWND hwnd)
    {
        PAINTSTRUCT ps {};
        HDC screen_dc = BeginPaint(hwnd, &ps);

        RECT client_rect {};
        GetClientRect(hwnd, &client_rect);

        int width = client_rect.right - client_rect.left;
        int height = client_rect.bottom - client_rect.top;

        HDC back_dc = CreateCompatibleDC(screen_dc);
        HBITMAP back_buffer = CreateCompatibleBitmap(screen_dc, width, height);
        HGDIOBJ old_bitmap = SelectObject(back_dc, back_buffer);

        Painter painter(back_dc);

        painter.fill_rect(
            Rect { 0, 0, width, height },
            Colors::DesktopBackground
        );

        if (m_root) {
            m_root->paint(painter);
        }

        BitBlt(
            screen_dc,
            0,
            0,
            width,
            height,
            back_dc,
            0,
            0,
            SRCCOPY
        );

        SelectObject(back_dc, old_bitmap);
        DeleteObject(back_buffer);
        DeleteDC(back_dc);

        EndPaint(hwnd, &ps);
    }

    void dispatch_mouse_event(EventType type, LPARAM lparam)
    {
        if (!m_root) {
            return;
        }

        Point position {
            GET_X_LPARAM(lparam),
            GET_Y_LPARAM(lparam)
        };

        Event event {};
        event.type = type;
        event.mouse_position = position;

        m_root->on_event(event);
        request_repaint();
    }

    void dispatch_resize_event(int width, int height)
    {
        if (!m_root) {
            return;
        }

        Event event {};
        event.type = EventType::WindowResize;
        event.window_size = Size { width, height };

        m_root->on_event(event);
        request_repaint();
    }

    HINSTANCE m_instance { nullptr };
    HWND m_hwnd { nullptr };
    std::unique_ptr<Widget> m_root;
};

} // namespace ZUI

// ============================================================
// Zarqaa OS demo application
// ============================================================

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int
)
{
    using namespace ZUI;

    Application& app = Application::instance();
    app.initialize(instance);

    auto desktop = std::make_unique<Panel>(Colors::DesktopBackground);
    desktop->set_bounds(Rect { 0, 0, 1000, 700 });

    auto main_window = std::make_unique<Window>(L"Zarqaa OS — ZUI Engine Demo");
    main_window->set_bounds(Rect { 120, 75, 760, 500 });

    auto title = std::make_unique<Label>(L"Welcome to Zarqaa OS");
    title->set_bounds(Rect { 155, 155, 500, 42 });
    title->set_font_size(28);
    title->set_bold(true);

    auto subtitle = std::make_unique<Label>(
        L"Native graphical interface engine prototype"
    );
    subtitle->set_bounds(Rect { 155, 202, 520, 30 });
    subtitle->set_font_size(16);
    subtitle->set_color(Colors::TextSecondary);

    auto info_panel = std::make_unique<Panel>(Color(31, 40, 59));
    info_panel->set_bounds(Rect { 155, 255, 690, 105 });
    info_panel->set_border(Colors::Border);

    auto info_label = std::make_unique<Label>(
        L"ZUI renders Widgets directly through a Painter layer.\n"
        L"No HTML engine is required for native Zarqaa OS applications."
    );
    info_label->set_bounds(Rect { 177, 272, 630, 74 });
    info_label->set_font_size(15);
    info_label->set_color(Colors::TextPrimary);

    auto status_label = std::make_unique<Label>(L"Status: Ready");
    status_label->set_bounds(Rect { 155, 445, 500, 30 });
    status_label->set_font_size(15);
    status_label->set_color(Colors::Success);

    Label* status_label_ptr = status_label.get();

    auto launch_button = std::make_unique<Button>(L"Open Explorer");
    launch_button->set_bounds(Rect { 155, 390, 170, 40 });
    launch_button->set_on_click([&app, status_label_ptr]() {
        status_label_ptr->set_text(
            L"Status: Explorer launch event received successfully."
        );
        status_label_ptr->set_color(Colors::Success);
        app.request_repaint();
    });

    auto settings_button = std::make_unique<Button>(L"System Settings");
    settings_button->set_bounds(Rect { 340, 390, 180, 40 });
    settings_button->set_on_click([&app, status_label_ptr]() {
        status_label_ptr->set_text(
            L"Status: Settings module is not installed yet."
        );
        status_label_ptr->set_color(Color(255, 195, 80));
        app.request_repaint();
    });

    auto exit_button = std::make_unique<Button>(L"Exit Demo");
    exit_button->set_bounds(Rect { 535, 390, 150, 40 });
    exit_button->set_on_click([&app, status_label_ptr]() {
        status_label_ptr->set_text(
            L"Status: Exit action requested."
        );
        status_label_ptr->set_color(Color(255, 130, 130));
        app.request_repaint();
    });

    main_window->add_child(std::move(title));
    main_window->add_child(std::move(subtitle));
    main_window->add_child(std::move(info_panel));
    main_window->add_child(std::move(info_label));
    main_window->add_child(std::move(launch_button));
    main_window->add_child(std::move(settings_button));
    main_window->add_child(std::move(exit_button));
    main_window->add_child(std::move(status_label));

    desktop->add_child(std::move(main_window));

    app.set_root_widget(std::move(desktop));

    return app.run(L"Zarqaa OS - ZUI Engine", 1000, 700);
}