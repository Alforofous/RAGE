#include "RAGE.hpp"
#include"imgui.h"
#include"imgui_impl_glfw.h"
#include"imgui_impl_opengl3.h"

RAGE_gui::RAGE_gui(GLFWwindow *glfw_window)
{
	scene_view = new RAGE_scene_view();
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO(); (void)io;
	io.IniFilename = NULL;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(glfw_window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
}

RAGE_gui::~RAGE_gui()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void draw_fps_graph(int fps)
{
	std::vector<float> frames;
	if (frames.size() > 100)
	{
		for (size_t i = 1; i < frames.size(); i++)
			frames[i - 1] = frames[i];
		frames[frames.size() - 1] = fps;
	}
	else
	{
		frames.push_back(fps);
	}
	
	ImGui::Begin("FPS Graph");
	
	char text[20];
	ImGui::Text(text);

	ImGui::PlotHistogram("Framerate", &frames[0], frames.size(), 0, NULL, 0.0f, 1000.0f, ImVec2(300, 100));
	ImGui::End();
}

void RAGE_gui::draw(RAGE *rage)
{
	bool drawTriangle = true;
	float size = 1.0f;
	float color[4] = { 0.8f, 0.3f, 0.02f, 1.0f };

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	std::string	deltaTimeString = "Elapsed time: " + std::to_string(rage->delta_time) + "ms";

	double fps = 1 / rage->delta_time;
	std::string	fpsString = "FPS: " + std::to_string((int)fps);
	
	ImGui::Begin("Inspector", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground);
	ImGui::SetWindowPos(ImVec2(0, 0));
	ImGui::SetWindowSize(ImVec2(0, 0));
	ImGui::Text("%s", deltaTimeString.c_str());
	ImGui::Text("%s", fpsString.c_str());
	ImGui::Checkbox("Draw Triangle", &drawTriangle);
	ImGui::SliderFloat("Size", &size, 0.5f, 2.0f);
	ImGui::ColorEdit4("Color", color);
	draw_fps_graph(fps);
	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}