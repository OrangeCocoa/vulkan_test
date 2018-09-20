
#include<vector>
#include<sstream>
#include<iostream>
#include<iterator>
#include"Graphics.h"
#define VK_USE_PLATFORM_WIN32_KHR
#define VULKAN_HPP_NO_SMART_HANDLE
#define VULKAN_HPP_NO_EXCEPTIONS
#include<vulkan/vulkan.hpp>
#include<vulkan/vk_sdk_platform.h>
#include<vulkan/vulkan_win32.h>

#include"..\Utilities\Settings.h"
#include"..\Utilities\Log.h"

#pragma comment(lib, "vulkan-1.lib")

// this call back is global
VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugMessageCallback(
	vk::DebugReportFlagsEXT flags,
	vk::DebugReportObjectTypeEXT objType,
	uint64_t srcObject,
	size_t location,
	int32_t msgCode,
	const char* pLayerPrefix,
	const char* pMsg,
	void* pUserData);

class Graphics::Impl
{
public:
	struct Depth
	{
		vk::Format				format;
		vk::Image				image;
		vk::ImageView			view;
		vk::MemoryAllocateInfo	mem_alloc;
		vk::DeviceMemory		mem;
	};

	struct BufferResource
	{
		vk::Buffer			buffer;
		vk::DeviceMemory	mem;
	};

	const std::unique_ptr<Window>&					window_;

	vk::Instance									instance_;
	vk::PhysicalDevice								gpu_;
	vk::PhysicalDeviceProperties					gpu_props_;
	vk::Device										device_;
	vk::PipelineCache								pipeline_cache_;
	vk::Queue										queue_;
	vk::CommandPool									command_pool_;
	std::vector<vk::CommandBuffer>					command_buffers_;
	vk::Semaphore									present_complete_semaphore_;
	vk::Semaphore									draw_complete_semaphore_;
	Depth											depth_target_;
	BufferResource									vertex_buffer_, index_buffer_;

	std::unique_ptr<vk::QueueFamilyProperties[]>	queue_props_;
	uint32_t										queue_family_count_;

#if defined(_DEBUG)
	PFN_vkCreateDebugReportCallbackEXT				create_debug_report_callback_;
	PFN_vkDestroyDebugReportCallbackEXT				destroy_debug_report_callback_;
	PFN_vkDebugReportMessageEXT						debug_break_callback_;
	VkDebugReportCallbackEXT						msg_callback_;
#endif

	// swap chain
	struct SwapchainImageResources
	{
		vk::Image		image;
		vk::ImageView	view;
		vk::Fence		fence;
		vk::Framebuffer	frame_buffer;
	};

	vk::Format										swap_target_format_;
	uint32_t										sc_image_count_;
	uint32_t										graphics_queue_family_index_;
	uint32_t										sc_current_image_;
	std::unique_ptr<SwapchainImageResources[]>		sc_resources_;
	vk::SurfaceKHR									surface_;
	vk::SwapchainKHR								swap_chain_;
	vk::PresentInfoKHR								present_info_;

	vk::PhysicalDeviceMemoryProperties mem_props_;
	vk::RenderPass render_pass_;

	static const char* debug_layers_[];

	Impl(std::unique_ptr<Window>& window)
		: window_(window)
		, sc_image_count_(0)
		, sc_current_image_(0)
#if defined(_DEBUG)
		, create_debug_report_callback_(VK_NULL_HANDLE)
		, destroy_debug_report_callback_(VK_NULL_HANDLE)
		, debug_break_callback_(VK_NULL_HANDLE)
#endif
	{
		present_info_.setSwapchainCount(1)
			.setPSwapchains(&swap_chain_)
			.setPImageIndices(&sc_current_image_);
	}

