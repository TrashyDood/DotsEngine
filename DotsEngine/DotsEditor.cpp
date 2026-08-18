#include "DotsEditor.h"
#include <Time/Time.h>
#include <imgui/imgui.h>
#include <iostream>
#include "DotsScene.h"
#include <windows.h>
#include <psapi.h>
#include <queue>

const float Leak_Check = 0.25f;
const int Leak_QueueMax = 25;
float Leak_Current = 0;
int Leak_Streak = 0;
int Leak_StreakCheck = 20;
std::queue<size_t> Leak_Queue;
size_t Leak_Threshold = 1024 * 1024;
size_t Leak_RecordHigh = 0;
size_t memory = 0;

size_t GetMemoryUsage()
{
	PROCESS_MEMORY_COUNTERS info;
	GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info));
	return info.WorkingSetSize;
}

DotsEditor::DotsEditor(DotsScene* aScene)
{
	lines = new DebugLines();
	scene = aScene;
}

void DotsEditor::DotsEditor_Draw()
{
	ImGui::Begin("Dots Editor");

	ImGui::NewLine();
	ImGui::Separator();
	ImGui::Indent(15);
	ImGui::Text("Performance");

	std::string fpsCounter = "FPS - " + std::to_string(Time::fps);
	ImGui::Text(fpsCounter.c_str());

	memory = GetMemoryUsage();


	DotsEditor_ShowLeak();
	ImGui::Unindent();
	ImGui::Separator();
	ImGui::NewLine();

	ImGui::Checkbox("Collision test", &CollisionCheck);

	ImGui::InputInt("DotAmount", &scene->pendingDotCount, 10, 100);
	ImGui::InputFloat("DotSpeed", &scene->dotSpeed, 1.0f, 10.0f);
	ImGui::SliderFloat("DotBounds", &scene->bounds, 10.0f, 200.0f);

	if (ImGui::Button("Start simulation"))
	{
		if (CollisionCheck)
		{
			scene->DotsScene_ClearDots();
			scene->DotsScene_CreateDot(glm::vec3(8, 0, 0), glm::vec3(-1, 0, 0));
			scene->DotsScene_CreateDot(glm::vec3(-8, 0, 0), glm::vec3(1, 0, 0));
		}
		else
		{
			scene->DotsScene_ClearDots();
			scene->DotsScene_Start();
		}
	}

	if (ImGui::Button("Leaderboard mode"))
		scene->DotsScene_LeaderBoardMode();

	ImGui::End();

	lines->DrawDebugLines();

	DotsEditor_CheckLeak();
}

void DotsEditor::DotsEditor_CheckLeak()
{
	//ask oscar if he has a better idea cuz this is nutz

	Leak_Current += Time::deltaTime;

	if (Leak_Current >= Leak_Check)
	{
		Leak_Current = 0;
		Leak_Queue.push(memory);
	}

	if (Leak_Queue.size() > Leak_QueueMax)
	{
		Leak_Queue.pop();
		if (memory > Leak_RecordHigh + Leak_Threshold)
		{
			Leak_Streak++;
			Leak_RecordHigh = memory;
		}
		else
			Leak_Streak--;

		if (Leak_Queue.back() > Leak_Queue.front() + Leak_Threshold)
			if (Leak_Streak < Leak_StreakCheck) Leak_Streak++;
		else
			if (Leak_Streak > 0) Leak_Streak--;
	}

	leakingMemory = Leak_Streak >= Leak_StreakCheck;
}

void DotsEditor::DotsEditor_ShowLeak()
{
	float pulse = (sinf(Time::totalTime * 15.0f) + 1.0f) * 0.5f;

	ImVec4 color = ImVec4(pulse, 0.0f, 0.0f, 1.0f);

	if (leakingMemory) ImGui::PushStyleColor(ImGuiCol_Text, color);

	ImGui::Text("RAM Usage: %.2f MB", memory / (1024.0f * 1024.0f));
	if (leakingMemory)
	{
		ImGui::Text("Martin thinks you might be");
		ImGui::Text("LEAKING MEMORY");

	}

	if (leakingMemory) ImGui::PopStyleColor();
}

DotsEditor::~DotsEditor()
{

}
