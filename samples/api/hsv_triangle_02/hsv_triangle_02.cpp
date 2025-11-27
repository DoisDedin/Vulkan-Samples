/* Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hsv_triangle_02.h"

#include "common/vk_common.h"
#include "core/buffer.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace
{
const std::array<HSVTriangle02::Vertex, 3> kTriangle = {{
    {{0.0f, 0.7f}},
    {{-0.7f, -0.6f}},
    {{0.7f, -0.6f}},
}};
}        // namespace

HSVTriangle02::HSVTriangle02()
{
	title = "HSV Triangle 02";
}

HSVTriangle02::~HSVTriangle02()
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
		if (descriptor_pool != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorPool(get_device().get_handle(), descriptor_pool, nullptr);
		}
	}
}

bool HSVTriangle02::prepare(const vkb::ApplicationOptions &options)
{
	if (!ApiVulkanSample::prepare(options))
	{
		return false;
	}

	create_vertex_buffer();
	create_uniform_buffer();
	setup_descriptor_pool();
	setup_descriptor_set_layout();
	setup_descriptor_set();
	prepare_pipeline();
	build_command_buffers();
	prepared = true;
	return true;
}

void HSVTriangle02::create_vertex_buffer()
{
	const VkDeviceSize buffer_size = kTriangle.size() * sizeof(Vertex);
	vertex_buffer                  = std::make_unique<vkb::core::BufferC>(
        get_device(),
        buffer_size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU);
	vertex_buffer->update(kTriangle.data(), buffer_size);
}

void HSVTriangle02::create_uniform_buffer()
{
	uniform_buffer = std::make_unique<vkb::core::BufferC>(
	    get_device(),
	    sizeof(UniformBufferObject),
	    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
	    VMA_MEMORY_USAGE_CPU_TO_GPU);
	update_uniform_buffer();
}

glm::vec3 HSVTriangle02::hsv_to_rgb(float h_deg, float s, float v) const
{
	float c  = v * s;
	float hh = h_deg / 60.0f;
	float x  = c * (1.0f - std::fabsf(std::fmod(hh, 2.0f) - 1.0f));

	glm::vec3 rgb_prime;
	if (0.0f <= hh && hh < 1.0f)
		rgb_prime = {c, x, 0.0f};
	else if (1.0f <= hh && hh < 2.0f)
		rgb_prime = {x, c, 0.0f};
	else if (2.0f <= hh && hh < 3.0f)
		rgb_prime = {0.0f, c, x};
	else if (3.0f <= hh && hh < 4.0f)
		rgb_prime = {0.0f, x, c};
	else if (4.0f <= hh && hh < 5.0f)
		rgb_prime = {x, 0.0f, c};
	else
		rgb_prime = {c, 0.0f, x};

	float m = v - c;
	return rgb_prime + glm::vec3(m);
}

void HSVTriangle02::update_uniform_buffer()
{
	const float aspect = (height == 0) ? 1.0f : static_cast<float>(width) / static_cast<float>(height);
	glm::mat4 projection = glm::ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);

	UniformBufferObject ubo{};
	ubo.mvp   = projection;
	ubo.color = hsv_to_rgb(180.0f, 0.5f, 0.8f);

	uniform_buffer->update(&ubo, sizeof(ubo));
}

void HSVTriangle02::setup_descriptor_pool()
{
	const std::vector<VkDescriptorPoolSize> pool_sizes = {
	    vkb::initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)};
	VkDescriptorPoolCreateInfo pool_info = vkb::initializers::descriptor_pool_create_info(pool_sizes, 1);
	VK_CHECK(vkCreateDescriptorPool(get_device().get_handle(), &pool_info, nullptr, &descriptor_pool));
}

void HSVTriangle02::setup_descriptor_set_layout()
{
	const std::array<VkDescriptorSetLayoutBinding, 1> set_layout_bindings = {
	    vkb::initializers::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0)};

	VkDescriptorSetLayoutCreateInfo layout_info =
	    vkb::initializers::descriptor_set_layout_create_info(set_layout_bindings.data(), static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(get_device().get_handle(), &layout_info, nullptr, &descriptor_set_layout));

	VkPipelineLayoutCreateInfo pipeline_layout_info =
	    vkb::initializers::pipeline_layout_create_info(&descriptor_set_layout, 1);

	VK_CHECK(vkCreatePipelineLayout(get_device().get_handle(), &pipeline_layout_info, nullptr, &pipeline_layout));
}

void HSVTriangle02::setup_descriptor_set()
{
	VkDescriptorSetAllocateInfo alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(descriptor_pool, &descriptor_set_layout, 1);
	VK_CHECK(vkAllocateDescriptorSets(get_device().get_handle(), &alloc_info, &descriptor_set));

	VkDescriptorBufferInfo ubo_info = create_descriptor(*uniform_buffer);

	const std::array<VkWriteDescriptorSet, 1> writes = {
	    vkb::initializers::write_descriptor_set(descriptor_set, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &ubo_info)};

	vkUpdateDescriptorSets(get_device().get_handle(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void HSVTriangle02::prepare_pipeline()
{
	VkPipelineInputAssemblyStateCreateInfo input_assembly =
	    vkb::initializers::pipeline_input_assembly_state_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterization_state =
	    vkb::initializers::pipeline_rasterization_state_create_info(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);

	VkPipelineColorBlendAttachmentState blend_attachment =
	    vkb::initializers::pipeline_color_blend_attachment_state(0xf, VK_FALSE);

	VkPipelineColorBlendStateCreateInfo color_blend_state =
	    vkb::initializers::pipeline_color_blend_state_create_info(1, &blend_attachment);

	VkPipelineDepthStencilStateCreateInfo depth_stencil_state =
	    vkb::initializers::pipeline_depth_stencil_state_create_info(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);

	VkPipelineViewportStateCreateInfo viewport_state =
	    vkb::initializers::pipeline_viewport_state_create_info(1, 1, 0);

	VkPipelineMultisampleStateCreateInfo multisample_state =
	    vkb::initializers::pipeline_multisample_state_create_info(VK_SAMPLE_COUNT_1_BIT);

	const std::array<VkDynamicState, 2> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo    dynamic_state  = vkb::initializers::pipeline_dynamic_state_create_info(
           dynamic_states.data(), static_cast<uint32_t>(dynamic_states.size()));

	const std::array<VkVertexInputBindingDescription, 1> vertex_bindings = {
	    vkb::initializers::vertex_input_binding_description(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX)};

	const std::array<VkVertexInputAttributeDescription, 1> vertex_attributes = {
	    vkb::initializers::vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, position))};

	VkPipelineVertexInputStateCreateInfo vertex_input_state = vkb::initializers::pipeline_vertex_input_state_create_info();
	vertex_input_state.vertexBindingDescriptionCount        = static_cast<uint32_t>(vertex_bindings.size());
	vertex_input_state.pVertexBindingDescriptions           = vertex_bindings.data();
	vertex_input_state.vertexAttributeDescriptionCount      = static_cast<uint32_t>(vertex_attributes.size());
	vertex_input_state.pVertexAttributeDescriptions         = vertex_attributes.data();

	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{};
	shader_stages[0] = load_shader("hsv_triangle_02", "hsv_triangle.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shader_stages[1] = load_shader("hsv_triangle_02", "hsv_triangle.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	VkGraphicsPipelineCreateInfo pipeline_info = vkb::initializers::pipeline_create_info(pipeline_layout, render_pass, 0);
	pipeline_info.pVertexInputState            = &vertex_input_state;
	pipeline_info.pInputAssemblyState          = &input_assembly;
	pipeline_info.pRasterizationState          = &rasterization_state;
	pipeline_info.pColorBlendState             = &color_blend_state;
	pipeline_info.pMultisampleState            = &multisample_state;
	pipeline_info.pViewportState               = &viewport_state;
	pipeline_info.pDepthStencilState           = &depth_stencil_state;
	pipeline_info.pDynamicState                = &dynamic_state;
	pipeline_info.stageCount                   = static_cast<uint32_t>(shader_stages.size());
	pipeline_info.pStages                      = shader_stages.data();

	VK_CHECK(vkCreateGraphicsPipelines(get_device().get_handle(), pipeline_cache, 1, &pipeline_info, nullptr, &pipeline));
}

void HSVTriangle02::build_command_buffers()
{
	VkCommandBufferBeginInfo begin_info = vkb::initializers::command_buffer_begin_info();

	std::array<VkClearValue, 2> clear_values{};
	clear_values[0].color        = default_clear_color;
	clear_values[1].depthStencil = {1.0f, 0};

	VkRenderPassBeginInfo render_pass_info = vkb::initializers::render_pass_begin_info();
	render_pass_info.renderPass            = render_pass;
	render_pass_info.renderArea.offset     = {0, 0};
	render_pass_info.renderArea.extent     = {width, height};
	render_pass_info.clearValueCount       = static_cast<uint32_t>(clear_values.size());
	render_pass_info.pClearValues          = clear_values.data();

	for (size_t i = 0; i < draw_cmd_buffers.size(); ++i)
	{
		render_pass_info.framebuffer = framebuffers[i];
		VK_CHECK(vkBeginCommandBuffer(draw_cmd_buffers[i], &begin_info));

		vkCmdBeginRenderPass(draw_cmd_buffers[i], &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

		const VkViewport viewport = vkb::initializers::viewport(static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
		const VkRect2D   scissor  = vkb::initializers::rect2D(width, height, 0, 0);
		vkCmdSetViewport(draw_cmd_buffers[i], 0, 1, &viewport);
		vkCmdSetScissor(draw_cmd_buffers[i], 0, 1, &scissor);

		vkCmdBindPipeline(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		const VkDeviceSize offsets[] = {0};
		const VkBuffer     buffers[] = {vertex_buffer->get_handle()};
		vkCmdBindVertexBuffers(draw_cmd_buffers[i], 0, 1, buffers, offsets);
		vkCmdBindDescriptorSets(draw_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

		vkCmdDraw(draw_cmd_buffers[i], static_cast<uint32_t>(kTriangle.size()), 1, 0, 0);

		draw_ui(draw_cmd_buffers[i]);

		vkCmdEndRenderPass(draw_cmd_buffers[i]);
		VK_CHECK(vkEndCommandBuffer(draw_cmd_buffers[i]));
	}
}

void HSVTriangle02::render(float)
{
	if (!prepared)
	{
		return;
	}

	draw_frame();
}

bool HSVTriangle02::resize(uint32_t new_width, uint32_t new_height)
{
	if (!ApiVulkanSample::resize(new_width, new_height))
	{
		return false;
	}

	update_uniform_buffer();
	return true;
}

std::unique_ptr<vkb::Application> create_hsv_triangle_02()
{
	return std::make_unique<HSVTriangle02>();
}

void HSVTriangle02::draw_frame()
{
	ApiVulkanSample::prepare_frame();
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers    = &draw_cmd_buffers[current_buffer];
	VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));
	ApiVulkanSample::submit_frame();
}
