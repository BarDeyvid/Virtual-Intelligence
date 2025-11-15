// unchanged – kept for reference
import 'package:flutter/material.dart';

class ThemeNotifier extends ChangeNotifier {
  bool _isDark = false;

  bool get isDark => _isDark;

  void toggleTheme() {
    _isDark = !_isDark;
    notifyListeners();
  }

  ThemeData get currentTheme =>
      _isDark ? _darkTheme : _lightTheme;

  static final ThemeData _darkTheme = ThemeData.dark().copyWith(
    primaryColor: Colors.deepPurple,
    scaffoldBackgroundColor: const Color(0xFF121212),
    colorScheme: ColorScheme.fromSeed(
      seedColor: Colors.deepPurple,
      surface: const Color(0xFF1E1E1E),
      primary: Colors.deepPurpleAccent,
      secondary: Colors.purpleAccent,
      onPrimary: Colors.white,
      onSurface: Colors.grey[200]!,
    ),
    textTheme: ThemeData.dark().textTheme.apply(
      bodyColor: Colors.grey[300],
      displayColor: Colors.grey[300],
    ),
  );

  static final ThemeData _lightTheme = ThemeData.light().copyWith(
    primaryColor: Colors.deepPurple,
    scaffoldBackgroundColor: Colors.white,
    colorScheme: ColorScheme.fromSeed(
      seedColor: Colors.deepPurple,
      surface: Colors.grey[100]!,
      primary: Colors.deepPurpleAccent,
      secondary: Colors.purpleAccent,
      onPrimary: Colors.black,
      onSurface: Colors.grey[800]!,
    ),
  );
}