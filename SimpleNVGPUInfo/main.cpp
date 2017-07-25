#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstdarg>
#include <vector>
#include <string>
#include <nvapi.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

struct GPUInfo
{
    NvPhysicalGpuHandle physical_gpu;
    std::string name;
    NvU32 vram_in_kb;
};

void PrintError(NvAPI_Status error, const char* msg, ...)
{
    va_list ap;
    va_start(ap, msg);

    char buf[256];
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%sError 0x%08X\n",
                msg, static_cast<unsigned int>(error));
    va_end(ap);

    fprintf(stderr, "%s", buf);
}

std::string FormatClockSpeed(const NV_GPU_CLOCK_FREQUENCIES& frequencies, NvU32 domain)
{
    if (!frequencies.domain[domain].bIsPresent)
        return "<not present>";

    char buf[128];
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%.2f MHz",
                static_cast<float>(frequencies.domain[domain].frequency) / 1000.0f);
    return buf;
}

std::string FormatSizeKB(NvU32 size_in_kb)
{
    char buf[128];
    if (size_in_kb > 1048576)
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%.2f GiB", static_cast<float>(size_in_kb) / 1048576.0f);
    else if (size_in_kb > 1024)
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%.2f MiB", static_cast<float>(size_in_kb) / 1024.0f);
    else
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "%u KiB", static_cast<unsigned int>(size_in_kb));
    return buf;
}

void PrintGPUClockInfo(const GPUInfo& gpu)
{
    NV_GPU_CLOCK_FREQUENCIES base_frequencies = {};
    NV_GPU_CLOCK_FREQUENCIES boost_frequencies = {};
    bool has_boost_frequencies = false;
    base_frequencies.version = NV_GPU_CLOCK_FREQUENCIES_VER;
    base_frequencies.ClockType = NV_GPU_CLOCK_FREQUENCIES_BASE_CLOCK;
    boost_frequencies.version = NV_GPU_CLOCK_FREQUENCIES_VER;
    boost_frequencies.ClockType = NV_GPU_CLOCK_FREQUENCIES_BOOST_CLOCK;

    NvAPI_Status status = NvAPI_GPU_GetAllClockFrequencies(gpu.physical_gpu, &base_frequencies);
    if (status != NVAPI_OK)
    {
        PrintError(status, "NvAPI_GPU_GetAllClockFrequencies failed:");
        return;
    }

    has_boost_frequencies = (NvAPI_GPU_GetAllClockFrequencies(gpu.physical_gpu, &boost_frequencies) == NVAPI_OK);

    auto PrintClock = [&](const char* type, NvU32 domain) {
        fprintf(stdout, "%s: %s", type, FormatClockSpeed(base_frequencies, domain).c_str());
        if (has_boost_frequencies)
            fprintf(stdout, " (boost %s)", FormatClockSpeed(boost_frequencies, domain).c_str());
        fprintf(stdout, "\n");
    };

    PrintClock("Graphics clock speed", NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS);
    PrintClock("Memory clock speed", NVAPI_GPU_PUBLIC_CLOCK_MEMORY);
    //PrintClock("Processor clock speed", NVAPI_GPU_PUBLIC_CLOCK_PROCESSOR);
    //PrintClock("Video clock speed", NVAPI_GPU_PUBLIC_CLOCK_VIDEO);
}

void PrintGPUInfo(const GPUInfo& gpu)
{
    fprintf(stdout, "GPU: %s\n", gpu.name.c_str());
    fprintf(stdout, "-------------------------------\n");
    
    PrintGPUClockInfo(gpu);
}

void PrintGPUCurrentClocks(const GPUInfo& gpu)
{
    NV_GPU_CLOCK_FREQUENCIES frequencies = {};
    frequencies.version = NV_GPU_CLOCK_FREQUENCIES_VER;
    frequencies.ClockType = NV_GPU_CLOCK_FREQUENCIES_CURRENT_FREQ;

    NvAPI_Status status = NvAPI_GPU_GetAllClockFrequencies(gpu.physical_gpu, &frequencies);
    if (status != NVAPI_OK)
    {
        PrintError(status, "NvAPI_GPU_GetAllClockFrequencies failed:");
        return;
    }

    auto PrintClock = [&](const char* type, NvU32 domain) {
        fprintf(stdout, "%s: %s", type, FormatClockSpeed(frequencies, domain).c_str());
        fprintf(stdout, "\t");
    };

    PrintClock("Graphics clock speed", NVAPI_GPU_PUBLIC_CLOCK_GRAPHICS);
    PrintClock("Memory clock speed", NVAPI_GPU_PUBLIC_CLOCK_MEMORY);
}

