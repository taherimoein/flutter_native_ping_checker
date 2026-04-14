import 'dart:async';
import 'package:flutter/services.dart';
import 'ping_config.dart';

// Re-export PingConfigException for convenience
export 'ping_config.dart' show PingConfigException, PingConfig;

/// Main class for checking internet connectivity using native ping
class NativePingChecker {
  static const MethodChannel _channel = MethodChannel('native_ping_checker');

  static PingConfig? _config;
  static bool _isInitialized = false;
  static Timer? _periodicTimer;
  static StreamController<bool>? _connectionController;
  static bool _lastStatus = false;
  static Function(bool isConnected)? _onStatusChangeCallback;
  static bool _checkInFlight = false;

  /// Stream of connection status updates
  static Stream<bool> get connectionStream {
    _connectionController ??= StreamController<bool>.broadcast();
    return _connectionController!.stream;
  }

  /// Get current configuration
  static PingConfig get config {
    _ensureInitialized();
    return _config!;
  }

  /// Whether the plugin is initialized
  static bool get isInitialized => _isInitialized;

  /// Last known connection status
  static bool get lastStatus => _lastStatus;

  /// Whether periodic checking is currently running
  static bool get isPeriodicCheckRunning => _periodicTimer?.isActive ?? false;

  /// Ensure plugin is initialized
  static void _ensureInitialized() {
    if (!_isInitialized || _config == null) {
      throw StateError(
        'NativePingChecker is not initialized!\n'
        'Please call init() before using:\n'
        'await NativePingChecker.init(config: {...});',
      );
    }
  }

  /// Initialize the plugin with configuration map
  ///
  /// [config] - Required configuration map with the following keys:
  ///   - target (String): IP address or domain to ping
  ///   - pingCount (int): Number of ping attempts
  ///   - timeoutMs (int): Timeout per ping in milliseconds
  ///   - minSuccessCount (int): Minimum successful pings required
  ///   - checkIntervalSeconds (int): Interval between periodic checks
  ///
  /// [autoStartPeriodicCheck] - If true, starts periodic checking automatically (default: true)
  ///
  /// [onStatusChange] - Callback called whenever connection status changes
  ///
  /// Throws [PingConfigException] if any required field is missing.
  static Future<void> init({
    required Map<String, dynamic> config,
    bool autoStartPeriodicCheck = true,
    Function(bool isConnected)? onStatusChange,
  }) async {
    // Parse and validate config - throws PingConfigException if invalid
    _config = PingConfig.fromMap(config);
    _isInitialized = true;
    _onStatusChangeCallback = onStatusChange;
    _connectionController ??= StreamController<bool>.broadcast();

    if (autoStartPeriodicCheck) {
      await startPeriodicCheck(onStatusChange: _onStatusChangeCallback);
    }
  }

  /// Check internet connection once
  static Future<bool> checkConnection() async {
    _ensureInitialized();

    if (_checkInFlight) {
      return _lastStatus;
    }
    _checkInFlight = true;
    try {
      final bool result = await _channel.invokeMethod(
        'checkInternet',
        _config!.toMap(),
      );
      _lastStatus = result;
      return result;
    } catch (e) {
      _lastStatus = false;
      return false;
    } finally {
      _checkInFlight = false;
    }
  }

  /// Start periodic connection checking
  static Future<void> startPeriodicCheck({
    Function(bool isConnected)? onStatusChange,
  }) async {
    _ensureInitialized();
    stopPeriodicCheck();

    // Initial check
    final initialStatus = await checkConnection();
    _connectionController?.add(initialStatus);
    onStatusChange?.call(initialStatus);

    // Periodic check
    _periodicTimer = Timer.periodic(
      Duration(seconds: _config!.checkIntervalSeconds),
      (_) {
        if (_checkInFlight) {
          return;
        }
        unawaited((() async {
          final previousStatus = _lastStatus;
          final isConnected = await checkConnection();

          if (isConnected != previousStatus) {
            _connectionController?.add(isConnected);
            onStatusChange?.call(isConnected);
          }
        })());
      },
    );
  }

  /// Stop periodic connection checking
  static void stopPeriodicCheck() {
    _periodicTimer?.cancel();
    _periodicTimer = null;
  }

  /// Dispose and release all resources
  static void dispose() {
    stopPeriodicCheck();
    _connectionController?.close();
    _connectionController = null;
    _config = null;
    _isInitialized = false;
  }
}
