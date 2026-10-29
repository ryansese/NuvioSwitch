#include <borealis.hpp>
#include <cstdlib>
#include <cstring>

#include "activity/main_activity.hpp"
#include "tabs/placeholder_tab.hpp"
#include "version.hpp"

using namespace brls::literals; // for _i18n

// Nuvio's default dark theme (NuvioTV ui/theme/PrimitiveTokens.kt).
static void applyNuvioTheme()
{
    brls::Theme& dark = brls::Theme::getDarkTheme();
    dark.addColor("brls/background", nvgRGB(0x0D, 0x0D, 0x0D));
    dark.addColor("brls/backdrop", nvgRGBA(0x0D, 0x0D, 0x0D, 0xB2));
    dark.addColor("brls/sidebar/background", nvgRGB(0x1A, 0x1A, 0x1A));
    dark.addColor("brls/sidebar/active_item", nvgRGB(0xFF, 0xFF, 0xFF));
    dark.addColor("brls/text", nvgRGB(0xFF, 0xFF, 0xFF));
    dark.addColor("brls/text_disabled", nvgRGB(0xB3, 0xB3, 0xB3));
    dark.addColor("brls/accent", nvgRGB(0xFF, 0xFF, 0xFF));
    dark.addColor("brls/highlight/color1", nvgRGB(0xFF, 0xFF, 0xFF));
    dark.addColor("brls/highlight/color2", nvgRGB(0xFF, 0xFF, 0xFF));
    dark.addColor("brls/highlight/background", nvgRGB(0x30, 0x30, 0x30));
    dark.addColor("nuvio/card", nvgRGB(0x24, 0x24, 0x24));
}

int main(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if (std::strcmp(argv[i], "-d") == 0)
            brls::Logger::setLogLevel(brls::LogLevel::LOG_DEBUG);
        else if (std::strcmp(argv[i], "-v") == 0)
            brls::Application::enableDebuggingView(true);
    }

    brls::Platform::APP_LOCALE_DEFAULT = brls::LOCALE_AUTO;

    if (!brls::Application::init())
    {
        brls::Logger::error("Unable to init borealis");
        return EXIT_FAILURE;
    }

    brls::Application::createWindow(NUVIO_APP_NAME);

    applyNuvioTheme();
    brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);

    // + (Start) quits from any activity; borealis leaves this off by default
    brls::Application::setGlobalQuit(true);

    brls::Application::registerXMLView("PlaceholderTab", PlaceholderTab::create);

    brls::Application::pushActivity(new MainActivity());

    while (brls::Application::mainLoop())
        ;

    return EXIT_SUCCESS;
}
