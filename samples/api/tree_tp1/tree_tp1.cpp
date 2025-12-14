/* Copyright (c) 2019-2025, Arm Limited and Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "tree_tp1.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <numeric>
#include <queue>
#include <vector>

#include "core/util/logging.hpp"
#include "common/vk_common.h"
#include "core/buffer.h"
#include "filesystem/legacy.h"

#include <glm/gtc/matrix_transform.hpp>

namespace
{
struct DatasetDefinition
{
	std::string              label;
	std::vector<std::string> files;
};

DatasetDefinition make_dataset(const std::string &label, const std::string &folder, std::initializer_list<const char *> entries)
{
	DatasetDefinition definition{};
	definition.label = label;
	definition.files.reserve(entries.size());
	const std::string base = "TP_CCO_Pacote_Dados/TP1_2D/" + folder + "/";
	for (const char *entry : entries)
	{
		definition.files.emplace_back(base + entry);
	}
	return definition;
}

const std::array<DatasetDefinition, 3> &datasets()
{
	static const std::array<DatasetDefinition, 3> definitions = {
	    make_dataset("Nterm 064", "Nterm_064",
	                 {"tree2D_Nterm0064_step0008.vtk",
	                  "tree2D_Nterm0064_step0016.vtk",
	                  "tree2D_Nterm0064_step0024.vtk",
	                  "tree2D_Nterm0064_step0032.vtk",
	                  "tree2D_Nterm0064_step0040.vtk",
	                  "tree2D_Nterm0064_step0048.vtk",
	                  "tree2D_Nterm0064_step0056.vtk",
	                  "tree2D_Nterm0064_step0064.vtk"}),
	    make_dataset("Nterm 128", "Nterm_128",
	                 {"tree2D_Nterm0128_step0016.vtk",
	                  "tree2D_Nterm0128_step0032.vtk",
	                  "tree2D_Nterm0128_step0048.vtk",
	                  "tree2D_Nterm0128_step0064.vtk",
	                  "tree2D_Nterm0128_step0080.vtk",
	                  "tree2D_Nterm0128_step0096.vtk",
	                  "tree2D_Nterm0128_step0112.vtk",
	                  "tree2D_Nterm0128_step0128.vtk"}),
	    make_dataset("Nterm 256", "Nterm_256",
	                 {"tree2D_Nterm0256_step0032.vtk",
	                  "tree2D_Nterm0256_step0064.vtk",
	                  "tree2D_Nterm0256_step0096.vtk",
	                  "tree2D_Nterm0256_step0128.vtk",
	                  "tree2D_Nterm0256_step0160.vtk",
	                  "tree2D_Nterm0256_step0192.vtk",
	                  "tree2D_Nterm0256_step0224.vtk",
	                  "tree2D_Nterm0256_step0256.vtk"})};
	return definitions;
}
}        // namespace

// Ajustes iniciais de câmera e título da amostra. Como herdamos do framework,
// basta posicionar a câmera para olhar o plano XY onde a árvore será desenhada.
TreeTp1::TreeTp1()
{
	title = "TP1 - Arterial Tree 2D";
	camera.type = vkb::CameraType::LookAt;
	camera.set_position({0.0f, 0.0f, 3.0f});
	camera.set_rotation({0.0f, 0.0f, 0.0f});
	camera.set_perspective(60.0f, 1.0f, 0.1f, 256.0f);
}

TreeTp1::~TreeTp1()
{
	if (has_device())
	{
		if (pipeline != VK_NULL_HANDLE)
		{
			vkDestroyPipeline(get_device().get_handle(), pipeline, nullptr);
		}
		if (pipeline_layout != VK_NULL_HANDLE)
		{
			vkDestroyPipelineLayout(get_device().get_handle(), pipeline_layout, nullptr);
		}
		if (descriptor_set_layout != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorSetLayout(get_device().get_handle(), descriptor_set_layout, nullptr);
		}
	}
}

// `prepare` é chamado pelo framework. Aqui configuramos todos os recursos Vulkan
// necessários (buffers, pipelines e carregamento inicial dos dados VTK).
bool TreeTp1::prepare(const vkb::ApplicationOptions &options)
{
	if (!ApiVulkanSample::prepare(options))
	{
		return false;
	}

	if (!set_dataset(parameters.dataset_index))
	{
		return false;
	}

	create_uniform_buffers();
	setup_descriptor_pool();
	setup_descriptor_set_layout();
	setup_descriptor_set();
	prepare_pipeline();

	if (!load_frame(playback.frame_index))
	{
		return false;
	}

	build_command_buffers();
	prepared = true;
	return true;
}

// UBO simples contendo a matriz MVP e informações de faixa de raio. Atualizamos
	// esse buffer toda frame via `update_uniform_buffer`.
void TreeTp1::create_uniform_buffers()
{
	uniform_buffers.scene = std::make_unique<vkb::core::BufferC>(get_device(),
	                                                             sizeof(UniformBufferObject),
	                                                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	                                                             VMA_MEMORY_USAGE_CPU_TO_GPU);
}

void TreeTp1::setup_descriptor_pool()
{
	const std::vector<VkDescriptorPoolSize> pool_sizes = {
	    vkb::initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)};

	VkDescriptorPoolCreateInfo pool_info =
	    vkb::initializers::descriptor_pool_create_info(pool_sizes, 1);

	VK_CHECK(vkCreateDescriptorPool(get_device().get_handle(), &pool_info, nullptr, &descriptor_pool));
}

void TreeTp1::setup_descriptor_set_layout()
{
	// Layout com um único UBO (set 0, binding 0) compartilhado entre VS e FS.
	// `vkb::initializers` já configura `VkDescriptorSetLayoutBinding` com tipo/tamanho corretos.
	const std::array<VkDescriptorSetLayoutBinding, 1> set_layout_bindings = {
	    vkb::initializers::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0)};

	VkDescriptorSetLayoutCreateInfo descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(set_layout_bindings.data(), static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(get_device().get_handle(), &descriptor_layout, nullptr, &descriptor_set_layout));

	// Pipeline layout simples, apenas com o set acima. Sem push constants nesta amostra.
	VkPipelineLayoutCreateInfo pipeline_layout_info =
	    vkb::initializers::pipeline_layout_create_info(&descriptor_set_layout, 1);

	VK_CHECK(vkCreatePipelineLayout(get_device().get_handle(), &pipeline_layout_info, nullptr, &pipeline_layout));
}

void TreeTp1::setup_descriptor_set()
{
	// Aloca o descriptor set com base no layout criado acima e atualiza com o buffer do UBO.
	// Essa estrutura é padrão em todas as amostras do framework.
	VkDescriptorSetAllocateInfo alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(descriptor_pool, &descriptor_set_layout, 1);

	VK_CHECK(vkAllocateDescriptorSets(get_device().get_handle(), &alloc_info, &descriptor_set));

	VkDescriptorBufferInfo ubo_descriptor = create_descriptor(*uniform_buffers.scene);

	const std::array<VkWriteDescriptorSet, 1> write_descriptor_sets = {
	    vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &ubo_descriptor)};

	vkUpdateDescriptorSets(get_device().get_handle(), static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, nullptr);
}

void TreeTp1::prepare_pipeline()
{
	VkPipelineInputAssemblyStateCreateInfo input_assembly =
	    vkb::initializers::pipeline_input_assembly_state_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterization_state =
	    vkb::initializers::pipeline_rasterization_state_create_info(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
	rasterization_state.lineWidth = 1.5f;

	VkPipelineColorBlendAttachmentState blend_attachment_state =
	    vkb::initializers::pipeline_color_blend_attachment_state(0xf, VK_TRUE);
	blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend_attachment_state.colorBlendOp        = VK_BLEND_OP_ADD;
	blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

	VkPipelineColorBlendStateCreateInfo color_blend_state =
	    vkb::initializers::pipeline_color_blend_state_create_info(1, &blend_attachment_state);

	VkPipelineDepthStencilStateCreateInfo depth_stencil_state =
	    vkb::initializers::pipeline_depth_stencil_state_create_info(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);

	VkPipelineViewportStateCreateInfo viewport_state =
	    vkb::initializers::pipeline_viewport_state_create_info(1, 1, 0);

	VkPipelineMultisampleStateCreateInfo multisample_state =
	    vkb::initializers::pipeline_multisample_state_create_info(VK_SAMPLE_COUNT_1_BIT, 0);

	const std::array<VkDynamicState, 2> dynamic_state_enables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo    dynamic_state         = vkb::initializers::pipeline_dynamic_state_create_info(dynamic_state_enables.data(),
                                                                                                                     static_cast<uint32_t>(dynamic_state_enables.size()),
                                                                                                                     0);

	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{};
	shader_stages[0] = load_shader("tree_tp1", "tree.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shader_stages[1] = load_shader("tree_tp1", "tree.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	const std::array<VkVertexInputBindingDescription, 1> vertex_input_bindings = {
	    vkb::initializers::vertex_input_binding_description(0, sizeof(SegmentVertex), VK_VERTEX_INPUT_RATE_VERTEX)};

	const std::array<VkVertexInputAttributeDescription, 6> vertex_input_attributes = {
	    vkb::initializers::vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SegmentVertex, position)),
	    vkb::initializers::vertex_input_attribute_description(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SegmentVertex, capsule_start)),
	    vkb::initializers::vertex_input_attribute_description(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SegmentVertex, capsule_end)),
	    vkb::initializers::vertex_input_attribute_description(0, 3, VK_FORMAT_R32_SFLOAT, offsetof(SegmentVertex, capsule_radius)),
	    vkb::initializers::vertex_input_attribute_description(0, 4, VK_FORMAT_R32_SFLOAT, offsetof(SegmentVertex, dataset_radius)),
	    vkb::initializers::vertex_input_attribute_description(0, 5, VK_FORMAT_R32_SFLOAT, offsetof(SegmentVertex, branch_depth_factor))};

	VkPipelineVertexInputStateCreateInfo vertex_input_state = vkb::initializers::pipeline_vertex_input_state_create_info();
	vertex_input_state.vertexBindingDescriptionCount        = static_cast<uint32_t>(vertex_input_bindings.size());
	vertex_input_state.pVertexBindingDescriptions           = vertex_input_bindings.data();
	vertex_input_state.vertexAttributeDescriptionCount      = static_cast<uint32_t>(vertex_input_attributes.size());
	vertex_input_state.pVertexAttributeDescriptions         = vertex_input_attributes.data();

	VkGraphicsPipelineCreateInfo pipeline_create_info =
	    vkb::initializers::pipeline_create_info(pipeline_layout, render_pass, 0);
	pipeline_create_info.pVertexInputState   = &vertex_input_state;
	pipeline_create_info.pInputAssemblyState = &input_assembly;
	pipeline_create_info.pRasterizationState = &rasterization_state;
	pipeline_create_info.pColorBlendState    = &color_blend_state;
	pipeline_create_info.pMultisampleState   = &multisample_state;
	pipeline_create_info.pViewportState      = &viewport_state;
	pipeline_create_info.pDepthStencilState  = &depth_stencil_state;
	pipeline_create_info.pDynamicState       = &dynamic_state;
	pipeline_create_info.stageCount          = static_cast<uint32_t>(shader_stages.size());
	pipeline_create_info.pStages             = shader_stages.data();

	VK_CHECK(vkCreateGraphicsPipelines(get_device().get_handle(), pipeline_cache, 1, &pipeline_create_info, nullptr, &pipeline));
}

void TreeTp1::build_command_buffers()
{
	// Os command buffers deixam o fluxo de desenho direto: bind do pipeline, vertex buffer
	// e descriptor set únicos, concluindo com apenas um vkCmdDraw por framebuffer.
	VkCommandBufferBeginInfo command_buffer_begin_info = vkb::initializers::command_buffer_begin_info();

	std::array<VkClearValue, 2> clear_values{};
	clear_values[0].color        = default_clear_color;
	clear_values[1].depthStencil = {1.0f, 0};

	VkRenderPassBeginInfo render_pass_begin_info    = vkb::initializers::render_pass_begin_info();
	render_pass_begin_info.renderPass               = render_pass;
	render_pass_begin_info.renderArea.offset.x      = 0;
	render_pass_begin_info.renderArea.offset.y      = 0;
	render_pass_begin_info.renderArea.extent.width  = width;
	render_pass_begin_info.renderArea.extent.height = height;
	render_pass_begin_info.clearValueCount          = static_cast<uint32_t>(clear_values.size());
	render_pass_begin_info.pClearValues             = clear_values.data();

	for (size_t i = 0; i < draw_cmd_buffers.size(); ++i)
	{
		render_pass_begin_info.framebuffer = framebuffers[i];

		VK_CHECK(vkBeginCommandBuffer(draw_cmd_buffers[i], &command_buffer_begin_info));

		vkCmdBeginRenderPass(draw_cmd_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = vkb::initializers::viewport(static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
		vkCmdSetViewport(draw_cmd_buffers[i], 0, 1, &viewport);

		VkRect2D scissor = vkb::initializers::rect2D(width, height, 0, 0);
		vkCmdSetScissor(draw_cmd_buffers[i], 0, 1, &scissor);

		if (pipeline != VK_NULL_HANDLE)
		{
			vkCmdBindPipeline(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		}

		if (vertex_buffer && vertex_count > 0)
		{
			const VkDeviceSize offsets[] = {0};
			VkBuffer           vb        = vertex_buffer->get_handle();
			vkCmdBindVertexBuffers(draw_cmd_buffers[i], 0, 1, &vb, offsets);
			vkCmdBindDescriptorSets(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
			vkCmdDraw(draw_cmd_buffers[i], vertex_count, 1, 0, 0);
		}

		draw_ui(draw_cmd_buffers[i]);

		vkCmdEndRenderPass(draw_cmd_buffers[i]);

		VK_CHECK(vkEndCommandBuffer(draw_cmd_buffers[i]));
	}
}

void TreeTp1::handle_animation(float delta_time)
{
	// Controla o playback dos passos VTK fornecidos pelo professor (stepXXXX),
	// percorrendo a lista com a taxa de quadros configurada e voltando ao início ao chegar no fim.
	if (!parameters.animate || current_frames.size() <= 1)
	{
		return;
	}

	const float speed = std::max(parameters.animation_speed, 0.1f);
	const float step  = 1.0f / speed;
	playback.accumulator += delta_time;

	while (playback.accumulator >= step)
	{
		playback.accumulator -= step;
		uint32_t next_index = playback.frame_index + 1;
		if (next_index >= current_frames.size())
		{
			next_index = 0;
		}
		load_frame(next_index);
	}
}

void TreeTp1::update_growth(float delta_time)
{
	// Atualiza a animação de crescimento por galho: usando "growth_speed" (segmentos/s) incrementa
	// `growth_progress` e reconstrói a malha sempre que um novo segmento deve aparecer.
	if (!parameters.growth_auto || !current_geometry.valid())
	{
		return;
	}

	if (parameters.growth_progress >= 1.0f)
	{
		return;
	}

	const size_t total_segments = current_geometry.render_order.size();
	if (total_segments == 0)
	{
		return;
	}

	const float segments_per_second = std::max(parameters.growth_speed, 0.01f);
	const float ratio_increment = (segments_per_second / static_cast<float>(total_segments)) * delta_time;
	parameters.growth_progress       = glm::min(parameters.growth_progress + ratio_increment, 1.0f);

	const float threshold = 1.0f / static_cast<float>(total_segments);
	if (parameters.growth_progress - last_growth_used >= threshold - 1e-5f)
	{
		rebuild_current_mesh();
	}
}

void TreeTp1::update_uniform_buffer()
{
	// Reescreve o único UBO com a matriz MVP atual e a faixa de raios do dataset,
	// garantindo que os shaders recalculam as cores mesmo quando a UI altera parâmetros.
	if (!uniform_buffers.scene)
	{
		return;
	}

	UniformBufferObject ubo{};

	const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
	glm::mat4           projection = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
	ubo.mvp                          = projection * build_model_matrix();

	const float range = std::max(0.0001f, current_geometry.max_radius - current_geometry.min_radius);
	ubo.radius_range               = glm::vec4(current_geometry.min_radius, current_geometry.max_radius, range, 0.0f);

	uniform_buffers.scene->update(&ubo, sizeof(UniformBufferObject));
}

glm::mat4 TreeTp1::build_model_matrix() const
{
	// Monta a matriz modelo composta:
	//   1. Aplica a translação definida pelo usuário
	//   2. Realiza rotações X/Y/Z para criar a sensação de profundidade
	//   3. Faz auto-scale para caber em [-1,1] e multiplica pela escala manual
	//   4. Recentraliza a árvore removendo o centro do bounding box
	glm::mat4 model(1.0f);

	const glm::vec2 center = 0.5f * (current_geometry.min_bounds + current_geometry.max_bounds);
	const float     auto_scale = compute_auto_scale();
	const float     manual     = parameters.scale;
	const float     final_scale = auto_scale * manual;

	model = glm::translate(model, glm::vec3(parameters.translation, 0.0f));
	model = glm::rotate(model, glm::radians(parameters.rotation_x_deg), glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, glm::radians(parameters.rotation_y_deg), glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::rotate(model, glm::radians(parameters.rotation_z_deg), glm::vec3(0.0f, 0.0f, 1.0f));
	model = glm::scale(model, glm::vec3(final_scale, final_scale, final_scale));
	model = glm::translate(model, glm::vec3(-center, 0.0f));

	return model;
}

void TreeTp1::draw()
{
	ApiVulkanSample::prepare_frame();

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers    = &draw_cmd_buffers[current_buffer];

	VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));

	ApiVulkanSample::submit_frame();
}

void TreeTp1::render(float delta_time)
{
	if (!prepared)
	{
		return;
	}

	handle_animation(delta_time);
	update_growth(delta_time);
	update_uniform_buffer();
	draw();
}

void TreeTp1::on_update_ui_overlay(vkb::Drawer &drawer)
{
	// O Drawer constrói a barra lateral com todos os controles exigidos.
	// Chamamos tudo a cada frame, mas só executamos trabalho pesado quando algum widget muda.
	if (!drawer.header("TP1 Controls"))
	{
		return;
	}

	const auto &defs = datasets();
	std::vector<std::string> dataset_labels;
	dataset_labels.reserve(defs.size());
	for (const auto &def : defs)
	{
		dataset_labels.push_back(def.label);
	}

	int selected_dataset = parameters.dataset_index;
	if (drawer.combo_box("Dataset", &selected_dataset, dataset_labels))
	{
		// Ao trocar o dataset precisamos reinicializar cache e geometria para manter tudo sincronizado.
		if (set_dataset(selected_dataset))
		{
			load_frame(0);
			playback.accumulator = 0.0f;
		}
	}

	if (!current_frames.empty())
	{
		int frame = static_cast<int>(playback.frame_index);
		if (drawer.slider_int("Frame", &frame, 0, static_cast<int>(current_frames.size()) - 1))
		{
			if (load_frame(static_cast<uint32_t>(frame)))
			{
				playback.accumulator = 0.0f;
			}
		}
	}

	if (drawer.checkbox("Animate", &parameters.animate))
	{
		// Zera o acumulador para evitar pulos de frame quando o usuário liga/desliga a animação.
		playback.accumulator = 0.0f;
	}

	drawer.slider_float("Speed (fps)", &parameters.animation_speed, 0.2f, 5.0f);

	bool   growth_changed   = false;
	size_t segment_count    = current_geometry.render_order.size();
	if (segment_count > 0)
	{
		int visible_segments = static_cast<int>(std::round(parameters.growth_progress * static_cast<float>(segment_count)));
		visible_segments     = glm::clamp(visible_segments, 0, static_cast<int>(segment_count));
		if (drawer.slider_int("Galhos visíveis", &visible_segments, 0, static_cast<int>(segment_count)))
		{
			parameters.growth_progress = segment_count > 0 ? static_cast<float>(visible_segments) / static_cast<float>(segment_count) : 1.0f;
			growth_changed             = true;
		}
	}

	if (drawer.checkbox("Animate growth", &parameters.growth_auto))
	{
		if (parameters.growth_auto)
		{
			parameters.growth_progress = 0.0f;
			growth_changed             = true;
		}
	}
	if (parameters.growth_auto)
	{
		if (drawer.slider_float("Growth speed (galhos/s)", &parameters.growth_speed, 0.05f, 3.0f))
		{
			parameters.growth_speed = std::max(0.01f, parameters.growth_speed);
		}
	}
	if (growth_changed)
	{
		rebuild_current_mesh();
	}

	static const std::vector<std::string> radius_modes = {"Raio fixo", "Raio variável"};
	int                                   mode_index   = parameters.use_dataset_radius ? 1 : 0;
	if (drawer.combo_box("Modo de raio", &mode_index, radius_modes))
	{
		parameters.use_dataset_radius = (mode_index == 1);
		rebuild_current_mesh();
	}

	if (parameters.use_dataset_radius)
	{
		bool changed = false;
		changed |= drawer.slider_float("Raio mínimo", &parameters.dataset_min_thickness, 0.002f, 0.05f);
		changed |= drawer.slider_float("Raio máximo", &parameters.dataset_max_thickness, 0.01f, 0.08f);
		if (changed)
		{
			rebuild_current_mesh();
		}
	}
	else
	{
		if (drawer.slider_float("Raio fixo", &parameters.fixed_radius, 0.001f, 0.2f))
		{
			rebuild_current_mesh();
		}
	}

	if (drawer.slider_float("Scale", &parameters.scale, 0.25f, 4.0f) ||
	    drawer.slider_float("Rotação X", &parameters.rotation_x_deg, -85.0f, 85.0f) ||
	    drawer.slider_float("Rotação Z", &parameters.rotation_z_deg, -180.0f, 180.0f) ||
	    drawer.slider_float("Rotação Y", &parameters.rotation_y_deg, -89.0f, 89.0f))
	{
		// Sliders de transformação afetam apenas o UBO, então evitamos reconstruir a geometria.
		update_uniform_buffer();
	}

	bool translate_changed = false;
	translate_changed |= drawer.slider_float("Translate X", &parameters.translation.x, -2.0f, 2.0f);
	translate_changed |= drawer.slider_float("Translate Y", &parameters.translation.y, -2.0f, 2.0f);

	if (translate_changed)
	{
		update_uniform_buffer();
	}
}

bool TreeTp1::resize(uint32_t new_width, uint32_t new_height)
{
	if (!ApiVulkanSample::resize(new_width, new_height))
	{
		return false;
	}

	rebuild_command_buffers();
	update_uniform_buffer();
	return true;
}

bool TreeTp1::set_dataset(int dataset_index)
{
	// Seleciona um grupo de arquivos (Nterm 64/128/256) e reinicia o estado de reprodução.
	const auto &defs = datasets();
	if (dataset_index < 0 || dataset_index >= static_cast<int>(defs.size()))
	{
		return false;
	}

	parameters.dataset_index = dataset_index;
	current_frames           = defs[dataset_index].files;
	frame_cache.clear();
	playback.frame_index = 0;
	playback.accumulator = 0.0f;
	parameters.growth_progress = parameters.growth_auto ? 0.0f : 1.0f;
	last_growth_used           = -1.0f;

	return !current_frames.empty();
}

bool TreeTp1::load_frame(uint32_t frame_index)
{
	// Carrega ou parseia o frame solicitado, atualiza `current_geometry` e reconstrói a malha.
	if (current_frames.empty())
	{
		return false;
	}

	frame_index = std::min<uint32_t>(frame_index, static_cast<uint32_t>(current_frames.size() - 1));
	const std::string &relative_path = current_frames[frame_index];

	auto cache_it = frame_cache.find(relative_path);
	if (cache_it == frame_cache.end())
	{
		FrameGeometry parsed = parse_vtk_file(relative_path);
		if (!parsed.valid())
		{
			LOGE("Falha ao ler arquivo VTK: %s", relative_path.c_str());
			return false;
		}
		cache_it = frame_cache.emplace(relative_path, std::move(parsed)).first;
	}

	current_geometry = cache_it->second;
	playback.frame_index = frame_index;
	last_growth_used     = -1.0f;

	if (!rebuild_current_mesh())
	{
		return false;
	}

	update_uniform_buffer();
	return true;
}

bool TreeTp1::rebuild_current_mesh()
{
	// Converte os dados parseados em vértices para a GPU e refaz os command buffers.
	std::vector<SegmentVertex> vertices;
	if (!build_vertices_from_segments(current_geometry, vertices, parameters.growth_progress))
	{
		vertex_buffer.reset();
		vertex_count = 0;
		return false;
	}

	if (!upload_geometry(vertices))
	{
		return false;
	}

	if (prepared)
	{
		rebuild_command_buffers();
	}

	last_growth_used = parameters.growth_progress;
	return true;
}

// Converte os dados do parser em cápsulas desenháveis. Também respeita o número
// de galhos visíveis (growth_progress) para que a animação adicione um segmento por vez.
bool TreeTp1::build_vertices_from_segments(const FrameGeometry &geometry, std::vector<SegmentVertex> &out_vertices, float growth_progress) const
{
	// Emite dois triângulos por segmento de galho. O CPU calcula tangentes, raios e atributos
	// de shading para que os shaders lidem apenas com projeção e avaliação do SDF.
	if (!geometry.valid())
	{
		return false;
	}

	out_vertices.clear();
	if (geometry.segments.empty())
	{
		return false;
	}

	out_vertices.reserve(geometry.segments.size() * 6);

	const std::vector<size_t> *order_ptr = &geometry.render_order;
	std::vector<size_t> fallback_order;
	if (order_ptr->empty())
	{
		fallback_order.resize(geometry.segments.size());
		std::iota(fallback_order.begin(), fallback_order.end(), 0);
		order_ptr = &fallback_order;
	}

	size_t total_segments = order_ptr->size();
	float  clamped_growth = glm::clamp(growth_progress, 0.0f, 1.0f);
	float  scaled         = clamped_growth * static_cast<float>(total_segments);
	size_t visible_segments =
	    std::min(total_segments, static_cast<size_t>(std::floor(scaled + 1e-4f)));
	if (clamped_growth >= 1.0f)
	{
		visible_segments = total_segments;
	}
	if (visible_segments == 0)
	{
		return true;
	}

	const float auto_scale     = compute_auto_scale();
	const float inv_auto_scale = auto_scale > std::numeric_limits<float>::epsilon() ? 1.0f / auto_scale : 1.0f;

	const float min_thickness_ui = std::min(parameters.dataset_min_thickness, parameters.dataset_max_thickness);
	const float max_thickness_ui = std::max(parameters.dataset_min_thickness, parameters.dataset_max_thickness);
	float       min_thickness    = std::max(min_thickness_ui * inv_auto_scale, 0.00015f);
	float       max_thickness    = std::max(max_thickness_ui * inv_auto_scale, min_thickness + 0.0002f);
	const float fixed_radius     = std::max(parameters.fixed_radius * inv_auto_scale, 0.00015f);
	const float radius_range     = std::max(geometry.max_radius - geometry.min_radius, 0.0001f);
	const float min_radius_dataset = geometry.min_radius;
	const float depth_bias = 0.0f;
	const float depth_range = 0.0f;

	for (size_t ordered_index = 0; ordered_index < visible_segments; ++ordered_index)
	{
		const SegmentData &segment = geometry.segments[(*order_ptr)[ordered_index]];

		const glm::vec2 segment_vector = segment.end_point - segment.start_point;
		const float     planar_length  = glm::length(segment_vector);
		if (planar_length <= std::numeric_limits<float>::epsilon())
		{
			continue;
		}

		const glm::vec2 normalized_direction = segment_vector / planar_length;
		glm::vec3       tangent_vector       = glm::normalize(glm::vec3(normalized_direction.x, normalized_direction.y, 0.1f));
		glm::vec3       bitangent_vector     = glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), tangent_vector);
		if (glm::dot(bitangent_vector, bitangent_vector) < 1e-5f)
		{
			bitangent_vector = glm::vec3(0.0f, 1.0f, 0.0f);
		}
		bitangent_vector = glm::normalize(bitangent_vector);

		glm::vec3 planar_extension(normalized_direction.x, normalized_direction.y, 0.0f);
		if (glm::dot(planar_extension, planar_extension) < 1e-5f)
		{
			planar_extension = glm::vec3(1.0f, 0.0f, 0.0f);
		}
		planar_extension = glm::normalize(planar_extension);

		const float dataset_factor = glm::clamp((segment.dataset_radius - min_radius_dataset) / radius_range, 0.0f, 1.0f);

		float capsule_radius = fixed_radius;
		if (parameters.use_dataset_radius)
		{
			const float branch_factor   = glm::clamp(segment.depth_factor, 0.0f, 1.0f);
			float       combined_factor = glm::clamp(branch_factor * 0.65f + dataset_factor * 0.35f, 0.0f, 1.0f);
			combined_factor             = glm::mix(combined_factor, combined_factor * combined_factor, 0.45f);
			capsule_radius              = glm::mix(min_thickness, max_thickness, combined_factor);
			const float tip_taper       = glm::mix(0.55f, 1.0f, branch_factor);
			capsule_radius *= tip_taper;
		}
		capsule_radius = std::max(capsule_radius, 0.0001f);

		const float depth_value    = depth_bias + dataset_factor * depth_range;
		const float depth_variance = 0.0f;
		glm::vec3   start_position(segment.start_point.x, segment.start_point.y, depth_value);
		glm::vec3   end_position(segment.end_point.x, segment.end_point.y, depth_value + depth_variance);

		const glm::vec3 radial_offset = bitangent_vector * capsule_radius;
		const glm::vec3 axial_offset  = planar_extension * capsule_radius;

		const glm::vec3 start_capsule = start_position - axial_offset;
		const glm::vec3 end_capsule   = end_position + axial_offset;

		auto emit_vertex = [&](const glm::vec3 &position) {
			SegmentVertex vertex{};
			vertex.position             = position;
			vertex.capsule_start        = start_position;
			vertex.capsule_end          = end_position;
			vertex.capsule_radius       = capsule_radius;
			vertex.dataset_radius       = segment.dataset_radius;
			vertex.branch_depth_factor  = segment.depth_factor;
			out_vertices.push_back(vertex);
		};

		emit_vertex(start_capsule + radial_offset);
		emit_vertex(start_capsule - radial_offset);
		emit_vertex(end_capsule + radial_offset);
		emit_vertex(start_capsule - radial_offset);
		emit_vertex(end_capsule - radial_offset);
		emit_vertex(end_capsule + radial_offset);
	}

	return !out_vertices.empty();
}

bool TreeTp1::upload_geometry(const std::vector<SegmentVertex> &vertices)
{
	// Cria um vertex buffer mapeável (CPU→GPU) e copia todo o vetor de vértices para ele.
	vertex_count = static_cast<uint32_t>(vertices.size());
	if (vertex_count == 0)
	{
		vertex_buffer.reset();
		return false;
	}

	const VkDeviceSize buffer_size = static_cast<VkDeviceSize>(vertex_count * sizeof(SegmentVertex));

	vertex_buffer = std::make_unique<vkb::core::BufferC>(get_device(),
	                                                     buffer_size,
	                                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	                                                     VMA_MEMORY_USAGE_CPU_TO_GPU);

	vertex_buffer->update(vertices.data(), buffer_size);
	return true;
}

TreeTp1::FrameGeometry TreeTp1::parse_vtk_file(const std::string &relative_path) const
{
	// Parser completo de VTK: lê POINTS, LINES e CELL_DATA; converte cada linha em SegmentData
	// e executa Dijkstra no grafo de conectividade para estimar profundidade (usada nas animações e taper).
	FrameGeometry geometry{};

	const std::string absolute_path = vkb::fs::path::get(vkb::fs::path::Assets, relative_path);
	std::ifstream     stream(absolute_path, std::ios::in);
	if (!stream)
	{
		LOGE("Não foi possível abrir o arquivo: %s", absolute_path.c_str());
		return geometry;
	}

	struct SegmentIndices
	{
		uint32_t a = 0;
		uint32_t b = 0;
	};

	std::vector<glm::vec2> points;
	std::vector<SegmentIndices> segments;
	std::vector<float> radii;

	std::string token;
	while (stream >> token)
	{
		if (token == "POINTS")
		{
			uint32_t count = 0;
			std::string type;
			stream >> count >> type;
			points.resize(count);
			for (uint32_t i = 0; i < count; ++i)
			{
				float x = 0.0f, y = 0.0f, z = 0.0f;
				stream >> x >> y >> z;
				points[i] = {x, y};
			}
		}
		else if (token == "LINES")
		{
			uint32_t segment_count = 0;
			uint32_t total_entries = 0;
			stream >> segment_count >> total_entries;
			segments.reserve(segment_count);
			for (uint32_t i = 0; i < segment_count; ++i)
			{
				uint32_t vertex_count = 0;
				stream >> vertex_count;
				if (vertex_count != 2)
				{
					for (uint32_t j = 0; j < vertex_count; ++j)
					{
						uint32_t skip;
						stream >> skip;
					}
					continue;
				}

				SegmentIndices indices{};
				stream >> indices.a >> indices.b;
				segments.push_back(indices);
			}
		}
		else if (token == "CELL_DATA")
		{
			uint32_t cell_count = 0;
			stream >> cell_count;

			std::string scalars;
			std::string name;
			std::string type;
			stream >> scalars >> name >> type;

			std::string lookup_key;
			std::string lookup_value;
			stream >> lookup_key >> lookup_value;

			radii.resize(cell_count);
			for (uint32_t i = 0; i < cell_count; ++i)
			{
				stream >> radii[i];
			}
		}
	}

	if (segments.empty() || points.empty())
	{
		return geometry;
	}

	if (radii.size() != segments.size())
	{
		LOGW("Quantidade de raios diferente dos segmentos em %s", relative_path.c_str());
		radii.resize(segments.size(), radii.empty() ? 0.0f : radii.back());
	}

	std::vector<std::vector<std::pair<uint32_t, float>>> adjacency(points.size());
	for (size_t i = 0; i < segments.size(); ++i)
	{
		const SegmentIndices &segment = segments[i];
		if (segment.a >= points.size() || segment.b >= points.size())
		{
			continue;
		}
		const glm::vec2 &a = points[segment.a];
		const glm::vec2 &b = points[segment.b];
		float length = glm::length(a - b);
		if (length <= std::numeric_limits<float>::epsilon())
		{
			length = 0.0001f;
		}
		adjacency[segment.a].emplace_back(segment.b, length);
		adjacency[segment.b].emplace_back(segment.a, length);
	}

	std::vector<float> distances(points.size(), std::numeric_limits<float>::max());
	if (!points.empty() && !segments.empty())
	{
		uint32_t root_index = segments[0].a < points.size() ? segments[0].a : 0;
		distances[root_index] = 0.0f;

		using Node = std::pair<float, uint32_t>;
		auto compare = [](const Node &lhs, const Node &rhs) { return lhs.first > rhs.first; };
		std::priority_queue<Node, std::vector<Node>, decltype(compare)> pq(compare);
		pq.emplace(0.0f, root_index);

		while (!pq.empty())
		{
			auto [current_distance, vertex] = pq.top();
			pq.pop();

			if (current_distance > distances[vertex])
			{
				continue;
			}

			for (const auto &[neighbor, weight] : adjacency[vertex])
			{
				if (weight <= 0.0f)
				{
					continue;
				}

				float candidate = current_distance + weight;
				if (candidate < distances[neighbor])
				{
					distances[neighbor] = candidate;
					pq.emplace(candidate, neighbor);
				}
			}
		}
	}

	float max_distance = 0.0f;
	for (float d : distances)
	{
		if (d < std::numeric_limits<float>::max())
		{
			max_distance = std::max(max_distance, d);
		}
	}
	if (max_distance <= 0.0f)
	{
		max_distance = 1.0f;
	}

	glm::vec2 min_bounds(std::numeric_limits<float>::max());
	glm::vec2 max_bounds(std::numeric_limits<float>::lowest());
	float     min_radius = std::numeric_limits<float>::max();
	float     max_radius = std::numeric_limits<float>::lowest();

	for (size_t i = 0; i < segments.size(); ++i)
	{
		const SegmentIndices &segment = segments[i];
		if (segment.a >= points.size() || segment.b >= points.size())
		{
			continue;
		}

		const glm::vec2 &a      = points[segment.a];
		const glm::vec2 &b      = points[segment.b];
		const float      radius = radii[i];

		float depth_a = segment.a < distances.size() ? distances[segment.a] : std::numeric_limits<float>::max();
		float depth_b = segment.b < distances.size() ? distances[segment.b] : std::numeric_limits<float>::max();
		float depth   = std::min(depth_a, depth_b);
		if (depth == std::numeric_limits<float>::max())
		{
			depth = max_distance;
		}
		float depth_factor = 1.0f - (depth / max_distance);
		depth_factor       = glm::clamp(depth_factor, 0.0f, 1.0f);

		SegmentData data{};
		data.start_point  = a;
		data.end_point    = b;
		data.dataset_radius = radius;
		data.depth_factor = depth_factor;
		geometry.segments.push_back(data);

		min_bounds.x = std::min(min_bounds.x, std::min(a.x, b.x));
		min_bounds.y = std::min(min_bounds.y, std::min(a.y, b.y));
		max_bounds.x = std::max(max_bounds.x, std::max(a.x, b.x));
		max_bounds.y = std::max(max_bounds.y, std::max(a.y, b.y));

		min_radius = std::min(min_radius, radius);
		max_radius = std::max(max_radius, radius);
	}

	if (!geometry.segments.empty())
	{
		geometry.min_bounds = min_bounds;
		geometry.max_bounds = max_bounds;
		geometry.min_radius = min_radius;
		geometry.max_radius = max_radius;
	}

	geometry.render_order.resize(geometry.segments.size());
	std::iota(geometry.render_order.begin(), geometry.render_order.end(), 0);
	std::stable_sort(geometry.render_order.begin(), geometry.render_order.end(),
	                 [&](size_t lhs, size_t rhs) {
		                 const SegmentData &a_seg = geometry.segments[lhs];
		                 const SegmentData &b_seg = geometry.segments[rhs];
		                 if (a_seg.depth_factor == b_seg.depth_factor)
		                 {
			                 return a_seg.dataset_radius > b_seg.dataset_radius;
		                 }
		                 return a_seg.depth_factor > b_seg.depth_factor;
	                 });

	return geometry;
}

float TreeTp1::compute_auto_scale() const
{
	if (!current_geometry.valid())
	{
		return 1.0f;
	}

	const glm::vec2 extent = current_geometry.max_bounds - current_geometry.min_bounds;
	const float     max_extent = std::max(std::max(std::abs(extent.x), std::abs(extent.y)), 0.0001f);
	return 2.0f / max_extent;
}

std::unique_ptr<vkb::Application> create_tree_tp1()
{
	return std::make_unique<TreeTp1>();
}
