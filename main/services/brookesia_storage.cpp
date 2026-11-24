#include "services/brookesia_storage.hpp"

#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/storage/file_system.hpp"
#include "esp_littlefs.h"
#include "esp_log.h"

namespace desk_buddy {

namespace {

namespace hal = esp_brookesia::hal;
using hal::storage::FileSystemIface;
constexpr const char *kStorageName = "DeskBuddy:LittleFS";
constexpr const char *kMountPoint = "/littlefs";
constexpr const char *kPartition = "littlefs";

class InternalFileSystem final : public FileSystemIface {
public:
    InternalFileSystem()
    {
        info_list_.push_back({FileSystemType::LittleFS, MediumType::Flash, kMountPoint, kMountPoint, true});
    }

    bool get_capacity(const char *mount_point, Capacity &capacity) override
    {
        size_t total = 0;
        size_t used = 0;
        if (!mount_point || std::string_view(mount_point) != kMountPoint ||
                esp_littlefs_info(kPartition, &total, &used) != ESP_OK) {
            return false;
        }
        capacity = {total, used, total - used};
        return true;
    }
};

class InternalStorageDevice final : public hal::Device {
public:
    InternalStorageDevice() : Device(kStorageName) {}

private:
    bool probe() override
    {
        return esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_LITTLEFS,
                                        kPartition) != nullptr;
    }

    std::vector<hal::InterfaceSpec> get_interface_specs() const override
    {
        return {{FileSystemIface::NAME, kStorageName}};
    }

    bool on_init() override
    {
        esp_vfs_littlefs_conf_t config{};
        config.base_path = kMountPoint;
        config.partition_label = kPartition;
        config.format_if_mount_failed = true;
        const auto result = esp_vfs_littlefs_register(&config);
        if (result != ESP_OK) {
            ESP_LOGE("DB:BrookesiaFS", "LittleFS mount failed: %s", esp_err_to_name(result));
            return false;
        }
        if (!publish_interface(kStorageName, std::make_shared<InternalFileSystem>())) {
            esp_vfs_littlefs_unregister(kPartition);
            return false;
        }
        return true;
    }

    void on_deinit() override
    {
        interfaces_.clear();
        esp_vfs_littlefs_unregister(kPartition);
    }
};

BROOKESIA_PLUGIN_REGISTER(hal::Device, InternalStorageDevice, std::string(kStorageName));

}

bool initBrookesiaStorage()
{
    static const auto storage = hal::acquire_interface<FileSystemIface>(kStorageName);
    return static_cast<bool>(storage);
}

}
