/* Copyright (c) 2018 Hans-Kristian Arntzen
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "vulkan.h"
#include <stdint.h>
#include <memory>
#include <vector>

#if defined(_MSC_VER) && (_MSC_VER <= 1800)
#define FOSSILIZE_NOEXCEPT
#else
#define FOSSILIZE_NOEXCEPT noexcept
#endif

namespace Fossilize
{
class Exception : public std::exception
{
public:
	Exception(const char *what)
		: msg(what)
	{
	}

	const char *what() const FOSSILIZE_NOEXCEPT override
	{
		return msg;
	}

private:
	const char *msg;
};

using Hash = uint64_t;

class ScratchAllocator
{
public:
	// alignof(T) doesn't work on MSVC 2013.
	template <typename T>
	T *allocate()
	{
		return static_cast<T *>(allocate_raw(sizeof(T), 16));
	}

	template <typename T>
	T *allocate_cleared()
	{
		return static_cast<T *>(allocate_raw_cleared(sizeof(T), 16));
	}

	template <typename T>
	T *allocate_n(size_t count)
	{
		if (count == 0)
			return nullptr;
		return static_cast<T *>(allocate_raw(sizeof(T) * count, 16));
	}

	template <typename T>
	T *allocate_n_cleared(size_t count)
	{
		if (count == 0)
			return nullptr;
		return static_cast<T *>(allocate_raw_cleared(sizeof(T) * count, 16));
	}

	void *allocate_raw(size_t size, size_t alignment);
	void *allocate_raw_cleared(size_t size, size_t alignment);

private:
	struct Block
	{
		Block(size_t size);
		size_t offset = 0;
		std::vector<uint8_t> blob;
	};
	std::vector<Block> blocks;

	void add_block(size_t minimum_size);
};

class StateCreatorInterface
{
public:
	virtual ~StateCreatorInterface() = default;
	virtual bool set_num_samplers(unsigned /*count*/) { return true; }
	virtual bool set_num_descriptor_set_layouts(unsigned /*count*/) { return true; }
	virtual bool set_num_pipeline_layouts(unsigned /*count*/) { return true; }
	virtual bool set_num_shader_modules(unsigned /*count*/) { return true; }
	virtual bool set_num_render_passes(unsigned /*count*/) { return true; }
	virtual bool set_num_compute_pipelines(unsigned /*count*/) { return true; }
	virtual bool set_num_graphics_pipelines(unsigned /*count*/) { return true; }

	virtual bool enqueue_create_sampler(Hash hash, unsigned index, const VkSamplerCreateInfo *create_info, VkSampler *sampler) = 0;
	virtual bool enqueue_create_descriptor_set_layout(Hash hash, unsigned index, const VkDescriptorSetLayoutCreateInfo *create_info, VkDescriptorSetLayout *layout) = 0;
	virtual bool enqueue_create_pipeline_layout(Hash hash, unsigned index, const VkPipelineLayoutCreateInfo *create_info, VkPipelineLayout *layout) = 0;
	virtual bool enqueue_create_shader_module(Hash hash, unsigned index, const VkShaderModuleCreateInfo *create_info, VkShaderModule *module) = 0;
	virtual bool enqueue_create_render_pass(Hash hash, unsigned index, const VkRenderPassCreateInfo *create_info, VkRenderPass *render_pass) = 0;
	virtual bool enqueue_create_compute_pipeline(Hash hash, unsigned index, const VkComputePipelineCreateInfo *create_info, VkPipeline *pipeline) = 0;
	virtual bool enqueue_create_graphics_pipeline(Hash hash, unsigned index, const VkGraphicsPipelineCreateInfo *create_info, VkPipeline *pipeline) = 0;
	virtual void wait_enqueue() {}
};

class StateReplayer
{
public:
	StateReplayer();
	~StateReplayer();
	void parse(StateCreatorInterface &iface, const void *buffer, size_t size);
	ScratchAllocator &get_allocator();

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

class StateRecorder
{
public:
	StateRecorder();
	~StateRecorder();
	ScratchAllocator &get_allocator();

	// TODO: create_device which can capture which features/exts are used to create the device.
	// This can be relevant when using more exotic features.

	unsigned register_descriptor_set_layout(Hash hash, const VkDescriptorSetLayoutCreateInfo &layout_info);
	unsigned register_pipeline_layout(Hash hash, const VkPipelineLayoutCreateInfo &layout_info);
	unsigned register_shader_module(Hash hash, const VkShaderModuleCreateInfo &create_info);
	unsigned register_graphics_pipeline(Hash hash, const VkGraphicsPipelineCreateInfo &create_info);
	unsigned register_compute_pipeline(Hash hash, const VkComputePipelineCreateInfo &create_info);
	unsigned register_render_pass(Hash hash, const VkRenderPassCreateInfo &create_info);
	unsigned register_sampler(Hash hash, const VkSamplerCreateInfo &create_info);

	void set_descriptor_set_layout_handle(unsigned index, VkDescriptorSetLayout layout);
	void set_pipeline_layout_handle(unsigned index, VkPipelineLayout layout);
	void set_shader_module_handle(unsigned index, VkShaderModule module);
	void set_graphics_pipeline_handle(unsigned index, VkPipeline pipeline);
	void set_compute_pipeline_handle(unsigned index, VkPipeline pipeline);
	void set_render_pass_handle(unsigned index, VkRenderPass render_pass);
	void set_sampler_handle(unsigned index, VkSampler sampler);

	Hash get_hash_for_descriptor_set_layout(VkDescriptorSetLayout layout) const;
	Hash get_hash_for_pipeline_layout(VkPipelineLayout layout) const;
	Hash get_hash_for_shader_module(VkShaderModule module) const;
	Hash get_hash_for_graphics_pipeline_handle(VkPipeline pipeline) const;
	Hash get_hash_for_compute_pipeline_handle(VkPipeline pipeline) const;
	Hash get_hash_for_render_pass(VkRenderPass render_pass) const;
	Hash get_hash_for_sampler(VkSampler sampler) const;

	std::vector<uint8_t> serialize() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

namespace Hashing
{
Hash compute_hash_descriptor_set_layout(const StateRecorder &recorder, const VkDescriptorSetLayoutCreateInfo &layout);
Hash compute_hash_pipeline_layout(const StateRecorder &recorder, const VkPipelineLayoutCreateInfo &layout);
Hash compute_hash_shader_module(const StateRecorder &recorder, const VkShaderModuleCreateInfo &create_info);
Hash compute_hash_graphics_pipeline(const StateRecorder &recorder, const VkGraphicsPipelineCreateInfo &create_info);
Hash compute_hash_compute_pipeline(const StateRecorder &recorder, const VkComputePipelineCreateInfo &create_info);
Hash compute_hash_render_pass(const StateRecorder &recorder, const VkRenderPassCreateInfo &create_info);
Hash compute_hash_sampler(const StateRecorder &recorder, const VkSamplerCreateInfo &create_info);
}

}
