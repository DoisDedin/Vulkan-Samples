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

#pragma once

#include "api_vulkan_sample.h"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class CcoTree3d : public ApiVulkanSample
{
  public:
	CcoTree3d();
	~CcoTree3d() override;

	bool prepare(const vkb::ApplicationOptions &options) override;
	void build_command_buffers() override;
	void render(float delta_time) override;
	void on_update_ui_overlay(vkb::Drawer &drawer) override;
	bool resize(uint32_t width, uint32_t height) override;
	void input_event(const vkb::InputEvent &input_event) override;

  private:
	struct TreeVertex
	{
		glm::vec3 position{0.0f};
		glm::vec3 normal{0.0f};
		float     dataset_radius{0.0f};
		float     depth_factor{0.0f};
		float     segment_id{0.0f};
	};

	struct SegmentData
	{
		glm::vec3 start_point{0.0f};
		glm::vec3 end_point{0.0f};
		float     dataset_radius{0.0f};
		float     depth_factor{1.0f};
		float     length{0.0f};
		uint32_t  id{0};
	};

	struct SegmentRange
	{
		uint32_t index_offset = 0;
		uint32_t index_count  = 0;
	};

	struct FrameGeometry
	{
		std::vector<SegmentData> segments;
		std::vector<size_t>      render_order;
		glm::vec3                min_bounds{0.0f};
		glm::vec3                max_bounds{0.0f};
		float                    min_radius{0.0f};
		float                    max_radius{1.0f};

		bool valid() const
		{
			return !segments.empty();
		}
	};

	struct UniformBufferObject
	{
		glm::mat4 model{1.0f};
		glm::mat4 view{1.0f};
		glm::mat4 proj{1.0f};
		glm::mat4 mvp{1.0f};
		glm::mat4 normal_matrix{1.0f};
		glm::vec4 light_dir{0.0f, 0.0f, 1.0f, 0.0f};
		glm::vec4 camera_pos{0.0f, 0.0f, 0.0f, 0.0f};
		glm::vec4 radius_range{0.0f};
		glm::vec4 params{0.0f};
	};

	struct UniformBuffers
	{
		std::unique_ptr<vkb::core::BufferC> scene;
	} uniform_buffers;

	struct Parameters
	{
		int         dataset_index          = 0;
		float       scale                  = 1.0f;
		glm::vec3   translation{0.0f, 0.0f, 0.0f};
		bool        animate                = true;
		float       animation_speed        = 1.0f;
		bool        use_dataset_radius     = true;
		float       dataset_min_thickness  = 0.003f;
		float       dataset_max_thickness  = 0.02f;
		float       fixed_radius           = 0.006f;
		float       growth_progress        = 1.0f;
		bool        growth_auto            = false;
		float       growth_speed           = 0.6f;
		int         lighting_model         = 1;
		int         sides                  = 16;
		bool        orbit_enabled          = true;
		float       orbit_radius           = 3.5f;
		float       orbit_speed            = 0.35f;
	} parameters;

	struct PlaybackState
	{
		uint32_t frame_index = 0;
		float    accumulator = 0.0f;
	} playback;

	std::unique_ptr<vkb::core::BufferC> vertex_buffer;
	std::unique_ptr<vkb::core::BufferC> index_buffer;
	uint32_t                             index_count = 0;

	FrameGeometry                                   current_geometry;
	std::vector<std::string>                       current_frames;
	std::unordered_map<std::string, FrameGeometry> frame_cache;
	std::vector<SegmentRange>                      segment_ranges;
	float                                           last_growth_used = 1.0f;
	float                                           orbit_time      = 0.0f;

	int selected_segment_index = -1;
	int hovered_segment_index  = -1;

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
	bool build_mesh_from_segments(const FrameGeometry &geometry, std::vector<TreeVertex> &out_vertices,
	                              std::vector<uint32_t> &out_indices, float growth_progress,
	                              std::vector<SegmentRange> &out_ranges) const;

	bool set_dataset(int dataset_index);
	bool load_frame(uint32_t frame_index);
	bool upload_geometry(const std::vector<TreeVertex> &vertices, const std::vector<uint32_t> &indices);
	FrameGeometry parse_vtk_file(const std::string &relative_path) const;
	glm::mat4 build_model_matrix() const;
	float    compute_auto_scale() const;

	int  pick_segment(int32_t mouse_x, int32_t mouse_y);
	bool compute_ray(float mouse_x, float mouse_y, glm::vec3 &out_origin, glm::vec3 &out_direction) const;
	float distance_ray_segment(const glm::vec3 &ray_origin, const glm::vec3 &ray_dir,
	                            const glm::vec3 &a, const glm::vec3 &b, float &out_t) const;
};

std::unique_ptr<vkb::Application> create_cco_tree_3d();
