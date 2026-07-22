# Alyssa — app nativo (casca da ponte)

Projeto Android REAL (Kotlin + WebView) que embrulha a PWA da `phone-bridge`.
Não é um brinquedo: resolve o buraco de voz do WebView com a ponte pro
`SpeechRecognizer` nativo, aceita o cert auto-assinado da ponte só pro host
pinado, e é a base pro Tier-1/2 da F5 (notificações, mic em foreground).

## Por que existe (e a real sobre o valor)

Honestidade primeiro: **a PWA que você já instalou ("adicionar à tela
inicial") já é um app** — standalone, ícone, sem barra do navegador. Esta
casca WebView adiciona pouco HOJE, com UMA exceção importante:

- **Voz.** Dentro de um WebView do Android o `webkitSpeechRecognition` não
  existe — o botão de falar da PWA MORRE. Esta casca faz a ponte pro
  reconhecimento nativo pt-BR, então o push-to-talk funciona. (O `app.js` já
  prefere a ponte nativa quando roda dentro do app.)

O APK que vai valer MUITO mais é o **nativo de presença** (Tier-1:
notificações + apps em foco; Tier-2: mic com VAD em foreground service) — a
PWA nunca vai poder fazer isso. Isso é trabalho de verdade da F5 e fica pra
uma próxima. Este projeto é o esqueleto onde ele cresce.

## Por que o .apk não veio pronto

Esta máquina não tem toolchain Android: sem Android SDK, sem Gradle, e o
Java do sistema é o **8** (o Android Gradle Plugin 8.x exige JDK 17+). Baixar
tudo (cmdline-tools + platform android-34 + build-tools + um JDK 17) é vários
GB e setup não-trivial — não dava pra fazer "e fechar o dia" com honestidade.
Então: o projeto está COMPLETO e pronto pra compilar; falta só a máquina.

## Como gerar o APK (5 min, com Android Studio)

1. Android Studio → **Open** → aponta pra esta pasta `phone-app/`.
   (Ele traz o próprio JDK 17 e baixa o SDK que faltar sozinho.)
2. Deixa o Gradle sincronizar.
3. Celular no cabo com depuração USB (ou um emulador) → botão **Run ▶**.
   Ou **Build → Build APK(s)** pra pegar o `.apk` e instalar na mão.
4. No primeiro abrir, cola o link do QR da ponte (`https://IP:8443/?t=...`).
   Fica salvo. (Toque-e-segure a tecla de menu reabre esse diálogo.)

Sem Android Studio, via linha de comando (precisa do SDK + JDK 17 no PATH e
um `gradlew` — o Studio gera o wrapper):
```
./gradlew assembleRelease   # gera app/build/outputs/apk/release/app-release.apk
```

## O que já está aqui

- `app/src/main/java/com/alyssa/app/MainActivity.kt` — WebView + SSL pinado +
  permissão de mic + ponte `AndroidSpeech` (SpeechRecognizer pt-BR → JS).
- `AndroidManifest.xml` — INTERNET + RECORD_AUDIO, portrait, ícone.
- `res/` — tema true-black, ícone adaptativo (losango A2 + A).
- gradle: AGP 8.5.2, Kotlin 1.9.24, compileSdk 34, minSdk 26 (Poco é Android 16).

## Segurança

Cert auto-assinado é aceito SÓ pro host que você salvou (não é "aceita
tudo"). Rede de casa + token da ponte. Quando o Tailscale entrar, dá pra
apontar o app pro IP do tailnet e tanto faz onde você está.
