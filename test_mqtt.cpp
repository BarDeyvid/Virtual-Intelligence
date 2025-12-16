#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "mqtt/async_client.h"

using namespace std;

// --- Configurações de Conexão (Adaptado de config.py) ---
// Combine o broker e a porta no formato de URI.
const string MQTT_BROKER_URI{"tcp://192.168.1.9:1883"};
const string CLIENT_ID{"CppAsyncSubscriberClient"}; // ID ligeiramente alterado para C++
// const string MQTT_USER = "alyssa_user";         // Usuário
// const string MQTT_PASSWORD = "ESP32";           // Senha

// --- Tópicos MQTT (Adaptado de config.py) ---
const string TOPIC_TEMP{"esp32/alyssa/quarto/temperatura"};
const string TOPIC_HUM{"esp32/alyssa/quarto/umidade"};

// Quality of Service (QoS). 1 é um bom padrão.
const int QOS = 1;

/////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    // A URI pode ser sobrescrita como argumento de linha de comando.
    auto serverUri = (argc > 1) ? string{argv[1]} : MQTT_BROKER_URI;

    mqtt::async_client cli(serverUri, CLIENT_ID);

    // 1. Configurações de Conexão (incluindo autenticação se necessário)
    auto connOpts = mqtt::connect_options_builder::v3()
                        .keep_alive_interval(30s)
                        .clean_session(false) // Permite sessão persistente
                        .automatic_reconnect(true) // Habilita reconexão automática
                        // .user_name(MQTT_USER)    // Descomentar se for usar autenticação
                        // .password(MQTT_PASSWORD) // Descomentar se for usar autenticação
                        .finalize();

    // 2. Lista de Tópicos e QoS para a subscrição
    // É possível subscrever vários tópicos de uma vez:
    vector<string> topics { TOPIC_TEMP, TOPIC_HUM };
    vector<int> qos { QOS, QOS };

    try {
        // Inicia a fila de consumo (buffer de mensagens) antes de conectar.
        cli.start_consuming();

        // Conecta ao servidor
        cout << "Conectando ao broker em: " << serverUri << "..." << flush;
        auto tok = cli.connect(connOpts);

        // Aguarda a conexão ser completada e obtém a resposta.
        auto rsp = tok->get_connect_response();

        // 3. Subscrição
        // Se a sessão não estiver presente no broker, subscreve os tópicos.
        // Se estiver presente (clean_session=false), o broker já deve ter as subscrições.
        if (!rsp.is_session_present()) {
            cout << "  Nenhuma sessão encontrada. Subscrevendo tópicos..." << flush;
            cli.subscribe(topics, qos)->wait();
        }

        cout << "OK" << endl;

        // 4. Consumo de Mensagens
        cout << "\nEsperando por mensagens nos tópicos monitorados..." << endl;
        cout << "Pressione Ctrl+C para finalizar." << endl;

        // O loop consome eventos de conexão/desconexão e as mensagens recebidas.
        while (true) {
            auto evt = cli.consume_event();

            // Se for uma mensagem
            if (const auto* p = evt.get_message_if()) {
                auto& msg = *p;
                if (msg)
                    cout << ">>> " << msg->get_topic() << ": " << msg->to_string() << endl;
            }
            // Se for um evento de conexão
            else if (evt.is_connected())
                cout << "\n*** Reconectado ao Broker ***" << endl;
            // Se for um evento de desconexão
            else if (evt.is_connection_lost())
                cout << "*** CONEXÃO PERDIDA *** Tentando reconectar automaticamente..." << endl;
        }
    }
    catch (const mqtt::exception& exc) {
        cerr << "\n  Erro: " << exc << endl;
        return 1;
    }

    return 0;
}