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

#include "cco_tree_3d.h"

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

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>

namespace
{
struct DatasetDefinition
{
	std::string              label;
	std::vector<std::string> files;
};

std::string resolve_dataset_root()
{
	const std::string preferred = "TP_CCO_Pacote_Dados/TP2_3D/";
	const std::string fallback  = "TP_CCO_Pacote_Dados/TP1_3D/";

	const std::string preferred_abs = vkb::fs::path::get(vkb::fs::path::Assets, preferred);
	if (vkb::fs::is_directory(preferred_abs))
	{
		return preferred;
	}

	static bool logged = false;
	if (!logged)
	{
		LOGW("Pasta TP2_3D não encontrada. Usando assets em %s", fallback.c_str());
		logged = true;
	}

	return fallback;
}

DatasetDefinition make_dataset(const std::string &label, const std::string &folder, std::initializer_list<const char *> entries)
{
	DatasetDefinition definition{};
	definition.label = label;
	definition.files.reserve(entries.size());

	const std::string base = resolve_dataset_root() + folder + "/";
	for (const char *entry : entries)
	{
		definition.files.emplace_back(base + entry);
	}
	return definition;
}

const std::array<DatasetDefinition, 3> &datasets()
{
	static const std::array<DatasetDefinition, 3> definitions = {
	    make_dataset("Nterm 128", "Nterm_128",
	                 {"tree3D_Nterm0128_step0016.vtk",
	                  "tree3D_Nterm0128_step0032.vtk",
	                  "tree3D_Nterm0128_step0048.vtk",
	                  "tree3D_Nterm0128_step0064.vtk",
	                  "tree3D_Nterm0128_step0080.vtk",
	                  "tree3D_Nterm0128_step0096.vtk",
	                  "tree3D_Nterm0128_step0112.vtk",
	                  "tree3D_Nterm0128_step0128.vtk"}),
	    make_dataset("Nterm 256", "Nterm_256",
	                 {"tree3D_Nterm0256_step0032.vtk",
	                  "tree3D_Nterm0256_step0064.vtk",
	                  "tree3D_Nterm0256_step0096.vtk",
	                  "tree3D_Nterm0256_step0128.vtk",
	                  "tree3D_Nterm0256_step0160.vtk",
	                  "tree3D_Nterm0256_step0192.vtk",
	                  "tree3D_Nterm0256_step0224.vtk",
	                  "tree3D_Nterm0256_step0256.vtk"}),
	    make_dataset("Nterm 512", "Nterm_512",
	                 {"tree3D_Nterm0512_step0064.vtk",
	                  "tree3D_Nterm0512_step0128.vtk",
	                  "tree3D_Nterm0512_step0192.vtk",
	                  "tree3D_Nterm0512_step0256.vtk",
	                  "tree3D_Nterm0512_step0320.vtk",
	                  "tree3D_Nterm0512_step0384.vtk",
	                  "tree3D_Nterm0512_step0448.vtk",
	                  "tree3D_Nterm0512_step0512.vtk"})};
	return definitions;
}
}        // namespace

CcoTree3d::CcoTree3d()
{
	title = "TP2 - Arterial Tree 3D";
	camera.type = vkb::CameraType::LookAt;
	camera.set_position({0.0f, 0.0f, -3.25f});
	camera.set_rotation({0.0f, 0.0f, 0.0f});
	camera.set_perspective(60.0f, 1.0f, 0.05f, 256.0f);
	rotation_speed = 0.8f;
	zoom_speed     = 1.2f;
}

CcoTree3d::~CcoTree3d()
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

bool CcoTree3d::prepare(const vkb::ApplicationOptions &options)
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

void CcoTree3d::create_uniform_buffers()
{
	uniform_buffers.scene = std::make_unique<vkb::core::BufferC>(get_device(),
	                                                             sizeof(UniformBufferObject),
	                                                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	                                                             VMA_MEMORY_USAGE_CPU_TO_GPU);
}

