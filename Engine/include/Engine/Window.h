#pragma once
#include <functional>
#include <string>
#include <memory>

namespace Engine
{

    struct WindowProps
    {
        std::string Title;
        unsigned int Width = 1280;
        unsigned int Height = 720;
        WindowProps(const std::string &title = "Engine",
                    unsigned int width = 1280,
                    unsigned int height = 720)
            : Title(title), Width(width), Height(height) {}
    };

    class Window
    {
    public:
        using EventCallbackFn = std::function<void(const std::string &)>;

        virtual ~Window() = default;

        virtual void OnUpdate() = 0;
        virtual unsigned int GetWidth() const = 0;
        virtual unsigned int GetHeight() const = 0;

        virtual void SetEventCallback(const EventCallbackFn &callback) = 0;
        virtual void *GetWindowPointer() = 0;

        // Input helpers (used by Sample)
        virtual void GetCursorPosition(double &x, double &y) const = 0;

        // Factory
        static std::unique_ptr<Window> Create(const WindowProps &props = WindowProps());
    };

} // namespace Engine