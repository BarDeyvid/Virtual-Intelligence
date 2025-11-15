// lib/emoji_mapper.dart
Map<String, String> _emojiTable = {
  'triste': '😢',
  'feliz': '😊',
  'surpreso': '😲',
  'bravo': '😠',
  'amor': '😍',
  'riso': '😂',
  'confuso': '😕',
  'assustado': '😱',
  'rainha': '👑',
  'fogo': '🔥',
  'estrela': '⭐',
  'coração': '❤️',
  'raiva': '😡',
  'chorando': '😭',
  'sorriso': '😄',
  'piscando': '😉',
  'pensativo': '🤔',
  'sono': '😴',
};

String mapEmoji(String text) {
  final regex = RegExp(r'$Emoji:\s*([^)]+)$');
  return text.replaceAllMapped(regex, (match) {
    final key = match.group(1)?.trim().toLowerCase() ?? '';
    return _emojiTable[key] ?? match.group(0)!; // se não encontrar, deixa o texto original
  });
}
