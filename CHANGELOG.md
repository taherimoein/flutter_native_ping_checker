## 0.0.2

* **Dart:** Prevent overlapping `checkConnection()` calls with an in-flight guard; concurrent callers get the last known status until the current check finishes.
* **Dart:** Periodic monitoring skips starting a new check while one is already running, and uses `unawaited` so timer callbacks do not stack async work.
* **Windows:** Resolve ping targets with Winsock (`getaddrinfo`) so hostnames work for ICMP, not only dotted IPv4; initialize Winsock once and link `ws2_32` (include order adjusted so `winsock2.h` is safe with Flutter’s headers).
* **Windows:** Run `checkInternet` work on a background thread so long ICMP/WinInet operations do not block the platform message loop or trigger “Not responding.”
* **Windows:** Shorter delay between successive ICMP echo attempts (100 ms → 20 ms).

## 0.0.1

* Initial release
* Windows support with native ICMP ping
* Android support with InetAddress.isReachable
* Configurable ping count, timeout, and minimum success count
* Periodic connection checking with Stream
* Auto-start periodic check option