	bool CreateInstance(void);
	bool CreateDebugLayer(void);
	bool EnumeratePhysicalDevice(void);
	bool CreateSurface(void);
	bool CreateDevice(void);
	bool CreateQueue(void);
	bool CreatePipelineCache(void);
	bool CreateCommandPool(void);		// similar command allocater
	bool CreateSwapChain(void);
	bool CreateRenderPass(void);
	bool InitSemaphoreSettings(void);
	bool CreateCommandBufffer(void);
	bool CreateSwapChainResources(void);
	bool CreateDepthImage(void);

	bool CreateFrameBuffer(void);

	uint32_t FindQueue(vk::QueueFlags flag)
	{
		for (uint32_t i = 0; i < queue_family_count_; ++i)
		{
			if (queue_props_[i].queueFlags & flag)
			{
				uint32_t supported;
				if (gpu_.getSurfaceSupportKHR(i, surface_, &supported) != vk::Result::eSuccess)
				{
					continue;
				}

				return i;
			}
		}

		return 0xffffffff;
	}

	uint32_t FindMemoryTypeIndex(uint32_t type_bits, vk::MemoryPropertyFlags requirements_mask)
	{
		// Search memtypes to find first index with those properties
		for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; ++i)
		{
			if ((type_bits & 1) == 1)
			{
				// Type is available, does it match user properties?
				if ((mem_props_.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask)
				{
					return i;
				}
			}
			type_bits >>= 1;
		}

		// No memory types matched, return failure
		return 0xffffffff;
	}

	uint32_t AcquireNextImage(vk::Semaphore present_completed)
	{
		auto result = device_.acquireNextImageKHR(swap_chain_, UINT64_MAX, present_completed, vk::Fence());

		if (result.result != vk::Result::eSuccess)
		{
			return sc_current_image_;
		}

		sc_current_image_ = result.value;
		return sc_current_image_;
	}

