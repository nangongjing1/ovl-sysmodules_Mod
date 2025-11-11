#pragma once
#include <list>
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
    std::list<SystemModule> m_sysmoduleListItems;
    bool m_scanned = false;
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