#include "voice/ElevenLabsTTS.hpp"
#pragma warning(disable: 4244 4267 4458 4966 4018 4127)

using json = nlohmann::json;

/**
 * @brief Load configuration from JSON file.
 * @param path Path to configuration file.
 * @return true if load successful, false otherwise.
 */
static bool loadConfig(const std::string& path, std::string& key, std::string& voice, int& rate) {
    const std::string& filepath = path;
    std::ifstream i(filepath);
    if (!i.is_open()) {
        std::cerr << "ERRO: Não foi possível abrir o arquivo de configuração: " << path << std::endl;
        return false;
    }

    try {
        json j;
        i >> j;

        key = j.at("elevenlabs_api_key").get<std::string>();
        voice = j.at("voice_id").get<std::string>();
        rate = j.at("sample_rate").get<int>();
        
        return true;
    } catch (const json::exception& e) {
        std::cerr << "ERRO JSON: Falha ao analisar ou campo JSON faltando: " << e.what() << std::endl;
        return false;
    }
}

/**
 * @brief Callback para escrever dados HTTP
 */
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::vector<char>* buffer) {
    size_t total_size = size * nmemb;
    buffer->insert(buffer->end(), (char*)contents, (char*)contents + total_size);
    return total_size;
}

// --- Construtor e Destrutor ---
/**
* @brief Default constructor.
* @param api_key My API KEY which isn't going to git
* @param voice_id which voice Alyssa Should have
*/
ElevenLabsTTS::ElevenLabsTTS(const std::string& api_key, 
                           const std::string& voice_id,
                           int sample_rate) 
    : sample_rate_(sample_rate) { 
    
    std::cout << "Inicializando ElevenLabsTTS..." << std::endl;
    
    initializePortAudio();
    initializeFFmpeg();

    // TENTA CARREGAR DO ARQUIVO
    std::string temp_key, temp_voice;
    int temp_rate;

    if (loadConfig("config/ConfigsTTS.json", temp_key, temp_voice, temp_rate)) {
        api_key_ = temp_key;
        voice_id_ = temp_voice;
        sample_rate_ = temp_rate;
    } else {
        api_key_ = api_key;
        voice_id_ = voice_id;
        sample_rate_ = sample_rate;
    }
    }

/**
* @brief Default destructur.
*/
ElevenLabsTTS::~ElevenLabsTTS() {
    std::cout << "Finalizando ElevenLabsTTS..." << std::endl;
    
    closeAudioStream();
    terminatePortAudio();
    cleanupFFmpeg();
    curl_global_cleanup();
}

// --- Inicialização FFmpeg ---

void ElevenLabsTTS::initializeFFmpeg() {
    // Registrar todos os codecs e formatos    
    ffmpeg_initialized_ = true;
    std::cout << "FFmpeg inicializado." << std::endl;
}

void ElevenLabsTTS::cleanupFFmpeg() {
    // FFmpeg não requer cleanup explícito em versões recentes
    ffmpeg_initialized_ = false;
}

// --- Métodos Privados de Gerenciamento ---

void ElevenLabsTTS::initializePortAudio() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        throw std::runtime_error("Erro ao inicializar PortAudio: " + std::string(Pa_GetErrorText(err)));
    }
}

void ElevenLabsTTS::terminatePortAudio() {
    Pa_Terminate();
}

void ElevenLabsTTS::openAudioStream() {
    PaError err;

    // Configuração do stream
    err = Pa_OpenDefaultStream(
        &stream_, // O ponteiro para o PaStream
        0,        // 0 canais de entrada (apenas saída)
        CHANNELS, // 1 canal de saída (mono, conforme sua decodificação)
        PA_SAMPLE_TYPE, // paFloat32
        sample_rate_,   // 44100 Hz
        FRAMES_PER_BUFFER, // 256
        NULL,             // Callback (NULL para modo blocking)
        NULL              // User data
    );

    if (err != paNoError) {
        // Mude o throw para uma mensagem mais clara
        std::string error_msg = "Erro ao abrir o stream de áudio: " + std::string(Pa_GetErrorText(err));
        throw std::runtime_error(error_msg); 
    }
}