	void SetPipelineBarrier(
		vk::CommandBuffer cmd_buffer,
		vk::Image image,
		vk::ImageLayout old_layout,
		vk::ImageLayout new_layout,
		vk::ImageSubresourceRange sub_range)
	{
		vk::ImageMemoryBarrier image_memory_barrier = vk::ImageMemoryBarrier()
			.setImage(image)
			.setOldLayout(old_layout)
			.setNewLayout(new_layout)
			.setSubresourceRange(sub_range);

		/*old layout*/ {
			if (old_layout == vk::ImageLayout::ePreinitialized)
				image_memory_barrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
			// transfer destination
			else if (old_layout == vk::ImageLayout::eTransferDstOptimal)
				image_memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			// transfer source
			else if (old_layout == vk::ImageLayout::eTransferSrcOptimal)
				image_memory_barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
			// color
			else if (old_layout == vk::ImageLayout::eColorAttachmentOptimal)
				image_memory_barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
			// depth stencil
			else if (old_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
				image_memory_barrier.srcAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			// shader resource
			else if (old_layout == vk::ImageLayout::eShaderReadOnlyOptimal)
				image_memory_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
		}

		/*next layout*/ {
			// transfer destination
			if (new_layout == vk::ImageLayout::eTransferDstOptimal)
				image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
			// transfer source
			else if (new_layout == vk::ImageLayout::eTransferSrcOptimal)
				image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;// | image_memory_barrier.srcAccessMask;
																					   // color																																	 // ÉJÉâÅ[
			else if (new_layout == vk::ImageLayout::eColorAttachmentOptimal)
				image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
			// depth stencil
			else if (new_layout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
				image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
			// shader resource
			else if (new_layout == vk::ImageLayout::eShaderReadOnlyOptimal)
				image_memory_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		}

		cmd_buffer.pipelineBarrier(
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::DependencyFlags(),
			nullptr, nullptr, image_memory_barrier);
	}

	void Present(vk::Semaphore wait_semaphore)
	{
		present_info_.waitSemaphoreCount = wait_semaphore ? 1 : 0;
		present_info_.pWaitSemaphores = &wait_semaphore;
		queue_.presentKHR(present_info_);
	}

	vk::Fence GetSubmitFence(bool destroy = false)
	{
		auto& sc_resouce = sc_resources_[sc_current_image_];

		while (sc_resouce.fence)
		{
			auto fence_res = device_.waitForFences(sc_resouce.fence, VK_TRUE, 100000000000);
			if (fence_res == vk::Result::eSuccess)
			{
				if (destroy)
				{
					device_.destroyFence(sc_resouce.fence);
				}
				sc_resouce.fence = vk::Fence();
			}
		}

		device_.createFence(&vk::FenceCreateInfo(), nullptr, &sc_resouce.fence);
		return sc_resouce.fence;
	}
};

const char* Graphics::Impl::debug_layers_[] = { "VK_LAYER_LUNARG_standard_validation" };

Graphics::Graphics(std::unique_ptr<Window>& window) : impl_(std::make_unique<Impl>(window)) {}

Graphics::~Graphics() = default;

bool Graphics::Initialize(void)
{
	if (!impl_->CreateInstance()) return false;

	if (!impl_->EnumeratePhysicalDevice()) return false;

	if (!impl_->CreateSurface()) return false;

	if (!impl_->CreateDevice()) return false;

	if (!impl_->CreateDebugLayer()) return false;

	if (!impl_->CreatePipelineCache()) return false;

	if (!impl_->CreateQueue()) return false;

	if (!impl_->CreateCommandPool()) return false;

	if (!impl_->CreateSwapChain()) return false;

	if (!impl_->CreateRenderPass()) return false;

	if (!impl_->InitSemaphoreSettings()) return false;

	if (!impl_->CreateCommandBufffer()) return false;

	if (!impl_->CreateSwapChainResources()) return false;

	if (!impl_->CreateDepthImage()) return false;

	//if (!impl_->CreateFrameBuffer()) return false;

	return true;
}

bool Graphics::Run(void)
{
	// get next buffer
	uint32_t current_buffer = impl_->AcquireNextImage(impl_->present_complete_semaphore_);

	auto& cmd_buffer = impl_->command_buffers_[current_buffer];

	/*command buffer stack*/ {
		cmd_buffer.reset(vk::CommandBufferResetFlagBits::eReleaseResources);
		vk::CommandBufferBeginInfo cmd_buf_info;
		cmd_buffer.begin(cmd_buf_info);

		vk::ClearColorValue clear_color(std::array<float, 4>{ 0.0f, 0.5f, 0.5f, 1.0f });

		vk::ImageSubresourceRange image_sub_range = vk::ImageSubresourceRange()
			.setAspectMask(vk::ImageAspectFlagBits::eColor)
			.setLevelCount(1)
			.setLayerCount(1);

		auto& color_image = impl_->sc_resources_[impl_->sc_current_image_].image;

		impl_->SetPipelineBarrier(
			cmd_buffer,
			color_image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			image_sub_range);

		cmd_buffer.clearColorImage(color_image, vk::ImageLayout::eTransferDstOptimal, clear_color, image_sub_range);

		impl_->SetPipelineBarrier(
			cmd_buffer,
			color_image,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::ePresentSrcKHR,
			image_sub_range);

		cmd_buffer.end();
	}

	/*command buffer submit*/ {
		vk::PipelineStageFlags pipeline_stages = vk::PipelineStageFlagBits::eBottomOfPipe;
		vk::SubmitInfo submitInfo;
		submitInfo.pWaitDstStageMask = &pipeline_stages;
		// wait semaphore
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &impl_->present_complete_semaphore_;
		// pass command buffer
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &impl_->command_buffers_[current_buffer];
		// render semaphore registering
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &impl_->draw_complete_semaphore_;

		// submit at queue
		vk::Fence fence = impl_->GetSubmitFence(true);
		impl_->queue_.submit(submitInfo, fence);
		auto result = impl_->device_.waitForFences(fence, VK_TRUE, 100000000000);
		assert(result == vk::Result::eSuccess);
		//impl_->queue_.waitIdle();
	}

	impl_->Present(impl_->draw_complete_semaphore_);

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
		.setPEngineName(Settings::application_name<char[]>)
		.setApiVersion(VK_API_VERSION_1_0);

	const char* extention[] =
	{
		VK_KHR_SURFACE_EXTENSION_NAME,			// necessary
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,	// necessary win32
#if defined(_DEBUG)
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME
#endif
	};
	unsigned int extension_count = std::size(extention);

	auto const instance_info = vk::InstanceCreateInfo()
		.setPApplicationInfo(&app_info)
#if defined(_DEBUG)
		.setEnabledLayerCount(std::size(debug_layers_))
		.setPpEnabledLayerNames(debug_layers_)
#endif
		.setEnabledExtensionCount(extension_count)
		.setPpEnabledExtensionNames(extention);

	vk::createInstance(&instance_info, nullptr, &instance_);

	if (!instance_)
	{
		Log::Info("Instance cannot create.");
		return false;
	}

	Log::Info("Instance create done.");

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

			Log::Info("\n================ VulkanPhysicalDevice[%d/%d] ================", i + 1, gpu_count);
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

			/*device features*/ {
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
				Log::Info("inheritedQueries = %d\n", dev_features.inheritedQueries);
			}

			/*memory propaties*/ {
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
				"vkCreateInstance Failure\n");
		}

		gpu_.getQueueFamilyProperties(&queue_family_count_, nullptr);
		queue_props_.reset(new vk::QueueFamilyProperties[queue_family_count_]);
		gpu_.getQueueFamilyProperties(&queue_family_count_, queue_props_.get());
		gpu_.getMemoryProperties(&mem_props_);
	}
	else
	{
		Log::Error("vkEnumeratePhysicalDevices reported zero accessible devices.");

		return false;
	}