void PrintGPUCurrentTemperature(const GPUInfo& gpu)
{
    NV_GPU_THERMAL_SETTINGS thermal = {};
    thermal.version = NV_GPU_THERMAL_SETTINGS_VER;

    NvAPI_Status status = NvAPI_GPU_GetThermalSettings(gpu.physical_gpu, 0, &thermal);
    if (status != NVAPI_OK)
    {
        PrintError(status, "NvAPI_GPU_GetAllClockFrequencies failed:");
        return;
    }

    if (thermal.count > 0)
    {
        fprintf(stdout, "Temperature: %dC", static_cast<int>(thermal.sensor[0].currentTemp));
        fprintf(stdout, "\t");
    }
}

void PrintGPUCurrentFanSpeed(const GPUInfo& gpu)
{
    NvU32 fan_speed = 0;
    NvAPI_GPU_GetTachReading(gpu.physical_gpu, &fan_speed);

    fprintf(stdout, "Fan speed: %u RPM\t", fan_speed);
}

void PrintGPUCurrentMemoryUsage(const GPUInfo& gpu)
{
    NV_DISPLAY_DRIVER_MEMORY_INFO memory_info = {};
    memory_info.version = NV_DISPLAY_DRIVER_MEMORY_INFO_VER;
    
    NvAPI_Status status = NvAPI_GPU_GetMemoryInfo(gpu.physical_gpu, &memory_info);
    if (status != NVAPI_OK)
    {
        PrintError(status, "NvAPI_GPU_GetAllClockFrequencies failed:");
        return;
    }

    NvU32 used_memory = gpu.vram_in_kb - memory_info.curAvailableDedicatedVideoMemory;
    fprintf(stdout, "Memory usage: %s / %s", FormatSizeKB(used_memory).c_str(), FormatSizeKB(gpu.vram_in_kb).c_str());
    fprintf(stdout, "\t");
}

void PrintGPUStatus(const GPUInfo& gpu)
{
    fprintf(stdout, "GPU: %s\n", gpu.name.c_str());
    fprintf(stdout, "-------------------------------\n");

    PrintGPUCurrentClocks(gpu);
    fprintf(stdout, "\n");

    PrintGPUCurrentTemperature(gpu);
    PrintGPUCurrentFanSpeed(gpu);
    fprintf(stdout, "\n");

    PrintGPUCurrentMemoryUsage(gpu);
    fprintf(stdout, "\n");

    fprintf(stdout, "-------------------------------\n");
    fprintf(stdout, "\n");
}

std::vector<GPUInfo> EnumerateGPUs()
{
    NvPhysicalGpuHandle gpu_handles[NVAPI_MAX_LOGICAL_GPUS];
    NvU32 num_gpus = NVAPI_MAX_LOGICAL_GPUS;
    NvAPI_Status status = NvAPI_EnumPhysicalGPUs(gpu_handles, &num_gpus);
    NvAPI_ShortString str;
    if (status != NVAPI_OK)
    {
        PrintError(status, "NvAPI_EnumPhysicalGPUs failed: ");
        return {};
    }

    std::vector<GPUInfo> gpus;
    for (NvU32 i = 0; i < num_gpus; i++)
    {
        GPUInfo info;
        info.physical_gpu = gpu_handles[i];
        status = NvAPI_GPU_GetFullName(info.physical_gpu, str);
        if (status != NVAPI_OK)
        {
            PrintError(status, "NvAPI_GPU_GetFullName failed: ");
            continue;
        }

        info.name = str;

        NvU32 vram_size_kb;
        status = NvAPI_GPU_GetPhysicalFrameBufferSize(info.physical_gpu, &vram_size_kb);
        if (status != NVAPI_OK)
        {
            PrintError(status, "NvAPI_GPU_GetPhysicalFrameBufferSize failed: ");
            continue;
        }
        info.vram_in_kb = vram_size_kb;

        gpus.emplace_back(std::move(info));
    }

    return gpus;
}

int main(int argc, char* argv[])
{
    NvAPI_Status status = NvAPI_Initialize();
    if (status != NVAPI_OK)
    {
        PrintError(status, "NVAPI_Initialize failed: ");
        return EXIT_FAILURE;
    }

    std::vector<GPUInfo> gpus = EnumerateGPUs();
    if (gpus.empty())
    {
        fprintf(stderr, "No GPUs found.\n");
        NvAPI_Unload();
        return EXIT_FAILURE;
    }

    for (const GPUInfo& gpu : gpus)
        PrintGPUInfo(gpu);

    for (;;)
    {
        for (const GPUInfo& gpu : gpus)
            PrintGPUStatus(gpu);

        Sleep(1000);
    }

    NvAPI_Unload();
    return EXIT_SUCCESS;
}