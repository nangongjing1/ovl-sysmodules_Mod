#define TESLA_INIT_IMPL
#include "gui_main.hpp"

class OverlaySysmodules : public tsl::Overlay {
  public:
    OverlaySysmodules() {}
    ~OverlaySysmodules() {}

    void initServices() override {
        pmshellInitialize();
        
        if (isFileOrDirectory("sdmc:/config/sysmodules/theme.ini"))
            THEME_CONFIG_INI_PATH = "sdmc:/config/sysmodules/theme.ini"; // Override theme path (optional)
        if (isFileOrDirectory("sdmc:/config/sysmodules/wallpaper.rgba"))
            WALLPAPER_PATH = "sdmc:/config/sysmodules/wallpaper.rgba"; // Overrride wallpaper path (optional)
        
        tsl::initializeThemeVars(); // for ultrahand themes
        tsl::initializeUltrahandSettings(); // for opaque screenshots and swipe to open
    }

    void exitServices() override {
        pmshellExit();
    }

    std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return std::make_unique<GuiMain>();
    }
};

int main(int argc, char **argv) {
    return tsl::loop<OverlaySysmodules>(argc, argv);
}
