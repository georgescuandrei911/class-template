#define UNICODE
#define _UNICODE
#define NOMINMAX

#include "Harbor2D.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <GL/gl.h>

#include <chrono>
#include <cstring>
#include <exception>
#include <map>
#include <utility>

namespace harbor
{
using Clock = std::chrono::steady_clock;
namespace
{
void finite(double v)
{
    if (!std::isfinite(v))
        throw std::invalid_argument("non-finite number");
}
void valid(Vec2 v)
{
    finite(v.x);
    finite(v.y);
}
void positive(double v)
{
    finite(v);
    if (v <= 0)
        throw std::invalid_argument("expected positive number");
}
void paintColor(Color c)
{
    glColor4f(c.r, c.g, c.b, c.a);
}
} // namespace
struct App::Impl
{
    struct Control
    {
        HWND hwnd = nullptr;
        Rect area{};
        std::wstring kind;
        HFONT font = nullptr;
        std::function<void()> click;
        std::function<void(bool)> check;
        std::function<void(std::wstring)> edit;
        std::function<void(int)> change;
        std::vector<ObjectId> rows;
        bool objects = false;
        int minimum = 0, maximum = 100;
    };
    App *owner;
    Config config;
    HWND window = nullptr, canvas = nullptr, panel = nullptr;
    HDC dc = nullptr;
    HGLRC gl = nullptr;
    std::map<int, Control> controls;
    std::map<ObjectId, Object> objects;
    ObjectId nextObject = 1, selection = 0;
    int nextControl = 100, width = 1, height = 1, scroll = 0;
    Camera cam;
    Vec2 world{900, 650};
    Color bg{.07f, .085f, .125f, 1}, grid{.17f, .20f, .26f, 1};
    Color border{.85f, .65f, .25f, 1}, highlight{1, 1, .2f, 1};
    bool isPaused = false, gridOn = true, axesOn = true, borderOn = true;
    bool cameraOn = true, pickingOn = true, cull = true, drag = false, dirty = true;
    bool running = false, hasRun = false, closing = false;
    POINT mouse{};
    double scale = 1, maxDelta = .05, limit = 0, spacing = 0;
    double measuredFps = 0, frameMs = 0, elapsed = 0, dt = 0;
    int segments = 24;
    GLuint fontBase = 0;
    bool drawing = false;
    std::function<void()> customDraw;
    std::vector<Vec2> circle;
    std::function<void(double)> update, frame;
    std::function<void(ObjectId)> selectionChanged;
    std::function<void(int, int)> resized;
    std::function<void(int, bool)> key;
    std::exception_ptr error;
    explicit Impl(App *a, Config c) : owner(a), config(std::move(c)) {}
    ~Impl()
    {
        // Do not call user callbacks while application locals are being destroyed.
        if (window)
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        if (canvas)
            SetWindowLongPtrW(canvas, GWLP_USERDATA, 0);
        if (panel)
            SetWindowLongPtrW(panel, GWLP_USERDATA, 0);
        if (gl)
        {
            wglMakeCurrent(dc, gl);
            if (fontBase)
                glDeleteLists(fontBase, 95);
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(gl);
        }
        if (dc && canvas)
            ReleaseDC(canvas, dc);
        if (window)
            DestroyWindow(window);
        for (auto &pair : controls)
            if (pair.second.font)
                DeleteObject(pair.second.font);
    }
    Object &obj(ObjectId id)
    {
        auto it = objects.find(id);
        if (it == objects.end())
            throw std::out_of_range("object ID");
        return it->second;
    }
    const Object &obj(ObjectId id) const
    {
        auto it = objects.find(id);
        if (it == objects.end())
            throw std::out_of_range("object ID");
        return it->second;
    }
    Control &ctrl(int id)
    {
        auto it = controls.find(id);
        if (it == controls.end())
            throw std::out_of_range("control ID");
        return it->second;
    }
    const Control &ctrl(int id) const
    {
        auto it = controls.find(id);
        if (it == controls.end())
            throw std::out_of_range("control ID");
        return it->second;
    }
    void require(const Control &c, const wchar_t *kind) const
    {
        if (c.kind != kind)
            throw std::invalid_argument("wrong control type");
    }
    void stopWithError()
    {
        error = std::current_exception();
        closing = true;
        PostQuitMessage(1);
    }
    static LRESULT CALLBACK MainProc(HWND h, UINT m, WPARAM w, LPARAM l)
    {
        auto self = reinterpret_cast<Impl *>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (m == WM_NCCREATE)
        {
            self = static_cast<Impl *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
            SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self)
            return DefWindowProcW(h, m, w, l);
        try
        {
            return self->mainMessage(h, m, w, l);
        }
        catch (...)
        {
            self->stopWithError();
            return 0;
        }
    }
    static LRESULT CALLBACK CanvasProc(HWND h, UINT m, WPARAM w, LPARAM l)
    {
        auto self = reinterpret_cast<Impl *>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (m == WM_NCCREATE)
        {
            self = static_cast<Impl *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
            SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self)
            return DefWindowProcW(h, m, w, l);
        try
        {
            return self->canvasMessage(h, m, w, l);
        }
        catch (...)
        {
            self->stopWithError();
            return 0;
        }
    }
    static LRESULT CALLBACK PanelProc(HWND h, UINT m, WPARAM w, LPARAM l)
    {
        auto self = reinterpret_cast<Impl *>(GetWindowLongPtrW(h, GWLP_USERDATA));
        if (m == WM_NCCREATE)
        {
            self = static_cast<Impl *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
            SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self)
            return DefWindowProcW(h, m, w, l);
        try
        {
            if (m == WM_COMMAND || m == WM_HSCROLL)
                return self->mainMessage(self->window, m, w, l);
            if (m == WM_VSCROLL)
            {
                SCROLLINFO si{};
                si.cbSize = sizeof(si);
                si.fMask = SIF_ALL;
                GetScrollInfo(h, SB_VERT, &si);
                int n = self->scroll;
                switch (LOWORD(w))
                {
                case SB_LINEUP:
                    n -= 24;
                    break;
                case SB_LINEDOWN:
                    n += 24;
                    break;
                case SB_PAGEUP:
                    n -= static_cast<int>(si.nPage);
                    break;
                case SB_PAGEDOWN:
                    n += static_cast<int>(si.nPage);
                    break;
                case SB_THUMBTRACK:
                    n = si.nTrackPos;
                    break;
                }
                self->setScroll(n);
                return 0;
            }
            if (m == WM_MOUSEWHEEL)
            {
                self->setScroll(self->scroll - GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA * 60);
                return 0;
            }
        }
        catch (...)
        {
            self->stopWithError();
            return 0;
        }
        return DefWindowProcW(h, m, w, l);
    }
    void setScroll(int n)
    {
        int bottom = 0;
        for (auto &pair : controls)
            bottom = std::max(bottom, pair.second.area.y + pair.second.area.height + 12);
        scroll = std::clamp(n, 0, std::max(0, bottom - height));
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0;
        si.nMax = std::max(0, bottom - 1);
        si.nPage = height;
        si.nPos = scroll;
        SetScrollInfo(panel, SB_VERT, &si, TRUE);
        for (auto &pair : controls)
        {
            auto &c = pair.second;
            MoveWindow(c.hwnd, c.area.x, c.area.y - scroll, c.area.width, c.area.height, TRUE);
        }
        InvalidateRect(panel, nullptr, TRUE);
    }
    void layout()
    {
        if (!canvas || !panel)
            return;
        RECT r{};
        GetClientRect(window, &r);
        width = std::max(1, static_cast<int>(r.right) - config.panelWidth);
        height = std::max(1, static_cast<int>(r.bottom));
        MoveWindow(canvas, 0, 0, width, height, TRUE);
        MoveWindow(panel, width, 0, config.panelWidth, height, TRUE);
        setScroll(scroll);
        auto cb = resized;
        if (cb)
            cb(width, height);
    }
    void initialize()
    {
        if (config.width < 320 || config.height < 240 || config.panelWidth < 0 || config.panelWidth > 2000)
            throw std::invalid_argument("invalid window configuration");
        SetProcessDPIAware();
        INITCOMMONCONTROLSEX cc{sizeof(cc), ICC_BAR_CLASSES | ICC_PROGRESS_CLASS};
        if (!InitCommonControlsEx(&cc))
            throw std::runtime_error("InitCommonControlsEx failed");
        HINSTANCE instance = GetModuleHandleW(nullptr);
        auto reg = [&](const wchar_t *name, WNDPROC proc, UINT style, HBRUSH brush)
        {
            WNDCLASSW wc{};
            wc.style = style;
            wc.lpfnWndProc = proc;
            wc.hInstance = instance;
            wc.lpszClassName = name;
            wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
            wc.hbrBackground = brush;
            if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
                throw std::runtime_error("RegisterClass failed");
        };
        reg(L"HarborMainV1", MainProc, 0, GetSysColorBrush(COLOR_BTNFACE));
        reg(L"HarborCanvasV1", CanvasProc, CS_OWNDC, nullptr);
        reg(L"HarborPanelV1", PanelProc, 0, GetSysColorBrush(COLOR_BTNFACE));
        window = CreateWindowExW(WS_EX_CONTROLPARENT, L"HarborMainV1", config.title.c_str(),
                                 WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                                 config.width, config.height, nullptr, nullptr, instance, this);
        if (!window)
            throw std::runtime_error("CreateWindow failed");
        canvas = CreateWindowExW(0, L"HarborCanvasV1", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 1,
                                 1, window, nullptr, instance, this);
        panel = CreateWindowExW(WS_EX_CONTROLPARENT, L"HarborPanelV1", L"",
                                WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 1, 1, window,
                                nullptr, instance, this);
        if (!canvas || !panel)
            throw std::runtime_error("child window creation failed");
        layout();
        dc = GetDC(canvas);
        if (!dc)
            throw std::runtime_error("GetDC failed");
        PIXELFORMATDESCRIPTOR pf{};
        pf.nSize = sizeof(pf);
        pf.nVersion = 1;
        pf.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pf.iPixelType = PFD_TYPE_RGBA;
        pf.cColorBits = 32;
        pf.iLayerType = PFD_MAIN_PLANE;
        int f = ChoosePixelFormat(dc, &pf);
        if (!f || !SetPixelFormat(dc, f, &pf))
            throw std::runtime_error("OpenGL pixel format failed");
        gl = wglCreateContext(dc);
        if (!gl || !wglMakeCurrent(dc, gl))
            throw std::runtime_error("OpenGL context failed");
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        fontBase = glGenLists(95);
        if (!fontBase || !wglUseFontBitmapsW(dc, 32, 95, fontBase))
            throw std::runtime_error("OpenGL text font failed");
        setSegments(segments);
        owner->setVSync(config.vsync);
    }
    void setSegments(int n)
    {
        segments = std::clamp(n, 8, 256);
        circle.resize(segments + 1);
        for (int i = 0; i <= segments; ++i)
        {
            double a = 6.283185307179586 * i / segments;
            circle[i] = {std::cos(a), std::sin(a)};
        }
    }
    LRESULT mainMessage(HWND h, UINT m, WPARAM w, LPARAM l)
    {
        switch (m)
        {
        case WM_SIZE:
            layout();
            return 0;
        case WM_GETMINMAXINFO:
        {
            auto q = reinterpret_cast<MINMAXINFO *>(l);
            q->ptMinTrackSize = {std::max(400, config.panelWidth + 160), 280};
            return 0;
        }
        case WM_CLOSE:
            closing = true;
            if (running)
                PostQuitMessage(0);
            return 0;
        case WM_COMMAND:
        {
            auto it = controls.find(LOWORD(w));
            if (it == controls.end())
                return 0;
            int id = it->first;
            auto kind = it->second.kind;
            int code = HIWORD(w);
            if (kind == L"button" && code == BN_CLICKED)
            {
                auto cb = it->second.click;
                if (cb)
                    cb();
            }
            else if (kind == L"check" && code == BN_CLICKED)
            {
                auto cb = it->second.check;
                bool v = owner->checked(id);
                if (cb)
                    cb(v);
            }
            else if (kind == L"edit" && code == EN_CHANGE)
            {
                auto cb = it->second.edit;
                auto value = owner->text(id);
                if (cb)
                    cb(value);
            }
            else if ((kind == L"list" && code == LBN_SELCHANGE) ||
                     (kind == L"combo" && code == CBN_SELCHANGE))
            {
                int index = owner->selectedIndex(id);
                if (it->second.objects)
                {
                    if (index >= 0 && index < static_cast<int>(it->second.rows.size()))
                    {
                        auto target = it->second.rows[index];
                        if (objects.count(target) && obj(target).selectable)
                            owner->select(target);
                        else
                            syncLists();
                    }
                }
                else
                {
                    auto cb = it->second.change;
                    if (cb)
                        cb(index);
                }
            }
            return 0;
        }
        case WM_HSCROLL:
        {
            for (auto &pair : controls)
                if (pair.second.hwnd == reinterpret_cast<HWND>(l))
                {
                    int v = owner->value(pair.first);
                    auto cb = pair.second.change;
                    if (cb)
                        cb(v);
                    break;
                }
            return 0;
        }
        }
        return DefWindowProcW(h, m, w, l);
    }
    LRESULT canvasMessage(HWND h, UINT m, WPARAM w, LPARAM l)
    {
        switch (m)
        {
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps{};
            BeginPaint(h, &ps);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN:
        {
            SetFocus(h);
            if (pickingOn)
                owner->select(
                    owner->pick(owner->screenToWorld({double(GET_X_LPARAM(l)), double(GET_Y_LPARAM(l))})));
            return 0;
        }
        case WM_RBUTTONDOWN:
            if (cameraOn)
            {
                drag = true;
                mouse = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
                SetFocus(h);
                SetCapture(h);
            }
            return 0;
        case WM_MOUSEMOVE:
            if (drag)
            {
                int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
                cam.center.x -= (x - mouse.x) / cam.zoom;
                cam.center.y -= (y - mouse.y) / cam.zoom;
                mouse = {x, y};
            }
            return 0;
        case WM_RBUTTONUP:
            if (drag)
            {
                drag = false;
                ReleaseCapture();
            }
            return 0;
        case WM_CAPTURECHANGED:
            drag = false;
            return 0;
        case WM_MOUSEWHEEL:
            if (cameraOn)
            {
                POINT q{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
                ScreenToClient(h, &q);
                if (q.x < 0 || q.y < 0 || q.x >= width || q.y >= height)
                    return 0;
                cam.zoomAt({double(q.x), double(q.y)}, std::pow(1.2, GET_WHEEL_DELTA_WPARAM(w) / 120.0),
                           width, height);
            }
            return 0;
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            auto cb = key;
            if (cb)
                cb(static_cast<int>(w), m == WM_KEYDOWN);
            return 0;
        }
        }
        return DefWindowProcW(h, m, w, l);
    }
    int add(std::wstring kind, const wchar_t *cls, std::wstring text, Rect r, DWORD style)
    {
        if (r.width <= 0 || r.height <= 0)
            throw std::invalid_argument("control size");
        if (nextControl >= 65000)
            throw std::runtime_error("control ID space exhausted");
        int id = nextControl++;
        HWND h = CreateWindowExW(0, cls, text.c_str(), WS_CHILD | WS_VISIBLE | style, r.x, r.y - scroll,
                                 r.width, r.height, panel, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                 GetModuleHandleW(nullptr), nullptr);
        if (!h)
            throw std::runtime_error("control creation failed");
        Control c;
        c.hwnd = h;
        c.area = r;
        c.kind = std::move(kind);
        controls.emplace(id, std::move(c));
        SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        setScroll(scroll);
        return id;
    }
    void syncLists()
    {
        for (auto &pair : controls)
        {
            auto &c = pair.second;
            if (!c.objects)
                continue;
            int top = static_cast<int>(SendMessageW(c.hwnd, LB_GETTOPINDEX, 0, 0));
            if (dirty)
            {
                SendMessageW(c.hwnd, WM_SETREDRAW, FALSE, 0);
                SendMessageW(c.hwnd, LB_RESETCONTENT, 0, 0);
                c.rows.clear();
                for (auto &entry : objects)
                {
                    std::wstring line = L"#" + std::to_wstring(entry.first) + L"  " + entry.second.name;
                    LRESULT result =
                        SendMessageW(c.hwnd, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
                    if (result == LB_ERR || result == LB_ERRSPACE)
                        throw std::runtime_error("object list allocation failed");
                    c.rows.push_back(entry.first);
                }
            }
            auto found = std::find(c.rows.begin(), c.rows.end(), selection);
            int index = found == c.rows.end() ? -1 : static_cast<int>(found - c.rows.begin());
            SendMessageW(c.hwnd, LB_SETCURSEL, index, 0);
            if (dirty)
            {
                if (top >= 0)
                    SendMessageW(c.hwnd, LB_SETTOPINDEX, top, 0);
                SendMessageW(c.hwnd, WM_SETREDRAW, TRUE, 0);
                InvalidateRect(c.hwnd, nullptr, TRUE);
            }
        }
        dirty = false;
    }
    void render()
    {
        auto lo = cam.screenToWorld({0, 0}, width, height),
             hi = cam.screenToWorld({double(width), double(height)}, width, height);
        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(lo.x, hi.x, hi.y, lo.y, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glClearColor(bg.r, bg.g, bg.b, bg.a);
        glClear(GL_COLOR_BUFFER_BIT);
        if (gridOn)
        {
            double step = spacing;
            if (step == 0)
            {
                double target = 80 / cam.zoom, power = std::pow(10.0, std::floor(std::log10(target))),
                       r = target / power;
                step = (r <= 2 ? 2 : r <= 5 ? 5 : 10) * power;
            }
            // Avoid pathological amounts of geometry for explicitly tiny spacing.
            if ((hi.x - lo.x) / step + (hi.y - lo.y) / step < 20000)
            {
                paintColor(grid);
                glBegin(GL_LINES);
                double firstX = std::ceil(lo.x / step) * step;
                int nx = static_cast<int>(std::min(20000.0, std::ceil((hi.x - lo.x) / step) + 2));
                for (int i = 0; i < nx; ++i)
                {
                    double x = firstX + i * step;
                    if (x > hi.x)
                        break;
                    glVertex2d(x, lo.y);
                    glVertex2d(x, hi.y);
                }
                double firstY = std::ceil(lo.y / step) * step;
                int ny = static_cast<int>(std::min(20000.0, std::ceil((hi.y - lo.y) / step) + 2));
                for (int i = 0; i < ny; ++i)
                {
                    double y = firstY + i * step;
                    if (y > hi.y)
                        break;
                    glVertex2d(lo.x, y);
                    glVertex2d(hi.x, y);
                }
                glEnd();
            }
        }
        if (axesOn)
        {
            glBegin(GL_LINES);
            glColor3f(.65f, .25f, .25f);
            glVertex2d(lo.x, 0);
            glVertex2d(hi.x, 0);
            glColor3f(.25f, .55f, .35f);
            glVertex2d(0, lo.y);
            glVertex2d(0, hi.y);
            glEnd();
        }
        if (borderOn)
        {
            paintColor(border);
            glBegin(GL_LINE_LOOP);
            glVertex2d(0, 0);
            glVertex2d(world.x, 0);
            glVertex2d(world.x, world.y);
            glVertex2d(0, world.y);
            glEnd();
        }
        glBegin(GL_TRIANGLES);
        for (auto &entry : objects)
        {
            const auto &o = entry.second;
            if (!o.visible)
                continue;
            if (cull && (o.position.x + o.radius < lo.x || o.position.x - o.radius > hi.x ||
                         o.position.y + o.radius < lo.y || o.position.y - o.radius > hi.y))
                continue;
            paintColor(o.color);
            for (int i = 0; i < segments; ++i)
            {
                glVertex2d(o.position.x, o.position.y);
                glVertex2d(o.position.x + circle[i].x * o.radius, o.position.y + circle[i].y * o.radius);
                glVertex2d(o.position.x + circle[i + 1].x * o.radius,
                           o.position.y + circle[i + 1].y * o.radius);
            }
        }
        glEnd();
        auto it = objects.find(selection);
        if (it != objects.end() && it->second.visible)
        {
            auto &o = it->second;
            paintColor(highlight);
            double r = o.radius + 3 / cam.zoom;
            glBegin(GL_LINE_LOOP);
            for (auto v : circle)
                glVertex2d(o.position.x + v.x * r, o.position.y + v.y * r);
            glEnd();
        }
        drawing = true;
        try
        {
            auto cb = customDraw;
            if (cb)
                cb();
        }
        catch (...)
        {
            drawing = false;
            throw;
        }
        drawing = false;
        if (!SwapBuffers(dc))
            throw std::runtime_error("SwapBuffers failed");
    }
};

App::App(Config c) : p(std::make_unique<Impl>(this, std::move(c)))
{
    p->initialize();
}
App::~App() = default;
int App::run()
{
    if (p->hasRun)
        throw std::logic_error("run can only be called once");
    p->hasRun = true;
    if (p->closing)
        return 0;
    p->running = true;
    ShowWindow(p->window, p->config.maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
    p->layout();
    auto start = Clock::now(), previous = start, stats = start;
    std::uint64_t frames = 0;
    MSG msg{};
    try
    {
        while (!p->closing)
        {
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    p->closing = true;
                    break;
                }
                if (!IsDialogMessageW(p->window, &msg))
                {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
            }
            if (p->error)
                std::rethrow_exception(p->error);
            if (p->closing)
                break;
            if (IsIconic(p->window))
            {
                WaitMessage();
                previous = Clock::now();
                stats = previous;
                frames = 0;
                continue;
            }
            auto begin = Clock::now();
            p->dt = std::min(p->maxDelta, std::chrono::duration<double>(begin - previous).count());
            previous = begin;
            p->elapsed = std::chrono::duration<double>(begin - start).count();
            auto frame = p->frame;
            if (frame)
                frame(p->dt);
            if (!p->isPaused)
            {
                auto cb = p->update;
                if (cb)
                    cb(p->dt * p->scale);
            }
            if (p->closing)
                break;
            if (p->dirty)
                p->syncLists();
            p->render();
            if (p->limit > 0)
            {
                double remaining =
                    1.0 / p->limit - std::chrono::duration<double>(Clock::now() - begin).count();
                if (remaining > 0)
                {
                    // Wake early for input rather than freezing the native controls.
                    DWORD waitMs = static_cast<DWORD>(std::ceil(remaining * 1000.0));
                    MsgWaitForMultipleObjectsEx(0, nullptr, waitMs, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                }
            }
            ++frames;
            auto end = Clock::now();
            double interval = std::chrono::duration<double>(end - stats).count();
            if (interval >= .5)
            {
                p->measuredFps = frames / interval;
                p->frameMs = interval * 1000 / frames;
                stats = end;
                frames = 0;
            }
        }
    }
    catch (...)
    {
        p->running = false;
        throw;
    }
    p->running = false;
    return 0;
}
void App::close()
{
    p->closing = true;
}
void App::setTitle(const std::wstring &t)
{
    p->config.title = t;
    SetWindowTextW(p->window, t.c_str());
}
void App::setMaximized(bool v)
{
    p->config.maximized = v;
    if (p->running)
        ShowWindow(p->window, v ? SW_MAXIMIZE : SW_RESTORE);
}
void App::setVSync(bool v)
{
    p->config.vsync = v;
    using Fn = BOOL(WINAPI *)(int);
    auto address = wglGetProcAddress("wglSwapIntervalEXT");
    if (address && address != reinterpret_cast<PROC>(1) && address != reinterpret_cast<PROC>(2) &&
        address != reinterpret_cast<PROC>(3) && address != reinterpret_cast<PROC>(-1))
    {
        Fn fn = nullptr;
        static_assert(sizeof(fn) == sizeof(address));
        std::memcpy(&fn, &address, sizeof(fn));
        fn(v ? 1 : 0);
    }
}
void App::setFpsLimit(double v)
{
    finite(v);
    if (v < 0 || v > 10000 || (v > 0 && v < 1))
        throw std::invalid_argument("FPS limit must be 0 or between 1 and 10000");
    p->limit = v;
}
double App::fps() const
{
    return p->measuredFps;
}
double App::frameMilliseconds() const
{
    return p->frameMs;
}
double App::elapsedSeconds() const
{
    return p->elapsed;
}
double App::deltaSeconds() const
{
    return p->dt;
}
void App::setMaxDelta(double v)
{
    positive(v);
    p->maxDelta = v;
}
void App::setPaused(bool v)
{
    p->isPaused = v;
}
bool App::paused() const
{
    return p->isPaused;
}
void App::togglePaused()
{
    p->isPaused = !p->isPaused;
}
void App::setTimeScale(double v)
{
    finite(v);
    if (v < 0)
        throw std::invalid_argument("time scale");
    p->scale = v;
}
double App::timeScale() const
{
    return p->scale;
}
void App::onUpdate(std::function<void(double)> f)
{
    p->update = std::move(f);
}
void App::onFrame(std::function<void(double)> f)
{
    p->frame = std::move(f);
}
void App::onDraw(std::function<void()> f)
{
    p->customDraw = std::move(f);
}
void App::drawLine(Vec2 a, Vec2 b, Color c)
{
    if (!p->drawing)
        throw std::logic_error("draw only inside onDraw");
    valid(a);
    valid(b);
    paintColor(c);
    glBegin(GL_LINES);
    glVertex2d(a.x, a.y);
    glVertex2d(b.x, b.y);
    glEnd();
}
void App::drawRectangle(Vec2 a, Vec2 size, Color c, bool filled)
{
    if (!p->drawing)
        throw std::logic_error("draw only inside onDraw");
    valid(a);
    valid(size);
    paintColor(c);
    glBegin(filled ? GL_QUADS : GL_LINE_LOOP);
    glVertex2d(a.x, a.y);
    glVertex2d(a.x + size.x, a.y);
    glVertex2d(a.x + size.x, a.y + size.y);
    glVertex2d(a.x, a.y + size.y);
    glEnd();
}
void App::drawCircle(Vec2 a, double r, Color c)
{
    if (!p->drawing)
        throw std::logic_error("draw only inside onDraw");
    valid(a);
    positive(r);
    paintColor(c);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2d(a.x, a.y);
    for (auto v : p->circle)
        glVertex2d(a.x + v.x * r, a.y + v.y * r);
    glEnd();
}
void App::drawText(Vec2 a, const std::wstring &t, Color c)
{
    if (!p->drawing)
        throw std::logic_error("draw only inside onDraw");
    valid(a);
    paintColor(c);
    glRasterPos2d(a.x, a.y);
    for (wchar_t ch : t)
    {
        unsigned index =
            (ch >= 32 && ch <= 126) ? static_cast<unsigned>(ch - 32) : static_cast<unsigned>('?' - 32);
        glCallList(p->fontBase + index);
    }
}
void App::drawScreenText(Vec2 a, const std::wstring &t, Color c)
{
    if (!p->drawing)
        throw std::logic_error("draw only inside onDraw");
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, p->width, p->height, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    drawText(a, t, c);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}
void App::onSelect(std::function<void(ObjectId)> f)
{
    p->selectionChanged = std::move(f);
}
void App::onResize(std::function<void(int, int)> f)
{
    p->resized = std::move(f);
}
void App::onKey(std::function<void(int, bool)> f)
{
    p->key = std::move(f);
}
bool App::keyDown(int k) const
{
    return GetForegroundWindow() == p->window && (GetAsyncKeyState(k) & 0x8000) != 0;
}

ObjectId App::addCircle(std::wstring name, Vec2 position, double radius, Color color)
{
    valid(position);
    positive(radius);
    Object o;
    o.id = p->nextObject++;
    o.name = std::move(name);
    o.position = position;
    o.radius = radius;
    o.color = color;
    p->objects.emplace(o.id, o);
    p->dirty = true;
    return o.id;
}
bool App::removeObject(ObjectId id)
{
    auto n = p->objects.erase(id);
    if (!n)
        return false;
    p->dirty = true;
    if (p->selection == id)
        clearSelection();
    return true;
}
void App::clearObjects()
{
    p->objects.clear();
    p->dirty = true;
    clearSelection();
}
bool App::hasObject(ObjectId id) const
{
    return p->objects.count(id) != 0;
}
std::size_t App::objectCount() const
{
    return p->objects.size();
}
std::vector<ObjectId> App::objectIds() const
{
    std::vector<ObjectId> r;
    r.reserve(p->objects.size());
    for (auto &entry : p->objects)
        r.push_back(entry.first);
    return r;
}
Object App::object(ObjectId id) const
{
    return p->obj(id);
}
void App::setObjectName(ObjectId id, const std::wstring &n)
{
    p->obj(id).name = n;
    p->dirty = true;
}
std::wstring App::objectName(ObjectId id) const
{
    return p->obj(id).name;
}
void App::setPosition(ObjectId id, Vec2 v)
{
    valid(v);
    p->obj(id).position = v;
}
Vec2 App::position(ObjectId id) const
{
    return p->obj(id).position;
}
void App::moveObject(ObjectId id, Vec2 v)
{
    valid(v);
    auto &o = p->obj(id);
    setPosition(id, {o.position.x + v.x, o.position.y + v.y});
}
void App::setVelocity(ObjectId id, Vec2 v)
{
    valid(v);
    p->obj(id).velocity = v;
}
Vec2 App::velocity(ObjectId id) const
{
    return p->obj(id).velocity;
}
void App::setRadius(ObjectId id, double v)
{
    positive(v);
    p->obj(id).radius = v;
}
double App::radius(ObjectId id) const
{
    return p->obj(id).radius;
}
void App::setColor(ObjectId id, Color v)
{
    p->obj(id).color = v;
}
Color App::color(ObjectId id) const
{
    return p->obj(id).color;
}
void App::setVisible(ObjectId id, bool v)
{
    p->obj(id).visible = v;
}
bool App::visible(ObjectId id) const
{
    return p->obj(id).visible;
}
void App::setSelectable(ObjectId id, bool v)
{
    p->obj(id).selectable = v;
    if (!v && p->selection == id)
        clearSelection();
}
bool App::selectable(ObjectId id) const
{
    return p->obj(id).selectable;
}
void App::setUserValue(ObjectId id, double v)
{
    finite(v);
    p->obj(id).userValue = v;
}
double App::userValue(ObjectId id) const
{
    return p->obj(id).userValue;
}
void App::integrate(double dt)
{
    finite(dt);
    for (auto &entry : p->objects)
    {
        auto &o = entry.second;
        o.position.x += o.velocity.x * dt;
        o.position.y += o.velocity.y * dt;
    }
}
void App::bounceInWorld(double dt)
{
    finite(dt);
    for (auto &entry : p->objects)
    {
        auto &o = entry.second;
        bounceAxis(o.position.x, o.velocity.x, o.radius, p->world.x - o.radius, dt);
        bounceAxis(o.position.y, o.velocity.y, o.radius, p->world.y - o.radius, dt);
    }
}
void App::setWorldSize(double w, double h)
{
    positive(w);
    positive(h);
    p->world = {w, h};
}
Vec2 App::worldSize() const
{
    return p->world;
}
void App::setWorldBorderVisible(bool v)
{
    p->borderOn = v;
}
bool App::worldBorderVisible() const
{
    return p->borderOn;
}
void App::setWorldBorderColor(Color v)
{
    p->border = v;
}
void App::setBackground(Color v)
{
    p->bg = v;
}
Color App::background() const
{
    return p->bg;
}
void App::setCircleSegments(int n)
{
    p->setSegments(n);
}
int App::circleSegments() const
{
    return p->segments;
}
void App::setCulling(bool v)
{
    p->cull = v;
}
bool App::culling() const
{
    return p->cull;
}
void App::select(ObjectId id)
{
    if (id && !p->obj(id).selectable)
        throw std::invalid_argument("unselectable object");
    if (p->selection == id)
        return;
    p->selection = id;
    p->syncLists();
    auto cb = p->selectionChanged;
    if (cb)
        cb(id);
}
void App::clearSelection()
{
    select(NoObject);
}
ObjectId App::selected() const
{
    return p->selection;
}
ObjectId App::pick(Vec2 point) const
{
    valid(point);
    for (auto it = p->objects.rbegin(); it != p->objects.rend(); ++it)
    {
        auto &o = it->second;
        if (o.visible && o.selectable && contains(o, point))
            return o.id;
    }
    return NoObject;
}
void App::focusObject(ObjectId id)
{
    p->cam.center = p->obj(id).position;
}
void App::setSelectionColor(Color v)
{
    p->highlight = v;
}
void App::setCamera(Vec2 v)
{
    valid(v);
    p->cam.center = v;
}
Vec2 App::camera() const
{
    return p->cam.center;
}
void App::pan(Vec2 v)
{
    valid(v);
    setCamera({p->cam.center.x + v.x, p->cam.center.y + v.y});
}
void App::setZoom(double v)
{
    positive(v);
    p->cam.zoom = std::clamp(v, .002, 10000.0);
}
double App::zoom() const
{
    return p->cam.zoom;
}
void App::zoomAt(Vec2 v, double f)
{
    valid(v);
    p->cam.zoomAt(v, f, p->width, p->height);
}
Vec2 App::screenToWorld(Vec2 v) const
{
    return p->cam.screenToWorld(v, p->width, p->height);
}
Vec2 App::worldToScreen(Vec2 v) const
{
    return p->cam.worldToScreen(v, p->width, p->height);
}
void App::fitWorld(double padding)
{
    positive(padding);
    setCamera({p->world.x * .5, p->world.y * .5});
    setZoom(padding * std::min(p->width / p->world.x, p->height / p->world.y));
}
void App::setCameraEnabled(bool v)
{
    p->cameraOn = v;
    if (!v && p->drag)
    {
        p->drag = false;
        ReleaseCapture();
    }
}
bool App::cameraEnabled() const
{
    return p->cameraOn;
}
void App::setPickingEnabled(bool v)
{
    p->pickingOn = v;
}
bool App::pickingEnabled() const
{
    return p->pickingOn;
}
int App::canvasWidth() const
{
    return p->width;
}
int App::canvasHeight() const
{
    return p->height;
}
void App::setPanelWidth(int n)
{
    if (n < 0 || n > 2000)
        throw std::invalid_argument("panel width");
    p->config.panelWidth = n;
    p->layout();
}
int App::panelWidth() const
{
    return p->config.panelWidth;
}
void App::setGridVisible(bool v)
{
    p->gridOn = v;
}
bool App::gridVisible() const
{
    return p->gridOn;
}
void App::setGridColor(Color v)
{
    p->grid = v;
}
void App::setGridSpacing(double v)
{
    finite(v);
    if (v < 0)
        throw std::invalid_argument("grid spacing");
    p->spacing = v;
}
double App::gridSpacing() const
{
    return p->spacing;
}
void App::setAxesVisible(bool v)
{
    p->axesOn = v;
}
bool App::axesVisible() const
{
    return p->axesOn;
}

ControlId App::addButton(std::wstring t, Rect r, std::function<void()> f)
{
    int id = p->add(L"button", L"BUTTON", std::move(t), r, BS_PUSHBUTTON | WS_TABSTOP);
    p->ctrl(id).click = std::move(f);
    return id;
}
ControlId App::addLabel(std::wstring t, Rect r)
{
    return p->add(L"label", L"STATIC", std::move(t), r, SS_LEFT);
}
ControlId App::addCheckbox(std::wstring t, Rect r, bool v, std::function<void(bool)> f)
{
    int id = p->add(L"check", L"BUTTON", std::move(t), r, BS_AUTOCHECKBOX | WS_TABSTOP);
    p->ctrl(id).check = std::move(f);
    setChecked(id, v);
    return id;
}
ControlId App::addEdit(std::wstring t, Rect r, std::function<void(std::wstring)> f)
{
    int id = p->add(L"edit", L"EDIT", std::move(t), r, WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL);
    p->ctrl(id).edit = std::move(f);
    return id;
}
ControlId App::addSlider(Rect r, int lo, int hi, int v, std::function<void(int)> f)
{
    if (lo > hi)
        throw std::invalid_argument("range");
    int id = p->add(L"slider", TRACKBAR_CLASSW, L"", r, TBS_HORZ | WS_TABSTOP);
    p->ctrl(id).change = std::move(f);
    setRange(id, lo, hi);
    setValue(id, v);
    return id;
}
ControlId App::addListBox(Rect r, std::function<void(int)> f)
{
    int id = p->add(L"list", L"LISTBOX", L"", r,
                    WS_BORDER | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT);
    p->ctrl(id).change = std::move(f);
    return id;
}
ControlId App::addObjectList(Rect r)
{
    int id = addListBox(r);
    p->ctrl(id).objects = true;
    p->dirty = true;
    return id;
}
ControlId App::addProgressBar(Rect r, int lo, int hi)
{
    if (lo > hi)
        throw std::invalid_argument("range");
    int id = p->add(L"progress", PROGRESS_CLASSW, L"", r, 0);
    setRange(id, lo, hi);
    return id;
}
ControlId App::addComboBox(Rect r, std::vector<std::wstring> items, std::function<void(int)> f)
{
    int id = p->add(L"combo", L"COMBOBOX", L"", r, CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP);
    p->ctrl(id).change = std::move(f);
    setItems(id, items);
    return id;
}
void App::removeControl(ControlId id)
{
    auto &c = p->ctrl(id);
    HWND h = c.hwnd;
    HFONT f = c.font;
    p->controls.erase(id);
    DestroyWindow(h);
    if (f)
        DeleteObject(f);
    p->setScroll(p->scroll);
}
void App::clearControls()
{
    while (!p->controls.empty())
        removeControl(p->controls.begin()->first);
}
void App::setText(ControlId id, const std::wstring &t)
{
    SetWindowTextW(p->ctrl(id).hwnd, t.c_str());
}
std::wstring App::text(ControlId id) const
{
    HWND h = p->ctrl(id).hwnd;
    int n = GetWindowTextLengthW(h);
    std::wstring t(n + 1, L'\0');
    int read = GetWindowTextW(h, t.data(), n + 1);
    t.resize(read);
    return t;
}
void App::setRect(ControlId id, Rect r)
{
    if (r.width <= 0 || r.height <= 0)
        throw std::invalid_argument("control size");
    p->ctrl(id).area = r;
    p->setScroll(p->scroll);
}
Rect App::rect(ControlId id) const
{
    return p->ctrl(id).area;
}
void App::setControlVisible(ControlId id, bool v)
{
    ShowWindow(p->ctrl(id).hwnd, v ? SW_SHOW : SW_HIDE);
}
bool App::controlVisible(ControlId id) const
{
    return (GetWindowLongPtrW(p->ctrl(id).hwnd, GWL_STYLE) & WS_VISIBLE) != 0;
}
void App::setEnabled(ControlId id, bool v)
{
    EnableWindow(p->ctrl(id).hwnd, v);
}
bool App::enabled(ControlId id) const
{
    return IsWindowEnabled(p->ctrl(id).hwnd) != FALSE;
}
void App::setChecked(ControlId id, bool v)
{
    auto &c = p->ctrl(id);
    p->require(c, L"check");
    SendMessageW(c.hwnd, BM_SETCHECK, v ? BST_CHECKED : BST_UNCHECKED, 0);
}
bool App::checked(ControlId id) const
{
    auto &c = p->ctrl(id);
    p->require(c, L"check");
    return SendMessageW(c.hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED;
}
void App::setValue(ControlId id, int v)
{
    auto &c = p->ctrl(id);
    if (c.kind != L"slider" && c.kind != L"progress")
        throw std::invalid_argument("expected slider/progress");
    v = std::clamp(v, c.minimum, c.maximum);
    SendMessageW(c.hwnd, c.kind == L"slider" ? TBM_SETPOS : PBM_SETPOS, c.kind == L"slider" ? TRUE : v,
                 c.kind == L"slider" ? v : 0);
}
int App::value(ControlId id) const
{
    auto &c = p->ctrl(id);
    if (c.kind != L"slider" && c.kind != L"progress")
        throw std::invalid_argument("expected slider/progress");
    return static_cast<int>(SendMessageW(c.hwnd, c.kind == L"slider" ? TBM_GETPOS : PBM_GETPOS, 0, 0));
}
void App::setRange(ControlId id, int lo, int hi)
{
    if (lo > hi)
        throw std::invalid_argument("range");
    auto &c = p->ctrl(id);
    if (c.kind != L"slider" && c.kind != L"progress")
        throw std::invalid_argument("expected slider/progress");
    c.minimum = lo;
    c.maximum = hi;
    if (c.kind == L"slider")
    {
        SendMessageW(c.hwnd, TBM_SETRANGEMIN, FALSE, lo);
        SendMessageW(c.hwnd, TBM_SETRANGEMAX, TRUE, hi);
    }
    else
        SendMessageW(c.hwnd, PBM_SETRANGE32, lo, hi);
}
void App::setItems(ControlId id, const std::vector<std::wstring> &items)
{
    clearItems(id);
    for (auto &s : items)
        addItem(id, s);
}
void App::addItem(ControlId id, const std::wstring &s)
{
    auto &c = p->ctrl(id);
    if (c.objects)
        throw std::invalid_argument("object list is managed automatically");
    if (c.kind != L"list" && c.kind != L"combo")
        throw std::invalid_argument("expected list/combo");
    auto result = SendMessageW(c.hwnd, c.kind == L"list" ? LB_ADDSTRING : CB_ADDSTRING, 0,
                               reinterpret_cast<LPARAM>(s.c_str()));
    if (result < 0)
        throw std::runtime_error("list allocation failed");
}
void App::clearItems(ControlId id)
{
    auto &c = p->ctrl(id);
    if (c.objects)
        throw std::invalid_argument("object list is managed automatically");
    if (c.kind != L"list" && c.kind != L"combo")
        throw std::invalid_argument("expected list/combo");
    SendMessageW(c.hwnd, c.kind == L"list" ? LB_RESETCONTENT : CB_RESETCONTENT, 0, 0);
}
int App::itemCount(ControlId id) const
{
    auto &c = p->ctrl(id);
    if (c.kind != L"list" && c.kind != L"combo")
        throw std::invalid_argument("expected list/combo");
    return static_cast<int>(SendMessageW(c.hwnd, c.kind == L"list" ? LB_GETCOUNT : CB_GETCOUNT, 0, 0));
}
void App::setSelectedIndex(ControlId id, int index)
{
    auto &c = p->ctrl(id);
    if (c.kind != L"list" && c.kind != L"combo")
        throw std::invalid_argument("expected list/combo");
    if (c.objects)
        throw std::invalid_argument("use select(object ID) for object lists");
    if (index < -1 || index >= itemCount(id))
        throw std::out_of_range("list index");
    SendMessageW(c.hwnd, c.kind == L"list" ? LB_SETCURSEL : CB_SETCURSEL, index, 0);
}
int App::selectedIndex(ControlId id) const
{
    auto &c = p->ctrl(id);
    if (c.kind != L"list" && c.kind != L"combo")
        throw std::invalid_argument("expected list/combo");
    return static_cast<int>(SendMessageW(c.hwnd, c.kind == L"list" ? LB_GETCURSEL : CB_GETCURSEL, 0, 0));
}
void App::setButtonAction(ControlId id, std::function<void()> f)
{
    auto &c = p->ctrl(id);
    p->require(c, L"button");
    c.click = std::move(f);
}
void App::focusControl(ControlId id)
{
    SetFocus(p->ctrl(id).hwnd);
}
void App::setFontSize(ControlId id, int pixels)
{
    if (pixels < 6 || pixels > 200)
        throw std::invalid_argument("font size");
    auto &c = p->ctrl(id);
    HFONT f =
        CreateFontW(-pixels, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    if (!f)
        throw std::runtime_error("font creation failed");
    SendMessageW(c.hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE);
    if (c.font)
        DeleteObject(c.font);
    c.font = f;
}
void App::showMessage(const std::wstring &t, const std::wstring &m)
{
    MessageBoxW(p->window, m.c_str(), t.c_str(), MB_OK | MB_ICONINFORMATION);
}
} // namespace harbor
