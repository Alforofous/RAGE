#include "RAGE.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <sstream>
#include <iomanip>
#include "RAGE_gui.hpp"


RAGE_gui::RAGE_gui(RAGE *rage)
{
	glGenFramebuffers(1, &framebuffer);
	glGenTextures(1, &texture);
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO(); (void)io;

	std::string font_path = rage->executable_path + "/assets/fonts/Roboto/Roboto-Bold.ttf";
	std::ifstream font_file(font_path.c_str());
	if (!font_file.good())
	{
		std::cout << "Failed to load font file: " << font_path << std::endl;
	}
	else
	{
		font_file.close();
		ImFont *font = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 16.0f);
	}

	io.IniFilename = NULL;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(rage->window->glfw_window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
}

void RAGE_gui::draw_fps_graph(RAGE *rage)
{
	double fps = 1000 / rage->delta_time;
	
	if (frames.size() > 100)
	{
		for (size_t i = 1; i < frames.size(); i++)
			frames[i - 1] = frames[i];
		frames[frames.size() - 1] = (float)fps;
	}
	else
	{
		frames.push_back((float)fps);
	}

	std::stringstream stream;
	stream << std::fixed << std::setprecision(5) << rage->delta_time;

	std::string delta_time_string = "Elapsed time: " + stream.str() + "ms";
	ImGui::Text("%s", delta_time_string.c_str());

	std::string	fps_string = "FPS: " + std::to_string((int)fps);
	ImGui::Text("%s", fps_string.c_str());
	ImGui::PlotHistogram("", &frames[0], (int)frames.size(), 0, NULL, 0.0f, 360.0f, ImVec2(200, 40));
}

void RAGE_gui::draw_scene_view(RAGE *rage)
{
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

	int width, height;
	glfwGetFramebufferSize(rage->window->glfw_window, &width, &height);

	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
	glBindTexture(GL_TEXTURE_2D, texture);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthbuffer);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

	// Check the framebuffer status
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		// Handle the error
		std::cerr << "Framebuffer is not complete!" << std::endl;
		return;
	}

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	rage->scene.draw(rage);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	ImGui::Begin("OpenGL");
	ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
	ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
	if (canvas_sz.x < 50.0f)
		canvas_sz.x = 50.0f;
	if (canvas_sz.y < 50.0f)
		canvas_sz.y = 50.0f;
	ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

	ImGui::Image((void *)(intptr_t)texture, canvas_sz, ImVec2(0, 1), ImVec2(1, 0));
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

	ImGui::Begin("Inspector", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
	ImGui::SetWindowPos(ImVec2(0, 0));
	ImGui::SetWindowSize(ImVec2(0, 0));
	draw_fps_graph(rage);
	ImGui::Checkbox("Draw Triangle", &drawTriangle);
	ImGui::SliderFloat("Size", &size, 0.5f, 2.0f);
	ImGui::ColorEdit4("Color", color);
	ImGui::End();

	ImGui::Begin("Scene");
	ImGui::SetWindowPos(ImVec2(0, 0));
	ImGui::SetWindowSize(ImVec2(100, 100));
	ImGui::End();

	draw_scene_view(rage);

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

RAGE_gui::~RAGE_gui()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glDeleteRenderbuffers(1, &depthbuffer);
}