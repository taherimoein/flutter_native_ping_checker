import 'package:flutter/material.dart';
import 'package:native_ping_checker/native_ping_checker.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();

  try {
    // Initialize with config map - all fields are required
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
        debugPrint('Connection status: ${isConnected ? "Online" : "Offline"}');
      },
    );

    runApp(const MyApp());
  } on PingConfigException catch (e) {
    // Show missing config fields error
    runApp(ConfigErrorApp(error: e.message));
  }
}

/// App to display configuration error
class ConfigErrorApp extends StatelessWidget {
  final String error;

  const ConfigErrorApp({super.key, required this.error});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      home: Scaffold(
        backgroundColor: Colors.red.shade50,
        body: Center(
          child: Padding(
            padding: const EdgeInsets.all(32),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Icon(Icons.error_outline, size: 80, color: Colors.red.shade700),
                const SizedBox(height: 24),
                Text(
                  'Configuration Error',
                  style: TextStyle(
                    fontSize: 24,
                    fontWeight: FontWeight.bold,
                    color: Colors.red.shade700,
                  ),
                ),
                const SizedBox(height: 16),
                Container(
                  padding: const EdgeInsets.all(16),
                  decoration: BoxDecoration(
                    color: Colors.white,
                    borderRadius: BorderRadius.circular(12),
                    border: Border.all(color: Colors.red.shade200),
                  ),
                  child: SelectableText(
                    error,
                    textAlign: TextAlign.left,
                    style: TextStyle(
                      fontSize: 14,
                      color: Colors.grey.shade800,
                      fontFamily: 'Courier',
                    ),
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

class MyApp extends StatelessWidget {
  const MyApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Native Ping Checker Demo',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.blue),
        useMaterial3: true,
      ),
      home: const HomePage(),
    );
  }
}

class HomePage extends StatefulWidget {
  const HomePage({super.key});

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  bool _isOnline = false;
  bool _isChecking = false;
  String _statusText = 'Unknown';

  @override
  void initState() {
    super.initState();
    // Listen to connection stream
    // Periodic check is auto-started in main
    _listenToConnectionStream();

    // Get last known status
    _isOnline = NativePingChecker.lastStatus;
    _statusText = _isOnline ? 'Online' : 'Offline';
  }

  void _listenToConnectionStream() {
    NativePingChecker.connectionStream.listen((isConnected) {
      if (mounted) {
        setState(() {
          _isOnline = isConnected;
          _statusText = isConnected ? 'Online' : 'Offline';
        });
      }
    });
  }

  Future<void> _checkNow() async {
    setState(() {
      _isChecking = true;
      _statusText = 'Checking...';
    });

    final result = await NativePingChecker.checkConnection();

    setState(() {
      _isOnline = result;
      _isChecking = false;
      _statusText = result ? 'Online' : 'Offline';
    });
  }

  @override
  void dispose() {
    NativePingChecker.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        backgroundColor: Theme.of(context).colorScheme.inversePrimary,
        title: const Text('Native Ping Checker'),
        centerTitle: true,
      ),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            // Status icon
            AnimatedContainer(
              duration: const Duration(milliseconds: 300),
              padding: const EdgeInsets.all(30),
              decoration: BoxDecoration(
                shape: BoxShape.circle,
                color: _isOnline
                    ? Colors.green.withValues(alpha: 0.2)
                    : Colors.red.withValues(alpha: 0.2),
              ),
              child: Icon(
                _isOnline ? Icons.wifi : Icons.wifi_off,
                size: 100,
                color: _isOnline ? Colors.green : Colors.red,
              ),
            ),

            const SizedBox(height: 30),

            // Status text
            Text(
              _statusText,
              style: TextStyle(
                fontSize: 28,
                fontWeight: FontWeight.bold,
                color: _isOnline ? Colors.green : Colors.red,
              ),
            ),

            const SizedBox(height: 10),

            // Current config
            Container(
              margin: const EdgeInsets.all(20),
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                color: Colors.grey.shade100,
                borderRadius: BorderRadius.circular(12),
              ),
              child: Column(
                children: [
                  Text(
                    'Current Configuration:',
                    style: TextStyle(
                      fontSize: 16,
                      fontWeight: FontWeight.bold,
                      color: Colors.grey.shade700,
                    ),
                  ),
                  const SizedBox(height: 8),
                  Text(
                    NativePingChecker.config.toString(),
                    textAlign: TextAlign.center,
                    style: TextStyle(
                      fontSize: 12,
                      color: Colors.grey.shade600,
                    ),
                  ),
                ],
              ),
            ),

            const SizedBox(height: 20),

            // Check button
            ElevatedButton.icon(
              onPressed: _isChecking ? null : _checkNow,
              icon: _isChecking
                  ? const SizedBox(
                      width: 20,
                      height: 20,
                      child: CircularProgressIndicator(strokeWidth: 2),
                    )
                  : const Icon(Icons.refresh),
              label: Text(_isChecking ? 'Checking...' : 'Check Now'),
              style: ElevatedButton.styleFrom(
                padding: const EdgeInsets.symmetric(
                  horizontal: 30,
                  vertical: 15,
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
