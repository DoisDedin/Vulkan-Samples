/* Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "api_vulkan_sample.h"

class HSVTriangle02 : public ApiVulkanSample
{
  public:
	struct Vertex
	{
		glm::vec2 position;
	};

	HSVTriangle02();
	~HSVTriangle02() override;

	bool prepare(const vkb::ApplicationOptions &options) override;
	void build_command_buffers() override;
	void render(float delta_time) override;
	bool resize(uint32_t width, uint32_t height) override;

  private:

	struct UniformBufferObject
	{
		glm::mat4 mvp{1.0f};
		glm::vec3 color{1.0f};
	};

	void create_vertex_buffer();
	void create_uniform_buffer();
	void update_uniform_buffer();
	void setup_descriptor_pool();
	void setup_descriptor_set_layout();
	void setup_descriptor_set();
	void prepare_pipeline();
	void draw_frame();

	glm::vec3 hsv_to_rgb(float h_deg, float s, float v) const;

	std::unique_ptr<vkb::core::BufferC> vertex_buffer;
	std::unique_ptr<vkb::core::BufferC> uniform_buffer;

	VkPipelineLayout      pipeline_layout      = VK_NULL_HANDLE;
	VkPipeline            pipeline             = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
	VkDescriptorPool      descriptor_pool      = VK_NULL_HANDLE;
	VkDescriptorSet       descriptor_set       = VK_NULL_HANDLE;
};

std::unique_ptr<vkb::Application> create_hsv_triangle_02();
