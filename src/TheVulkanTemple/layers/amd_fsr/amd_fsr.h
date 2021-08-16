#ifndef THEVULKANTEMPLE_AMD_FSR_H
#define THEVULKANTEMPLE_AMD_FSR_H

#include "../../volk.h"
#include "../../vulkan_helper.h"
#include <unordered_map>
#include <array>
#include <string>

class AmdFsr {
    public:
        enum class Preset : uint32_t {
            NONE = 0,
            PERFORMANCE = 1,
            BALANCED = 2,
            QUALITY = 3,
            ULTRA_QUALITY = 4
        };

        enum class Precision {
            FP32,
            FP16
        };

        struct Settings {
            Preset preset;
            Precision precision;
        };

    AmdFsr(VkDevice device, Settings fsr_settings, std::string shader_dir_path);
    ~AmdFsr();

	VkExtent2D get_recommended_input_resolution(VkExtent2D display_image_size);
	float get_negative_mip_bias();
    VkBuffer get_device_buffer() { return device_fsr_constants_buffer; };
    VkImage get_device_image() { return device_out_easu_image; };
    std::pair<std::unordered_map<VkDescriptorType, uint32_t>, uint32_t> get_required_descriptor_pool_size_and_sets();

    void create_resources(VkExtent2D input_image_size, VkExtent2D output_image_size);
    void set_rcas_sharpness(float sharpness); // If not called, 0.2 value is put by default
    void allocate_descriptor_sets(VkDescriptorPool descriptor_pool, VkImageView input_image_view, VkImageView out_image_view);
    void record_into_command_buffer(VkCommandBuffer command_buffer, VkImage input_image, VkImage output_image);

    private:
		// Array that when indexed by Preset gives the ratio between input and output resolutions
		std::array<float, 5> scaling_factors = {1.0f, 1.0f/2.0f, 1.0f/1.7f,
											   1.0f/1.5f, 1.0f/1.3f};

		std::array<float, 5> negative_mip_biases = {0.0f, -1.0f, -0.79f,
											-0.58f, -0.38f};

        VkDevice device;
        Settings fsr_settings;
        VkSampler common_fsr_sampler;
        VkDescriptorSetLayout common_fsr_descriptor_set_layout;
        VkBuffer device_fsr_constants_buffer = VK_NULL_HANDLE;

        std::array<VkDescriptorSet, 2> fsr_descriptor_sets; // easu + rcas

        VkPipelineLayout common_fsr_pipeline_layout;
        std::array<VkPipeline, 2> fsr_pipelines;

        VkExtent2D input_image_size;
        VkExtent2D output_image_size;

        VkImage device_out_easu_image = VK_NULL_HANDLE;
        VkImageView device_out_easu_image_view = VK_NULL_HANDLE;

        std::array<glm::uvec4, 5> fsr_constants;
        bool fsr_constants_changed = false;
        int fsr_to_copy = 6;
        static constexpr int threadGroupWorkRegionDim = 16;
        glm::uvec3 dispatch_size;

        void create_pipelines(std::string shader_dir_path);
        void create_image_views();
};


#endif
