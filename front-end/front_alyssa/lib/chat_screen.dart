import 'package:flutter/material.dart';
import 'chat_service.dart';
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

  @override
  void initState() {
    super.initState();
    _checkApiConnection();
  }

  void _checkApiConnection() async {
    final isConnected = await _chatService.checkHealth();
    setState(() {
      _apiConnected = isConnected;
      _checkingConnection = false;
    });

    if (!isConnected) {
      final warningMessage = Message(
        text: '⚠️ Não foi possível conectar com a API em http://localhost:8181. '
              'Verifique se o servidor está rodando.',
        isUser: false,
        timestamp: DateTime.now(),
      );
      setState(() {
        _messages.add(warningMessage);
      });
    }
  }

  void _sendMessage() async {
    if (!_apiConnected) {
      _showConnectionError();
      return;
    }

    final text = _textController.text.trim();
    if (text.isEmpty) return;

    final userMessage = Message(
      text: text,
      isUser: true,
      timestamp: DateTime.now(),
    );
    
    setState(() {
      _messages.add(userMessage);
      _textController.clear();
      _isLoading = true;
    });

    try {
      // Para debug: verificar resposta completa da API
      if (text.toLowerCase().contains('debug')) {
        final debugInfo = await _chatService.debugApiResponse(text);
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

      final botResponse = await _chatService.sendMessage(text);
      
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
      buffer.writeln('✅ Conexão bem-sucedida');
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
              _checkApiConnection();
            },
            child: const Text('Tentar Novamente'),
          ),
        ],
      ),
    );
  }

  void _retryConnection() async {
    setState(() {
      _checkingConnection = true;
    });
    
    _checkApiConnection();
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
  
  void _testConnection() async {
    setState(() {
      _isLoading = true;
    });
    
    final results = await _chatService.testConnection();
    
    final debugMessage = Message(
      text: '🔍 RESULTADOS DO TESTE:\n\n'
            '🏥 Health: ${results['health_status']}\n'
            '🤔 Think: ${results['think_status']}\n' 
            '🔄 Fusion: ${results['fusion_status']}\n'
            '📦 Error: ${results['error'] ?? "Nenhum"}\n\n'
            'Se Health=200, a API está acessível!',
      isUser: false,
      timestamp: DateTime.now(),
    );
    
    setState(() {
      _messages.add(debugMessage);
      _isLoading = false;
    });
  }


  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Alyssa'),
        backgroundColor: _apiConnected ? Colors.blue : Colors.orange,
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
                  valueColor: AlwaysStoppedAnimation<Color>(Colors.white),
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
              tooltip: _apiConnected ? 'API Conectada' : 'API Desconectada',
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

        ],
      ),
      body: Column(
        children: [
          if (!_apiConnected && !_checkingConnection)
            Container(
              width: double.infinity,
              padding: const EdgeInsets.all(12),
              color: Colors.orange[100],
              child: Row(
                children: [
                  const Icon(Icons.warning, color: Colors.orange),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      'API desconectada',
                      style: TextStyle(
                        color: Colors.orange[800],
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
            child: _messages.isEmpty && !_checkingConnection
                ? Center(
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Icon(
                          _apiConnected ? Icons.chat_bubble_outline : Icons.error_outline,
                          size: 64,
                          color: _apiConnected ? Colors.blue : Colors.orange,
                        ),
                        const SizedBox(height: 16),
                        Text(
                          _apiConnected 
                              ? 'Como posso ajudar você hoje?'
                              : 'Conecte-se à API para começar',
                          style: TextStyle(
                            fontSize: 18,
                            color: _apiConnected ? Colors.grey : Colors.orange,
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
                    itemCount: _messages.length + (_isLoading ? 1 : 0),
                    itemBuilder: (context, index) {
                      if (index == _messages.length && _isLoading) {
                        return _buildLoadingIndicator();
                      }
                      return _buildMessageBubble(_messages[index]);
                    },
                  ),
          ),
          _buildInputArea(),
        ],
      ),
    );
  }

  // ... métodos _buildMessageBubble, _buildLoadingIndicator, _buildInputArea, _formatTime permanecem iguais ...
  Widget _buildMessageBubble(Message message) {
    return Container(
      margin: const EdgeInsets.symmetric(vertical: 8),
      child: Row(
        mainAxisAlignment: message.isUser
            ? MainAxisAlignment.end
            : MainAxisAlignment.start,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          if (!message.isUser)
            Container(
              margin: const EdgeInsets.only(right: 8, top: 4),
              child: CircleAvatar(
                backgroundColor: _apiConnected ? Colors.blue : Colors.orange,
                radius: 16,
                child: const Icon(
                  Icons.smart_toy,
                  color: Colors.white,
                  size: 18,
                ),
              ),
            ),
          Flexible(
            child: Container(
              padding: const EdgeInsets.all(12),
              decoration: BoxDecoration(
                color: message.isUser
                    ? Colors.blue
                    : Colors.grey[200],
                borderRadius: BorderRadius.circular(16),
              ),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    message.text,
                    style: TextStyle(
                      color: message.isUser
                          ? Colors.white
                          : Colors.black,
                      fontSize: 16,
                    ),
                  ),
                  const SizedBox(height: 4),
                  Text(
                    _formatTime(message.timestamp),
                    style: TextStyle(
                      color: message.isUser
                          ? Colors.white70
                          : Colors.grey[600],
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
    return Container(
      margin: const EdgeInsets.symmetric(vertical: 8),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.start,
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Container(
            margin: const EdgeInsets.only(right: 8, top: 4),
            child: CircleAvatar(
              backgroundColor: _apiConnected ? Colors.blue : Colors.orange,
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
              color: Colors.grey[200],
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
                    valueColor: AlwaysStoppedAnimation<Color>(
                      _apiConnected ? Colors.blue : Colors.orange,
                    ),
                  ),
                ),
                const SizedBox(width: 8),
                Text(
                  'Processando...',
                  style: TextStyle(
                    color: Colors.grey[600],
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

  Widget _buildInputArea() {
    return Container(
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: Colors.white,
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
          Expanded(
            child: TextField(
              controller: _textController,
              enabled: _apiConnected && !_isLoading,
              decoration: InputDecoration(
                hintText: _apiConnected 
                    ? 'Digite sua mensagem...'
                    : 'Aguardando conexão com a API...',
                border: OutlineInputBorder(
                  borderRadius: BorderRadius.circular(24),
                  borderSide: BorderSide.none,
                ),
                filled: true,
                fillColor: _apiConnected ? Colors.grey[100] : Colors.grey[200],
                contentPadding: const EdgeInsets.symmetric(
                  horizontal: 16,
                  vertical: 12,
                ),
              ),
              onSubmitted: (_) => _sendMessage(),
            ),
          ),
          const SizedBox(width: 8),
          CircleAvatar(
            backgroundColor: _apiConnected && !_isLoading ? Colors.blue : Colors.grey,
            child: IconButton(
              icon: const Icon(
                Icons.send,
                color: Colors.white,
              ),
              onPressed: _apiConnected && !_isLoading ? _sendMessage : null,
            ),
          ),
        ],
      ),
    );
  }

  String _formatTime(DateTime timestamp) {
    return '${timestamp.hour.toString().padLeft(2, '0')}:${timestamp.minute.toString().padLeft(2, '0')}';
  }
}