	Log::Info("Physical device captured.");

	return true;
}

bool Graphics::Impl::CreateSurface(void)
{
	auto const create_info = vk::Win32SurfaceCreateInfoKHR()
		.setHinstance(window_->GetWindowInstance())
		.setHwnd(window_->GetWindowHandle());

	auto result = instance_.createWin32SurfaceKHR(&create_info, nullptr, &surface_);
	if (result != vk::Result::eSuccess)
	{
		Log::Error("Surface cannot created.");
		return false;
	}

	Log::Info("Surface create done.");

	return true;
}

bool Graphics::Impl::CreateDevice(void)
{
	graphics_queue_family_index_ = FindQueue(vk::QueueFlagBits::eGraphics);

	float const priorities[] = { 0.0 };

	vk::DeviceQueueCreateInfo queue_info;
	queue_info.setQueueFamilyIndex(graphics_queue_family_index_)
		.setQueueCount(1)
		.setPQueuePriorities(priorities);

	const char *extention_name[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	auto device_info = vk::DeviceCreateInfo()
		.setQueueCreateInfoCount(1)
		.setPQueueCreateInfos(&queue_info)
		.setEnabledLayerCount(std::size(debug_layers_))
		.setPpEnabledLayerNames(debug_layers_)
		.setEnabledExtensionCount(std::size(extention_name))
		.setPpEnabledExtensionNames(reinterpret_cast<const char* const*>(extention_name))
		.setPEnabledFeatures(&gpu_.getFeatures());

	gpu_.createDevice(&device_info, nullptr, &device_);

	if (!device_)
	{
		Log::Info("Device cannot created.");
		return false;
	}

	Log::Info("Device create done.");

	return true;
}

bool Graphics::Impl::CreateDebugLayer(void)
{
#if defined(_DEBUG)
	{
		create_debug_report_callback_ = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(instance_.getProcAddr("vkCreateDebugReportCallbackEXT"));
		destroy_debug_report_callback_ = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(instance_.getProcAddr("vkDestroyDebugReportCallbackEXT"));
		debug_break_callback_ = reinterpret_cast<PFN_vkDebugReportMessageEXT>(instance_.getProcAddr("vkDebugReportMessageEXT"));

		vk::DebugReportFlagsEXT flags = vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eWarning;

		vk::DebugReportCallbackCreateInfoEXT dbg_create_info = vk::DebugReportCallbackCreateInfoEXT()
			.setPfnCallback(reinterpret_cast<PFN_vkDebugReportCallbackEXT>(DebugMessageCallback))
			.setFlags(static_cast<vk::DebugReportFlagsEXT>(flags.operator VkSubpassDescriptionFlags()));

		auto result = create_debug_report_callback_(static_cast<VkInstance>(instance_), reinterpret_cast<VkDebugReportCallbackCreateInfoEXT*>(&dbg_create_info), nullptr, &msg_callback_);

		if (static_cast<vk::Result>(result) != vk::Result::eSuccess)
		{
			Log::Error("Debug layer cannot created.");
			return false;
		}
	}
#endif

	return true;
}

bool Graphics::Impl::CreatePipelineCache(void)
{
	auto result = device_.createPipelineCache(vk::PipelineCacheCreateInfo());
	if (result.result != vk::Result::eSuccess)
	{
		Log::Error("Pipeline cache cannot created.");
		return false;
	}

	pipeline_cache_ = result.value;

	Log::Info("Pipeline cache create done.");

	return true;
}

bool Graphics::Impl::CreateQueue(void)
{
	device_.getQueue(graphics_queue_family_index_, 0, &queue_);

	Log::Info("Graphics queue create done.");

	return true;
}

bool Graphics::Impl::CreateCommandPool(void)
{
	auto command_info = vk::CommandPoolCreateInfo()
		.setQueueFamilyIndex(graphics_queue_family_index_)
		.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

	device_.createCommandPool(&command_info, nullptr, &command_pool_);

	if (!command_pool_)
	{
		Log::Info("Commandpool cannot created.");
		return false;
	}

	Log::Info("Commandpool create done.");

	return true;
}

bool Graphics::Impl::CreateSwapChain(void)
{
	auto old_sc = swap_chain_;

	swap_target_format_ = vk::Format::eR8G8B8A8Unorm;

	vk::SurfaceCapabilitiesKHR caps;
	auto result = gpu_.getSurfaceCapabilitiesKHR(surface_, &caps);
	assert(result == vk::Result::eSuccess);

	uint32_t format_count;
	result = gpu_.getSurfaceFormatsKHR(surface_, &format_count, nullptr);
	assert(result == vk::Result::eSuccess);
	
	auto surface_format = std::make_unique<vk::SurfaceFormatKHR[]>(format_count);
	
	for (unsigned int i = 0; i < format_count; ++i)
	{
		gpu_.getSurfaceFormatsKHR(surface_, &i, &surface_format[i]);
		// color formats
		Log::Info("[%d]colorSpace : %d", i, surface_format[i].colorSpace);
	}

	result = gpu_.getSurfaceFormatsKHR(surface_, &format_count, surface_format.get());
	assert(result == vk::Result::eSuccess);

	uint32_t present_mode_count;
	result = gpu_.getSurfacePresentModesKHR(surface_, &present_mode_count, nullptr);
	assert(result == vk::Result::eSuccess);

	auto present_mode = std::make_unique<vk::PresentModeKHR[]>(present_mode_count);
	result = gpu_.getSurfacePresentModesKHR(surface_, &present_mode_count, present_mode.get());
	assert(result == vk::Result::eSuccess);

	uint32_t image_count = 2;
	if (image_count < caps.minImageCount)
	{
		image_count = caps.minImageCount;
	}
	if ((caps.maxImageCount > 0) && image_count > caps.maxImageCount)
	{
		image_count = caps.maxImageCount;
	}

	vk::Extent2D extent;
	if (caps.currentExtent.width == static_cast<uint32_t>(-1))
	{
		extent.width = Settings::window_width<uint32_t>;
		extent.height = Settings::window_height<uint32_t>;
	}
	else
	{
		extent = caps.currentExtent;
	}

	vk::CompositeAlphaFlagBitsKHR composite_alpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	vk::CompositeAlphaFlagBitsKHR composite_alpha_flags[] = {
		vk::CompositeAlphaFlagBitsKHR::eOpaque,
		vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
		vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
		vk::CompositeAlphaFlagBitsKHR::eInherit,
	};
	for (uint32_t i = 0; i < sizeof(composite_alpha_flags) / sizeof(vk::CompositeAlphaFlagBitsKHR); ++i)
	{
		if (caps.supportedCompositeAlpha & composite_alpha_flags[i])
		{
			composite_alpha = composite_alpha_flags[i];
			break;
		}
	}

	vk::SurfaceTransformFlagBitsKHR pre_transform;
	if (caps.currentTransform == vk::SurfaceTransformFlagBitsKHR::eIdentity)
	{
		pre_transform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
	}
	else
	{
		pre_transform = caps.currentTransform;
	}

	auto swap_chain_info = vk::SwapchainCreateInfoKHR()
		.setSurface(surface_)
		.setMinImageCount(image_count)
		.setImageFormat(swap_target_format_)
		.setImageColorSpace(vk::ColorSpaceKHR::eSrgbNonlinear)
		.setImageExtent(extent)
		.setImageArrayLayers(1)
		.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
		.setImageSharingMode(vk::SharingMode::eExclusive)
		.setCompositeAlpha(composite_alpha)
		.setPreTransform(pre_transform)
		.setPresentMode(vk::PresentModeKHR::eFifo)
		.setClipped(true)
		.setQueueFamilyIndexCount(0)
		.setPQueueFamilyIndices(nullptr)
		.setOldSwapchain(old_sc);

	result = device_.createSwapchainKHR(&swap_chain_info, nullptr, &swap_chain_);
	if (result != vk::Result::eSuccess)
	{
		Log::Error("Swapchain cannot created.");
		return false;
	}

	if (old_sc)
	{
		device_.destroySwapchainKHR(old_sc, nullptr);
	}

	auto sc_image_count = device_.getSwapchainImagesKHR(swap_chain_).value;
	sc_image_count_ = static_cast<uint32_t>(sc_image_count.size());

	Log::Info("Swapchain create done.");

	return true;
}

bool Graphics::Impl::CreateRenderPass(void)
{
	const vk::AttachmentDescription attachments[] =
	{
		vk::AttachmentDescription()
		.setFormat(swap_target_format_)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eStore)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eUndefined)
		.setFinalLayout(vk::ImageLayout::ePresentSrcKHR),
		vk::AttachmentDescription()
		.setFormat(depth_target_.format)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setLoadOp(vk::AttachmentLoadOp::eClear)
		.setStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
		.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
		.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
		.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
	};

	auto const color_reference = vk::AttachmentReference().setAttachment(0).setLayout(vk::ImageLayout::eColorAttachmentOptimal);

	auto const depth_reference = vk::AttachmentReference().setAttachment(1).setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

	auto const subpass = vk::SubpassDescription()
		.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
		.setInputAttachmentCount(0)
		.setPInputAttachments(nullptr)
		.setColorAttachmentCount(1)
		.setPColorAttachments(&color_reference)
		.setPResolveAttachments(nullptr)
		.setPDepthStencilAttachment(&depth_reference)
		.setPreserveAttachmentCount(0)
		.setPPreserveAttachments(nullptr);

	auto const dependency = vk::SubpassDependency()
		.setSrcSubpass(0)
		.setDstSubpass(VK_SUBPASS_EXTERNAL)
		.setSrcStageMask(vk::PipelineStageFlagBits::eBottomOfPipe)
		.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput)
		.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite)
		.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead);

	auto const rp_info = vk::RenderPassCreateInfo()
		.setAttachmentCount(std::size(attachments))
		.setPAttachments(attachments)
		.setSubpassCount(1)
		.setPSubpasses(&subpass)
		.setDependencyCount(1)
		.setPDependencies(&dependency);

	auto result = device_.createRenderPass(&rp_info, nullptr, &render_pass_);
	if (result != vk::Result::eSuccess)
	{
		Log::Error("Render pass cannot created.");
		return false;
	}

	Log::Info("Render pass create done.");

	return true;
}

