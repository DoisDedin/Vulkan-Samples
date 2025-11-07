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

#include "triangle_play.h"

#include <array>
#include <cmath>
#include <vector>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "common/vk_common.h"
#include "core/buffer.h"
#include "platform/window.h"

namespace
{
constexpr uint32_t kTriangleCount = 3;

struct TriangleVertex
{
	glm::vec2 position;
	float     triangle_id;
	glm::vec3 color;
};

const std::array<TriangleVertex, 9> kVertices = {
    TriangleVertex{{-0.6f, -0.5f}, 0.0f, {1.0f, 0.3f, 0.3f}},
    TriangleVertex{{0.6f, -0.5f}, 0.0f, {1.0f, 0.8f, 0.4f}},
    TriangleVertex{{0.0f, 0.7f}, 0.0f, {1.0f, 0.5f, 0.5f}},

    TriangleVertex{{-0.45f, -0.35f}, 1.0f, {0.4f, 0.8f, 1.0f}},
    TriangleVertex{{0.45f, -0.35f}, 1.0f, {0.2f, 0.6f, 1.0f}},
    TriangleVertex{{0.0f, 0.55f}, 1.0f, {0.5f, 0.9f, 1.0f}},

    TriangleVertex{{-0.5f, -0.4f}, 2.0f, {0.8f, 1.0f, 0.6f}},
    TriangleVertex{{0.5f, -0.4f}, 2.0f, {0.6f, 1.0f, 0.3f}},
    TriangleVertex{{0.0f, 0.6f}, 2.0f, {0.7f, 1.0f, 0.5f}},
};
}        // namespace

TrianglePlay::TrianglePlay()
{
	title = "Triangle play";
	camera.type = vkb::CameraType::LookAt;
	camera.set_position({0.0f, 0.0f, 3.0f});
	camera.set_rotation({0.0f, 0.0f, 0.0f});
	camera.set_perspective(60.0f, 1.0f, 0.1f, 256.0f);
}

TrianglePlay::~TrianglePlay()
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

bool TrianglePlay::prepare(const vkb::ApplicationOptions &options)
{
	if (!ApiVulkanSample::prepare(options))
	{
		return false;
	}

	create_vertex_buffer();
	create_uniform_buffers();
	setup_descriptor_pool();
	setup_descriptor_set_layout();
	setup_descriptor_set();
	prepare_pipeline();
	build_command_buffers();
	prepared = true;
	return true;
}

void TrianglePlay::create_vertex_buffer()
{
	const VkDeviceSize buffer_size = static_cast<VkDeviceSize>(kVertices.size() * sizeof(TriangleVertex));

	vertex_buffer = std::make_unique<vkb::core::BufferC>(get_device(),
	                                                     buffer_size,
	                                                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	                                                     VMA_MEMORY_USAGE_CPU_TO_GPU);

	vertex_buffer->update(kVertices.data(), buffer_size);
}

void TrianglePlay::create_uniform_buffers()
{
	uniform_buffers.scene = std::make_unique<vkb::core::BufferC>(get_device(),
	                                                             sizeof(UniformBufferObject),
	                                                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	                                                             VMA_MEMORY_USAGE_CPU_TO_GPU);

	update_uniform_buffer(0.0f);
}

void TrianglePlay::setup_descriptor_pool()
{
	const std::vector<VkDescriptorPoolSize> pool_sizes = {
	    vkb::initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)};

	VkDescriptorPoolCreateInfo pool_info =
	    vkb::initializers::descriptor_pool_create_info(pool_sizes, 1);

	VK_CHECK(vkCreateDescriptorPool(get_device().get_handle(), &pool_info, nullptr, &descriptor_pool));
}

void TrianglePlay::setup_descriptor_set_layout()
{
	const std::array<VkDescriptorSetLayoutBinding, 1> set_layout_bindings = {
	    vkb::initializers::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0)};

	VkDescriptorSetLayoutCreateInfo descriptor_layout =
	    vkb::initializers::descriptor_set_layout_create_info(set_layout_bindings.data(), static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(get_device().get_handle(), &descriptor_layout, nullptr, &descriptor_set_layout));

	VkPipelineLayoutCreateInfo pipeline_layout_info =
	    vkb::initializers::pipeline_layout_create_info(&descriptor_set_layout, 1);

	VK_CHECK(vkCreatePipelineLayout(get_device().get_handle(), &pipeline_layout_info, nullptr, &pipeline_layout));
}

