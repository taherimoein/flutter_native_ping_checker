/// Exception thrown when required configuration is missing
class PingConfigException implements Exception {
  final String message;
  final List<String> missingFields;

  PingConfigException(this.missingFields)
      : message = 'Missing required configuration fields: ${missingFields.join(", ")}\n\n'
            'Required fields:\n'
            '  - target (String): IP address or domain to ping\n'
            '  - pingCount (int): Number of ping attempts\n'
            '  - timeoutMs (int): Timeout per ping in milliseconds\n'
            '  - minSuccessCount (int): Minimum successful pings required\n'
            '  - checkIntervalSeconds (int): Interval between periodic checks';

  @override
  String toString() => 'PingConfigException: $message';
}

/// Configuration for ping checker
class PingConfig {
  final String target;
  final int pingCount;
  final int timeoutMs;
  final int minSuccessCount;
  final int checkIntervalSeconds;

  const PingConfig({
    required this.target,
    required this.pingCount,
    required this.timeoutMs,
    required this.minSuccessCount,
    required this.checkIntervalSeconds,
  });

  /// Create from Map with validation
  ///
  /// Throws [PingConfigException] if any required field is missing.
  factory PingConfig.fromMap(Map<String, dynamic> map) {
    final List<String> missingFields = [];

    final target = map['target']?.toString() ?? map['ping_target']?.toString();
    if (target == null || target.isEmpty) missingFields.add('target');

    final pingCount = _parseInt(map['pingCount'] ?? map['ping_count']);
    if (pingCount == null) missingFields.add('pingCount');

    final timeoutMs = _parseInt(map['timeoutMs'] ?? map['ping_timeout_ms']);
    if (timeoutMs == null) missingFields.add('timeoutMs');

    final minSuccessCount = _parseInt(map['minSuccessCount'] ?? map['min_success_count']);
    if (minSuccessCount == null) missingFields.add('minSuccessCount');

    final checkIntervalSeconds = _parseInt(map['checkIntervalSeconds'] ?? map['check_interval_seconds']);
    if (checkIntervalSeconds == null) missingFields.add('checkIntervalSeconds');

    if (missingFields.isNotEmpty) {
      throw PingConfigException(missingFields);
    }

    return PingConfig(
      target: target!,
      pingCount: pingCount!,
      timeoutMs: timeoutMs!,
      minSuccessCount: minSuccessCount!,
      checkIntervalSeconds: checkIntervalSeconds!,
    );
  }

  static int? _parseInt(dynamic value) {
    if (value == null) return null;
    if (value is int) return value;
    return int.tryParse(value.toString());
  }

  /// Convert to Map for sending to native code
  Map<String, dynamic> toMap() => {
        'target': target,
        'pingCount': pingCount,
        'timeoutMs': timeoutMs,
        'minSuccess': minSuccessCount,
      };

  @override
  String toString() =>
      'PingConfig(target: $target, count: $pingCount, timeout: $timeoutMs, minSuccess: $minSuccessCount, interval: $checkIntervalSeconds)';
}