bool Graphics::Impl::InitSemaphoreSettings(void)
{
	vk::SemaphoreCreateInfo semaphore_info;

	// present command completed comfirm
	device_.createSemaphore(&semaphore_info, nullptr, &present_complete_semaphore_);

	// draw command completed comfirm
	device_.createSemaphore(&semaphore_info, nullptr, &draw_complete_semaphore_);

	if (!present_complete_semaphore_ || !draw_complete_semaphore_)
	{
		Log::Error("Semaphores cannot setting.");
		return false;
	}

	Log::Info("Semaphores setting done.");

	return true;
}

bool Graphics::Impl::CreateCommandBufffer(void)
{
	vk::CommandBufferAllocateInfo alloc_info;
	alloc_info.commandPool = command_pool_;
	alloc_info.commandBufferCount = sc_image_count_;

	auto result = device_.allocateCommandBuffers(alloc_info);
	if (result.result != vk::Result::eSuccess)
	{
		Log::Error("Command buffers cannot created.");
		return false;
	}

	command_buffers_ = result.value;

	Log::Info("Command buffers create done.");

	return true;
}

bool Graphics::Impl::CreateSwapChainResources(void)
{
	auto result = device_.getSwapchainImagesKHR(swap_chain_, &sc_image_count_, nullptr);
	if (result != vk::Result::eSuccess)
	{
		Log::Error("Swap chain image count cannot get.");
		return false;
	}

	std::unique_ptr<vk::Image[]> images(new vk::Image[sc_image_count_]);
	result = device_.getSwapchainImagesKHR(swap_chain_, &sc_image_count_, images.get());
	if (result != vk::Result::eSuccess)
	{
		Log::Error("Swap chain image cannot get.");
		return false;
	}

	sc_resources_.reset(new SwapchainImageResources[sc_image_count_]);

	for (unsigned int i = 0; i < sc_image_count_; ++i)
	{
		auto image_view_info = vk::ImageViewCreateInfo()
			.setViewType(vk::ImageViewType::e2D)
			.setFormat(swap_target_format_)
			.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

		sc_resources_[i].image = images[i];
		image_view_info.image = sc_resources_[i].image;

		sc_resources_[i].fence = vk::Fence();

		result = device_.createImageView(&image_view_info, nullptr, &sc_resources_[i].view);
		if (result != vk::Result::eSuccess)
		{
			Log::Error("Swap chain resources cannot created.");
			return false;
		}
	}

	Log::Info("Swap chain resources create done.");

	return true;
}

