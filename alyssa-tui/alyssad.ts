// src/alyssad.ts
import { Socket } from 'net';
import { createInterface } from 'readline';
import { EventEmitter } from 'events';

export interface HormoneProfile {
    cortisol: number;
    dopamine: number;
    oxytocin: number;
    serotonin: number;
    adrenaline: number;
    emotional_state: string;
}

export interface SelfOpinion { topic: string; stance: string; confidence: number; }
export interface SelfGoal { desc: string; progress: string; priority: number; }
export interface SelfAgendaItem { bring_up: string; reason: string; }

/** O self persistente dela (state/self.json), via `status` (v0.2/F3). */
export interface SelfState {
    opinions: SelfOpinion[];
    goals: SelfGoal[];
    agenda: SelfAgendaItem[];
    yesterday_summary: string;
    last_consolidation_date: number;
}

/** Shape da resposta do método `status` (docs/alyssad-protocol.md). */
export interface DaemonStatus {
    echo: boolean;
    busy: boolean;
    voice_available: boolean;
    voice_in?: boolean;
    self?: SelfState;
    hormones?: HormoneProfile;
    emotional_state?: string;
    ambient?: string;
}

export class AlyssadClient extends EventEmitter {
    private socket: Socket | null = null;
    private msgId = 0;
    private disposed = false;

    constructor(private port: number = Number(process.env.ALYSSAD_PORT || 8377)) {
        super();
        this.connect();
    }

    private connect() {
        if (this.disposed) return;

        // Socket NOVO a cada tentativa: reusar o antigo acumula listeners de
        // 'close' e cada falha dispara todos eles → tempestade de reconexão.
        const socket = new Socket();
        this.socket = socket;

        socket.connect(this.port, '127.0.0.1', () => {
            // v0.2: auth por token quando o daemon exigir (ALYSSAD_TOKEN dos
            // dois lados). Sem token no daemon o auth é no-op de sucesso.
            const token = process.env.ALYSSAD_TOKEN;
            if (token) this.send('auth', { token });
            this.emit('connected');
            this.requestStatus(); // Get initial hormones + ambient
        });

        const rl = createInterface({ input: socket });
        rl.on('line', (line) => {
            try {
                this.handleMessage(JSON.parse(line));
            } catch (e) {
                // Ignore parse errors from bad chunks
            }
        });

        // 'close' sempre vem depois de 'error'; a reconexão mora só no close.
        socket.on('error', () => {});
        socket.once('close', () => {
            rl.close();
            socket.removeAllListeners();
            this.emit('disconnected');
            if (!this.disposed) {
                // unref: um timer de reconexão pendente não segura o processo
                // vivo depois que a TUI fecha.
                setTimeout(() => this.connect(), 2000).unref();
            }
        });
    }

    private handleMessage(msg: any) {
        if (msg.type === 'event') {
            const data = msg.data ?? {};
            if (msg.event === 'error') {
                // Evento de erro do daemon carrega {message}; normaliza pra
                // string, igual aos erros de request — a UI só lida com um shape.
                this.emit('daemon-error', String(data.message ?? 'erro desconhecido'));
            } else {
                this.emit(msg.event, data);
            }
        } else if (msg.type === 'res') {
            if (msg.ok === false) {
                // "busy", "params.text vazio"... antes isso morria em silêncio.
                this.emit('daemon-error', String(msg.error ?? 'request falhou'));
                return;
            }
            // Resposta de `status`: o shape completo pra quem quiser (a UI usa
            // voice_available), mais os recortes hormones/ambient que já existiam.
            if (msg.data && typeof msg.data.echo === 'boolean') this.emit('status', msg.data);
            if (msg.data?.hormones) this.emit('hormones', msg.data.hormones);
            if (typeof msg.data?.ambient === 'string') this.emit('ambient', msg.data.ambient);
        }
    }

    private send(method: string, params: Record<string, unknown> = {}) {
        if (!this.socket || this.socket.destroyed) return;
        const payload = { type: 'req', id: `msg_${++this.msgId}`, method, params };
        this.socket.write(JSON.stringify(payload) + '\n');
    }

    /**
     * tts omitido = decisão fica com o daemon (fala quando subiu com --voice).
     * Mandar `false` fixo aqui silenciava a voz mesmo com o daemon em --voice.
     * client:'tui' identifica a origem — o broadcast `user_text` volta pra
     * todos e a TUI ignora o próprio eco.
     */
    public say(text: string, tts?: boolean) {
        this.send('say', tts === undefined ? { text, client: 'tui' } : { text, tts, client: 'tui' });
    }

    /** Voice-in do daemon (mic → Whisper). v0.2, método `listen`. */
    public listen(enabled: boolean) {
        this.send('listen', { enabled });
    }

    /** Consolidação manual (ela "dorme e digere o dia" agora). */
    public consolidate() {
        this.send('consolidate');
    }

    public requestStatus() {
        this.send('status');
    }

    /** Pede pro daemon encerrar (espera o turno em andamento terminar). */
    public shutdown() {
        this.send('shutdown');
    }

    /** Encerra o cliente de vez (sem reconexão). A TUI chama ao sair. */
    public dispose() {
        this.disposed = true;
        this.socket?.destroy();
    }
}