void ElevenLabsTTS::closeAudioStream() {
    if (stream_) {
        Pa_StopStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
}

// --- Limpeza de Texto ---

std::string ElevenLabsTTS::cleanText(const std::string& text) {
    std::string result = text;
    
    // Remover conteúdo entre parênteses
    size_t start_pos = 0;
    while ((start_pos = result.find('(', start_pos)) != std::string::npos) {
        size_t end_pos = result.find(')', start_pos);
        if (end_pos != std::string::npos) {
            result.erase(start_pos, end_pos - start_pos + 1);
        } else {
            break;
        }
    }
    
    // Remover espaços múltiplos
    std::string cleaned;
    bool last_was_space = false;
    for (char c : result) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!last_was_space) {
                cleaned += ' ';
                last_was_space = true;
            }
        } else {
            cleaned += c;
            last_was_space = false;
        }
    }
    
    // Remover espaços no início e fim
    size_t start = cleaned.find_first_not_of(" \t\n\r");
    size_t end = cleaned.find_last_not_of(" \t\n\r");
    
    if (start == std::string::npos || end == std::string::npos) {
        return "";
    }
    
    return cleaned.substr(start, end - start + 1);
}

// --- Síntese de Áudio ---

std::vector<float> ElevenLabsTTS::generateAudio(const std::string& text) {
    CURL* curl = curl_easy_init();
    std::vector<char> response_buffer;
    std::vector<float> audio_data;
    
    if (!curl) {
        std::cerr << "Erro ao inicializar cURL" << std::endl;
        return audio_data;
    }

    // URL da API ElevenLabs
    std::string url = "https://api.elevenlabs.io/v1/text-to-speech/" + voice_id_;

    // Headers
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Accept: audio/mpeg");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, ("xi-api-key: " + api_key_).c_str());

    // JSON data - importante escapar as aspas no texto
    std::string escaped_text;
    for (char c : text) {
        if (c == '"' || c == '\\') {
            escaped_text += '\\';
        }
        escaped_text += c;
    }

    std::stringstream json_data;
    json_data << R"({
        "text": ")" << escaped_text << R"(",
        "model_id": "eleven_multilingual_v2",
        "voice_settings": {
            "stability": 0.75,
            "similarity_boost": 0.75
        }
    })";

    std::string json_str = json_data.str();

    // Configurar cURL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    std::cout << "Enviando requisição para ElevenLabs..." << std::endl;

    // Executar requisição
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::cerr << "Erro na requisição cURL: " << curl_easy_strerror(res) << std::endl;
    } else {
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if (http_code == 200 && !response_buffer.empty()) {
            std::cout << "Áudio MP3 recebido com sucesso (" << response_buffer.size() << " bytes)" << std::endl;
            audio_data = decodeAudioWithFFmpeg(response_buffer);
        } else {
            std::cerr << "Erro HTTP: " << http_code << std::endl;
            if (!response_buffer.empty()) {
                std::cerr << "Resposta: " << std::string(response_buffer.begin(), response_buffer.end()) << std::endl;
            }
        }
    }

    // Limpeza
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return audio_data;
}

// Estrutura auxiliar para I/O em memória
struct BufferData {
    const uint8_t* ptr;
    size_t size;
    size_t offset;
};

// Callback de leitura
static int read_packet(void* opaque, uint8_t* buf, int buf_size) {
    BufferData* bd = (BufferData*)opaque;
    buf_size = FFMIN(buf_size, bd->size - bd->offset);
    if (!buf_size)
        return AVERROR_EOF;
    memcpy(buf, bd->ptr + bd->offset, buf_size);
    bd->offset += buf_size;
    return buf_size;
}

