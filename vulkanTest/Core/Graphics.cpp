
#include<iterator>
#include"Graphics.h"
#define VK_USE_PLATFORM_WIN32_KHR
#include<vulkan/vulkan.hpp>
#include<vulkan/vk_sdk_platform.h>
#include<vulkan/vulkan_win32.h>

#include"..\Utilities\Settings.h"
#include"..\Utilities\Log.h"

#pragma comment(lib, "vulkan-1.lib")

class Graphics::Impl
{
public:
	const std::unique_ptr<Window>& window_;

	vk::Instance instance_;
	vk::PhysicalDevice gpu_;
	vk::PhysicalDeviceProperties gpu_props_;
	std::unique_ptr<vk::QueueFamilyProperties[]> queue_props_;
	uint32_t queue_family_count_;
	uint32_t graphics_queue_family_index_;
	vk::Device device_;
	vk::CommandPool command_pool_;
	vk::Fence fence_;
	// swap chain
	vk::SurfaceKHR surface_;
	vk::PresentInfoKHR present_info_;
	vk::SwapchainKHR swap_chain_;
	vk::ImageView image_view_;
	vk::RenderPass render_pass_;
	vk::Framebuffer frame_buffer_;

	Impl(std::unique_ptr<Window>& window) : window_(window){}

	bool CreateInstance(void);
	bool CreateDebugLayer(void);
	bool EnumeratePhysicalDevice(void);
	bool CreateSurface(void);
	bool CreateDevice(void);
	bool CreateCommandPool(void);		// similar command allocater
	bool CreateFence(void);				// sync mechanism
	bool CreateSwapChain(void);
	bool CreateImageView(void);
	bool CreateRenderPass(void);
	bool CreateFrameBuffer(void);
	bool CreatePipeline(void);

	bool check_layers(uint32_t check_count, char const *const *const check_names, uint32_t layer_count, vk::LayerProperties *layers)
	{
		for (uint32_t i = 0; i < check_count; ++i) {
			bool found = false;
			for (uint32_t j = 0; j < layer_count; ++j) {
				if (!strcmp(check_names[i], layers[j].layerName)) {
					found = true;
					break;
				}
			}
			if (!found) {
				Log::Error("Cannot find layer: %s", check_names[i]);
				return 0;
			}
		}
		return true;
	}
};

Graphics::Graphics(std::unique_ptr<Window>& window) : impl_(std::make_unique<Impl>(window)) {}

Graphics::~Graphics() = default;

bool Graphics::Initialize(void)
{
	if (!impl_->CreateInstance()) return false;

	if (!impl_->EnumeratePhysicalDevice()) return false;

	if (!impl_->CreateSurface()) return false;
	
	if (!impl_->CreateDevice()) return false;

	if (!impl_->CreateCommandPool()) return false;

	if (!impl_->CreateFence()) return false;

	return true;
}

bool Graphics::Run(void)
{
	return true;
}

void Graphics::Exit(void)
{

}

void Graphics::BeginFrame(void)
{

}

void Graphics::EndFrame(void)
{

}

bool Graphics::Impl::CreateInstance(void)
{
	auto const app_info = vk::ApplicationInfo()
		.setPApplicationName(Settings::application_name<char[]>)
		.setApplicationVersion(0)
		.setPEngineName(Settings::application_name<char[]>)
		.setEngineVersion(0)
		.setApiVersion(VK_API_VERSION_1_0);

	unsigned int layer_count = 1;
	const char* validation_layers[] = { "VK_LAYER_LUNARG_standard_validation" };
	unsigned int extension_count = 3;
	const char* validation_extention[] =
	{
		VK_KHR_SURFACE_EXTENSION_NAME,			// necessary
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,	// necessary
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME
	};

	auto const instance_info = vk::InstanceCreateInfo()
		.setPApplicationInfo(&app_info)
		.setEnabledLayerCount(layer_count)
		.setPpEnabledLayerNames(validation_layers)
		.setEnabledExtensionCount(extension_count)
		.setPpEnabledExtensionNames(validation_extention);

	auto result = vk::createInstance(&instance_info, nullptr, &instance_);
	assert(result == vk::Result::eSuccess);

	return true;
}

bool Graphics::Impl::CreateDebugLayer(void)
{
	return true;
}

