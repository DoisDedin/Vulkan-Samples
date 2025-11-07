/* Copyright (c) 2019-2024, Arm Limited and Contributors
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

#pragma once

#include "api_vulkan_sample.h"

#include <memory>
#include <glm/glm.hpp>

class TrianglePlay : public ApiVulkanSample
{
  public:
	TrianglePlay();
	~TrianglePlay() override;

	bool prepare(const vkb::ApplicationOptions &options) override;
	void build_command_buffers() override;
	void render(float delta_time) override;
	void on_update_ui_overlay(vkb::Drawer &drawer) override;
	bool resize(uint32_t width, uint32_t height) override;

  private:
	struct UniformBufferObject
	{
		glm::mat4 mvp;
		glm::vec4 offsets[3];
		glm::vec4 params;
	};

	struct UniformBuffers
	{
		std::unique_ptr<vkb::core::BufferC> scene;
	} uniform_buffers;

	struct Parameters
	{
		float scale               = 0.65f;
		float orbit_radius        = 0.75f;
		float local_rotation_speed = 70.0f;
		float orbit_speed         = 35.0f;
		bool  animate             = true;
	} parameters;

	struct State
	{
		float local_rotation = 0.0f;
		float orbit_phase    = 0.0f;
	} state;

	std::unique_ptr<vkb::core::BufferC> vertex_buffer;

	VkPipelineLayout      pipeline_layout      = VK_NULL_HANDLE;
	VkPipeline            pipeline             = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
	VkDescriptorSet       descriptor_set       = VK_NULL_HANDLE;

	void create_vertex_buffer();
	void create_uniform_buffers();
	void setup_descriptor_pool();
	void setup_descriptor_set_layout();
	void setup_descriptor_set();
	void prepare_pipeline();
	void update_uniform_buffer(float delta_time);
	void draw();
};

std::unique_ptr<vkb::Application> create_triangle_play();
