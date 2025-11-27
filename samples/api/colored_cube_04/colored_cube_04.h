/* Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "api_vulkan_sample.h"

class ColoredCube04 : public ApiVulkanSample
{
  public:
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 color;
		glm::vec3 normal;
	};

	ColoredCube04();
	~ColoredCube04() override;

	bool prepare(const vkb::ApplicationOptions &options) override;
	void build_command_buffers() override;
	void render(float delta_time) override;
	bool resize(uint32_t width, uint32_t height) override;

  private:

	struct UniformBufferObject
	{
		glm::mat4 mvp{1.0f};
		glm::mat4 model{1.0f};
		glm::mat3 normal{1.0f};
		glm::vec3 light_dir = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.6f));
		float     _padding  = 0.0f;
	};

	void create_vertex_buffer();
	void create_uniform_buffer();
	void update_uniform_buffer(float delta_time);
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

	float rotation = 0.0f;
};

std::unique_ptr<vkb::Application> create_colored_cube_04();