bool Graphics::Impl::CreateDepthImage(void)
{
	// supported check
	depth_target_.format = vk::Format::eD32SfloatS8Uint;
	vk::FormatProperties format_props = gpu_.getFormatProperties(depth_target_.format);
	if (!(format_props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment))
	{
		Log::Error("Depth buffer format do not supported ""eD32SfloatS8Uint"".");
		return false;
	}

	vk::Extent3D extent = vk::Extent3D()
		.setWidth(Settings::window_width<uint32_t>)
		.setHeight(Settings::window_height<uint32_t>)
		.setDepth(1);

	auto const image = vk::ImageCreateInfo()
		.setImageType(vk::ImageType::e2D)
		.setFormat(depth_target_.format)
		.setExtent(extent)
		.setMipLevels(1)
		.setArrayLayers(1)
		.setSamples(vk::SampleCountFlagBits::e1)
		.setTiling(vk::ImageTiling::eOptimal)
		.setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
		.setSharingMode(vk::SharingMode::eExclusive)
		.setQueueFamilyIndexCount(0)
		.setPQueueFamilyIndices(nullptr)
		.setInitialLayout(vk::ImageLayout::eUndefined);

	auto result = device_.createImage(&image, nullptr, &depth_target_.image);
	if (result != vk::Result::eSuccess)
	{
		Log::Error("Depth buffer image cannot created.");
		return false;
	}

	vk::MemoryRequirements mem_reqs;
	device_.getImageMemoryRequirements(depth_target_.image, &mem_reqs);

	depth_target_.mem_alloc.setAllocationSize(mem_reqs.size)
		.setMemoryTypeIndex(FindMemoryTypeIndex(mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal));

	result = device_.allocateMemory(&depth_target_.mem_alloc, nullptr, &depth_target_.mem);
	if (result != vk::Result::eSuccess)
	{
		Log::Error("Depth buffer memory cannot allocated.");
		return false;
	}

	result = device_.bindImageMemory(depth_target_.image, depth_target_.mem, 0);
	if (result != vk::Result::eSuccess)
	{
		Log::Error("Depth buffer memory cannot binded.");
		return false;
	}

	vk::ImageViewCreateInfo view = vk::ImageViewCreateInfo()
		.setImage(depth_target_.image)
		.setViewType(vk::ImageViewType::e2D)
		.setFormat(depth_target_.format)
		//.setComponents({ vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA })
		.setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1));

	result = device_.createImageView(&view, nullptr, &depth_target_.view);
	if (result != vk::Result::eSuccess)
	{
		Log::Error("Depth buffer image cannot created.");
		return false;
	}

	Log::Info("Depth image create done.");

	return true;
}

