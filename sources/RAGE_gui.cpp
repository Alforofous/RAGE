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

void RAGE_gui::draw(RAGE *rage)
{
	bool drawTriangle = true;
	float size = 1.0f;
	float color[4] = { 0.8f, 0.3f, 0.02f, 1.0f };

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	std::string	deltaTimeString = "Elapsed time: " + std::to_string((int)rage->deltaTime) + "ms";
	
	ImGui::Begin("Inspector", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
	ImGui::SetWindowPos(ImVec2(0, 0));
	ImGui::SetWindowSize(ImVec2(0, 0));
	ImGui::Text(&deltaTimeString[0]);
	ImGui::Checkbox("Draw Triangle", &drawTriangle);
	ImGui::SliderFloat("Size", &size, 0.5f, 2.0f);
	ImGui::ColorEdit4("Color", color);
	ImGui::End();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
