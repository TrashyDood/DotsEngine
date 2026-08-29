#pragma once
#include "DebugLines.h"

class DotsScene;

class DotsEditor
{
public:
	DotsEditor(DotsScene* aScene);
	~DotsEditor();

	void DotsEditor_Draw();
	void DotsEditor_CheckLeak();
	void DotsEditor_ShowLeak();

private:
	DotsScene* scene = nullptr;
	bool leakingMemory = false;
};