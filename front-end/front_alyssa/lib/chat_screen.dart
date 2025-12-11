import 'package:flutter_markdown_plus/flutter_markdown_plus.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'responsive_input_bar.dart';
import 'theme_notifier.dart';
import 'emoji_mapper.dart';
import 'chat_service.dart';
import 'text_parser.dart';
import 'message.dart';

class ChatScreen extends StatefulWidget {
  const ChatScreen({super.key});

  @override
  State<ChatScreen> createState() => _ChatScreenState();
}

class _ChatScreenState extends State<ChatScreen> {
  final List<Message> _messages = [];
  final TextEditingController _textController = TextEditingController();
  final ChatService _chatService = ChatService();
  bool _isLoading = false;
  bool _apiConnected = false;
  bool _checkingConnection = true;

  // ignore: unused_field
  late ThemeNotifier _themeNotifier;

  // Maintain a conversation history
  final List<String> _conversationHistory = [];

  @override
  void initState() {
    super.initState();
    _checkApiConnection();          // ← now defined below
    _themeNotifier = context.read<ThemeNotifier>();
  }

  /* ------------------------------------------------------------------ */
  /* ------------- API & debugging helpers (all used in the UI) ------- */
  /* ------------------------------------------------------------------ */

  Future<void> _checkApiConnection() async {
    final isConnected = await _chatService.checkHealth();
    setState(() {
      _apiConnected = isConnected;
      _checkingConnection = false;
    });

    if (!isConnected) {
      final warningMessage = Message(
        text:
            '⚠️ Não foi possível conectar com a API. Verifique se o servidor está rodando.',
        isUser: false,
        timestamp: DateTime.now(),
      );
      setState(() => _messages.add(warningMessage));
    }
  }

  Future<void> _retryConnection() async {
    setState(() => _checkingConnection = true);
    await _checkApiConnection();
  }

