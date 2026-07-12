// src/ui.tsx
import React, { useState, useEffect, useRef } from 'react';
import { render, Box, Text, useInput, useApp, useStdout } from 'ink';
import TextInput from 'ink-text-input';
import { AlyssadClient, HormoneProfile, DaemonStatus } from './alyssad';

const client = new AlyssadClient();

interface Message {
    sender: 'Deyvid' | 'Alyssa' | 'System';
    text: string;
    latencyMs?: number;
    /** true quando a resposta saiu falada pelo TTS */
    spoken?: boolean;
}

type TtsMode = 'auto' | 'on' | 'off';

const HELP_TEXT = [
    '/help — esta lista',
    '/clear — limpa o histórico da tela',
    '/tts auto|on|off — voz por mensagem (auto = decisão do daemon)',
    '/status — snapshot do daemon',
    '/quit — fecha a TUI (daemon continua)',
    '/shutdown — encerra o daemon e a TUI',
    'Tab alterna abas · PgUp/PgDn rola o histórico · ↑/↓ repete mensagens',
].join('\n');

const ProgressBar = ({ label, value, color }: { label: string, value: number, color: string }) => {
    const barWidth = 20;
    const filled = Math.max(0, Math.min(barWidth, Math.round(value * barWidth)));
    const empty = barWidth - filled;

    return (
        <Box>
            <Box width={12}><Text color={color}>{label}</Text></Box>
            <Text color={color}>{'█'.repeat(filled)}{'░'.repeat(empty)}</Text>
            <Text dimColor> {value.toFixed(4)}</Text>
        </Box>
    );
};

