#pragma once
// Harbor2D 1.0: Windows C++17, Win32 controls, compatibility OpenGL.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace harbor
{
using ObjectId = std::uint64_t;
using ControlId = int;
constexpr ObjectId NoObject = 0;
struct Vec2
{
    double x = 0, y = 0;
};
struct Rect
{
    int x = 0, y = 0, width = 160, height = 30;
};
struct Color
{
    float r = 1, g = 1, b = 1, a = 1;
};
struct Object
{
    ObjectId id = 0;
    std::wstring name = L"Object";
    Vec2 position{}, velocity{};
    double radius = 5;
    Color color{};
    bool visible = true, selectable = true;
    double userValue = 0; // Application-owned scalar (e.g. mass).
};
struct Camera
{
    Vec2 center{450, 325};
    double zoom = 1;
    Vec2 screenToWorld(Vec2 p, int w, int h) const
    {
        return {center.x + (p.x - w * .5) / zoom, center.y + (p.y - h * .5) / zoom};
    }
    Vec2 worldToScreen(Vec2 p, int w, int h) const
    {
        return {(p.x - center.x) * zoom + w * .5, (p.y - center.y) * zoom + h * .5};
    }
    void zoomAt(Vec2 p, double factor, int w, int h)
    {
        if (!std::isfinite(factor) || factor <= 0)
            throw std::invalid_argument("zoom factor");
        auto before = screenToWorld(p, w, h);
        zoom = std::clamp(zoom * factor, .002, 10000.0);
        center = {before.x - (p.x - w * .5) / zoom, before.y - (p.y - h * .5) / zoom};
    }
};
// Native-independent geometry: also used by the automated tests.
inline bool contains(const Object &o, Vec2 p)
{
    return std::hypot(p.x - o.position.x, p.y - o.position.y) <= o.radius;
}
inline void bounceAxis(double &p, double &v, double lo, double hi, double dt)
{
    double length = hi - lo;
    if (length <= 0)
    {
        p = (lo + hi) * .5;
        return;
    }
    double q = std::fmod(p - lo + v * dt, 2 * length);
    if (q < 0)
        q += 2 * length;
    if (q < length)
        p = lo + q;
    else
    {
        p = hi - (q - length);
        v = -v;
    }
}
struct Config
{
    std::wstring title = L"Harbor2D";
    int width = 1280, height = 800, panelWidth = 280;
    bool maximized = true, vsync = false;
};
class App
{
  public:
    explicit App(Config config = {});
    ~App();
    App(const App &) = delete;
    App &operator=(const App &) = delete;

    // Lifecycle and timing. All functions are called on the UI thread.
    int run();
    void close();
    void setTitle(const std::wstring &text);
    void setMaximized(bool value);
    void setVSync(bool value);
    void setFpsLimit(double fps); // 0 = uncapped
    double fps() const;
    double frameMilliseconds() const;
    double elapsedSeconds() const;
    double deltaSeconds() const; // Unscaled, clamped to maxDelta.
    void setMaxDelta(double seconds);
    void setPaused(bool value);
    bool paused() const;
    void togglePaused();
    void setTimeScale(double value);
    double timeScale() const;
    void onUpdate(std::function<void(double)> callback); // Scaled dt; skipped while paused.
    void onFrame(std::function<void(double)> callback);  // Unscaled dt; also while paused.
    void onSelect(std::function<void(ObjectId)> callback);
    void onDraw(std::function<void()> callback); // Custom drawing after objects, before presentation.
    void drawLine(Vec2 a, Vec2 b, Color color = {});
    void drawRectangle(Vec2 topLeft, Vec2 size, Color color = {}, bool filled = false);
    void drawCircle(Vec2 center, double radius, Color color = {});
    void drawText(Vec2 worldPosition, const std::wstring &text, Color color = {});
    void drawScreenText(Vec2 pixelPosition, const std::wstring &text, Color color = {});
    void onResize(std::function<void(int, int)> callback);
    void onKey(std::function<void(int, bool)> callback);
    bool keyDown(int virtualKey) const;

    // Scene and objects. IDs are never reused during one App's lifetime.
    ObjectId addCircle(std::wstring name, Vec2 position, double radius, Color color = {});
    bool removeObject(ObjectId id);
    void clearObjects();
    bool hasObject(ObjectId id) const;
    std::size_t objectCount() const;
    std::vector<ObjectId> objectIds() const;
    Object object(ObjectId id) const; // Copy; throws std::out_of_range for stale IDs.
    void setObjectName(ObjectId id, const std::wstring &name);
    std::wstring objectName(ObjectId id) const;
    void setPosition(ObjectId id, Vec2 position);
    Vec2 position(ObjectId id) const;
    void moveObject(ObjectId id, Vec2 offset);
    void setVelocity(ObjectId id, Vec2 velocity);
    Vec2 velocity(ObjectId id) const;
    void setRadius(ObjectId id, double radius);
    double radius(ObjectId id) const;
    void setColor(ObjectId id, Color color);
    Color color(ObjectId id) const;
    void setVisible(ObjectId id, bool value);
    bool visible(ObjectId id) const;
    void setSelectable(ObjectId id, bool value);
    bool selectable(ObjectId id) const;
    void setUserValue(ObjectId id, double value);
    double userValue(ObjectId id) const;
    void integrate(double dt);
    void bounceInWorld(double dt); // Integrates position AND reflects velocity.
    void setWorldSize(double width, double height);
    Vec2 worldSize() const;
    void setWorldBorderVisible(bool value);
    bool worldBorderVisible() const;
    void setWorldBorderColor(Color color);
    void setBackground(Color color);
    Color background() const;
    void setCircleSegments(int count);
    int circleSegments() const;
    void setCulling(bool value);
    bool culling() const;

    // Selection, picking, camera, grid.
    void select(ObjectId id); // NoObject clears; invalid/unselectable ID throws.
    void clearSelection();
    ObjectId selected() const;
    ObjectId pick(Vec2 worldPoint) const; // Last drawn visible selectable circle wins.
    void focusObject(ObjectId id);
    void setSelectionColor(Color color);
    void setCamera(Vec2 center);
    Vec2 camera() const;
    void pan(Vec2 worldOffset);
    void setZoom(double value);
    double zoom() const;
    void zoomAt(Vec2 screenPoint, double factor);
    Vec2 screenToWorld(Vec2 point) const; // Coordinates relative to canvas.
    Vec2 worldToScreen(Vec2 point) const;
    void fitWorld(double padding = .9);
    void setCameraEnabled(bool value);
    bool cameraEnabled() const;
    void setPickingEnabled(bool value);
    bool pickingEnabled() const;
    int canvasWidth() const;
    int canvasHeight() const;
    void setPanelWidth(int pixels);
    int panelWidth() const;
    void setGridVisible(bool value);
    bool gridVisible() const;
    void setGridColor(Color color);
    void setGridSpacing(double worldUnits); // 0 = adaptive.
    double gridSpacing() const;
    void setAxesVisible(bool value);
    bool axesVisible() const;

    // Rectangles are in panel-client pixels, independent of world camera.
    ControlId addButton(std::wstring text, Rect rect, std::function<void()> action = {});
    ControlId addLabel(std::wstring text, Rect rect);
    ControlId addCheckbox(std::wstring text, Rect rect, bool checked, std::function<void(bool)> action = {});
    ControlId addEdit(std::wstring text, Rect rect, std::function<void(std::wstring)> action = {});
    ControlId addSlider(Rect rect, int minimum, int maximum, int value, std::function<void(int)> action = {});
    ControlId addListBox(Rect rect, std::function<void(int)> action = {});
    ControlId addObjectList(Rect rect); // Automatic two-way selection and names.
    ControlId addProgressBar(Rect rect, int minimum = 0, int maximum = 100);
    ControlId addComboBox(Rect rect, std::vector<std::wstring> items, std::function<void(int)> action = {});
    void removeControl(ControlId id);
    void clearControls();
    void setText(ControlId id, const std::wstring &text);
    std::wstring text(ControlId id) const;
    void setRect(ControlId id, Rect rect);
    Rect rect(ControlId id) const;
    void setControlVisible(ControlId id, bool value);
    bool controlVisible(ControlId id) const;
    void setEnabled(ControlId id, bool value);
    bool enabled(ControlId id) const;
    void setChecked(ControlId id, bool value);
    bool checked(ControlId id) const;
    void setValue(ControlId id, int value); // Slider or progress bar.
    int value(ControlId id) const;
    void setRange(ControlId id, int minimum, int maximum);
    void setItems(ControlId id, const std::vector<std::wstring> &items);
    void addItem(ControlId id, const std::wstring &item);
    void clearItems(ControlId id);
    int itemCount(ControlId id) const;
    void setSelectedIndex(ControlId id, int index);
    int selectedIndex(ControlId id) const;
    void setButtonAction(ControlId id, std::function<void()> action);
    void focusControl(ControlId id);
    void setFontSize(ControlId id, int pixels);
    void showMessage(const std::wstring &title, const std::wstring &message);

  private:
    struct Impl;
    std::unique_ptr<Impl> p;
};
} // namespace harbor