bool Graphics::Impl::EnumeratePhysicalDevice(void)
{
	uint32_t gpu_count;
	instance_.enumeratePhysicalDevices(&gpu_count, nullptr);

	if (gpu_count > 0)
	{
		std::unique_ptr<vk::PhysicalDevice[]> physical_devices(new vk::PhysicalDevice[gpu_count]);
		auto result = instance_.enumeratePhysicalDevices(&gpu_count, physical_devices.get());
		assert(result == vk::Result::eSuccess);

		for (unsigned int i = 0; i < gpu_count; ++i)
		{
			gpu_ = physical_devices[i];

			gpu_.getProperties(&gpu_props_);

			Log::Info("================ VulkanPhysicalDevice[%d/%d] ================", i + 1, gpu_count);
			Log::Info("%s", gpu_props_.deviceName);
			Log::Info("apiVersion = %d.%d.%d\n",
				VK_VERSION_MAJOR(gpu_props_.apiVersion),
				VK_VERSION_MINOR(gpu_props_.apiVersion),
				VK_VERSION_PATCH(gpu_props_.apiVersion));

			gpu_.getProperties(&gpu_props_);

			uint32_t family_count;
			std::unique_ptr<vk::QueueFamilyProperties[]> queue_props;

			gpu_.getQueueFamilyProperties(&family_count, nullptr);
			queue_props.reset(new vk::QueueFamilyProperties[family_count]);
			gpu_.getQueueFamilyProperties(&family_count, queue_props.get());

			for (unsigned int j = 0; j < family_count; ++j)
			{
				std::string str = "";
				if (queue_props[j].queueFlags && VK_QUEUE_GRAPHICS_BIT)       str += "GRAPHICS ";
				if (queue_props[j].queueFlags && VK_QUEUE_COMPUTE_BIT)       str += "COMPUTE ";
				if (queue_props[j].queueFlags && VK_QUEUE_TRANSFER_BIT)       str += "TRANSFER ";
				if (queue_props[j].queueFlags && VK_QUEUE_SPARSE_BINDING_BIT) str += "SPARSE ";
				if (queue_props[j].queueFlags && VK_QUEUE_PROTECTED_BIT)      str += "PROTECTED ";

				Log::Info("QueueFamily[%d/%d] queueFlags:" + str, j + 1, family_count);
			}

			uint32_t device_extension_count = 0;

			auto result = gpu_.enumerateDeviceExtensionProperties(nullptr, &device_extension_count, nullptr);
			assert(result == vk::Result::eSuccess);
			Log::Info("Extension Count = %d", device_extension_count);
			
			/*device features*/{
				vk::PhysicalDeviceFeatures dev_features;
				gpu_.getFeatures(&dev_features);

				Log::Info("enable features : true = 1, false = 0\n");
				Log::Info("robustBufferAccess = %d", dev_features.robustBufferAccess);
				Log::Info("fullDrawIndexUint32 = %d", dev_features.fullDrawIndexUint32);
				Log::Info("imageCubeArray = %d", dev_features.imageCubeArray);
				Log::Info("independentBlend = %d", dev_features.independentBlend);
				Log::Info("geometryShader = %d", dev_features.geometryShader);
				Log::Info("tessellationShader = %d", dev_features.tessellationShader);
				Log::Info("sampleRateShading = %d", dev_features.sampleRateShading);
				Log::Info("dualSrcBlend = %d", dev_features.dualSrcBlend);
				Log::Info("logicOp = %d", dev_features.logicOp);
				Log::Info("multiDrawIndirect = %d", dev_features.multiDrawIndirect);
				Log::Info("drawIndirectFirstInstance = %d", dev_features.drawIndirectFirstInstance);
				Log::Info("depthClamp = %d", dev_features.depthClamp);
				Log::Info("depthBiasClamp = %d", dev_features.depthBiasClamp);
				Log::Info("fillModeNonSolid = %d", dev_features.fillModeNonSolid);
				Log::Info("depthBounds = %d", dev_features.depthBounds);
				Log::Info("wideLines = %d", dev_features.wideLines);
				Log::Info("largePoints = %d", dev_features.largePoints);
				Log::Info("alphaToOne = %d", dev_features.alphaToOne);
				Log::Info("multiViewport = %d", dev_features.multiViewport);
				Log::Info("samplerAnisotropy = %d", dev_features.samplerAnisotropy);
				Log::Info("textureCompressionETC2 = %d", dev_features.textureCompressionETC2);
				Log::Info("textureCompressionASTC_LDR = %d", dev_features.textureCompressionASTC_LDR);
				Log::Info("textureCompressionBC = %d", dev_features.textureCompressionBC);
				Log::Info("occlusionQueryPrecise = %d", dev_features.occlusionQueryPrecise);
				Log::Info("pipelineStatisticsQuery = %d", dev_features.pipelineStatisticsQuery);
				Log::Info("vertexPipelineStoresAndAtomics = %d", dev_features.vertexPipelineStoresAndAtomics);
				Log::Info("fragmentStoresAndAtomics = %d", dev_features.fragmentStoresAndAtomics);
				Log::Info("shaderTessellationAndGeometryPointSize = %d", dev_features.shaderTessellationAndGeometryPointSize);
				Log::Info("shaderImageGatherExtended = %d", dev_features.shaderImageGatherExtended);
				Log::Info("shaderStorageImageExtendedFormats = %d", dev_features.shaderStorageImageExtendedFormats);
				Log::Info("shaderStorageImageMultisample = %d", dev_features.shaderStorageImageMultisample);
				Log::Info("shaderStorageImageReadWithoutFormat = %d", dev_features.shaderStorageImageReadWithoutFormat);
				Log::Info("shaderStorageImageWriteWithoutFormat = %d", dev_features.shaderStorageImageWriteWithoutFormat);
				Log::Info("shaderUniformBufferArrayDynamicIndexing = %d", dev_features.shaderUniformBufferArrayDynamicIndexing);
				Log::Info("shaderSampledImageArrayDynamicIndexing = %d", dev_features.shaderSampledImageArrayDynamicIndexing);
				Log::Info("shaderStorageBufferArrayDynamicIndexing = %d", dev_features.shaderStorageBufferArrayDynamicIndexing);
				Log::Info("shaderStorageImageArrayDynamicIndexing = %d", dev_features.shaderStorageImageArrayDynamicIndexing);
				Log::Info("shaderClipDistance = %d", dev_features.shaderClipDistance);
				Log::Info("shaderCullDistance = %d", dev_features.shaderCullDistance);
				Log::Info("shaderFloat64 = %d", dev_features.shaderFloat64);
				Log::Info("shaderInt64 = %d", dev_features.shaderInt64);
				Log::Info("shaderInt16 = %d", dev_features.shaderInt16);
				Log::Info("shaderResourceResidency = %d", dev_features.shaderResourceResidency);
				Log::Info("shaderResourceMinLod = %d", dev_features.shaderResourceMinLod);
				Log::Info("sparseBinding = %d", dev_features.sparseBinding);
				Log::Info("sparseResidencyBuffer = %d", dev_features.sparseResidencyBuffer);
				Log::Info("sparseResidencyImage2D = %d", dev_features.sparseResidencyImage2D);
				Log::Info("sparseResidencyImage3D = %d", dev_features.sparseResidencyImage3D);
				Log::Info("sparseResidency2Samples = %d", dev_features.sparseResidency2Samples);
				Log::Info("sparseResidency4Samples = %d", dev_features.sparseResidency4Samples);
				Log::Info("sparseResidency8Samples = %d", dev_features.sparseResidency8Samples);
				Log::Info("sparseResidency16Samples = %d", dev_features.sparseResidency16Samples);
				Log::Info("sparseResidencyAliased = %d", dev_features.sparseResidencyAliased);
				Log::Info("variableMultisampleRate = %d", dev_features.variableMultisampleRate);
				Log::Info("inheritedQueries = %d", dev_features.inheritedQueries);
			}

			/*memory propaties*/{
				vk::PhysicalDeviceMemoryProperties memory_props;
				gpu_.getMemoryProperties(&memory_props);
			}
		}

		/* For demo we just grab the first physical device */
		gpu_ = physical_devices[0];

		uint32_t device_extension_count = 0;
		bool swapchain_ext_found = false;
		unsigned int enabled_extension_count = 0;

		result = gpu_.enumerateDeviceExtensionProperties(nullptr, &device_extension_count, nullptr);
		assert(result == vk::Result::eSuccess);

		if (device_extension_count > 0)
		{
			std::unique_ptr<vk::ExtensionProperties[]> device_extensions(new vk::ExtensionProperties[device_extension_count]);
			result = gpu_.enumerateDeviceExtensionProperties(nullptr, &device_extension_count, device_extensions.get());
			assert(result == vk::Result::eSuccess);

			for (uint32_t i = 0; i < device_extension_count; ++i)
			{
				if (!strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME, device_extensions[i].extensionName))
				{
					swapchain_ext_found = true;
					//extension_names[enabled_extension_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
				}
				assert(enabled_extension_count < 64);
			}
		}

		if (!swapchain_ext_found)
		{
			Log::Error("vkEnumerateDeviceExtensionProperties failed to find the " VK_KHR_SWAPCHAIN_EXTENSION_NAME
				" extension.\n\n"
				"Do you have a compatible Vulkan installable client driver (ICD) installed?\n"
				"Please look at the Getting Started guide for additional information.\n",
				"vkCreateInstance Failure");
		}

		gpu_.getQueueFamilyProperties(&queue_family_count_, nullptr);
		queue_props_.reset(new vk::QueueFamilyProperties[queue_family_count_]);
		gpu_.getQueueFamilyProperties(&queue_family_count_, queue_props_.get());
	}
	else
	{
		Log::Error("vkEnumeratePhysicalDevices reported zero accessible devices.");

		return false;
	}

	return true;
}

