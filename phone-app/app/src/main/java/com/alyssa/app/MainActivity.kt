package com.alyssa.app

import android.Manifest
import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.PackageManager
import android.net.http.SslError
import android.os.Bundle
import android.speech.RecognizerIntent
import android.speech.SpeechRecognizer
import android.text.InputType
import android.webkit.*
import android.widget.EditText
import androidx.activity.ComponentActivity
import androidx.appcompat.app.AlertDialog
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import java.net.URI

/**
 * Alyssa — casca nativa da PWA da ponte (phone-bridge).
 *
 * Por que existe além da PWA "adicionar à tela inicial":
 *  1. Resolve o buraco de voz do WebView — o `webkitSpeechRecognition` NÃO
 *     existe dentro de um WebView do Android. Aqui a gente faz a ponte pro
 *     SpeechRecognizer NATIVO (pt-BR) e injeta o resultado no JS (window.
 *     AndroidSpeech). O app.js da PWA já prefere essa ponte quando existe.
 *  2. Aceita o cert auto-assinado da ponte SÓ pro host configurado (LAN).
 *  3. É a base pra F5 Tier-1/2 (notificações, mic em foreground) — que a PWA
 *     nunca vai poder fazer. Por ora é só a casca + voz.
 */
class MainActivity : ComponentActivity() {

    private lateinit var web: WebView
    private var bridgeHost: String = ""     // pino do SSL (só aceita este host)
    private var speech: SpeechRecognizer? = null

    private val prefs by lazy { getSharedPreferences("alyssa", Context.MODE_PRIVATE) }

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        web = WebView(this)
        setContentView(web)

        WebView.setWebContentsDebuggingEnabled(true)
        with(web.settings) {
            javaScriptEnabled = true
            domStorageEnabled = true            // localStorage do token + service worker
            mediaPlaybackRequiresUserGesture = false
        }

        // Ponte de voz nativa exposta ao JS como window.AndroidSpeech.
        web.addJavascriptInterface(SpeechBridge(), "AndroidSpeech")

        web.webViewClient = object : WebViewClient() {
            // Cert auto-assinado da ponte: aceita SÓ o host que a gente salvou.
            // Em rede confiável (casa) + token, o risco é aceitável; fora do
            // host pinado, recusa (não vira "aceita tudo").
            override fun onReceivedSslError(v: WebView, h: SslErrorHandler, e: SslError) {
                val errHost = runCatching { URI(e.url).host }.getOrNull()
                if (errHost != null && errHost == bridgeHost) h.proceed() else h.cancel()
            }
        }

        web.webChromeClient = object : WebChromeClient() {
            // A PWA pode pedir mic pela API web; concede na hora (já temos a
            // permissão de runtime). O push-to-talk usa a ponte nativa, mas
            // isso cobre qualquer getUserMedia que a página faça.
            override fun onPermissionRequest(request: PermissionRequest) {
                request.grant(request.resources)
            }
        }

        ensureMicPermission()
        loadBridgeOrAsk()
    }

    /** Carrega a URL salva da ponte; na primeira vez, pede pra colar o link do QR. */
    private fun loadBridgeOrAsk() {
        val saved = prefs.getString("bridge_url", null)
        if (saved.isNullOrBlank()) {
            askForBridgeUrl()
        } else {
            bridgeHost = runCatching { URI(saved).host }.getOrNull() ?: ""
            web.loadUrl(saved)
        }
    }

    private fun askForBridgeUrl() {
        val input = EditText(this).apply {
            hint = "https://192.168.x.x:8443/?t=..."
            inputType = InputType.TYPE_TEXT_VARIATION_URI
        }
        AlertDialog.Builder(this)
            .setTitle("Endereço da Alyssa")
            .setMessage("Cola o link do QR (o console da ponte mostra) — fica salvo.")
            .setView(input)
            .setCancelable(false)
            .setPositiveButton("Conectar") { _, _ ->
                val url = input.text.toString().trim()
                if (url.isNotEmpty()) {
                    prefs.edit().putString("bridge_url", url).apply()
                    bridgeHost = runCatching { URI(url).host }.getOrNull() ?: ""
                    web.loadUrl(url)
                }
            }
            .show()
    }

    private fun ensureMicPermission() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO)
            != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(Manifest.permission.RECORD_AUDIO), 1)
        }
    }

    /** Menu longo: toque e segure com dois dedos reabre o diálogo de endereço. */
    override fun onKeyLongPress(keyCode: Int, event: android.view.KeyEvent?): Boolean {
        askForBridgeUrl(); return true
    }

    override fun onDestroy() {
        speech?.destroy()
        super.onDestroy()
    }

    // Botão voltar navega no WebView antes de sair.
    override fun onBackPressed() {
        if (web.canGoBack()) web.goBack() else super.onBackPressed()
    }

    /**
     * Ponte JS→SpeechRecognizer nativo. O app.js chama AndroidSpeech.start();
     * a gente devolve o texto por window.onAndroidSpeech(text, isFinal).
     */
    inner class SpeechBridge {
        @JavascriptInterface
        fun available(): Boolean = SpeechRecognizer.isRecognitionAvailable(this@MainActivity)

        @JavascriptInterface
        fun start() {
            runOnUiThread {
                speech?.destroy()
                speech = SpeechRecognizer.createSpeechRecognizer(this@MainActivity).apply {
                    setRecognitionListener(object : android.speech.RecognitionListener {
                        override fun onResults(b: Bundle) = deliver(b, true)
                        override fun onPartialResults(b: Bundle) = deliver(b, false)
                        override fun onError(e: Int) = post("", true)
                        override fun onReadyForSpeech(p: Bundle?) {}
                        override fun onBeginningOfSpeech() {}
                        override fun onRmsChanged(v: Float) {}
                        override fun onBufferReceived(b: ByteArray?) {}
                        override fun onEndOfSpeech() {}
                        override fun onEvent(t: Int, p: Bundle?) {}
                        private fun deliver(b: Bundle, fin: Boolean) {
                            val hit = b.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION)
                            post(hit?.firstOrNull() ?: "", fin)
                        }
                    })
                }
                val intent = android.content.Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH).apply {
                    putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM)
                    putExtra(RecognizerIntent.EXTRA_LANGUAGE, "pt-BR")
                    putExtra(RecognizerIntent.EXTRA_PARTIAL_RESULTS, true)
                }
                speech?.startListening(intent)
            }
        }

        @JavascriptInterface
        fun stop() { runOnUiThread { speech?.stopListening() } }

        private fun post(text: String, fin: Boolean) {
            val safe = text.replace("\\", "\\\\").replace("'", "\\'")
            web.post {
                web.evaluateJavascript(
                    "window.onAndroidSpeech && window.onAndroidSpeech('$safe', $fin);", null)
            }
        }
    }
}
