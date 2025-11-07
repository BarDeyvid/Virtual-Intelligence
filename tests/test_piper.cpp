#include <fstream>
#include <iostream>
#include <piper.h>
#include <portaudio.h> // Biblioteca PortAudio

// --- Configurações de Áudio ---
// O Piper usa 22050 Hz.
#define SAMPLE_RATE 22050
// O Piper usa um único canal (mono).
#define CHANNELS 1

// Tipo de dados do PortAudio, correspondente ao float (32-bit float) do Piper
#define PA_SAMPLE_TYPE paFloat32

// Tamanho do buffer (pode ser ajustado, 0 indica buffer padrão)
#define FRAMES_PER_BUFFER 0

// Função de Callback para o PortAudio (não usada neste modo de streaming, mas bom ter)
// Vamos usar o modo síncrono para simplificar.

int main() {
    std::cout << "Inicializando PortAudio..." << std::endl;
    // 1. Inicializar PortAudio
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "Erro ao inicializar PortAudio: " << Pa_GetErrorText(err) << std::endl;
        return 1;
    }

    piper_synthesizer *synth = piper_create("models/en_US-ljspeech-high.onnx",
                                            "models/en_US-ljspeech-high.onnx.json",
                                            "piper/libpiper/build/espeak_ng-install/share/espeak-ng-data");

    if (!synth) {
        std::cerr << "Erro ao criar o sintetizador Piper." << std::endl;
        Pa_Terminate();
        return 1;
    }

    PaStream *stream;
    
    // 2. Abrir o Stream de Áudio
    err = Pa_OpenDefaultStream(&stream,
                               0, // Sem canais de entrada (input)
                               CHANNELS, // 1 canal de saída (output)
                               PA_SAMPLE_TYPE, // Tipo de amostra (float 32-bit)
                               SAMPLE_RATE, // Taxa de amostragem
                               FRAMES_PER_BUFFER, // Tamanho do buffer
                               NULL, // Callback function (NULL para modo síncrono)
                               NULL); // User data
    
    if (err != paNoError) {
        std::cerr << "Erro ao abrir o stream de áudio: " << Pa_GetErrorText(err) << std::endl;
        piper_free(synth);
        Pa_Terminate();
        return 1;
    }

    // 3. Iniciar a Reprodução
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "Erro ao iniciar o stream de áudio: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(stream);
        piper_free(synth);
        Pa_Terminate();
        return 1;
    }

    std::cout << "Sintetizando e reproduzindo..." << std::endl;

    piper_synthesize_options options = piper_default_synthesize_options(synth);

    piper_synthesize_start(synth, "Welcome to the world of speech synthesis! The audio is playing directly now.",
                           &options);

    piper_audio_chunk chunk;
    while (piper_synthesize_next(synth, &chunk) != PIPER_DONE) {
        // 4. Escrever o Chunk de Áudio no Stream do PortAudio
        // O PortAudio espera um ponteiro para os dados (chunk.samples)
        // e o número de amostras (chunk.num_samples).
        err = Pa_WriteStream(stream, 
                             chunk.samples, 
                             chunk.num_samples); 

        if (err != paNoError) {
            std::cerr << "Erro durante a escrita no stream: " << Pa_GetErrorText(err) << std::endl;
            break;
        }
    }
    
    // 5. Parar, Fechar e Finalizar
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    
    piper_free(synth);
    std::cout << "Reprodução concluída." << std::endl;

    return 0;
}