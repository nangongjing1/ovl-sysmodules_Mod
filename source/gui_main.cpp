#include "gui_main.hpp"

#include "dir_iterator.hpp"

#include <json.hpp>
using json = nlohmann::json;

constexpr const char* const amsContentsPath = "/atmosphere/contents";
constexpr const char* const boot2FlagFormat = "/atmosphere/contents/%016lX/flags/boot2.flag";
constexpr const char* const boot2FlagFolder = "/atmosphere/contents/%016lX/flags";

static char pathBuffer[FS_MAX_PATH];

constexpr const char* const descriptions[2][2] = {
    [0] = {
        [0] = "Off",
        [1] = "Off",
    },
    [1] = {
        [0] = "On",
        [1] = "On",
    },
};

GuiMain::GuiMain() {
    Result rc = fsOpenSdCardFileSystem(&this->m_fs);
    if (R_FAILED(rc))
        return;

    FsDir contentDir;
    std::strcpy(pathBuffer, amsContentsPath);
    rc = fsFsOpenDirectory(&this->m_fs, pathBuffer, FsDirOpenMode_ReadDirs, &contentDir);
    if (R_FAILED(rc))
        return;
    tsl::hlp::ScopeGuard dirGuard([&] { fsDirClose(&contentDir); });

    SystemModule module;

    std::string listItemText;

    /* Iterate over contents folder. */
    for (const auto& entry : FsDirIterator(contentDir)) {
        if (*(uint32_t*)entry.name == *(uint32_t*)&"0100" && *(uint64_t*)(&entry.name[4]) != *(uint64_t*)&"00000000")
            continue;
        FsFile toolboxFile;
        std::snprintf(pathBuffer, FS_MAX_PATH, "/atmosphere/contents/%.*s/toolbox.json", FS_MAX_PATH - 35, entry.name);
        rc = fsFsOpenFile(&this->m_fs, pathBuffer, FsOpenMode_Read, &toolboxFile);
        if (R_FAILED(rc))
            continue;
        tsl::hlp::ScopeGuard fileGuard([&] { fsFileClose(&toolboxFile); });

        /* Get toolbox file size. */
        static s64 size;
        rc = fsFileGetSize(&toolboxFile, &size);
        if (R_FAILED(rc))
            continue;

        /* Read toolbox file. */
        std::string toolBoxData(size, '\0');
        static u64 bytesRead;
        rc = fsFileRead(&toolboxFile, 0, toolBoxData.data(), size, FsReadOption_None, &bytesRead);
        if (R_FAILED(rc))
            continue;

        /* Parse toolbox file data. */
        json toolboxFileContent = json::parse(toolBoxData);

        const std::string& sysmoduleProgramIdString = toolboxFileContent["tid"];
        const u64 sysmoduleProgramId = std::strtoul(sysmoduleProgramIdString.c_str(), nullptr, 16);

        /* Let's not allow Tesla to be killed with this. */
        if (sysmoduleProgramId == 0x420000000007E51AULL)
            continue;

        listItemText = toolboxFileContent["name"];
        if (toolboxFileContent.contains("version")) {
            listItemText += "" + toolboxFileContent["version"].get<std::string>();
        }


        module = {
            .listItem = new tsl::elm::ListItem(listItemText),
            .programId = sysmoduleProgramId,
            .needReboot = toolboxFileContent["requires_reboot"],
        };

        module.listItem->setClickListener([this, module](u64 click) -> bool {
            /* if the folder "flags" does not exist, it will be created */
            std::snprintf(pathBuffer, FS_MAX_PATH, boot2FlagFolder, module.programId);
            fsFsCreateDirectory(&this->m_fs, pathBuffer);
            std::snprintf(pathBuffer, FS_MAX_PATH, boot2FlagFormat, module.programId);

            if (module.needReboot) module.listItem->isLocked = true;
            if (click & KEY_A && !module.needReboot) {
                if (this->isRunning(module)) {
                    /* Kill process. */
                    pmshellTerminateProgram(module.programId);

                    /* Remove boot2 flag file. */
                    //if (this->hasFlag(module))
                    //    fsFsDeleteFile(&this->m_fs, pathBuffer);
                } else {
                    /* Start process. */
                    const NcmProgramLocation programLocation{
                        .program_id = module.programId,
                        .storageID = NcmStorageId_None,
                    };
                    u64 pid = 0;
                    pmshellLaunchProgram(0, &programLocation, &pid);

                    /* Create boot2 flag file. */
                    //if (!this->hasFlag(module))
                    //    fsFsCreateFile(&this->m_fs, pathBuffer, 0, FsCreateOption(0));
                }
                return true;
            }

            if (click & KEY_Y) {// || (click & KEY_A && module.needReboot)) {
                if (this->hasFlag(module)) {
                    /* Remove boot2 flag file. */
                    fsFsDeleteFile(&this->m_fs, pathBuffer);

                } else {
                    /* Create boot2 flag file. */
                    fsFsCreateFile(&this->m_fs, pathBuffer, 0, FsCreateOption(0));
                }
                //module.listItem->triggerClickAnimation();
                triggerRumbleClick.store(true, std::memory_order_release);
                triggerSettingsSound.store(true, std::memory_order_release);
                return true;
            }

            return false;
        });
        this->m_sysmoduleListItems.push_back(std::move(module));
    }

    /* Sort modules alphabetically by name */
    this->m_sysmoduleListItems.sort([](const SystemModule &a, const SystemModule &b) {
        return a.listItem->getText() < b.listItem->getText();
    });

    /* Sort modules by program ID */
    //this->m_sysmoduleListItems.sort([](const SystemModule &a, const SystemModule &b) {
    //    return a.programId < b.programId;
    //});


    this->m_scanned = true;
}

