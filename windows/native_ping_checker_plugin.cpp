#include "native_ping_checker_plugin.h"

// IMPORTANT: These defines MUST come before any Windows headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Prevent winsock.h from being included
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <memory>
#include <string>

#pragma comment(lib, "iphlpapi.lib")

namespace native_ping_checker {

// Function pointer types for wininet functions (dynamic loading)
typedef BOOL (WINAPI *InternetGetConnectedStateFunc)(LPDWORD, DWORD);
typedef BOOL (WINAPI *InternetCheckConnectionAFunc)(LPCSTR, DWORD, DWORD);

// Flag for InternetCheckConnection
#define FLAG_ICC_FORCE_CONNECTION 0x01

// Dynamic wininet function loader
class WinInetLoader {
public:
  static WinInetLoader& Instance() {
    static WinInetLoader instance;
    return instance;
  }

  bool IsConnected() {
    if (!m_loaded || !m_getConnectedState) {
      return true; // Assume connected if we can't check
    }
    DWORD flags = 0;
    return m_getConnectedState(&flags, 0) != FALSE;
  }

  bool CheckConnection(const char* url) {
    if (!m_loaded || !m_checkConnection) {
      return false;
    }
    return m_checkConnection(url, FLAG_ICC_FORCE_CONNECTION, 0) != FALSE;
  }

private:
  WinInetLoader() : m_hWininet(nullptr), m_getConnectedState(nullptr), 
                    m_checkConnection(nullptr), m_loaded(false) {
    m_hWininet = LoadLibraryA("wininet.dll");
    if (m_hWininet) {
      m_getConnectedState = (InternetGetConnectedStateFunc)
          GetProcAddress(m_hWininet, "InternetGetConnectedState");
      m_checkConnection = (InternetCheckConnectionAFunc)
          GetProcAddress(m_hWininet, "InternetCheckConnectionA");
      m_loaded = (m_getConnectedState != nullptr);
    }
  }

  ~WinInetLoader() {
    if (m_hWininet) {
      FreeLibrary(m_hWininet);
    }
  }

  // Prevent copying
  WinInetLoader(const WinInetLoader&) = delete;
  WinInetLoader& operator=(const WinInetLoader&) = delete;

  HMODULE m_hWininet;
  InternetGetConnectedStateFunc m_getConnectedState;
  InternetCheckConnectionAFunc m_checkConnection;
  bool m_loaded;
};

// Convert IP string to ULONG
ULONG IpStringToUlong(const std::string &ipStr) {
  ULONG result = 0;
  int parts[4] = {0};
  int partIndex = 0;
  int currentNum = 0;
  
  for (size_t i = 0; i <= ipStr.length(); i++) {
    if (i == ipStr.length() || ipStr[i] == '.') {
      if (partIndex < 4) {
        parts[partIndex++] = currentNum;
      }
      currentNum = 0;
    } else if (ipStr[i] >= '0' && ipStr[i] <= '9') {
      currentNum = currentNum * 10 + (ipStr[i] - '0');
    }
  }
  
  if (partIndex == 4) {
    result = (parts[0]) | (parts[1] << 8) | (parts[2] << 16) | (parts[3] << 24);
  }
  
  return result;
}

// Method 1: Check using Windows Internet API (most reliable)
bool CheckWithWinInet() {
  auto& wininet = WinInetLoader::Instance();
  
  if (!wininet.IsConnected()) {
    return false;
  }
  
  // Try to actually reach an internet resource
  return wininet.CheckConnection("http://www.msftconnecttest.com/connecttest.txt");
}

// Method 2: Check using ICMP ping
bool CheckWithIcmpPing(const std::string &target, int pingCount, int timeoutMs, int minSuccess) {
  ULONG targetIP = IpStringToUlong(target);
  if (targetIP == 0) {
    return false;
  }

  HANDLE hIcmp = IcmpCreateFile();
  if (hIcmp == INVALID_HANDLE_VALUE) {
    return false;
  }

  char sendData[] = "ping";
  WORD sendSize = static_cast<WORD>(sizeof(sendData));
  
  // Buffer must be large enough for reply
  DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sendSize + 8;
  LPVOID replyBuffer = malloc(replySize);
  
  if (replyBuffer == nullptr) {
    IcmpCloseHandle(hIcmp);
    return false;
  }

  int successCount = 0;

  for (int i = 0; i < pingCount; i++) {
    DWORD result = IcmpSendEcho(
        hIcmp,
        targetIP,
        sendData,
        sendSize,
        nullptr,
        replyBuffer,
        replySize,
        timeoutMs);

    if (result > 0) {
      PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)replyBuffer;
      if (pEchoReply->Status == IP_SUCCESS) {
        successCount++;
      }
    }
    
    // Small delay between pings
    if (i < pingCount - 1) {
      Sleep(100);
    }
  }

  free(replyBuffer);
  IcmpCloseHandle(hIcmp);

  return successCount >= minSuccess;
}

// Combined check with fallback
bool CheckInternetWithFallback(const std::string &target, int pingCount, int timeoutMs, int minSuccess) {
  // First, quick check if we're connected at all
  auto& wininet = WinInetLoader::Instance();
  if (!wininet.IsConnected()) {
    return false;
  }
  
  // Try ICMP ping first
  if (CheckWithIcmpPing(target, pingCount, timeoutMs, minSuccess)) {
    return true;
  }
  
  // Fallback: Try WinInet method
  return CheckWithWinInet();
}

// static
void NativePingCheckerPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows *registrar) {
  auto channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "native_ping_checker",
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<NativePingCheckerPlugin>();

  channel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  registrar->AddPlugin(std::move(plugin));
}

NativePingCheckerPlugin::NativePingCheckerPlugin() {}

NativePingCheckerPlugin::~NativePingCheckerPlugin() {}

void NativePingCheckerPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {
  
  if (method_call.method_name().compare("checkInternet") == 0) {
    // Default values
    std::string target = "8.8.8.8";
    int pingCount = 5;
    int timeoutMs = 2000;
    int minSuccess = 3;

    // Get parameters from Flutter
    const auto *arguments = std::get_if<flutter::EncodableMap>(method_call.arguments());
    if (arguments) {
      auto targetIt = arguments->find(flutter::EncodableValue("target"));
      if (targetIt != arguments->end()) {
        target = std::get<std::string>(targetIt->second);
      }

      auto countIt = arguments->find(flutter::EncodableValue("pingCount"));
      if (countIt != arguments->end()) {
        pingCount = std::get<int>(countIt->second);
      }

      auto timeoutIt = arguments->find(flutter::EncodableValue("timeoutMs"));
      if (timeoutIt != arguments->end()) {
        timeoutMs = std::get<int>(timeoutIt->second);
      }

      auto minSuccessIt = arguments->find(flutter::EncodableValue("minSuccess"));
      if (minSuccessIt != arguments->end()) {
        minSuccess = std::get<int>(minSuccessIt->second);
      }
    }

    bool isOnline = CheckInternetWithFallback(target, pingCount, timeoutMs, minSuccess);
    result->Success(flutter::EncodableValue(isOnline));
  } else {
    result->NotImplemented();
  }
}

}  // namespace native_ping_checker
