// Input.h (only relevant parts shown)
#pragma once
#include <GLFW/glfw3.h>
#include <unordered_map>

class Input
{
public:
    static void Init(GLFWwindow* window);
    static void Update();

    // ... existing API ...
    static bool KeyDown(int key);
    static bool KeyPressed(int key);
    static bool KeyReleased(int key);

    static bool MouseDown(int button);
    static bool MousePressed(int button);
    static bool MouseReleased(int button);

    static double MouseX();
    static double MouseY();
    static double MouseDeltaX();
    static double MouseDeltaY();
    static double ScrollDelta();

private:
    static GLFWwindow* s_Window;

    static std::unordered_map<int, bool> s_CurrentKeys;
    static std::unordered_map<int, bool> s_PreviousKeys;

    static std::unordered_map<int, bool> s_CurrentMouse;
    static std::unordered_map<int, bool> s_PreviousMouse;

    static double s_MouseX, s_MouseY;
    static double s_LastMouseX, s_LastMouseY;
    static double s_ScrollDelta;

    // --- store previous callbacks so we can chain them ---
    static GLFWkeyfun           s_PrevKeyCallback;
    static GLFWmousebuttonfun   s_PrevMouseButtonCallback;
    static GLFWcursorposfun     s_PrevCursorPosCallback;
    static GLFWscrollfun        s_PrevScrollCallback;
    static GLFWcharfun          s_PrevCharCallback;

private:
    // our callbacks
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void CursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void CharCallback(GLFWwindow* window, unsigned int c);
};
