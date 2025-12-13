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

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class TreeTp1 : public ApiVulkanSample
{
  public:
	TreeTp1();
	~TreeTp1() override;

	bool prepare(const vkb::ApplicationOptions &options) override;
	void build_command_buffers() override;
	void render(float delta_time) override;
	void on_update_ui_overlay(vkb::Drawer &drawer) override;
	bool resize(uint32_t width, uint32_t height) override;

  private:
	struct SegmentVertex
	{
		glm::vec3 position{0.0f};
		glm::vec3 segment_a{0.0f};
		glm::vec3 segment_b{0.0f};
		float     capsule_radius{0.0f};
		float     radius_value{0.0f};
		float     tip_factor{0.0f};
	};

	struct SegmentData
	{
		glm::vec2 a{0.0f};
		glm::vec2 b{0.0f};
		float     radius{0.0f};
		float     depth_factor{1.0f};
	};

	struct FrameGeometry
	{
		std::vector<SegmentData> segments;
		std::vector<size_t>      render_order;
		glm::vec2                min_bounds{0.0f};
		glm::vec2                max_bounds{0.0f};
		float                    min_radius{0.0f};
		float                    max_radius{1.0f};

		bool valid() const
		{
			return !segments.empty();
		}
	};

	struct UniformBufferObject
	{
		glm::mat4 mvp{1.0f};
		glm::vec4 radius_range{0.0f};
	};

	struct UniformBuffers
	{
		std::unique_ptr<vkb::core::BufferC> scene;
	} uniform_buffers;

	struct Parameters
	{
		int         dataset_index   = 0;
		float       scale           = 1.0f;
		glm::vec2   translation{0.0f, 0.0f};
		float       rotation_x_deg  = 10.0f;
		float       rotation_y_deg  = 0.0f;
		float       rotation_z_deg  = 0.0f;
		bool        animate         = true;
		float       animation_speed     = 1.0f;        // frames per second
		bool        use_dataset_radius  = true;
		float       dataset_min_thickness = 0.01f;
		float       dataset_max_thickness = 0.05f;
		float       fixed_radius          = 0.012f;
		float       growth_progress       = 1.0f;
		bool        growth_auto           = false;
		float       growth_speed          = 0.35f;
	} parameters;

	struct PlaybackState
	{
		uint32_t frame_index = 0;
		float    accumulator = 0.0f;
	} playback;

	std::unique_ptr<vkb::core::BufferC> vertex_buffer;
	uint32_t                             vertex_count = 0;

		FrameGeometry                                      current_geometry;
		std::vector<std::string>                          current_frames;
		std::unordered_map<std::string, FrameGeometry>    frame_cache;
		float                                             last_growth_used = 1.0f;

		VkPipelineLayout      pipeline_layout      = VK_NULL_HANDLE;
		VkPipeline            pipeline             = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
	VkDescriptorSet       descriptor_set       = VK_NULL_HANDLE;

	void create_uniform_buffers();
	void setup_descriptor_pool();
	void setup_descriptor_set_layout();
	void setup_descriptor_set();
	void prepare_pipeline();

	void update_uniform_buffer();
		void draw();
		void handle_animation(float delta_time);
		void update_growth(float delta_time);

		bool rebuild_current_mesh();
		bool build_vertices_from_segments(const FrameGeometry &geometry, std::vector<SegmentVertex> &out_vertices, float growth_progress) const;

		bool set_dataset(int dataset_index);
		bool load_frame(uint32_t frame_index);
		bool upload_geometry(const std::vector<SegmentVertex> &vertices);
		FrameGeometry parse_vtk_file(const std::string &relative_path) const;
	glm::mat4 build_model_matrix() const;
	float    compute_auto_scale() const;
};

std::unique_ptr<vkb::Application> create_tree_tp1();