GuiMain::~GuiMain() {
    fsFsClose(&this->m_fs);
}

// Method to draw available RAM only
inline void drawMemoryWidget(auto renderer) {
    static char ramString[24];  // Buffer for RAM string
    static tsl::Color ramColor = {0,0,0,0};
    static u64 lastUpdateTick = 0;
    const u64 ticksPerSecond = armGetSystemTickFreq();
    
    // Get the current tick count
    const u64 currentTick = armGetSystemTick();
    
    // Check if this is the first run or at least one second has passed since the last update
    if (lastUpdateTick == 0 || currentTick - lastUpdateTick >= ticksPerSecond) {
        // Update RAM information
        static u64 RAM_Used_system_u, RAM_Total_system_u;
        svcGetSystemInfo(&RAM_Used_system_u, 1, INVALID_HANDLE, 2);
        svcGetSystemInfo(&RAM_Total_system_u, 0, INVALID_HANDLE, 2);
        
        // Calculate free RAM in bytes
        const u64 freeRamBytes = RAM_Total_system_u - RAM_Used_system_u;
        
        // Determine unit and value
        static float value;
        const char* unit;
        
        if (freeRamBytes >= 1024ULL * 1024 * 1024) {
            // Use GB (1 GB or more)
            value = static_cast<float>(freeRamBytes) / (1024.0f * 1024.0f * 1024.0f);
            unit = "GB";
        } else {
            // Use MB
            value = static_cast<float>(freeRamBytes) / (1024.0f * 1024.0f);
            unit = "MB";
        }
        
        // Format with 4 significant figures
        static int decimalPlaces;
        if (value >= 1000.0f) {
            decimalPlaces = 0;  // e.g., 1234
        } else if (value >= 100.0f) {
            decimalPlaces = 1;  // e.g., 123.4
        } else if (value >= 10.0f) {
            decimalPlaces = 2;  // e.g., 12.34
        } else if (value >= 1.0f) {
            decimalPlaces = 3;  // e.g., 1.234
        } else {
            decimalPlaces = 3;  // e.g., 0.123
        }
        
        snprintf(ramString, sizeof(ramString), "%.*f %s %s", decimalPlaces, value, unit, ult::FREE.c_str());
        
        // Convert to MB for threshold comparison (keep original thresholds)
        const float freeRamMB = static_cast<float>(freeRamBytes) / (1024.0f * 1024.0f);
        
        if (freeRamMB >= 9.0f){
            ramColor = tsl::healthyRamTextColor; // Green: R=0, G=15, B=0
        } else if (freeRamMB >= 3.0f) {
            ramColor = tsl::neutralRamTextColor; // Orange-ish: R=15, G=10, B=0 → roughly RGB888: 255, 170, 0
        } else {
            ramColor = tsl::badRamTextColor; // Red: R=15, G=0, B=0
        }
        
        // Update the last update tick
        lastUpdateTick = currentTick;
    }
    
    // Draw separator line
    renderer->drawRect(239, 15+2-2, 1, 64+2, (tsl::separatorColor));
    
    // Draw backdrop if not hidden
    if (!ult::hideWidgetBackdrop) {
        renderer->drawUniformRoundedRect(247, 15+2-2, (ult::extendedWidgetBackdrop) ? tsl::cfg::FramebufferWidth - 255 : tsl::cfg::FramebufferWidth - 255 + 40, 64+2, (tsl::widgetBackdropColor));
    }
    
    // Constants for centering calculations
    const int backdropCenterX = 247 + ((tsl::cfg::FramebufferWidth - 255) >> 1);
    
    // Calculate base Y offset
    const size_t y_offset = 55+2-1; // Adjusted y_offset for drawing
    
    if (ult::centerWidgetAlignment) {
        // CENTERED ALIGNMENT (current code logic)
        
        // Calculate total width for centering
        const int ramWidth = renderer->getTextDimensions(ramString, false, 20).first;
        
        // Draw RAM info centered
        const int currentX = backdropCenterX - (ramWidth >> 1);
        renderer->drawString(ramString, false, currentX, y_offset, 20, (ramColor));
        
    } else {
        // RIGHT ALIGNMENT (old code style)
        
        // Calculate string width
        const s32 ramWidth = renderer->getTextDimensions(ramString, false, 20).first;
        
        // Draw RAM string
        renderer->drawString(ramString, false, tsl::cfg::FramebufferWidth - ramWidth - 20 - 5, y_offset, 20, (ramColor));
    }
}