// Callback de seek (MP3 precisa de seek)
static int64_t seek_packet(void* opaque, int64_t offset, int whence) {
    BufferData* bd = (BufferData*)opaque;
    switch (whence) {
        case SEEK_SET:
            if (offset < 0 || offset > bd->size) {
                return -1;
            }
            bd->offset = offset;
            return bd->offset;
        case SEEK_CUR:
            if (bd->offset + offset < 0 || bd->offset + offset > bd->size) {
                return -1;
            }
            bd->offset += offset;
            return bd->offset;
        case SEEK_END:
            if (bd->size + offset < 0 || bd->size + offset > bd->size) {
                return -1;
            }
            bd->offset = bd->size + offset;
            return bd->offset;
    }
    return -1;
}

// --- Decodificação com FFmpeg ---

std::vector<float> ElevenLabsTTS::decodeAudioWithFFmpeg(const std::vector<char>& audio_data) {
    std::vector<float> pcm_data;

    BufferData bd = { (const uint8_t*)audio_data.data(), audio_data.size(), 0 };

    // Tamanho do buffer interno do AVIOContext
    int avio_ctx_buffer_size = 4096;
    unsigned char* avio_ctx_buffer = (unsigned char*)av_malloc(avio_ctx_buffer_size);
    
    AVFormatContext* format_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    SwrContext* swr_ctx = nullptr;
    AVPacket* packet = av_packet_alloc(); // Usar alocação moderna
    AVFrame* frame = nullptr;
    
    // Variáveis auxiliares para layout do canal
    int audio_stream_index = -1;
    AVChannelLayout original_channel_layout = {}; 
    int original_sample_rate = 0; // Novo
    AVSampleFormat original_sample_fmt = AV_SAMPLE_FMT_NONE; // Novo


    try {
        if (!avio_ctx_buffer) {
             throw std::runtime_error("Falha ao alocar AVIO buffer");
        }

        // Criar AVIO context para dados em memória com as callbacks
        AVIOContext* avio_ctx = avio_alloc_context(
            avio_ctx_buffer, // Buffer interno (alocado acima)
            avio_ctx_buffer_size,
            0, // read-only
            &bd, // Opaque data (o nosso BufferData)
            read_packet, // Callback de leitura
            nullptr, // Sem callback de escrita
            seek_packet // Callback de seek (MP3 precisa)
        );
        
        if (!avio_ctx) {
             // Liberar o buffer alocado por av_malloc
             av_freep(&avio_ctx_buffer); 
             throw std::runtime_error("Falha ao criar AVIO context");
        }
        
        // Alocar format context
        format_ctx = avformat_alloc_context();
        if (!format_ctx) {
            // Liberar o avio_ctx e seu buffer em caso de falha
            av_freep(&avio_ctx_buffer); // O av_freep de avio_ctx_buffer libera o buffer interno
            avio_context_free(&avio_ctx); // Esvaziar o ponteiro do AVIOContext
            throw std::runtime_error("Falha ao alocar format context");
        }
        
        // ASSOCIAR o AVIOContext ao AVFormatContext
        format_ctx->pb = avio_ctx;
        
        // Abrir input (o nome do arquivo é vazio pois estamos lendo da memória)
        if (avformat_open_input(&format_ctx, "", nullptr, nullptr) < 0) {
            throw std::runtime_error("Falha ao abrir input com FFmpeg");
        }
        
        // Encontrar stream info
        if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
            throw std::runtime_error("Falha ao encontrar stream info");
        }
        
        // Encontrar stream de áudio
        for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
            if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                audio_stream_index = i;
                break;
            }
        }
        
        if (audio_stream_index == -1) {
            throw std::runtime_error("Nenhuma stream de áudio encontrada");
        }
        
        // Obter codec e parâmetros originais
        AVCodecParameters* codec_params = format_ctx->streams[audio_stream_index]->codecpar;
        const AVCodec* codec = avcodec_find_decoder(codec_params->codec_id);
        if (!codec) {
            throw std::runtime_error("Codec não suportado");
        }

        // Capturar informações originais do stream ANTES de abrir o codec
        original_sample_rate = codec_params->sample_rate;
        original_sample_fmt = (AVSampleFormat)codec_params->format;

        // Copiar o layout do codec_params
        if (av_channel_layout_copy(&original_channel_layout, &codec_params->ch_layout) < 0) {
             throw std::runtime_error("Falha ao copiar AVChannelLayout");
        }
        
        // Alocar codec context
        codec_ctx = avcodec_alloc_context3(codec);
        if (!codec_ctx) {
            throw std::runtime_error("Falha ao alocar codec context");
        }
        
        // Copiar codec parameters
        if (avcodec_parameters_to_context(codec_ctx, codec_params) < 0) {
            throw std::runtime_error("Falha ao copiar codec parameters");
        }
        
        // Abrir codec
        if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
            throw std::runtime_error("Falha ao abrir codec");
        }
        
        // CORREÇÃO 2: Declarar e inicializar out_ch_layout
        AVChannelLayout out_ch_layout;
        // Definir a saída como MONO
        av_channel_layout_from_mask(&out_ch_layout, AV_CH_LAYOUT_MONO); 
        
        // swr_alloc_set_opts2 agora recebe ponteiros para AVChannelLayout
        int swr_ret = swr_alloc_set_opts2(&swr_ctx, // PASSAR O ENDEREÇO DO PONTEIRO swr_ctx
                                    &out_ch_layout, // Layout de saída (ponteiro)
                                    PA_SAMPLE_TYPE == paFloat32 ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S16, // Formato de saída
                                    sample_rate_, // Sample rate de saída (configurado)
                                    &original_channel_layout, // Layout de entrada (ponteiro)
                                    original_sample_fmt, // Formato de entrada (original)
                                    original_sample_rate, // Sample rate de entrada (original)
                                    0, nullptr);
        
        // VERIFICAR O VALOR DE RETORNO
        if (!swr_ctx || swr_ret < 0) { // Verifica se o contexto foi alocado e se não houve erro
            throw std::runtime_error("Falha ao alocar resample context");
        }
        
        if (swr_init(swr_ctx) < 0) {
            throw std::runtime_error("Falha ao inicializar resample context");
        }
        
        frame = av_frame_alloc();
        if (!frame) {
            throw std::runtime_error("Falha ao alocar frame");
        }
                
        std::cout << "Decodificando áudio: " 
                  << original_sample_rate << "Hz, " // Usar o valor original
                  << original_channel_layout.nb_channels << " canais" << std::endl;
        // Ler e decodificar frames
        while (av_read_frame(format_ctx, packet) >= 0) {
            if (packet->stream_index == audio_stream_index) {
                int ret = avcodec_send_packet(codec_ctx, packet);
                if (ret < 0) {
                    av_packet_unref(packet); // Liberar antes de continuar
                    continue;
                }
                
                while (ret >= 0) {
                    ret = avcodec_receive_frame(codec_ctx, frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    } else if (ret < 0) {
                        throw std::runtime_error("Erro durante decodificação");
                    }
                    
                    // Converter para float32
                    uint8_t* converted_data = nullptr;
                    int out_samples = swr_get_out_samples(swr_ctx, frame->nb_samples);
                    // O formato de saída é AV_SAMPLE_FMT_FLT (float32) ou o tipo de PA_SAMPLE_TYPE
                    int ret_alloc = av_samples_alloc(&converted_data, 
                                                     nullptr, 
                                                     CHANNELS, // 1 canal (MONO)
                                                     out_samples, 
                                                     PA_SAMPLE_TYPE == paFloat32 ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S16, 
                                                     0);
                    if (ret_alloc < 0) {
                        // Tratar erro de alocação se necessário
                    }

                    int converted_samples = swr_convert(swr_ctx, 
                                                       &converted_data, out_samples,
                                                       (const uint8_t**)frame->data, frame->nb_samples);
                    
                    if (converted_samples > 0) {
                        // Adicionar samples convertidos ao buffer
                        float* samples = reinterpret_cast<float*>(converted_data);
                        // Apenas adicionar se o formato de saída for FLT, como esperado pelo vector<float>
                        if (PA_SAMPLE_TYPE == paFloat32) {
                             pcm_data.insert(pcm_data.end(), samples, samples + converted_samples);
                        }
                    }
                    
                    if (converted_data) {
                        av_freep(&converted_data);
                    }
                    
                    av_frame_unref(frame);
                }
            }
            av_packet_unref(packet);
        }
        
        // Decodificar frames restantes (drenagem)
        avcodec_send_packet(codec_ctx, nullptr); // Enviar NULL para drenar o codec

        while (avcodec_receive_frame(codec_ctx, frame) >= 0) {
             // Processar frames restantes...
             uint8_t* converted_data = nullptr;
             int out_samples = swr_get_out_samples(swr_ctx, frame->nb_samples);

             int ret_alloc = av_samples_alloc(&converted_data, 
                                             nullptr, 
                                             CHANNELS, 
                                             out_samples, 
                                             PA_SAMPLE_TYPE == paFloat32 ? AV_SAMPLE_FMT_FLT : AV_SAMPLE_FMT_S16, 
                                             0);
            
            if (ret_alloc >= 0) {
                int converted_samples = swr_convert(swr_ctx, 
                                                    &converted_data, out_samples,
                                                    (const uint8_t**)frame->data, frame->nb_samples);
                
                if (converted_samples > 0 && PA_SAMPLE_TYPE == paFloat32) {
                    float* samples = reinterpret_cast<float*>(converted_data);
                    pcm_data.insert(pcm_data.end(), samples, samples + converted_samples);
                }
                
                if (converted_data) {
                    av_freep(&converted_data);
                }
            }
            av_frame_unref(frame);
        }

        std::cout << "Áudio decodificado: " << pcm_data.size() << " samples" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Erro na decodificação FFmpeg: " << e.what() << std::endl;
    }
    
    // Cleanup
    if (packet) av_packet_free(&packet);
    if (frame) av_frame_free(&frame);
    if (codec_ctx) avcodec_free_context(&codec_ctx);
    if (format_ctx) {
        if (format_ctx->pb) {
            // Liberar o buffer interno alocado por av_malloc
            av_freep(&format_ctx->pb->buffer);
            // Liberar o AVIOContext
            avio_context_free(&format_ctx->pb);
        }
        avformat_close_input(&format_ctx);
    }
    if (swr_ctx) swr_free(&swr_ctx);
    
    return pcm_data;
}

