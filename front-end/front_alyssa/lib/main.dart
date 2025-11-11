import 'package:flutter/material.dart';
import 'chat_screen.dart';

void main() {
  runApp(const ChatBotApp());
}

class ChatBotApp extends StatelessWidget {
  const ChatBotApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'ChatBot',
      theme: ThemeData(
        primarySwatch: Colors.blue,
        useMaterial3: true,
      ),
      home: const ChatScreen(),
      debugShowCheckedModeBanner: false,
    );
  }
}