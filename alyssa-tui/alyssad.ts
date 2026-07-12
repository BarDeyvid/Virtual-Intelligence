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

/** Shape da resposta do método `status` (docs/alyssad-protocol.md). */
export interface DaemonStatus {
    echo: boolean;
    busy: boolean;
    voice_available: boolean;
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
     */
    public say(text: string, tts?: boolean) {
        this.send('say', tts === undefined ? { text } : { text, tts });
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