  void _showDebugDialog() {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Informações de Debug'),
        content: SingleChildScrollView(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min,
            children: [
              Text('🔌 API Conectada: $_apiConnected'),
              const Text('📡 Endpoint: http://localhost:8181'),
              Text('💾 Mensagens: ${_messages.length}'),
              const SizedBox(height: 16),
              const Text(
                'Dica: Digite "debug" em qualquer mensagem para ver a resposta completa da API',
                style: TextStyle(fontStyle: FontStyle.italic),
              ),
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Fechar'),
          ),
        ],
      ),
    );
  }

  Future<void> _testConnection() async {
    setState(() => _isLoading = true);
    final results = await _chatService.testConnection();

    final debugMessage = Message(
      text:
          '🔍 RESULTADOS DO TESTE:\n\n🏥 Health: ${results['health_status']}\n🤔 Think: ${results['think_status']}\n🔄 Fusion: ${results['fusion_status']}\n📦 Error: ${results['error'] ?? "Nenhum"}\n\nSe Health=200, a API está acessível!',
      isUser: false,
      timestamp: DateTime.now(),
    );

    setState(() {
      _messages.add(debugMessage);
      _isLoading = false;
    });
  }

  /* ------------------------------------------------------------------ */
  /* -------------------------- UI ------------------------------------- */
  /* ------------------------------------------------------------------ */

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);          // current theme
    final themeNotifier = Provider.of<ThemeNotifier>(context);

    return Scaffold(
      appBar: AppBar(
        title: const Text('Alyssa'),
        backgroundColor:
            _apiConnected ? theme.colorScheme.primary : theme.colorScheme.error,
        foregroundColor: Colors.white,
        elevation: 2,
        actions: [
          if (_checkingConnection)
            const Padding(
              padding: EdgeInsets.all(12.0),
              child: SizedBox(
                width: 20,
                height: 20,
                child: CircularProgressIndicator(
                  strokeWidth: 2,
                  valueColor:
                      AlwaysStoppedAnimation<Color>(Colors.white),
                ),
              ),
            )
          else
            IconButton(
              icon: Icon(
                _apiConnected ? Icons.cloud_done : Icons.cloud_off,
                color: Colors.white,
              ),
              onPressed: _retryConnection,
              tooltip:
                  _apiConnected ? 'API Conectada' : 'API Desconectada',
            ),
          IconButton(
            icon: const Icon(Icons.bug_report),
            onPressed: _showDebugDialog,
            tooltip: 'Debug',
          ),
          IconButton(
            icon: const Icon(Icons.network_check),
            onPressed: _testConnection,
            tooltip: 'Testar Conexão',
          ),
          IconButton(
            icon: Icon(
              themeNotifier.isDark
                  ? Icons.dark_mode
                  : Icons.light_mode,
              color: Colors.white,
            ),
            onPressed: () => themeNotifier.toggleTheme(),
            tooltip: 'Alternar Tema',
          ),
        ],
      ),
      body: Column(
        children: [
          if (!_apiConnected && !_checkingConnection)
            Container(
              width: double.infinity,
              padding: const EdgeInsets.all(12),
              color:
                  theme.colorScheme.secondaryContainer.withValues(alpha: 0.2),
              child: Row(
                children: [
                  const Icon(Icons.warning, color: Colors.orange),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      'API desconectada',
                      style: TextStyle(
                        color: theme.colorScheme.error,
                        fontWeight: FontWeight.bold,
                      ),
                    ),
                  ),
                  TextButton(
                    onPressed: _retryConnection,
                    child: const Text('Tentar Conectar'),
                  ),
                ],
              ),
            ),
          Expanded(
            // Main chat area
            child: _messages.isEmpty && !_checkingConnection
                ? Center(
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Icon(_apiConnected
                            ? Icons.chat_bubble_outline
                            : Icons.error_outline,
                            size: 64,
                            color: _apiConnected
                                ? theme.colorScheme.primary
                                : theme.colorScheme.error),
                        const SizedBox(height: 16),
                        Text(
                          _apiConnected
                              ? 'Como posso ajudar você hoje?'
                              : 'Conecte‑se à API para começar',
                          style: TextStyle(
                            fontSize: 18,
                            color: _apiConnected
                                ? theme.textTheme.bodyLarge?.color
                                : theme.colorScheme.error,
                          ),
                        ),
                        if (_apiConnected) ...[
                          const SizedBox(height: 8),
                          const Text(
                            'Dica: Digite "debug" para ver respostas da API',
                            style: TextStyle(
                              fontSize: 12,
                              color: Colors.grey,
                              fontStyle: FontStyle.italic,
                            ),
                          ),
                        ],
                      ],
                    ),
                  )
                : ListView.builder(
                    padding: const EdgeInsets.all(16),
                    reverse: false,
                    itemCount:
                        _messages.length + (_isLoading ? 1 : 0),
                    itemBuilder: (context, index) {
                      if (index == _messages.length && _isLoading) {
                        return _buildLoadingIndicator();
                      }
                      return _buildMessageBubble(_messages[index]);
                    },
                  ),
          ),
          // Input bar
          _buildInputArea(),
        ],
      ),
    );
  }

  /* ------------------------------------------------------------------ */
  /* ------------------------ message widgets -------------------------- */
  /* ------------------------------------------------------------------ */

  Widget _buildMessageBubble(Message message) {
    final theme = Theme.of(context);
    return Container(
      margin: const EdgeInsets.symmetric(vertical: 8),
      child: Row(
        mainAxisAlignment:
            message.isUser ? MainAxisAlignment.end : MainAxisAlignment.start,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          if (!message.isUser)
            Container(
              margin: const EdgeInsets.only(right: 8, top: 4),
              child: Image.asset(
                'assets/icon/alyssa.png',
                width: 60,
                height: 60,
              ),
            ),
          Flexible(
            child: Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: message.isUser
                    ? theme.colorScheme.primary
                    : theme.colorScheme.surface.withValues(alpha: 0.1),
                borderRadius: BorderRadius.circular(16),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  !message.isUser
                      ? MarkdownBody(data: message.text)
                      : Text(
                          message.text,
                          style:
                              const TextStyle(color: Colors.white),
                        ),
                  const SizedBox(height: 4),
                  Text(
                    _formatTime(message.timestamp),
                    style: TextStyle(
                      color: message.isUser
                          ? theme.colorScheme.onPrimary.withValues(alpha: 0.7)
                          : theme.colorScheme.onSurface.withValues(alpha: 0.6),
                      fontSize: 12,
                    ),
                  ),
                ],
              ),
            ),
          ),
          if (message.isUser)
            Container(
              margin: const EdgeInsets.only(left: 8, top: 4),
              child: const CircleAvatar(
                backgroundColor: Colors.green,
                radius: 16,
                child: Icon(
                  Icons.person,
                  color: Colors.white,
                  size: 18,
                ),
              ),
            ),
        ],
      ),
    );
  }

  Widget _buildLoadingIndicator() {
    final theme = Theme.of(context);
    return Container(
      margin: const EdgeInsets.symmetric(vertical: 8),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.start,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Container(
            margin: const EdgeInsets.only(right: 8, top: 4),
            child: CircleAvatar(
              backgroundColor:
                  _apiConnected ? theme.colorScheme.primary : theme.colorScheme.error,
              radius: 16,
              child: const Icon(
                Icons.smart_toy,
                color: Colors.white,
                size: 18,
              ),
            ),
          ),
          Container(
            padding: const EdgeInsets.all(12),
            decoration: BoxDecoration(
              color: theme.colorScheme.surface.withValues(alpha: 0.1),
              borderRadius: BorderRadius.circular(16),
            ),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                SizedBox(
                  width: 20,
                  height: 20,
                  child: CircularProgressIndicator(
                    strokeWidth: 2,
                    valueColor:
                        AlwaysStoppedAnimation<Color>(theme.colorScheme.primary),
                  ),
                ),
                const SizedBox(width: 8),
                Text(
                  'Processando...',
                  style: TextStyle(
                    color: theme.textTheme.bodyMedium?.color,
                    fontSize: 16,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildInputArea() => ResponsiveInputBar(
        controller: _textController,
        isApiConnected: _apiConnected,
        isLoading: _isLoading,
        onSend: _sendMessage,
      );

  /* ------------------------------------------------------------------ */
  /* --------------------- send & format helpers ---------------------- */
  /* ------------------------------------------------------------------ */

  void _sendMessage() async {
    if (!_apiConnected) {
      _showConnectionError();
      return;
    }

    final rawText = _textController.text.trim();
    if (rawText.isEmpty) return;

    // Normaliza o texto do usuário
    final formattedInput = normalizeText(mapEmoji(rawText));

    final userMessage = Message(
      text: rawText,
      isUser: true,
      timestamp: DateTime.now(),
    );

    setState(() {
      _messages.add(userMessage);
      _textController.clear();
      _isLoading = true;
    });

    // Update conversation history with the new user input
    _conversationHistory.add("User: $formattedInput");

    try {
      if (rawText.toLowerCase().contains('debug')) {
        final debugInfo =
            await _chatService.debugApiResponse(rawText);
        final debugMessage = Message(
          text: '🔧 DEBUG INFO:\n${_formatDebugInfo(debugInfo)}',
          isUser: false,
          timestamp: DateTime.now(),
        );

        setState(() {
          _messages.add(debugMessage);
          _isLoading = false;
        });
        return;
      }

      // Send the message with the conversation history
      final botResponse =
          await _chatService.sendMessage(formattedInput, _conversationHistory);

      // Update conversation history with the new bot response
      _conversationHistory.add("Alyssa: $botResponse");

      final botMessage = Message(
        text: botResponse,
        isUser: false,
        timestamp: DateTime.now(),
      );

      setState(() {
        _messages.add(botMessage);
        _isLoading = false;
      });
    } catch (e) {
      final errorMessage = Message(
        text: '❌ Erro ao comunicar com a API: $e',
        isUser: false,
        timestamp: DateTime.now(),
      );

      setState(() {
        _messages.add(errorMessage);
        _isLoading = false;
      });
    }
  }

  String _formatDebugInfo(Map<String, dynamic> debugInfo) {
    final buffer = StringBuffer();

    if (debugInfo['success'] == true) {
      buffer.writeln(' Conexão bem‑sucedida');
      buffer.writeln('📊 Status Code: ${debugInfo['statusCode']}');
      buffer.writeln('📦 Resposta Bruta:');
      buffer.writeln(debugInfo['rawResponse'] ?? 'N/A');

      final data = debugInfo['data'] ?? {};
      buffer.writeln('\n🔍 Dados Parseados:');
      data.forEach((key, value) {
        buffer.writeln('   $key: $value');
      });
    } else {
      buffer.writeln('❌ Erro na conexão');
      buffer.writeln('📊 Status Code: ${debugInfo['statusCode']}');
      buffer.writeln('🚨 Erro: ${debugInfo['error']}');
      if (debugInfo['rawResponse'] != null) {
        buffer.writeln('📦 Resposta Bruta: ${debugInfo['rawResponse']}');
      }
    }

    return buffer.toString();
  }

  void _showConnectionError() {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Erro de Conexão'),
        content: const Text(
          'Não foi possível conectar com a API do chatbot. '
          'Verifique se o servidor está rodando em http://localhost:8181',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('OK'),
          ),
          TextButton(
            onPressed: () {
              Navigator.pop(context);
              _retryConnection();
            },
            child: const Text('Tentar Novamente'),
          ),
        ],
      ),
    );
  }

  String _formatTime(DateTime timestamp) =>
      '${timestamp.hour.toString().padLeft(2, '0')}:${timestamp.minute.toString().padLeft(2, '0')}';
}
