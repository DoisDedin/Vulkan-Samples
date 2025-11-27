/* Copyright (c) 2025
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "api_vulkan_sample.h"

class RoboticArm06 : public ApiVulkanSample
{
  public:
	struct Vertex
	{
		glm::vec3 position;
		glm::vec3 normal;
	};

	RoboticArm06();
	~RoboticArm06() override;

	bool prepare(const vkb::ApplicationOptions &options) override;
	void build_command_buffers() override;
	void render(float delta_time) override;
	bool resize(uint32_t width, uint32_t height) override;
	void on_update_ui_overlay(vkb::Drawer &drawer) override;

  private:

	struct UniformBufferObject
	{
		glm::mat4 vp{1.0f};
	};

	struct PushConstants
	{
		glm::mat4 model;
		glm::vec4 color;
	};

	void create_vertex_buffer();
	void create_uniform_buffer();
	void update_uniform_buffer();
	void setup_descriptor_pool();
	void setup_descriptor_set_layout();
	void setup_descriptor_set();
	void prepare_pipeline();

	void record_arm(VkCommandBuffer cmd);
	void draw_frame();

	std::unique_ptr<vkb::core::BufferC> vertex_buffer;
	std::unique_ptr<vkb::core::BufferC> uniform_buffer;

	VkPipelineLayout      pipeline_layout      = VK_NULL_HANDLE;
	VkPipeline            pipeline             = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
	VkDescriptorPool      descriptor_pool      = VK_NULL_HANDLE;
	VkDescriptorSet       descriptor_set       = VK_NULL_HANDLE;

	float camera_angle = 0.0f;
	glm::vec3 last_eye  = glm::vec3(0.0f, 10.0f, 12.0f);
};

std::unique_ptr<vkb::Application> create_robotic_arm_06();