void CcoTree3d::setup_descriptor_pool()
{
	const std::vector<VkDescriptorPoolSize> pool_sizes = {
	    vkb::initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)};

	VkDescriptorPoolCreateInfo pool_info =
	    vkb::initializers::descriptor_pool_create_info(pool_sizes, 1);

	VK_CHECK(vkCreateDescriptorPool(get_device().get_handle(), &pool_info, nullptr, &descriptor_pool));
}

void CcoTree3d::setup_descriptor_set_layout()
{
	const std::array<VkDescriptorSetLayoutBinding, 1> set_layout_bindings = {
	    vkb::initializers::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	                                                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0)};

	VkDescriptorSetLayoutCreateInfo descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(set_layout_bindings.data(), static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(get_device().get_handle(), &descriptor_layout, nullptr, &descriptor_set_layout));

	VkPipelineLayoutCreateInfo pipeline_layout_info =
	    vkb::initializers::pipeline_layout_create_info(&descriptor_set_layout, 1);

	VK_CHECK(vkCreatePipelineLayout(get_device().get_handle(), &pipeline_layout_info, nullptr, &pipeline_layout));
}

void CcoTree3d::setup_descriptor_set()
{
	VkDescriptorSetAllocateInfo alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(descriptor_pool, &descriptor_set_layout, 1);

	VK_CHECK(vkAllocateDescriptorSets(get_device().get_handle(), &alloc_info, &descriptor_set));

	VkDescriptorBufferInfo ubo_descriptor = create_descriptor(*uniform_buffers.scene);

	const std::array<VkWriteDescriptorSet, 1> write_descriptor_sets = {
	    vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &ubo_descriptor)};

	vkUpdateDescriptorSets(get_device().get_handle(), static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, nullptr);
}

void CcoTree3d::prepare_pipeline()
{
	VkPipelineInputAssemblyStateCreateInfo input_assembly =
	    vkb::initializers::pipeline_input_assembly_state_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterization_state =
	    vkb::initializers::pipeline_rasterization_state_create_info(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);

	VkPipelineColorBlendAttachmentState blend_attachment_state =
	    vkb::initializers::pipeline_color_blend_attachment_state(0xf, VK_FALSE);

	VkPipelineColorBlendStateCreateInfo color_blend_state =
	    vkb::initializers::pipeline_color_blend_state_create_info(1, &blend_attachment_state);

	VkPipelineDepthStencilStateCreateInfo depth_stencil_state =
	    vkb::initializers::pipeline_depth_stencil_state_create_info(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

	VkPipelineViewportStateCreateInfo viewport_state =
	    vkb::initializers::pipeline_viewport_state_create_info(1, 1, 0);

	VkPipelineMultisampleStateCreateInfo multisample_state =
	    vkb::initializers::pipeline_multisample_state_create_info(VK_SAMPLE_COUNT_1_BIT, 0);

	const std::array<VkDynamicState, 2> dynamic_state_enables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo    dynamic_state         = vkb::initializers::pipeline_dynamic_state_create_info(dynamic_state_enables.data(),
	                                                                                                                 static_cast<uint32_t>(dynamic_state_enables.size()),
	                                                                                                                 0);

	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{};
	shader_stages[0] = load_shader("cco_tree_3d", "tree3d.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shader_stages[1] = load_shader("cco_tree_3d", "tree3d.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	const std::array<VkVertexInputBindingDescription, 1> vertex_input_bindings = {
	    vkb::initializers::vertex_input_binding_description(0, sizeof(TreeVertex), VK_VERTEX_INPUT_RATE_VERTEX)};

	const std::array<VkVertexInputAttributeDescription, 5> vertex_input_attributes = {
	    vkb::initializers::vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TreeVertex, position)),
	    vkb::initializers::vertex_input_attribute_description(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TreeVertex, normal)),
	    vkb::initializers::vertex_input_attribute_description(0, 2, VK_FORMAT_R32_SFLOAT, offsetof(TreeVertex, dataset_radius)),
	    vkb::initializers::vertex_input_attribute_description(0, 3, VK_FORMAT_R32_SFLOAT, offsetof(TreeVertex, depth_factor)),
	    vkb::initializers::vertex_input_attribute_description(0, 4, VK_FORMAT_R32_SFLOAT, offsetof(TreeVertex, segment_id))};

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

