#include "include/native_ping_checker/native_ping_checker_plugin_c_api.h"

#include <flutter/plugin_registrar_windows.h>

#include "native_ping_checker_plugin.h"

void NativePingCheckerPluginCApiRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  native_ping_checker::NativePingCheckerPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
