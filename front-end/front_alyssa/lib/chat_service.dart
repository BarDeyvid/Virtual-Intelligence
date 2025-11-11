import 'dart:convert';
import 'package:http/http.dart' as http;

class ChatService {
  static const String baseUrl = 'https://canonistical-disruptively-nicolasa.ngrok-free.dev';
  
  // Headers mais completos para ngrok
  Map<String, String> get _headers => {
    'Content-Type': 'application/json',
    'Accept': 'application/json',
    'User-Agent': 'ChatBotApp/1.0',
    'ngrok-skip-browser-warning': 'true', // 🔥 IMPORTANTE para ngrok
  };

  Future<String> sendMessage(String message) async {
    try {
      print('🔄 Enviando mensagem para: $baseUrl/think/fusion');
      print('📝 Mensagem: $message');
      
      final response = await http.post(
        Uri.parse('$baseUrl/think/fusion'),
        headers: _headers,
        body: jsonEncode({
          'input': message,
        }),
      ).timeout(const Duration(seconds: 30)); // Aumente o timeout

      print('📊 Status Code: ${response.statusCode}');
      print('📦 Response Body: ${response.body}');

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        
        if (data['success'] == true) {
          final String output = data['output']?.toString() ?? '';
          
          if (output.trim().isEmpty) {
            return _handleEmptyResponse(message);
          }
          
          return output;
        } else {
          return '❌ Erro no servidor: ${data['error'] ?? "Erro desconhecido"}';
        }
      } else {
        return '🔌 Erro HTTP: ${response.statusCode} - ${response.body}';
      }
    } catch (e) {
      print('❌ Exception: $e');
      return _tryFallbackEndpoint(message);
    }
  }


  Future<String> _tryFallbackEndpoint(String message) async {
    try {
      print('🔄 Tentando endpoint fallback: $baseUrl/think');
      
      final response = await http.post(
        Uri.parse('$baseUrl/think'),
        headers: _headers,
        body: jsonEncode({
          'input': message,
        }),
      ).timeout(const Duration(seconds: 30));

      print('📊 Fallback Status: ${response.statusCode}');
      print('📦 Fallback Body: ${response.body}');

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        
        if (data['success'] == true) {
          final String output = data['output']?.toString() ?? '';
          
          if (output.trim().isEmpty) {
            return _handleEmptyResponse(message);
          }
          
          return output;
        } else {
          return '❌ Erro no servidor: ${data['error'] ?? "Erro desconhecido"}';
        }
      } else {
        return '🔌 Erro de conexão: Status ${response.statusCode}';
      }
    } catch (e) {
      print('❌ Fallback Exception: $e');
      return '🔌 Erro de conexão: $e\n\n'
             '✅ A API está acessível pelo navegador?\n'
             '🔗 URL: $baseUrl/health\n'
             '📱 Teste esta URL no navegador do celular';
    }
  }


  String _handleEmptyResponse(String originalMessage) {
    // Analisa o contexto da mensagem original para dar uma resposta mais útil
    final lowerMessage = originalMessage.toLowerCase();
    
    if (lowerMessage.contains('alyssa') || lowerMessage.contains('gestora')) {
      return '🤔 Hmm, sobre a Alyssa e o código com outro dev... Parece que isso tocou em algo complexo no meu sistema. Pode me contar mais sobre essa situação?';
    } else if (lowerMessage.contains('código') || lowerMessage.contains('dev')) {
      return '💻 Sobre desenvolvimento... Meu processamento emocional ficou confuso com esse contexto. Pode reformular?';
    } else {
      return '🤖 Recebi sua mensagem, mas minha resposta ficou vazia. Isso acontece quando o sistema encontra conflitos internos. Pode tentar formular de outra maneira?';
    }
  }

  // Método para verificar se a API está online
  Future<bool> checkHealth() async {
    try {
      print('🔍 Verificando saúde da API...');
      
      final response = await http.get(
        Uri.parse('$baseUrl/health'),
        headers: _headers,
      ).timeout(const Duration(seconds: 10));

      print('🏥 Health Status: ${response.statusCode}');
      print('🏥 Health Response: ${response.body}');

      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        return data['status'] == 'healthy' && data['initialized'] == true;
      }
      return false;
    } catch (e) {
      print('❌ Health Check Exception: $e');
      return false;
    }
  }

  Future<Map<String, dynamic>> testConnection() async {
  final results = <String, dynamic>{};
  
  try {
    // Teste 1: Health endpoint
    print('1. 🏥 Testando Health endpoint...');
    final healthResponse = await http.get(
      Uri.parse('$baseUrl/health'),
      headers: _headers,
    ).timeout(const Duration(seconds: 10));
    
    results['health_status'] = healthResponse.statusCode;
    results['health_body'] = healthResponse.body;
    
    // Teste 2: Think endpoint
    print('2. 🤔 Testando Think endpoint...');
    final thinkResponse = await http.post(
      Uri.parse('$baseUrl/think'),
      headers: _headers,
      body: jsonEncode({'input': 'test'}),
    ).timeout(const Duration(seconds: 10));
    
    results['think_status'] = thinkResponse.statusCode;
    results['think_body'] = thinkResponse.body;
    
    // Teste 3: Think/fusion endpoint
    print('3. 🔄 Testando Think/fusion endpoint...');
    final fusionResponse = await http.post(
      Uri.parse('$baseUrl/think/fusion'),
      headers: _headers,
      body: jsonEncode({'input': 'test'}),
    ).timeout(const Duration(seconds: 10));
    
    results['fusion_status'] = fusionResponse.statusCode;
    results['fusion_body'] = fusionResponse.body;
    
    print('✅ Teste completo: $results');
    
  } catch (e) {
    results['error'] = e.toString();
    print('❌ Erro no teste: $e');
  }
  
  return results;
}

  // Novo método para debug - verifica a resposta completa da API
  Future<Map<String, dynamic>> debugApiResponse(String message) async {
    try {
      final response = await http.post(
        Uri.parse('$baseUrl/think/fusion'),
        headers: {'Content-Type': 'application/json'},
        body: jsonEncode({'input': message}),
      );

      if (response.statusCode == 200) {
        return {
          'success': true,
          'data': jsonDecode(response.body),
          'rawResponse': response.body,
          'statusCode': response.statusCode,
        };
      } else {
        return {
          'success': false,
          'error': 'Status code: ${response.statusCode}',
          'rawResponse': response.body,
          'statusCode': response.statusCode,
        };
      }
    } catch (e) {
      return {
        'success': false,
        'error': e.toString(),
        'statusCode': 0,
      };
    }
  }
}