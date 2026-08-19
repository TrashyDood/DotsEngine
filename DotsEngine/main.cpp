#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <Time/Time.h>
#include <Input/Input.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "DotsEditor.h"
#include "DotsScene.h"

#include "tracy/Tracy.hpp"

#include "ThreadPool.h"

const char* glsl_version = "#version 330 core"; // please don't break my heart, my achey breaky heart

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}

bool setupImgui(GLFWwindow* window)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      

	ImGui::StyleColorsLight();


	if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) return false;
	if (!ImGui_ImplOpenGL3_Init(glsl_version)) return false;

	return true;
}

int main(int argc, char* argv[])
{
	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(1280.0f, 720.0f, "DotsEngine", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	if (!setupImgui(window))
	{
		std::cout << "Failed to initialize IMGUI" << std::endl;
		return -1;
	}

	glViewport(0, 0, 1280.0f, 720.0f);

	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	DotsScene* scene = new DotsScene();
	DotsEditor* editor = new DotsEditor(scene);
	glfwSetWindowUserPointer(window, scene);
	Input::Init(window);
	glEnable(GL_DEPTH_TEST);

	if (argc > 1)
	{
		scene->pendingDotsCount = std::atoi(argv[1]);
		if (scene->pendingDotsCount <= 0)
		{
			std::cerr << "Invalid dot count argument, using default.\n";
			scene->pendingDotsCount = 100000; // current default
		}
	}

	scene->DotsScene_LeaderBoardMode(); // for profiling

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
		{
			ImGui_ImplGlfw_Sleep(10);
			continue;
		}

		float currentTime = (float)glfwGetTime();
		Time::Update(currentTime);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		processInput(window);

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//
		editor->DotsEditor_Draw();
		scene->DotsScene_Update();
		scene->DotsScene_Render();
		//

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);

		Input::Update();

		FrameMark;
	}

	delete editor;
	delete scene;

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();
	return 0;
}

