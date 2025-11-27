/* Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "api_vulkan_sample.h"

#include <memory>

class InteractiveSquare01 : public ApiVulkanSample
{
  public:
	struct Vertex
	{
		glm::vec2 position;
		glm::vec3 color;
	};

	InteractiveSquare01();
	~InteractiveSquare01() override;

	bool prepare(const vkb::ApplicationOptions &options) override;
	void render(float delta_time) override;
	void build_command_buffers() override;
	void input_event(const vkb::InputEvent &input_event) override;
	bool resize(uint32_t width, uint32_t height) override;

  private:

	struct UniformBufferObject
	{
		glm::mat4 mvp{1.0f};
	};

	void create_vertex_buffer();
	void create_uniform_buffer();
	void update_uniform_buffer();
	void setup_descriptor_pool();
	void setup_descriptor_set_layout();
	void setup_descriptor_set();
	void prepare_pipeline();
	void draw_frame();

	std::unique_ptr<vkb::core::BufferC> vertex_buffer;
	std::unique_ptr<vkb::core::BufferC> uniform_buffer;

	VkPipelineLayout      pipeline_layout      = VK_NULL_HANDLE;
	VkPipeline            pipeline             = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
	VkDescriptorPool      descriptor_pool      = VK_NULL_HANDLE;
	VkDescriptorSet       descriptor_set       = VK_NULL_HANDLE;

	float rotation_degrees = 30.0f;
	bool  background_red   = false;
};

std::unique_ptr<vkb::Application> create_interactive_square_01();
