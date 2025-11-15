// lib/text_parser.dart
String normalizeText(String input) {
  var result = input;

  // Negrito **texto**
  final boldReg = RegExp(r'\*\*(.+?)\*\*');
  result = result.replaceAllMapped(boldReg, (m) => '<b>${m[1]}</b>');

  // Itálico _texto_ ou *texto*
  final italicReg = RegExp(r'(\*|_)(.+?)(\*|_)');
  result = result.replaceAllMapped(italicReg, (m) => '<i>${m[2]}</i>');

  // Links [label](url)
  final linkReg = RegExp(r'$(.+?)$$(https?:\/\/[^\s]+)$');
  result = result.replaceAllMapped(linkReg,
      (m) => '<a href="${m[2]}">${m[1]}</a>');
  
  return result;
}