bool Graphics::Impl::CreateSurface(void)
{
	auto const create_info = vk::Win32SurfaceCreateInfoKHR()
		.setHinstance(window_->GetWindowInstance())
		.setHwnd(window_->GetWindowHandle());

	auto result = instance_.createWin32SurfaceKHR(&create_info, nullptr, &surface_);
	assert(result == vk::Result::eSuccess);

	return true;
}

bool Graphics::Impl::CreateDevice(void)
{
	graphics_queue_family_index_ = UINT32_MAX;
	uint32_t present_queue_family_index = UINT32_MAX;
	for (uint32_t i = 0; i < queue_family_count_; ++i)
	{
		if (queue_props_[i].queueFlags & vk::QueueFlagBits::eGraphics)
		{
			graphics_queue_family_index_ = i;
		}
	}

	float const priorities[] = { 0.0 };

	vk::DeviceQueueCreateInfo queue;
	queue.setQueueFamilyIndex(graphics_queue_family_index_);
	queue.setQueueCount(1);
	queue.setPQueuePriorities(priorities);

	const char *extention_name[] = { "VK_KHR_swapchain" };

	auto device_info = vk::DeviceCreateInfo()
		.setQueueCreateInfoCount(1)
		.setPQueueCreateInfos(&queue)
		.setEnabledLayerCount(0)
		.setPpEnabledLayerNames(nullptr)
		.setEnabledExtensionCount(1)
		.setPpEnabledExtensionNames(reinterpret_cast<const char* const*>(extention_name))
		.setPEnabledFeatures(nullptr);

	auto result = gpu_.createDevice(&device_info, nullptr, &device_);
	assert(result == vk::Result::eSuccess);

	return true;
}

bool Graphics::Impl::CreateCommandPool(void)
{
	auto command_info = vk::CommandPoolCreateInfo()
		.setQueueFamilyIndex(graphics_queue_family_index_)
		.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

	auto result = device_.createCommandPool(&command_info, nullptr, &command_pool_);
	assert(result == vk::Result::eSuccess);

	return true;
}

bool Graphics::Impl::CreateFence(void)
{
	auto fence_info = vk::FenceCreateInfo();

	auto result = device_.createFence(&fence_info, nullptr, &fence_);
	assert(result == vk::Result::eSuccess);

	return true;
}

bool Graphics::Impl::CreateSwapChain(void)
{
	return true;
}

bool Graphics::Impl::CreateImageView(void)
{
	return true;
}

bool Graphics::Impl::CreateRenderPass(void)
{
	return true;
}

bool Graphics::Impl::CreateFrameBuffer(void)
{
	return true;
}

bool Graphics::Impl::CreatePipeline(void)
{
	return true;
}