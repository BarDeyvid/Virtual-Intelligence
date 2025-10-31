    logging.info(f"🗣️ Alyssa (voz no Discord): {cleaned_response_text}")
        
        # Gera os dados de áudio (MP3 em BytesIO)
        mp3_buffer = await alyssa_voice_client_tts._generate_audio_data(response_text)

        if mp3_buffer:
            try:
                source = discord.FFmpegPCMAudio(mp3_buffer, pipe=True) 
                
                if discord_voice_connection.is_playing():
                    discord_voice_connection.stop() 
                
                discord_voice_connection.play(source, after=lambda e: logging.error(f'Player error: {e}') if e else None)
                logging.info(f"🗣️ Alyssa (voz no Discord): Reproduzindo áudio.")
                
                while discord_voice_connection.is_playing():
                    await asyncio.sleep(0.1)
            except Exception as e:
                logging.error(f"❌ Erro ao reproduzir voz no Discord: {e}", exc_info=True)
                # Envia mensagem de texto APENAS SE A VOCALIZAÇÃO FALHOU
                await channel.send(f"❌ Alyssa: Desculpe, tive um problema com minha voz. {cleaned_response_text}")
        else:
            logging.warning("⚠️ Nenhum áudio MP3 recebido do ElevenLabs para reprodução no Discord.")
            # Envia mensagem de texto APENAS SE A VOCALIZAÇÃO FALHOU
            await channel.send(f"⚠️ Alyssa: (Problema ao gerar áudio) {cleaned_response_text}")
    else:
        # Sempre envia a resposta de texto se não houve tentativa de vocalizar
        await channel.send(f"🗣️ Alyssa: {cleaned_response_text}")
        logging.info(f"🗣️ Alyssa (texto no Discord): {cleaned_response_text}")