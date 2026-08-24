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
	glfwSetWindowUserPointer(window, scene);
	Input::Init(window);
	glEnable(GL_DEPTH_TEST);

	uint32_t pendingDotsCount = 200;
	if (argc > 1)
	{
		pendingDotsCount = std::atoi(argv[1]);
		if (pendingDotsCount <= 0)
		{
			std::cerr << "Invalid dot count argument, using default.\n";
			pendingDotsCount = DotsScene::defaultDotsCount; // current default
		}
	}

	scene->DotsScene_LeaderBoardMode(true, pendingDotsCount); // for profiling
	//glfwSwapInterval(0); // disable vsync for profiling

	DotsEditor* editor = new DotsEditor(scene);

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
		{
			ImGui_ImplGlfw_Sleep(10);
			continue;
		}

		float currentTime = (float)glfwGetTime();
		Time::UpdateMain(currentTime);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		processInput(window);

		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//
		if (!scene->threadedSimulation)
			scene->DotsScene_Simulate(Time::main_DeltaTime);

		scene->DotsScene_Render(scene->renderContext);
		scene->flyingCamera->Update();
		editor->DotsEditor_Draw();
		//

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);

		Input::Update();

		FrameMarkNamed("Main");
	}

	delete editor;
	delete scene;

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();
	return 0;
}