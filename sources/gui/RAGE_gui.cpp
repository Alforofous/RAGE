#include "RAGE.hpp"
#include "RAGE_gui.hpp"
#include "RAGE_primitive_objects.hpp"

RAGE_gui::RAGE_gui(RAGE *rage)
{
	this->show_performance_window = false;
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	this->io = &ImGui::GetIO();
	io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	std::string font_path = rage->executable_path + "/assets/fonts/Roboto/Roboto-Bold.ttf";
	std::ifstream font_file(font_path.c_str());
	if (!font_file.good())
	{
		std::cout << "Failed to load font file: " << font_path << std::endl;
	}
	else
	{
		font_file.close();
		ImFont *font = io->Fonts->AddFontFromFileTTF(font_path.c_str(), 16.0f);
	}

	io->IniFilename = NULL;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(rage->window->glfw_window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
}

void RAGE_gui::draw_performance_window(RAGE *rage)
{
	ImGui::Begin("Performance window", NULL, ImGuiWindowFlags_NoResize);
	static bool first_time_open = true;
	if (first_time_open == true)
	{
		ImGui::SetWindowPos(ImVec2(0, 0));
		ImGui::SetWindowSize(ImVec2(0, 0));
		first_time_open = false;
	}
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

	std::string fps_string = "FPS: " + std::to_string((int)fps);
	ImGui::Text("%s", fps_string.c_str());
	ImGui::PlotHistogram("", &frames[0], (int)frames.size(), 0, NULL, 0.0f, 360.0f, ImVec2(200, 40));
	ImGui::Text("Window position: %d, %d", rage->window->pixel_position.x, rage->window->pixel_position.y);
	ImGui::Text("Window size: %d, %d", rage->window->pixel_size.x, rage->window->pixel_size.y);
	ImGui::End();
}

void RAGE_gui::draw_inspector(RAGE *rage)
{
	ImGui::Begin("Inspector", NULL, ImGuiWindowFlags_NoCollapse);
	ImGui::SetWindowPos(ImVec2(0, 200));
	ImGui::SetWindowSize(ImVec2(0, 0));
	ImGui::Text("Scene object count: %zu", rage->scene.get_objects()->size());
	ImGui::Text("Dockspace size: %d, %d", this->dockspace_size.x, this->dockspace_size.y);
	ImGui::End();
}

void RAGE_gui::draw_dockspace(RAGE *rage)
{
	ImGuiViewport *viewport = ImGui::GetMainViewport();
	ImGui::DockSpaceOverViewport(viewport, ImGuiDockNodeFlags_None);
	this->dockspace_size = glm::vec2(viewport->WorkSize.x, viewport->WorkSize.y);
}

void RAGE_gui::reset_dockings()
{
	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImGuiID dockspace_id = ImGui::GetID("DockSpace");
	static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
	ImGui::DockBuilderRemoveNode(dockspace_id);
	ImGuiID docknode = ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

	ImGuiID dock_id_top = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.2f, nullptr, &dockspace_id);
	ImGuiID dock_id_down = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.25f, nullptr, &dockspace_id);
	ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.15f, nullptr, &dockspace_id);
	ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.15f, nullptr, &dockspace_id);

	if (ImGui::FindWindowByName("Scene") != NULL)
	{
		ImGui::DockBuilderDockWindow("Scene", docknode);
	}

	ImGui::DockBuilderFinish(dockspace_id);
}

void RAGE_gui::draw(RAGE *rage)
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	draw_dockspace(rage);
	if (show_performance_window == true)
		draw_performance_window(rage);
	scene_view.draw(rage);
	menu_bar.draw(rage);
	draw_inspector(rage);
	reset_dockings();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	ImGui::EndFrame();
	if (this->io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		GLFWwindow *backup_current_context = glfwGetCurrentContext();
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
		glfwMakeContextCurrent(backup_current_context);
	}
}

RAGE_gui::~RAGE_gui()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}