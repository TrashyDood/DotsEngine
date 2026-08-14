#include <Input/Input.h>

GLFWwindow* Input::s_Window = nullptr;

std::unordered_map<int, bool> Input::s_CurrentKeys;
std::unordered_map<int, bool> Input::s_PreviousKeys;

std::unordered_map<int, bool> Input::s_CurrentMouse;
std::unordered_map<int, bool> Input::s_PreviousMouse;

double Input::s_MouseX = 0.0;
double Input::s_MouseY = 0.0;
double Input::s_LastMouseX = 0.0;
double Input::s_LastMouseY = 0.0;
double Input::s_ScrollDelta = 0.0;

GLFWkeyfun			Input::s_PrevKeyCallback = nullptr;
GLFWmousebuttonfun	Input::s_PrevMouseButtonCallback = nullptr;
GLFWcursorposfun	Input::s_PrevCursorPosCallback = nullptr;
GLFWscrollfun		Input::s_PrevScrollCallback = nullptr;
GLFWcharfun			Input::s_PrevCharCallback = nullptr;

void Input::Init(GLFWwindow* window)
{
	s_Window = window;

	// remember this makes imgui crap itself sometimes, 
	// check main ImGui_ImplGlfw_InitForOpenGL
	s_PrevKeyCallback = glfwSetKeyCallback(window, KeyCallback);
	s_PrevMouseButtonCallback = glfwSetMouseButtonCallback(window, MouseButtonCallback);
	s_PrevCursorPosCallback = glfwSetCursorPosCallback(window, CursorPositionCallback);
	s_PrevScrollCallback = glfwSetScrollCallback(window, ScrollCallback);
	s_PrevCharCallback = glfwSetCharCallback(window, CharCallback);

	glfwGetCursorPos(window, &s_MouseX, &s_MouseY);
	s_LastMouseX = s_MouseX;
	s_LastMouseY = s_MouseY;
}

void Input::Update()
{
	s_PreviousKeys = s_CurrentKeys;
	s_PreviousMouse = s_CurrentMouse;

	s_LastMouseX = s_MouseX;
	s_LastMouseY = s_MouseY;

	s_ScrollDelta = 0.0; 
}

bool Input::KeyDown(int key)
{
	return s_CurrentKeys[key];
}

bool Input::KeyPressed(int key)
{
	return s_CurrentKeys[key] && !s_PreviousKeys[key];
}

bool Input::KeyReleased(int key)
{
	return !s_CurrentKeys[key] && s_PreviousKeys[key];
}

bool Input::MouseDown(int button)
{
	return s_CurrentMouse[button];
}

bool Input::MousePressed(int button)
{
	return s_CurrentMouse[button] && !s_PreviousMouse[button];
}

bool Input::MouseReleased(int button)
{
	return !s_CurrentMouse[button] && s_PreviousMouse[button];
}

double Input::MouseX() { return s_MouseX; }
double Input::MouseY() { return s_MouseY; }

double Input::MouseDeltaX()
{
	return s_MouseX - s_LastMouseX;
}

double Input::MouseDeltaY()
{
	return s_MouseY - s_LastMouseY;
}

double Input::ScrollDelta()
{
	return s_ScrollDelta;
}

void Input::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (action == GLFW_PRESS) 
	{
		s_CurrentKeys[key] = true;
	}
	else if (action == GLFW_RELEASE) 
	{
		s_CurrentKeys[key] = false;
	}

	if (s_PrevKeyCallback) 
	{
		s_PrevKeyCallback(window, key, scancode, action, mods);
	}
}

void Input::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	if (action == GLFW_PRESS)
	{
		s_CurrentMouse[button] = true;
	}
	else if (action == GLFW_RELEASE)
	{
		s_CurrentMouse[button] = false;
	}

	if (s_PrevMouseButtonCallback)
	{
		s_PrevMouseButtonCallback(window, button, action, mods);
	}
}

void Input::CursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
{
	s_MouseX = xpos;
	s_MouseY = ypos;

	if (s_PrevCursorPosCallback)
	{
		s_PrevCursorPosCallback(window, xpos, ypos);
	}
}

void Input::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	s_ScrollDelta = yoffset;

	if (s_PrevScrollCallback)
	{
		s_PrevScrollCallback(window, xoffset, yoffset);
	}
}

void Input::CharCallback(GLFWwindow* window, unsigned int c)
{
	if (s_PrevCharCallback)
	{
		s_PrevCharCallback(window, c);
	}
}
