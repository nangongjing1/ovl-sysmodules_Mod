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
};

class GuiMain : public tsl::Gui {
  private:
    std::vector<SystemModule> m_sysmoduleListItems;  // Changed from std::list to std::vector
    bool m_scanned = false;
    bool m_isActive = true;  // Track if GUI is still active
    
  public:
    GuiMain();
    ~GuiMain();
    
    virtual tsl::elm::Element *createUI();
    virtual void update() override;
    
  private:
    void updateStatus(const SystemModule &module);
    bool hasFlag(const SystemModule &module);
    bool isRunning(const SystemModule &module);
};