void TrianglePlay::setup_descriptor_set()
{
	VkDescriptorSetAllocateInfo alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(descriptor_pool, &descriptor_set_layout, 1);

	VK_CHECK(vkAllocateDescriptorSets(get_device().get_handle(), &alloc_info, &descriptor_set));

	VkDescriptorBufferInfo ubo_descriptor = create_descriptor(*uniform_buffers.scene);

	const std::array<VkWriteDescriptorSet, 1> write_descriptor_sets = {
	    vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &ubo_descriptor)};

	vkUpdateDescriptorSets(get_device().get_handle(), static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, nullptr);
}

void TrianglePlay::prepare_pipeline()
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
	shader_stages[0] = load_shader("triangle_play", "triangle.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shader_stages[1] = load_shader("triangle_play", "triangle.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	const std::array<VkVertexInputBindingDescription, 1> vertex_input_bindings = {
	    vkb::initializers::vertex_input_binding_description(0, sizeof(TriangleVertex), VK_VERTEX_INPUT_RATE_VERTEX)};

	const std::array<VkVertexInputAttributeDescription, 3> vertex_input_attributes = {
	    vkb::initializers::vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(TriangleVertex, position)),
	    vkb::initializers::vertex_input_attribute_description(0, 1, VK_FORMAT_R32_SFLOAT, offsetof(TriangleVertex, triangle_id)),
	    vkb::initializers::vertex_input_attribute_description(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(TriangleVertex, color))};

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

void TrianglePlay::build_command_buffers()
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

		vkCmdBindPipeline(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		const VkDeviceSize offsets[] = {0};
		VkBuffer           vb        = vertex_buffer->get_handle();
		vkCmdBindVertexBuffers(draw_cmd_buffers[i], 0, 1, &vb, offsets);

		vkCmdBindDescriptorSets(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

		vkCmdDraw(draw_cmd_buffers[i], static_cast<uint32_t>(kVertices.size()), 1, 0, 0);

		draw_ui(draw_cmd_buffers[i]);

		vkCmdEndRenderPass(draw_cmd_buffers[i]);

		VK_CHECK(vkEndCommandBuffer(draw_cmd_buffers[i]));
	}
}

void TrianglePlay::update_uniform_buffer(float delta_time)
{
	if (!uniform_buffers.scene)
	{
		return;
	}

	if (parameters.animate)
	{
		state.local_rotation += glm::radians(parameters.local_rotation_speed) * delta_time;
		state.orbit_phase += glm::radians(parameters.orbit_speed) * delta_time;
	}

	const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
	UniformBufferObject ubo{};
	ubo.mvp = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);

	const float base_angle = state.orbit_phase;
	for (uint32_t i = 0; i < kTriangleCount; ++i)
	{
		const float angle = base_angle + static_cast<float>(i) * glm::two_pi<float>() / static_cast<float>(kTriangleCount);
		float       radius = parameters.orbit_radius;
		ubo.offsets[i]     = glm::vec4(radius * std::cos(angle), radius * std::sin(angle), 0.0f, 0.0f);
	}

	ubo.params = glm::vec4(state.local_rotation, parameters.scale, parameters.orbit_radius, parameters.animate ? 1.0f : 0.0f);

	uniform_buffers.scene->update(&ubo, sizeof(UniformBufferObject));
}

void TrianglePlay::draw()
{
	ApiVulkanSample::prepare_frame();

	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers    = &draw_cmd_buffers[current_buffer];

	VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));

	ApiVulkanSample::submit_frame();
}

void TrianglePlay::render(float delta_time)
{
	if (!prepared)
	{
		return;
	}

	update_uniform_buffer(delta_time);
	draw();
}

void TrianglePlay::on_update_ui_overlay(vkb::Drawer &drawer)
{
	if (drawer.header("Triangle controls"))
	{
		if (drawer.checkbox("Animate", &parameters.animate))
		{
			if (!parameters.animate)
			{
				state.local_rotation = 0.0f;
				state.orbit_phase    = 0.0f;
			}
			update_uniform_buffer(0.0f);
		}
		if (drawer.slider_float("Scale", &parameters.scale, 0.2f, 1.2f))
		{
			update_uniform_buffer(0.0f);
		}
		if (drawer.slider_float("Orbit radius", &parameters.orbit_radius, 0.2f, 1.5f))
		{
			update_uniform_buffer(0.0f);
		}
		if (drawer.slider_float("Spin speed", &parameters.local_rotation_speed, 0.0f, 180.0f))
		{
		}
		if (drawer.slider_float("Orbit speed", &parameters.orbit_speed, 0.0f, 120.0f))
		{
		}
	}
}

bool TrianglePlay::resize(uint32_t new_width, uint32_t new_height)
{
	if (!ApiVulkanSample::resize(new_width, new_height))
	{
		return false;
	}

	rebuild_command_buffers();
	update_uniform_buffer(0.0f);
	return true;
}

std::unique_ptr<vkb::Application> create_triangle_play()
{
	return std::make_unique<TrianglePlay>();
}
