#include "RAGE.hpp"
#include "RAGE_mesh.hpp"
#include "gui/RAGE_inspector.hpp"
#include "loaders/GLB_attribute_buffer.hpp"
#include "loaders/GLB_utilities.hpp"

static void draw_drag_float3(RAGE *rage, std::string label, glm::vec3 &vector, float drag_speed, glm::vec3 *min = NULL, glm::vec3 *max = NULL, bool overflow = false)
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
	if (ImGui::IsItemActivated())
		glfwSetInputMode(rage->window->glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	if (ImGui::IsItemDeactivated())
		glfwSetInputMode(rage->window->glfw_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

static void display_attribute_buffer_data(GLB_attribute_buffer *attribute_buffer)
{
	if (attribute_buffer == NULL)
		return ;
	std::string attribute_buffer_uid = std::to_string((unsigned long long)attribute_buffer);
	if (ImGui::TreeNode(attribute_buffer_uid.c_str(), "Attribute Buffer - %s", attribute_buffer->get_name().c_str()))
	{
		ImGui::Text("Data type: %s", GLB_utilities::gl_data_type_to_string(attribute_buffer->get_data_type()).c_str());
		ImGui::Text("Component count: %zu", attribute_buffer->get_component_count());
		ImGui::Text("Byte size: %zu", attribute_buffer->get_byte_size());
		if (ImGui::TreeNode(attribute_buffer_uid.c_str(), "Data"))
		{
			std::string buffer_data = attribute_buffer->get_data_string();
			ImGui::TextUnformatted(buffer_data.c_str(), buffer_data.c_str() + buffer_data.size());
			ImGui::TreePop();
		}
		ImGui::TreePop();
	}
}

static void display_primitive_data(RAGE_primitive *primitive, std::string node_uid)
{
	if (primitive == NULL)
		return ;
	if (ImGui::TreeNode(("PrimitiveNode" + node_uid).c_str(), primitive->name.c_str()))
	{
		for (size_t attribute_buffer_index = 0; attribute_buffer_index < primitive->attribute_buffers.size(); attribute_buffer_index++)
		{
			GLB_attribute_buffer *attribute_buffer = primitive->attribute_buffers[attribute_buffer_index];
			display_attribute_buffer_data(attribute_buffer);
		}
		ImGui::TreePop();
	}
}

static void display_textures_data(std::vector<RAGE_texture2D> &textures, std::string node_uid)
{
	if (textures.size() == 0)
		return ;
	if (ImGui::TreeNode(("TexturesNode" + node_uid).c_str(), "Textures[%zu]", textures.size()))
	{
		for (size_t i = 0; i < textures.size(); i++)
		{
			glm::ivec2 imageSize = glm::ivec2(256, 256);
			float aspectRatio = textures[i].get_width() / (float)textures[i].get_height();
			if (aspectRatio > 1.0f)
				imageSize.y = (float)imageSize.x / aspectRatio;
			else
				imageSize.x = (float)imageSize.y * aspectRatio;
			ImGui::Image((void *)(intptr_t)textures[i].get_id(), ImVec2(imageSize.x, imageSize.y), ImVec2(0, 0), ImVec2(1, 1), ImVec4(1, 1, 1, 1), ImVec4(1, 1, 1, 0.5));
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text("ID: %d", textures[i].get_id());
				ImGui::Text("Size: %d x %d", textures[i].get_width(), textures[i].get_height());
				ImGui::Text("Display size: %d x %d", imageSize.x, imageSize.y);
				ImGui::EndTooltip();
			}
			ImGui::Separator();
		}
		ImGui::TreePop();
	}
}

static void display_material_data(RAGE_material *material, std::string node_uid)
{
	if (material == NULL)
		return ;
	if (ImGui::TreeNode(("MaterialNode" + node_uid).c_str(), "Material - %s", material->name.c_str()))
	{
		display_textures_data(material->get_textures(), node_uid);
		ImGui::TreePop();
	}
}

static void display_mesh_data(RAGE_mesh *mesh)
{
	std::string node_uid = std::to_string((unsigned long long)mesh);
	if (ImGui::TreeNode(("MeshNode" + node_uid).c_str(), "Mesh - %s", mesh->name.c_str()))
	{
		display_material_data(mesh->material, node_uid);
		ImGui::Separator();
		if (ImGui::TreeNode(("PrimitivesNode" + node_uid).c_str(), "Primitives[%zu]", mesh->primitives.size()))
		{
			for (size_t i = 0; i < mesh->primitives.size(); i++)
			{
				display_primitive_data(mesh->primitives[i], node_uid);
				ImGui::Separator();
			}
			ImGui::TreePop();
		}
		ImGui::TreePop();
	}
}

void RAGE_inspector::draw(RAGE *rage)
{
	ImGui::Begin("Inspector", NULL, ImGuiWindowFlags_NoCollapse);
	ImGui::Text("Camera:");
	glm::vec3 camera_position = rage->camera.position;
	ImGui::Text("Position: %f, %f, %f", camera_position.x, camera_position.y, camera_position.z);
	ImGui::Text("Forward: %f, %f, %f", rage->camera.get_forward().x, rage->camera.get_forward().y, rage->camera.get_forward().z);
	ImGui::Text("Fov: %f", rage->camera.m_perspective_matrix[0][0]);

	ImGui::Separator();
	ImGui::Text("Scene object count: %zu", rage->scene->get_objects()->size());
	std::vector<RAGE_object *> *selected_objects = rage->scene->get_selected_objects();
	ImGui::Text("Selected object count: %zu", selected_objects->size());
	ImGui::Separator();
	for (size_t selected_object_index = 0; selected_object_index < selected_objects->size(); selected_object_index++)
	{
		RAGE_object *selected_object = selected_objects->at(selected_object_index);
		ImGui::Text("Selected object name: %s", selected_object->name.c_str());
		ImGui::Text("uid: %llu", (unsigned long long)selected_object);

		draw_drag_float3(rage, "Position##" + std::to_string(selected_object_index), selected_object->position, 0.1f);
		glm::vec3 min_rotation = glm::vec3(-180.0f);
		glm::vec3 max_rotation = glm::vec3(180.0f);
		draw_drag_float3(rage, "Rotation##" + std::to_string(selected_object_index), selected_object->rotation, 1.0f, &min_rotation, &max_rotation, true);
		draw_drag_float3(rage, "Scale##" + std::to_string(selected_object_index), selected_object->scale, 0.1f);
		if (selected_object->mesh != NULL)
			display_mesh_data(selected_object->mesh);
		ImGui::Separator();
	}
	ImGui::End();
}