tsl::elm::Element* GuiMain::createUI() {
    //tsl::elm::OverlayFrame *rootFrame = new tsl::elm::OverlayFrame("Sysmodules", VERSION);

    auto* rootFrame = new tsl::elm::HeaderOverlayFrame(97);
    rootFrame->setHeader(new tsl::elm::CustomDrawer([this](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
        renderer->drawString("Sysmodules", false, 20, 50+2, 32, (tsl::defaultOverlayColor));
        renderer->drawString(VERSION, false, 20, 52+23, 15, (tsl::bannerVersionTextColor));

        drawMemoryWidget(renderer);
    }));


    if (this->m_sysmoduleListItems.size() == 0) {
        const char* description = this->m_scanned ? "No sysmodules found!" : "Scan failed!";

        auto* warning = new tsl::elm::CustomDrawer([description](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("\uE150", false, 180, 250, 90, (tsl::headerTextColor));
            renderer->drawString(description, false, 110, 340, 25, (tsl::headerTextColor));
        });

        rootFrame->setContent(warning);
    } else {
        tsl::elm::List* sysmoduleList = new tsl::elm::List();

        sysmoduleList->addItem(new tsl::elm::CategoryHeader("Dynamic   Auto Start   Toggle", true));
        sysmoduleList->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("  These sysmodules can be toggled at any time.", false, x + 5, y + 20-7, 15, (tsl::warningTextColor));
        }), 30);
        for (const auto& module : this->m_sysmoduleListItems) {
            if (!module.needReboot)
                sysmoduleList->addItem(module.listItem);
        }

        sysmoduleList->addItem(new tsl::elm::CategoryHeader("Static   Auto Start", true));
        sysmoduleList->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer* renderer, s32 x, s32 y, s32 w, s32 h) {
            renderer->drawString("  These sysmodules need a reboot to work.", false, x + 5, y + 20-7, 15, (tsl::warningTextColor));
        }), 30);
        for (const auto& module : this->m_sysmoduleListItems) {
            if (module.needReboot) {
                module.listItem->disableClickAnimation();
                sysmoduleList->addItem(module.listItem);
            }
        }
        rootFrame->setContent(sysmoduleList);
    }

    return rootFrame;
}

void GuiMain::update() {
    static u32 counter = 0;

    if (counter++ % 20 != 0)
        return;

    for (const auto& module : this->m_sysmoduleListItems) {
        this->updateStatus(module);
    }
}

void GuiMain::updateStatus(const SystemModule &module) {
    const bool running = this->isRunning(module);
    const bool hasFlag = this->hasFlag(module);

    const char* desc = descriptions[running][hasFlag];
    module.listItem->setValue(desc, !running);
}

bool GuiMain::hasFlag(const SystemModule &module) {
    FsFile flagFile;
    std::snprintf(pathBuffer, FS_MAX_PATH, boot2FlagFormat, module.programId);
    Result rc = fsFsOpenFile(&this->m_fs, pathBuffer, FsOpenMode_Read, &flagFile);
    if (R_SUCCEEDED(rc)) {
        fsFileClose(&flagFile);
        return true;
    } else {
        return false;
    }
}

bool GuiMain::isRunning(const SystemModule &module) {
    u64 pid = 0;
    if (R_FAILED(pmdmntGetProcessId(&pid, module.programId)))
        return false;

    return pid > 0;
}