#pragma once
#include <vector>
#include <sys/stat.h>
#include <dirent.h>
#include <cJSON.h>
#include <algorithm>
#include <tesla.hpp>

struct SystemModule {
    tsl::elm::ListItem *listItem;
    u64 programId;
    bool needReboot;
    char flagPath[FS_MAX_PATH];    // Cached flag file path
    char folderPath[FS_MAX_PATH];  // Cached flags folder path
    std::string displayName;       // Store original name + version
    std::string titleIdStr;        // Store formatted title ID
};

class GuiMain : public tsl::Gui {
  private:
    std::vector<SystemModule> m_sysmoduleListItems;
    bool m_scanned = false;
    bool m_isActive = true;
    bool m_showTitleIds = false;  // Toggle state
    
  public:
    GuiMain();
    ~GuiMain();
    
    virtual tsl::elm::Element *createUI();
    virtual void update() override;
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) override;
    
  private:
    void updateStatus(const SystemModule &module);
    bool hasFlag(const SystemModule &module);
    bool isRunning(const SystemModule &module);
    void toggleTitleIdDisplay();
};