const App = () => {
    const { exit } = useApp();
    const { stdout } = useStdout();
    const [connected, setConnected] = useState(false);
    const [activeTab, setActiveTab] = useState<'chat' | 'endocrine'>('chat');
    const [input, setInput] = useState('');
    const [history, setHistory] = useState<Message[]>([]);
    const [isThinking, setIsThinking] = useState(false);
    const [hormones, setHormones] = useState<HormoneProfile | null>(null);
    const [ambient, setAmbient] = useState('');
    const [streamText, setStreamText] = useState('');
    const [status, setStatus] = useState<DaemonStatus | null>(null);
    const [ttsMode, setTtsMode] = useState<TtsMode>('auto');
    const [scrollOffset, setScrollOffset] = useState(0); // 0 = colado no fim
    const [thinkingSince, setThinkingSince] = useState<number | null>(null);
    const [thinkingElapsed, setThinkingElapsed] = useState(0);

    // Recall de mensagens enviadas (↑/↓ estilo shell)
    const sentHistory = useRef<string[]>([]);
    const recallIndex = useRef<number>(-1);
    const draftBeforeRecall = useRef<string>('');

    // Já estivemos conectados? (pra só anunciar queda real, não cada retry)
    const wasConnected = useRef(false);

    const pushSystem = (text: string) =>
        setHistory(prev => [...prev, { sender: 'System', text }]);

    useEffect(() => {
        client.on('connected', () => {
            setConnected(true);
            if (wasConnected.current) pushSystem('reconectada ao alyssad');
            wasConnected.current = true;
        });
        client.on('disconnected', () => {
            setConnected(false);
            setIsThinking(false);
            if (wasConnected.current) {
                pushSystem('conexão com o alyssad caiu — tentando reconectar...');
                wasConnected.current = false;
            }
        });

        client.on('state', (data) => {
            const thinking = data.phase === 'thinking';
            setIsThinking(thinking);
            setThinkingSince(thinking ? Date.now() : null);
            if (thinking) setStreamText('');
        });

        // Pedaços crus conforme ela gera; o 'response' final substitui tudo
        // (tokens podem carregar marcações intermediárias de tool call)
        client.on('token', (data) => setStreamText(prev => prev + data.text));

        client.on('response', (data) => {
            setStreamText('');
            setScrollOffset(0); // resposta nova puxa a visão de volta pro fim
            setHistory(prev => [...prev, {
                sender: 'Alyssa', text: data.text,
                latencyMs: data.latency_ms, spoken: data.tts === true,
            }]);
        });

        client.on('hormones', (data) => setHormones(data));
        client.on('ambient', (line) => setAmbient(line));
        client.on('status', (data) => setStatus(data));

        client.on('daemon-error', (msg) => {
            pushSystem(`[Erro] ${msg}`);
        });

        return () => {
            client.removeAllListeners();
        };
    }, []);

    // Hormônios + ambient frescos mesmo sem conversar (o daemon só amostra
    // o ambient quando está ocioso, então pausamos o poll enquanto pensa)
    useEffect(() => {
        if (!connected || isThinking) return;
        const timer = setInterval(() => client.requestStatus(), 15000);
        return () => clearInterval(timer);
    }, [connected, isThinking]);

    // Cronômetro do "pensando..." — mostra quanto tempo o turno está levando
    useEffect(() => {
        if (thinkingSince === null) { setThinkingElapsed(0); return; }
        const timer = setInterval(
            () => setThinkingElapsed((Date.now() - thinkingSince) / 1000), 250);
        return () => clearInterval(timer);
    }, [thinkingSince]);

    // Quantas mensagens cabem na tela: cada mensagem ocupa ~3 linhas
    // (remetente + texto + respiro). O resto da moldura come ~14 linhas.
    const rows = stdout?.rows ?? 30;
    const visibleCount = Math.max(4, Math.floor((rows - 14) / 3));
    const maxScroll = Math.max(0, history.length - visibleCount);

    // Tab alterna abas. (Setas ← → ficam livres pro cursor do TextInput.)
    // PgUp/PgDn rolam o histórico; ↑/↓ repetem mensagens já enviadas.
    useInput((_char, key) => {
        if (key.tab) {
            setActiveTab(prev => prev === 'chat' ? 'endocrine' : 'chat');
            return;
        }
        if (activeTab !== 'chat') return;

        if (key.pageUp) setScrollOffset(prev => Math.min(maxScroll, prev + Math.max(1, visibleCount - 1)));
        if (key.pageDown) setScrollOffset(prev => Math.max(0, prev - Math.max(1, visibleCount - 1)));

        if (key.upArrow && sentHistory.current.length > 0) {
            if (recallIndex.current === -1) {
                draftBeforeRecall.current = input;
                recallIndex.current = sentHistory.current.length - 1;
            } else if (recallIndex.current > 0) {
                recallIndex.current--;
            }
            setInput(sentHistory.current[recallIndex.current] ?? '');
        }
        if (key.downArrow && recallIndex.current !== -1) {
            if (recallIndex.current < sentHistory.current.length - 1) {
                recallIndex.current++;
                setInput(sentHistory.current[recallIndex.current] ?? '');
            } else {
                recallIndex.current = -1;
                setInput(draftBeforeRecall.current);
            }
        }
    });

    const quit = (shutdownDaemon: boolean) => {
        if (shutdownDaemon) client.shutdown();
        client.dispose();
        exit();
    };

    /** Comandos locais (/help, /tts...). true = era comando, não vai pro daemon. */
    const handleCommand = (text: string): boolean => {
        const [cmd = '', ...rest] = text.trim().split(/\s+/);
        switch (cmd) {
            case '/help': pushSystem(HELP_TEXT); return true;
            case '/clear': setHistory([]); setScrollOffset(0); return true;
            case '/quit': case '/exit': quit(false); return true;
            case '/shutdown': quit(true); return true;
            case '/status': {
                client.requestStatus();
                const s = status;
                pushSystem(s
                    ? `daemon: ${s.echo ? 'echo (sem modelos)' : 'modelos carregados'} · voz ${s.voice_available ? 'disponível' : 'indisponível'} · ${s.busy ? 'ocupada' : 'ociosa'}`
                    : 'ainda sem status do daemon — pedindo agora...');
                return true;
            }
            case '/tts': {
                const arg = (rest[0] || '').toLowerCase();
                if (arg === 'auto' || arg === 'on' || arg === 'off') {
                    setTtsMode(arg);
                    pushSystem(`TTS: ${arg}${arg !== 'off' && status && !status.voice_available ? ' (daemon sem --voice: vai sair só texto)' : ''}`);
                } else {
                    pushSystem('uso: /tts auto|on|off');
                }
                return true;
            }
            default:
                if (cmd.startsWith('/')) {
                    pushSystem(`comando desconhecido: ${cmd} (/help lista os comandos)`);
                    return true;
                }
                return false;
        }
    };

    const handleSubmit = (text: string) => {
        if (!text.trim()) return;
        if (handleCommand(text)) { setInput(''); return; }
        if (isThinking || !connected) return;

        sentHistory.current.push(text);
        recallIndex.current = -1;
        setScrollOffset(0);
        setHistory(prev => [...prev, { sender: 'Deyvid', text }]);
        client.say(text, ttsMode === 'auto' ? undefined : ttsMode === 'on');
        setInput('');
    };

    const sliceEnd = history.length - scrollOffset;
    const visibleHistory = history.slice(Math.max(0, sliceEnd - visibleCount), sliceEnd);
    const hiddenAbove = Math.max(0, sliceEnd - visibleCount);
    const emotionalState = hormones?.emotional_state;

    return (
        <Box flexDirection="column" borderStyle="round" borderColor="cyan" padding={1}>
            {/* Header */}
            <Box borderStyle="single" borderColor="gray" marginBottom={1}>
                <Box flexGrow={1}>
                    <Text backgroundColor="cyan" color="black" bold> 🌌 ALYSSA AI </Text>
                    <Text dimColor> v2.1-MoE (Ink Edition)</Text>
                    {emotionalState && <Text color="magenta">  ♥ {emotionalState}</Text>}
                    {status?.echo && <Text color="yellow">  [echo]</Text>}
                </Box>
                <Box>
                    <Text color={connected ? 'green' : 'red'}>
                        {connected ? '● ONLINE' : '○ OFFLINE'}
                    </Text>
                </Box>
            </Box>

            {/* Tab Menu */}
            <Box marginBottom={1}>
                <Text color={activeTab === 'chat' ? 'cyan' : 'gray'} bold={activeTab === 'chat'}> 💬 Chat </Text>
                <Text dimColor> | </Text>
                <Text color={activeTab === 'endocrine' ? 'cyan' : 'gray'} bold={activeTab === 'endocrine'}> 🧠 Endocrine </Text>
                <Text dimColor>  (Tab alterna · /help)</Text>
            </Box>

            {/* Percepção dela: a linha [AMBIENTE] que entra em cada prompt */}
            {ambient !== '' && (
                <Box marginBottom={1}>
                    <Text dimColor>👁 {ambient}</Text>
                </Box>
            )}

            {/* Tab Content */}
            <Box flexGrow={1} flexDirection="column" minHeight={16}>
                {activeTab === 'chat' && (
                    <Box flexDirection="column" flexGrow={1}>
                        {hiddenAbove > 0 && (
                            <Text dimColor italic>↑ {hiddenAbove} mensagens acima (PgUp/PgDn)</Text>
                        )}
                        {history.length === 0 && <Text dimColor italic>Sem mensagens ainda... (/help lista os comandos)</Text>}
                        {visibleHistory.map((msg, i) => (
                            <Box key={i} flexDirection="column" marginBottom={1}>
                                <Box>
                                    <Text bold color={msg.sender === 'Alyssa' ? 'cyan' : msg.sender === 'System' ? 'yellow' : 'blue'}>
                                        {msg.sender}
                                    </Text>
                                    {msg.latencyMs !== undefined && (
                                        <Text dimColor> · {(msg.latencyMs / 1000).toFixed(1)}s</Text>
                                    )}
                                    {msg.spoken && <Text dimColor> · 🔊</Text>}
                                </Box>
                                <Text color={msg.sender === 'Alyssa' ? 'cyanBright' : msg.sender === 'System' ? 'gray' : 'white'}>
                                    {msg.text}
                                </Text>
                            </Box>
                        ))}
                        {scrollOffset > 0 && (
                            <Text dimColor italic>↓ {scrollOffset} mensagens abaixo</Text>
                        )}
                        {isThinking && streamText === '' && (
                            <Text dimColor italic>⏳ Alyssa tá pensando... {thinkingElapsed.toFixed(1)}s</Text>
                        )}
                        {isThinking && streamText !== '' && (
                            <Box flexDirection="column" marginBottom={1}>
                                <Box>
                                    <Text bold color="cyan">Alyssa</Text>
                                    <Text dimColor> · {thinkingElapsed.toFixed(1)}s</Text>
                                </Box>
                                <Text color="cyanBright">{streamText}<Text dimColor>▌</Text></Text>
                            </Box>
                        )}
                    </Box>
                )}

                {activeTab === 'endocrine' && (
                    <Box flexDirection="column" borderStyle="single" borderColor="gray" padding={1}>
                        <Text color="magenta" bold>Current State: {emotionalState || 'neutral'}</Text>
                        <Box marginBottom={1}><Text dimColor>──────────────────────────────────</Text></Box>
                        <ProgressBar label="Cortisol" value={hormones?.cortisol || 0} color="red" />
                        <ProgressBar label="Dopamine" value={hormones?.dopamine || 0} color="yellow" />
                        <ProgressBar label="Oxytocin" value={hormones?.oxytocin || 0} color="green" />
                        <ProgressBar label="Serotonin" value={hormones?.serotonin || 0} color="cyan" />
                        <ProgressBar label="Adrenaline" value={hormones?.adrenaline || 0} color="redBright" />
                    </Box>
                )}
            </Box>

            {/* Input Footer — o TextInput fica montado mesmo enquanto ela pensa
                (dá pra ir digitando a próxima mensagem; só o envio que espera) */}
            {activeTab === 'chat' && (
                <Box flexDirection="column" marginTop={1}>
                    <Box borderStyle="single" borderColor="gray" paddingTop={1}>
                        <Text bold color="cyan">❯ </Text>
                        <Box flexGrow={1}>
                            <TextInput
                                value={input}
                                onChange={setInput}
                                onSubmit={handleSubmit}
                                placeholder={isThinking ? 'ela ainda tá pensando — pode ir digitando...' : 'Escreva para Alyssa...'}
                            />
                        </Box>
                    </Box>
                    <Box>
                        <Text dimColor>
                            TTS: {ttsMode}{status ? (status.voice_available ? ' · voz ok' : ' · daemon sem voz') : ''}
                            {!connected ? ' · aguardando daemon na porta ' + (process.env.ALYSSAD_PORT || 8377) : ''}
                        </Text>
                    </Box>
                </Box>
            )}
        </Box>
    );
};

const app = render(<App />);
// Ctrl+C / /quit: solta o socket e o timer de reconexão pra o processo morrer
app.waitUntilExit().then(() => client.dispose());
