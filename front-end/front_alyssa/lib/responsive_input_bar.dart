import 'package:flutter/material.dart';

class ResponsiveInputBar extends StatelessWidget {
  final TextEditingController controller;
  final bool isApiConnected;
  final bool isLoading;
  final VoidCallback onSend;

  const ResponsiveInputBar({
    super.key,
    required this.controller,
    required this.isApiConnected,
    required this.isLoading,
    required this.onSend,
  });

  @override
  Widget build(BuildContext context) {
    // Current theme values
    final theme = Theme.of(context);
    final primaryColor = theme.colorScheme.primary;
    final disabledColor =
        theme.disabledColor.withValues(alpha: 0.5);          // dimmed when disabled
    final backgroundColor = theme.cardColor;          // container background
    final inputFillColor =
        isApiConnected ? theme.colorScheme.surface : Colors.grey[200];

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      decoration: BoxDecoration(
        color: backgroundColor,
        boxShadow: [
          BoxShadow(
            offset: const Offset(0, -2),
            blurRadius: 4,
            color: Colors.black.withValues(alpha: 0.1),
          ),
        ],
      ),
      child: Row(
        children: [
          // Expanded TextField so it takes the remaining space
          Expanded(
            child: TextField(
              controller: controller,
              enabled: isApiConnected && !isLoading,
              decoration: InputDecoration(
                hintText: isApiConnected
                    ? 'Digite sua mensagem…'
                    : 'Aguardando conexão com a API…',
                border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(24),
                  borderSide: BorderSide.none,
                ),
                filled: true,
                fillColor: inputFillColor,
                contentPadding:
                    const EdgeInsets.symmetric(horizontal: 20, vertical: 14),
              ),
              onSubmitted: (_) => onSend(),
            ),
          ),
          // Small space between field and button
          const SizedBox(width: 8),
          // Send button – always next to the text field
          _buildSendButton(primaryColor, disabledColor),
        ],
      ),
    );
  }

  // Botão reutilizado – usa os valores de cor que foram passados
  Widget _buildSendButton(Color primary, Color disabled) {
    return SizedBox(
      width: 48,
      height: 48,
      child: Material(
        color: (isApiConnected && !isLoading) ? primary : disabled,
        shape: const CircleBorder(),
        child: IconButton(
          icon: const Icon(Icons.send),
          color: Colors.white,
          onPressed:
              (isApiConnected && !isLoading) ? onSend : null,
        ),
      ),
    );
  }
}
