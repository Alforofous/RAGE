#include "RAGE.hpp"
#include "RAGE_scene_view.hpp"

RAGE_scene_view::RAGE_scene_view()
{
	glGenFramebuffers(1, &framebuffer);
	glGenRenderbuffers(1, &depthbuffer);
	glGenTextures(1, &texture);
	glfwGetWindowSize(glfwGetCurrentContext(), &window_size.x, &window_size.y);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
}

void RAGE_scene_view::draw(RAGE *rage)
{
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthbuffer);
	glBindTexture(GL_TEXTURE_2D, texture);

	glm::ivec2 framebuffer_size;
	glfwGetFramebufferSize(rage->window->glfw_window, &framebuffer_size.x, &framebuffer_size.y);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, this->window_size.x, this->window_size.y);
	glViewport(0, 0, this->window_size.x, this->window_size.y);

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthbuffer);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, this->window_size.x, this->window_size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		std::cerr << "Framebuffer is not complete!" << std::endl;
		return;
	}

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	rage->scene.draw(rage);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	ImGui::Begin("Scene View", NULL, ImGuiWindowFlags_NoCollapse);
	this->focused = ImGui::IsWindowFocused();
	ImGui::SetWindowPos(ImVec2((float)rage->window->pixel_position.x, (float)rage->window->pixel_position.y), ImGuiCond_FirstUseEver);
	ImGui::SetWindowSize(ImVec2((float)this->window_size.x, (float)this->window_size.y), ImGuiCond_FirstUseEver);
	rage->camera.set_aspect_ratio(this->window_size);
	this->window_size = glm::ivec2(ImGui::GetWindowSize().x, ImGui::GetWindowSize().y);
	ImGui::Text("Window size: %d, %d", this->window_size.x, this->window_size.y);
	ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
	ImGui::Image((void *)(intptr_t)texture, canvas_sz, ImVec2(0, 1), ImVec2(1, 0));
	ImGui::End();
}

bool RAGE_scene_view::is_focused()
{
	return (this->focused);
}

RAGE_scene_view::~RAGE_scene_view()
{
	glDeleteFramebuffers(1, &framebuffer);
	glDeleteRenderbuffers(1, &depthbuffer);
	glDeleteTextures(1, &texture);
}