void CcoTree3d::build_command_buffers()
{
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

		if (vertex_buffer && index_buffer && index_count > 0)
		{
			const VkDeviceSize offsets[] = {0};
			VkBuffer           vb        = vertex_buffer->get_handle();
			VkBuffer           ib        = index_buffer->get_handle();

			vkCmdBindVertexBuffers(draw_cmd_buffers[i], 0, 1, &vb, offsets);
			vkCmdBindIndexBuffer(draw_cmd_buffers[i], ib, 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindDescriptorSets(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
			vkCmdDrawIndexed(draw_cmd_buffers[i], index_count, 1, 0, 0, 0);
		}

		draw_ui(draw_cmd_buffers[i]);

		vkCmdEndRenderPass(draw_cmd_buffers[i]);

		VK_CHECK(vkEndCommandBuffer(draw_cmd_buffers[i]));
	}
}

void CcoTree3d::draw()
{
	ApiVulkanSample::prepare_frame();

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers    = &draw_cmd_buffers[current_buffer];

	VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));

	ApiVulkanSample::submit_frame();
}

void CcoTree3d::render(float delta_time)
{
	if (!prepared)
	{
		return;
	}

	handle_animation(delta_time);
	update_growth(delta_time);
	if (parameters.orbit_enabled)
	{
		orbit_time += delta_time;
	}
	update_uniform_buffer();
	draw();
}

void CcoTree3d::on_update_ui_overlay(vkb::Drawer &drawer)
{
	if (!drawer.header("TP2 Controls"))
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
		playback.accumulator = 0.0f;
	}

	drawer.slider_float("Speed (fps)", &parameters.animation_speed, 0.2f, 5.0f);

	if (drawer.header("Camera"))
	{
		if (drawer.checkbox("Auto orbit", &parameters.orbit_enabled))
		{
			orbit_time = 0.0f;
		}
		drawer.slider_float("Orbit radius", &parameters.orbit_radius, 1.0f, 10.0f);
		drawer.slider_float("Orbit speed (rad/s)", &parameters.orbit_speed, 0.05f, 2.0f);
	}

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

	static const std::vector<std::string> radius_modes = {"Raio fixo", "Raio variável"};
	int                                   mode_index   = parameters.use_dataset_radius ? 1 : 0;
	if (drawer.combo_box("Modo de raio", &mode_index, radius_modes))
	{
		parameters.use_dataset_radius = (mode_index == 1);
		growth_changed                = true;
	}

	if (parameters.use_dataset_radius)
	{
		bool changed = false;
		changed |= drawer.slider_float("Raio mínimo", &parameters.dataset_min_thickness, 0.001f, 0.05f);
		changed |= drawer.slider_float("Raio máximo", &parameters.dataset_max_thickness, 0.005f, 0.08f);
		if (changed)
		{
			growth_changed = true;
		}
	}
	else
	{
		if (drawer.slider_float("Raio fixo", &parameters.fixed_radius, 0.001f, 0.05f))
		{
			growth_changed = true;
		}
	}

	static const std::vector<std::string> lighting_modes = {"Gouraud", "Phong"};
	int                                   lighting_index = parameters.lighting_model;
	if (drawer.combo_box("Iluminação", &lighting_index, lighting_modes))
	{
		parameters.lighting_model = glm::clamp(lighting_index, 0, static_cast<int>(lighting_modes.size() - 1));
	}

	int sides = parameters.sides;
	if (drawer.slider_int("Detalhe cilindro", &sides, 6, 32))
	{
		parameters.sides = glm::clamp(sides, 6, 64);
		growth_changed  = true;
	}

	if (drawer.slider_float("Scale", &parameters.scale, 0.25f, 4.0f) ||
	    drawer.slider_float("Translate X", &parameters.translation.x, -2.0f, 2.0f) ||
	    drawer.slider_float("Translate Y", &parameters.translation.y, -2.0f, 2.0f) ||
	    drawer.slider_float("Translate Z", &parameters.translation.z, -2.0f, 2.0f))
	{
		update_uniform_buffer();
	}

	if (growth_changed)
	{
		rebuild_current_mesh();
	}

	drawer.text("Camera: arraste com botão esquerdo para orbitar, direito para zoom.");

	if (selected_segment_index >= 0 && selected_segment_index < static_cast<int>(current_geometry.segments.size()))
	{
		const SegmentData &seg = current_geometry.segments[static_cast<size_t>(selected_segment_index)];
		drawer.text("Segmento selecionado:");
		drawer.text("Id: %u", seg.id);
		drawer.text("Raio: %.5f", seg.dataset_radius);
		drawer.text("Comprimento: %.5f", seg.length);
	}
	else
	{
		drawer.text("Segmento selecionado: nenhum");
	}
}

