import 'package:flutter/material.dart';
import 'package:window_manager/window_manager.dart';
import 'package:provider/provider.dart';

import 'chat_screen.dart';
import 'theme_notifier.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();

  // Configurações de janela (opcional)
  await windowManager.ensureInitialized();
  const size = Size(1280, 720);
  const center = true;

  WindowOptions options = const WindowOptions(
    titleBarStyle: TitleBarStyle.normal,
    size: size,
    center: center,
    backgroundColor: Colors.transparent,
    skipTaskbar: false,
    alwaysOnTop: false,
  );

  await windowManager.waitUntilReadyToShow(options, () async {
    await windowManager.setTitle('Alys​sa');
    await windowManager.show();
  });

  runApp(
    ChangeNotifierProvider<ThemeNotifier>(
      create: (_) => ThemeNotifier(),
      child: const ChatBotApp(),
    ),
  );

}

class ChatBotApp extends StatelessWidget {
  const ChatBotApp({super.key});

  @override
  Widget build(BuildContext context) {
    // O tema é obtido do provider, e o app reage a mudanças
    final theme = Provider.of<ThemeNotifier>(context);

    return MaterialApp(
      title: 'Alyssa',
      theme: theme.currentTheme,
      debugShowCheckedModeBanner: false,
      home: const ChatScreen(),
    );
  }
}
