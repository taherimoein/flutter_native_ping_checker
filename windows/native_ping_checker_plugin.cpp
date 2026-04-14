// Flutter headers pull in windows.h; winsock2 must appear before the first
// windows.h include or the SDK headers conflict (sockaddr redefinition).
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>

#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "native_ping_checker_plugin.h"

#include <flutter/standard_method_codec.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

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

static void EnsureWinsockInitialized() {
  static std::once_flag once;
  std::call_once(once, []() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
  });
}

// Resolve hostname or dotted IPv4 to sin_addr.s_addr (network byte order).
static bool ResolveTargetToIpv4(const std::string& target, ULONG& out_addr) {
  EnsureWinsockInitialized();

  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  addrinfo* resolved = nullptr;
  if (getaddrinfo(target.c_str(), nullptr, &hints, &resolved) != 0 ||
      resolved == nullptr) {
    // Retry with a generic hint (some networks behave better without IPPROTO).
    hints.ai_protocol = 0;
    hints.ai_socktype = 0;
    if (getaddrinfo(target.c_str(), nullptr, &hints, &resolved) != 0 ||
        resolved == nullptr) {
      return false;
    }
  }

  for (addrinfo* ptr = resolved; ptr != nullptr; ptr = ptr->ai_next) {
    if (ptr->ai_family == AF_INET && ptr->ai_addrlen >= sizeof(sockaddr_in)) {
      const auto* sin = reinterpret_cast<const sockaddr_in*>(ptr->ai_addr);
      out_addr = sin->sin_addr.s_addr;
      freeaddrinfo(resolved);
      return out_addr != 0;
    }
  }
  freeaddrinfo(resolved);
  return false;
}

// Method 1: Check using Windows Internet API (most reliable)
bool CheckWithWinInet() {
  auto& wininet = WinInetLoader::Instance();

  if (!wininet.IsConnected()) {
    return false;
  }

  // Try to actually reach an internet resource
  return wininet.CheckConnection(
      "http://www.msftconnecttest.com/connecttest.txt");
}

// Method 2: Check using ICMP ping
bool CheckWithIcmpPing(const std::string& target, int pingCount, int timeoutMs,
                       int minSuccess) {
  ULONG target_ip = 0;
  if (!ResolveTargetToIpv4(target, target_ip)) {
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
    DWORD result = IcmpSendEcho(hIcmp, target_ip, sendData, sendSize, nullptr,
                                replyBuffer, replySize, timeoutMs);

    if (result > 0) {
      PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)replyBuffer;
      if (pEchoReply->Status == IP_SUCCESS) {
        successCount++;
      }
    }

    if (i < pingCount - 1) {
      Sleep(20);
    }
  }

  free(replyBuffer);
  IcmpCloseHandle(hIcmp);

  return successCount >= minSuccess;
}

// Combined check with fallback
bool CheckInternetWithFallback(const std::string& target, int pingCount,
                               int timeoutMs, int minSuccess) {
  // First, quick check if we're connected at all
  auto& wininet = WinInetLoader::Instance();
  if (!wininet.IsConnected()) {
    return false;
  }

  // Try ICMP ping first (now works for hostnames via getaddrinfo)
  if (CheckWithIcmpPing(target, pingCount, timeoutMs, minSuccess)) {
    return true;
  }

  // Fallback: Try WinInet method
  return CheckWithWinInet();
}

// static
void NativePingCheckerPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows* registrar) {
  auto channel =
      std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
          registrar->messenger(), "native_ping_checker",
          &flutter::StandardMethodCodec::GetInstance());

  auto plugin = std::make_unique<NativePingCheckerPlugin>();

  channel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto& call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  registrar->AddPlugin(std::move(plugin));
}

NativePingCheckerPlugin::NativePingCheckerPlugin() {}

NativePingCheckerPlugin::~NativePingCheckerPlugin() {}

void NativePingCheckerPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue>& method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result) {

  if (method_call.method_name().compare("checkInternet") == 0) {
    std::string target = "8.8.8.8";
    int pingCount = 5;
    int timeoutMs = 2000;
    int minSuccess = 3;

    const auto* arguments =
        std::get_if<flutter::EncodableMap>(method_call.arguments());
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

      auto minSuccessIt =
          arguments->find(flutter::EncodableValue("minSuccess"));
      if (minSuccessIt != arguments->end()) {
        minSuccess = std::get<int>(minSuccessIt->second);
      }
    }

    // Heavy work off the Windows platform thread so the shell message pump
    // keeps processing (avoids "Not Responding" during long ICMP/WinInet).
    std::thread([target, pingCount, timeoutMs, minSuccess,
                 result = std::move(result)]() mutable {
      const bool is_online =
          CheckInternetWithFallback(target, pingCount, timeoutMs, minSuccess);
      result->Success(flutter::EncodableValue(is_online));
    }).detach();
    return;
  }
  result->NotImplemented();
}

}  // namespace native_ping_checker