bool Graphics::Impl::CreateFrameBuffer(void)
{
	vk::ImageView attachments[2];
	attachments[1] = depth_target_.view;

	auto const fb_info = vk::FramebufferCreateInfo()
		.setRenderPass(render_pass_)
		.setAttachmentCount(std::size(attachments))
		.setPAttachments(attachments)
		.setWidth(Settings::window_width<uint32_t>)
		.setHeight(Settings::window_height<uint32_t>)
		.setLayers(1);

	for (uint32_t i = 0; i < sc_image_count_; ++i)
	{
		attachments[0] = sc_resources_[i].view;

		auto const result = device_.createFramebuffer(&fb_info, nullptr, &sc_resources_[i].frame_buffer);
		if (result != vk::Result::eSuccess)
		{
			Log::Error("Frame buffer cannot created.");
		}
	}

	Log::Info("Frame buffer create done.");

	return true;
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugMessageCallback(
	vk::DebugReportFlagsEXT flags,
	vk::DebugReportObjectTypeEXT objType,
	uint64_t srcObject,
	size_t location,
	int32_t msgCode,
	const char* pLayerPrefix,
	const char* pMsg,
	void* pUserData)
{
	std::string message;
	{
		std::stringstream buf;
		if (flags & vk::DebugReportFlagBitsEXT::eError) 
		{
			buf << "ERROR: ";
		}
		else if (flags & vk::DebugReportFlagBitsEXT::eWarning) 
		{
			buf << "WARNING: ";
		}
		else if (flags & vk::DebugReportFlagBitsEXT::ePerformanceWarning) 
		{
			buf << "PERF: ";
		}
		else 
		{
			return VK_FALSE;
		}
		buf << "[" << pLayerPrefix << "] Code " << msgCode << " : " << pMsg;
		message = buf.str();
	}

	Log::Warning(message);

	return VK_FALSE;
}