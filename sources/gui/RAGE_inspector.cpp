#include "RAGE.hpp"
#include "RAGE_inspector.hpp"

static void draw_drag_float3(std::string label, glm::vec3 &vector, float drag_speed, glm::vec3 *min = NULL, glm::vec3 *max = NULL, bool overflow = false)
{
	if (ImGui::DragFloat3(label.c_str(), &vector.x, drag_speed) == true)
	{
		if (min == NULL && max == NULL)
			return ;
		glm::vec3 new_min = glm::vec3(-FLT_MAX);
		glm::vec3 new_max = glm::vec3(FLT_MAX);
		if (min == NULL)
			min = &new_min;
		if (max == NULL)
			max = &new_max;
		if (overflow == false)
			vector = glm::clamp(vector, *min, *max);
		else
		{
			for (int i = 0; i < 3; i++)
			{
				if (vector[i] < (*min)[i])
					vector[i] = (*max)[i];
				else if (vector[i] > (*max)[i])
					vector[i] = (*min)[i];
			}
		}
	}
	GLFWwindow *window = glfwGetCurrentContext();
	if (window == NULL)
		return ;
	if (ImGui::IsItemActivated())
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if (ImGui::IsItemDeactivated())
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void RAGE_inspector::draw(RAGE *rage)
{
	ImGui::Begin("Inspector", NULL, ImGuiWindowFlags_NoCollapse);
	ImGui::Text("Scene object count: %zu", rage->scene.get_objects()->size());
	std::vector<RAGE_object *> *selected_objects = rage->scene.get_selected_objects();
	ImGui::Text("Selected object count: %zu", selected_objects->size());
	ImGui::Separator();
	for (int i = 0; i < selected_objects->size(); i++)
	{
		RAGE_object *selected_object = selected_objects->at(i);
		ImGui::Text("Selected object name: %s", selected_object->get_name().c_str());
		ImGui::Text("UUID: %llu", (unsigned long long)selected_object);

		draw_drag_float3("Position" + std::to_string(i), selected_object->get_position(), 0.1f);
		glm::vec3 min_rotation = glm::vec3(glm::radians(-180.0f));
		glm::vec3 max_rotation = glm::vec3(glm::radians(180.0f));
		draw_drag_float3("Rotation" + std::to_string(i), selected_object->get_rotation(), 0.1f, &min_rotation, &max_rotation, true);
		draw_drag_float3("Scale" + std::to_string(i), selected_object->get_scale(), 0.1f);
		ImGui::Separator();
	}
	ImGui::End();
}