bool CcoTree3d::resize(uint32_t new_width, uint32_t new_height)
{
	if (!ApiVulkanSample::resize(new_width, new_height))
	{
		return false;
	}

	rebuild_command_buffers();
	update_uniform_buffer();
	return true;
}

void CcoTree3d::input_event(const vkb::InputEvent &input_event)
{
	ApiVulkanSample::input_event(input_event);

	if (input_event.get_source() == vkb::EventSource::Mouse)
	{
		const auto &mouse_button = static_cast<const vkb::MouseButtonInputEvent &>(input_event);
		if (mouse_button.get_button() == vkb::MouseButton::Left && mouse_button.get_action() == vkb::MouseAction::Down)
		{
			bool gui_captures = false;
			if (has_gui())
			{
				ImGuiIO &io = ImGui::GetIO();
				gui_captures = io.WantCaptureMouse;
			}
			if (!gui_captures)
			{
				selected_segment_index = pick_segment(static_cast<int32_t>(mouse_button.get_pos_x()),
				                                      static_cast<int32_t>(mouse_button.get_pos_y()));
				update_uniform_buffer();
			}
		}
	}
}

bool CcoTree3d::set_dataset(int dataset_index)
{
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

bool CcoTree3d::load_frame(uint32_t frame_index)
{
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
	selected_segment_index = -1;

	if (!rebuild_current_mesh())
	{
		return false;
	}

	update_uniform_buffer();
	return true;
}

bool CcoTree3d::rebuild_current_mesh()
{
	std::vector<TreeVertex> vertices;
	std::vector<uint32_t>   indices;
	std::vector<SegmentRange> ranges;
	if (!build_mesh_from_segments(current_geometry, vertices, indices, parameters.growth_progress, ranges))
	{
		vertex_buffer.reset();
		index_buffer.reset();
		index_count = 0;
		segment_ranges.clear();
		return false;
	}

	if (vertices.empty() || indices.empty())
	{
		vertex_buffer.reset();
		index_buffer.reset();
		index_count = 0;
		segment_ranges.clear();
		if (prepared)
		{
			rebuild_command_buffers();
		}
		last_growth_used = parameters.growth_progress;
		return true;
	}

	if (!upload_geometry(vertices, indices))
	{
		return false;
	}

	segment_ranges = std::move(ranges);

	if (prepared)
	{
		rebuild_command_buffers();
	}

	last_growth_used = parameters.growth_progress;
	return true;
}

bool CcoTree3d::build_mesh_from_segments(const FrameGeometry &geometry, std::vector<TreeVertex> &out_vertices,
                                         std::vector<uint32_t> &out_indices, float growth_progress,
                                         std::vector<SegmentRange> &out_ranges) const
{
	if (!geometry.valid())
	{
		return false;
	}

	out_vertices.clear();
	out_indices.clear();
	out_ranges.clear();

	if (geometry.segments.empty())
	{
		return true;
	}

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
	float       min_thickness    = std::max(min_thickness_ui * inv_auto_scale, 0.0001f);
	float       max_thickness    = std::max(max_thickness_ui * inv_auto_scale, min_thickness + 0.0001f);
	const float fixed_radius     = std::max(parameters.fixed_radius * inv_auto_scale, 0.0001f);
	const float radius_range     = std::max(geometry.max_radius - geometry.min_radius, 0.0001f);
	const float min_radius_dataset = geometry.min_radius;

	const int sides = glm::clamp(parameters.sides, 6, 64);
	const float step = glm::two_pi<float>() / static_cast<float>(sides);

	out_vertices.reserve(visible_segments * (sides * 2 + 2));
	out_indices.reserve(visible_segments * (sides * 6 + sides * 6));
	out_ranges.reserve(visible_segments);

	for (size_t ordered_index = 0; ordered_index < visible_segments; ++ordered_index)
	{
		const SegmentData &segment = geometry.segments[(*order_ptr)[ordered_index]];

		glm::vec3 a = segment.start_point;
		glm::vec3 b = segment.end_point;
		glm::vec3 axis = b - a;
		float length = glm::length(axis);
		if (length <= std::numeric_limits<float>::epsilon())
		{
			continue;
		}
		axis /= length;

		glm::vec3 up = std::abs(axis.z) < 0.95f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
		glm::vec3 tangent = glm::normalize(glm::cross(up, axis));
		glm::vec3 bitangent = glm::normalize(glm::cross(axis, tangent));

		float radius = fixed_radius;
		if (parameters.use_dataset_radius)
		{
			float normalized = (segment.dataset_radius - min_radius_dataset) / radius_range;
			normalized = glm::clamp(normalized, 0.0f, 1.0f);
			radius = glm::mix(min_thickness, max_thickness, normalized);
		}

		const uint32_t vertex_offset = static_cast<uint32_t>(out_vertices.size());
		const uint32_t start_center_index = vertex_offset;

		TreeVertex start_center{};
		start_center.position       = a;
		start_center.normal         = -axis;
		start_center.dataset_radius = segment.dataset_radius;
		start_center.depth_factor   = segment.depth_factor;
		start_center.segment_id     = static_cast<float>(segment.id);
		out_vertices.push_back(start_center);

		const uint32_t end_center_index = start_center_index + 1;
		TreeVertex end_center = start_center;
		end_center.position = b;
		end_center.normal   = axis;
		out_vertices.push_back(end_center);

		for (int i = 0; i < sides; ++i)
		{
			float angle = step * static_cast<float>(i);
			float c = std::cos(angle);
			float s = std::sin(angle);
			glm::vec3 radial = tangent * c + bitangent * s;

			TreeVertex v0{};
			v0.position       = a + radial * radius;
			v0.normal         = radial;
			v0.dataset_radius = segment.dataset_radius;
			v0.depth_factor   = segment.depth_factor;
			v0.segment_id     = static_cast<float>(segment.id);

			TreeVertex v1 = v0;
			v1.position = b + radial * radius;

			out_vertices.push_back(v0);
			out_vertices.push_back(v1);
		}

		const uint32_t ring_start = vertex_offset + 2;

		SegmentRange range{};
		range.index_offset = static_cast<uint32_t>(out_indices.size());

		for (int i = 0; i < sides; ++i)
		{
			uint32_t next = (i + 1) % sides;
			uint32_t start0 = ring_start + static_cast<uint32_t>(i * 2);
			uint32_t end0   = start0 + 1;
			uint32_t start1 = ring_start + static_cast<uint32_t>(next * 2);
			uint32_t end1   = start1 + 1;

			out_indices.push_back(start0);
			out_indices.push_back(end0);
			out_indices.push_back(end1);

			out_indices.push_back(start0);
			out_indices.push_back(end1);
			out_indices.push_back(start1);

			out_indices.push_back(start_center_index);
			out_indices.push_back(start1);
			out_indices.push_back(start0);

			out_indices.push_back(end_center_index);
			out_indices.push_back(end0);
			out_indices.push_back(end1);
		}

		range.index_count = static_cast<uint32_t>(out_indices.size()) - range.index_offset;
		out_ranges.push_back(range);
	}

	return !out_indices.empty();
}

bool CcoTree3d::upload_geometry(const std::vector<TreeVertex> &vertices, const std::vector<uint32_t> &indices)
{
	index_count = static_cast<uint32_t>(indices.size());
	if (vertices.empty() || indices.empty())
	{
		vertex_buffer.reset();
		index_buffer.reset();
		index_count = 0;
		return false;
	}

	const VkDeviceSize vertex_size = static_cast<VkDeviceSize>(vertices.size() * sizeof(TreeVertex));
	const VkDeviceSize index_size  = static_cast<VkDeviceSize>(indices.size() * sizeof(uint32_t));

	vertex_buffer = std::make_unique<vkb::core::BufferC>(get_device(),
	                                                     vertex_size,
	                                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	                                                     VMA_MEMORY_USAGE_CPU_TO_GPU);

	index_buffer = std::make_unique<vkb::core::BufferC>(get_device(),
	                                                    index_size,
	                                                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	                                                    VMA_MEMORY_USAGE_CPU_TO_GPU);

	vertex_buffer->update(vertices.data(), vertex_size);
	index_buffer->update(indices.data(), index_size);

	return true;
}

void CcoTree3d::handle_animation(float delta_time)
{
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

void CcoTree3d::update_growth(float delta_time)
{
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

void CcoTree3d::update_uniform_buffer()
{
	if (!uniform_buffers.scene)
	{
		return;
	}

	UniformBufferObject ubo{};

	ubo.model = build_model_matrix();
	if (parameters.orbit_enabled)
	{
		const glm::vec3 target = parameters.translation;
		const float     radius = std::max(parameters.orbit_radius, 0.5f);
		const glm::vec3 base(0.0f, 0.0f, -radius);

		glm::mat4 rotation(1.0f);
		const float omega = parameters.orbit_speed;
		rotation = glm::rotate(rotation, orbit_time * omega, glm::vec3(1.0f, 0.0f, 0.0f));
		rotation = glm::rotate(rotation, orbit_time * omega, glm::vec3(0.0f, 1.0f, 0.0f));
		rotation = glm::rotate(rotation, orbit_time * omega, glm::vec3(0.0f, 0.0f, 1.0f));

		const glm::vec3 orbit_pos = target + glm::vec3(rotation * glm::vec4(base, 1.0f));
		ubo.view  = glm::lookAt(orbit_pos, target, glm::vec3(0.0f, 1.0f, 0.0f));
		ubo.camera_pos = glm::vec4(orbit_pos, 1.0f);
	}
	else
	{
		ubo.view  = camera.matrices.view;
		glm::mat4 inv_view = glm::inverse(ubo.view);
		glm::vec3 cam_pos = glm::vec3(inv_view[3]);
		ubo.camera_pos = glm::vec4(cam_pos, 1.0f);
	}
	ubo.proj  = camera.matrices.perspective;
	ubo.mvp   = ubo.proj * ubo.view * ubo.model;

	glm::mat3 normal_matrix = glm::transpose(glm::inverse(glm::mat3(ubo.model)));
	ubo.normal_matrix = glm::mat4(1.0f);
	ubo.normal_matrix[0] = glm::vec4(normal_matrix[0], 0.0f);
	ubo.normal_matrix[1] = glm::vec4(normal_matrix[1], 0.0f);
	ubo.normal_matrix[2] = glm::vec4(normal_matrix[2], 0.0f);

	ubo.light_dir  = glm::vec4(glm::normalize(glm::vec3(-0.4f, 0.6f, 0.75f)), 0.0f);

	const float range = std::max(0.0001f, current_geometry.max_radius - current_geometry.min_radius);
	ubo.radius_range = glm::vec4(current_geometry.min_radius, current_geometry.max_radius, range, 0.0f);

	float selected_id = selected_segment_index >= 0 && selected_segment_index < static_cast<int>(current_geometry.segments.size())
	                        ? static_cast<float>(current_geometry.segments[static_cast<size_t>(selected_segment_index)].id)
	                        : -1.0f;
	ubo.params = glm::vec4(static_cast<float>(parameters.lighting_model), selected_id, 0.0f, 0.25f);

	uniform_buffers.scene->update(&ubo, sizeof(UniformBufferObject));
}

CcoTree3d::FrameGeometry CcoTree3d::parse_vtk_file(const std::string &relative_path) const
{
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

	std::vector<glm::vec3> points;
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
				points[i] = {x, y, z};
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
		const glm::vec3 &a = points[segment.a];
		const glm::vec3 &b = points[segment.b];
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

	glm::vec3 min_bounds(std::numeric_limits<float>::max());
	glm::vec3 max_bounds(std::numeric_limits<float>::lowest());
	float     min_radius = std::numeric_limits<float>::max();
	float     max_radius = std::numeric_limits<float>::lowest();

	for (size_t i = 0; i < segments.size(); ++i)
	{
		const SegmentIndices &segment = segments[i];
		if (segment.a >= points.size() || segment.b >= points.size())
		{
			continue;
		}

		const glm::vec3 &a      = points[segment.a];
		const glm::vec3 &b      = points[segment.b];
		const float      radius = radii[i];
		const float      length = glm::length(b - a);

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
		data.start_point    = a;
		data.end_point      = b;
		data.dataset_radius = radius;
		data.depth_factor   = depth_factor;
		data.length         = length;
		data.id             = static_cast<uint32_t>(i);
		geometry.segments.push_back(data);

		min_bounds.x = std::min(min_bounds.x, std::min(a.x, b.x));
		min_bounds.y = std::min(min_bounds.y, std::min(a.y, b.y));
		min_bounds.z = std::min(min_bounds.z, std::min(a.z, b.z));
		max_bounds.x = std::max(max_bounds.x, std::max(a.x, b.x));
		max_bounds.y = std::max(max_bounds.y, std::max(a.y, b.y));
		max_bounds.z = std::max(max_bounds.z, std::max(a.z, b.z));

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

glm::mat4 CcoTree3d::build_model_matrix() const
{
	glm::mat4 model(1.0f);

	const glm::vec3 center = 0.5f * (current_geometry.min_bounds + current_geometry.max_bounds);
	const float     auto_scale = compute_auto_scale();
	const float     manual     = parameters.scale;
	const float     final_scale = auto_scale * manual;

	model = glm::translate(model, parameters.translation);
	model = glm::scale(model, glm::vec3(final_scale, final_scale, final_scale));
	model = glm::translate(model, -center);

	return model;
}

float CcoTree3d::compute_auto_scale() const
{
	if (!current_geometry.valid())
	{
		return 1.0f;
	}

	const glm::vec3 extent = current_geometry.max_bounds - current_geometry.min_bounds;
	const float     max_extent = std::max(std::max(std::abs(extent.x), std::abs(extent.y)), std::abs(extent.z));
	return 2.0f / std::max(max_extent, 0.0001f);
}

int CcoTree3d::pick_segment(int32_t mouse_x, int32_t mouse_y)
{
	if (!current_geometry.valid())
	{
		return -1;
	}

	glm::vec3 ray_origin{};
	glm::vec3 ray_dir{};
	if (!compute_ray(static_cast<float>(mouse_x), static_cast<float>(mouse_y), ray_origin, ray_dir))
	{
		return -1;
	}

	const glm::mat4 model = build_model_matrix();
	const float auto_scale = compute_auto_scale();
	const float final_scale = auto_scale * parameters.scale;
	const float inv_auto_scale = auto_scale > std::numeric_limits<float>::epsilon() ? 1.0f / auto_scale : 1.0f;

	const float min_thickness_ui = std::min(parameters.dataset_min_thickness, parameters.dataset_max_thickness);
	const float max_thickness_ui = std::max(parameters.dataset_min_thickness, parameters.dataset_max_thickness);
	const float min_thickness    = std::max(min_thickness_ui * inv_auto_scale, 0.0001f);
	const float max_thickness    = std::max(max_thickness_ui * inv_auto_scale, min_thickness + 0.0001f);
	const float fixed_radius     = std::max(parameters.fixed_radius * inv_auto_scale, 0.0001f);
	const float radius_range     = std::max(current_geometry.max_radius - current_geometry.min_radius, 0.0001f);

	const std::vector<size_t> &order = current_geometry.render_order;
	size_t total_segments = order.size();
	float clamped_growth = glm::clamp(parameters.growth_progress, 0.0f, 1.0f);
	size_t visible_segments = static_cast<size_t>(std::floor(clamped_growth * static_cast<float>(total_segments) + 1e-4f));
	if (clamped_growth >= 1.0f)
	{
		visible_segments = total_segments;
	}

	float best_t = std::numeric_limits<float>::max();
	int   best_index = -1;

	for (size_t i = 0; i < visible_segments; ++i)
	{
		const SegmentData &segment = current_geometry.segments[order[i]];
		glm::vec3 a = glm::vec3(model * glm::vec4(segment.start_point, 1.0f));
		glm::vec3 b = glm::vec3(model * glm::vec4(segment.end_point, 1.0f));
		float radius_model = fixed_radius;
		if (parameters.use_dataset_radius)
		{
			float normalized = (segment.dataset_radius - current_geometry.min_radius) / radius_range;
			normalized = glm::clamp(normalized, 0.0f, 1.0f);
			radius_model = glm::mix(min_thickness, max_thickness, normalized);
		}
		float radius = radius_model * final_scale;
		float t = 0.0f;
		float dist = distance_ray_segment(ray_origin, ray_dir, a, b, t);
		if (dist <= radius * 1.1f && t >= 0.0f && t < best_t)
		{
			best_t = t;
			best_index = static_cast<int>(order[i]);
		}
	}

	return best_index;
}

bool CcoTree3d::compute_ray(float mouse_x, float mouse_y, glm::vec3 &out_origin, glm::vec3 &out_direction) const
{
	if (width == 0 || height == 0)
	{
		return false;
	}

	float ndc_x = (2.0f * mouse_x) / static_cast<float>(width) - 1.0f;
	float ndc_y = 1.0f - (2.0f * mouse_y) / static_cast<float>(height);

	glm::mat4 view = camera.matrices.view;
	glm::mat4 proj = camera.matrices.perspective;
	glm::mat4 inv_vp = glm::inverse(proj * view);

	glm::vec4 near_point = inv_vp * glm::vec4(ndc_x, ndc_y, 0.0f, 1.0f);
	glm::vec4 far_point  = inv_vp * glm::vec4(ndc_x, ndc_y, 1.0f, 1.0f);

	if (near_point.w == 0.0f || far_point.w == 0.0f)
	{
		return false;
	}

	near_point /= near_point.w;
	far_point  /= far_point.w;

	glm::vec3 cam_pos = glm::vec3(glm::inverse(view)[3]);
	out_origin = cam_pos;
	out_direction = glm::normalize(glm::vec3(far_point - near_point));

	return true;
}

float CcoTree3d::distance_ray_segment(const glm::vec3 &ray_origin, const glm::vec3 &ray_dir,
                                      const glm::vec3 &a, const glm::vec3 &b, float &out_t) const
{
	const float eps = 1e-6f;
	glm::vec3 v = b - a;
	glm::vec3 w = ray_origin - a;

	float dv = glm::dot(ray_dir, v);
	float vv = glm::dot(v, v);
	float dw = glm::dot(ray_dir, w);
	float vw = glm::dot(v, w);

	float denom = vv - dv * dv;
	float t = 0.0f;
	if (denom > eps)
	{
		t = (dv * dw - vw) / denom;
		t = glm::clamp(t, 0.0f, 1.0f);
	}
	else
	{
		t = 0.0f;
	}

	float s = -dw + t * dv;
	if (s < 0.0f)
	{
		s = 0.0f;
		t = vv > eps ? glm::clamp(-vw / vv, 0.0f, 1.0f) : 0.0f;
	}

	out_t = s;
	glm::vec3 closest_seg = a + t * v;
	glm::vec3 closest_ray = ray_origin + s * ray_dir;
	return glm::length(closest_seg - closest_ray);
}

std::unique_ptr<vkb::Application> create_cco_tree_3d()
{
	return std::make_unique<CcoTree3d>();
}