// --- Reprodução de Áudio ---

void ElevenLabsTTS::playAudio(const std::vector<float>& audio_data) {
    if (audio_data.empty()) {
        std::cerr << "Nenhum dado de áudio para reproduzir" << std::endl;
        return;
    }

    // Abrir stream de áudio se necessário
    if (!stream_) {
        openAudioStream();
    }

    // Iniciar stream
    PaError err = Pa_StartStream(stream_);
    if (err != paNoError) {
        std::cerr << "Erro ao iniciar stream: " << Pa_GetErrorText(err) << std::endl;
        return;
    }

    // Escrever áudio no stream
    err = Pa_WriteStream(stream_, audio_data.data(), audio_data.size());
    if (err != paNoError) {
        std::cerr << "Erro durante escrita no stream: " << Pa_GetErrorText(err) << std::endl;
    }

    // Aguardar término da reprodução
    while (Pa_GetStreamWriteAvailable(stream_) < FRAMES_PER_BUFFER) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Parar stream
    err = Pa_StopStream(stream_);
    if (err != paNoError) {
        std::cerr << "Erro ao parar stream: " << Pa_GetErrorText(err) << std::endl;
    }

    std::cout << "Reprodução concluída" << std::endl;
}

// --- Método Principal ---

void ElevenLabsTTS::synthesizeAndPlay(const std::string& text) {
    std::cout << "\n TTS: \"" << text << "\"" << std::endl;

    // Limpar texto
    std::string cleaned_text = cleanText(text);
    if (cleaned_text != text) {
        std::cout << "Texto limpo: \"" << cleaned_text << "\"" << std::endl;
    }

    if (cleaned_text.empty()) {
        std::cout << "Texto vazio após limpeza" << std::endl;
        return;
    }

    // Gerar áudio
    std::vector<float> audio_data = generateAudio(cleaned_text);
    
    if (audio_data.empty()) {
        std::cerr << "Falha ao gerar áudio" << std::endl;
        return;
    }

    // Reproduzir áudio
    playAudio(audio_data);
}