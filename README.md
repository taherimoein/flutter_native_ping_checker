# Native Ping Checker

A Flutter plugin for checking internet connectivity using native ICMP ping.

## Features

- Native ICMP ping (no HTTP requests)
- Windows & Android support
- Simple Map-based configuration
- All configuration fields are required
- Automatic periodic checking
- Stream-based status updates

## Installation

```yaml
dependencies:
  native_ping_checker: ^0.0.1
```

## Usage

```dart
import 'package:native_ping_checker/native_ping_checker.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  try {
    await NativePingChecker.init(
      config: {
        'target': '8.8.8.8',
        'pingCount': 5,
        'timeoutMs': 2000,
        'minSuccessCount': 3,
        'checkIntervalSeconds': 30,
      },
      autoStartPeriodicCheck: true,
      onStatusChange: (isConnected) {
        print('Connection: ${isConnected ? "Online" : "Offline"}');
      },
    );

    runApp(MyApp());
  } on PingConfigException catch (e) {
    print(e.message);
  }
}
```

## Configuration

All fields are **required**:

| Field | Type | Description |
|-------|------|-------------|
| `target` | String | IP address or domain to ping |
| `pingCount` | int | Number of ping attempts |
| `timeoutMs` | int | Timeout per ping (ms) |
| `minSuccessCount` | int | Minimum successful pings |
| `checkIntervalSeconds` | int | Interval between checks (seconds) |

## API

### Initialize

```dart
await NativePingChecker.init(
  config: {...},
  autoStartPeriodicCheck: true,  // default: true
  onStatusChange: (isConnected) {},
);
```

### Check Once

```dart
bool isOnline = await NativePingChecker.checkConnection();
```

### Stream

```dart
NativePingChecker.connectionStream.listen((isConnected) {
  print('Status: $isConnected');
});
```

### Control

```dart
NativePingChecker.startPeriodicCheck();
NativePingChecker.stopPeriodicCheck();
NativePingChecker.dispose();
```

### Properties

```dart
bool lastStatus = NativePingChecker.lastStatus;
bool isRunning = NativePingChecker.isPeriodicCheckRunning;
bool isInit = NativePingChecker.isInitialized;
PingConfig config = NativePingChecker.config;
```

## Platform Support

| Platform | Status |
|----------|--------|
| Windows | Supported |
| Android | Supported |
| iOS | Planned |
| macOS | Planned |

## License

MIT License
