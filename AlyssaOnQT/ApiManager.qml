import QtQuick 2.6
import QtNetwork 5.15

Item {
    id: apiManager
    property string baseUrl: "http://localhost:8181"
    property bool connected: false
    property bool modelLoaded: true // Backend always has model loaded

    signal connectionStatusChanged(bool status)
    signal modelStatusChanged(bool status)

    function checkConnection() {
        var xhr = new XMLHttpRequest()
        xhr.open("GET", baseUrl + "/health", true)

        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 200) {
                    try {
                        var response = JSON.parse(xhr.responseText)
                        if (response.status === "healthy") {
                            connected = true
                            console.log("✓ Backend connection established")
                            connectionStatusChanged(true)
                        } else {
                            connected = false
                            console.warn("Backend health check failed:", response)
                            connectionStatusChanged(false)
                        }
                    } catch (e) {
                        connected = false
                        console.error("Invalid health response:", e)
                        connectionStatusChanged(false)
                    }
                } else {
                    connected = false
                    console.error("Health check failed with status:", xhr.status)
                    connectionStatusChanged(false)
                }
            }
        }

        xhr.onerror = function() {
            connected = false
            console.error("Network error during health check")
            connectionStatusChanged(false)
        }

        xhr.send()
    }

    function sendMessage(messages, callback) {
        if (!connected) {
            console.error("Cannot send message: Backend not connected")
            callback({
                success: false,
                error: "Backend not connected. Please check if server is running."
            })
            return
        }

        // Extract last user message (backend only needs latest input)
        var lastUserMessage = ""
        for (var i = messages.length - 1; i >= 0; i--) {
            if (messages[i].role === "user") {
                lastUserMessage = messages[i].content
                break
            }
        }

        if (!lastUserMessage) {
            callback({
                success: false,
                error: "No user message found"
            })
            return
        }

        var xhr = new XMLHttpRequest()
        xhr.open("POST", baseUrl + "/think/fusion", true)
        xhr.setRequestHeader("Content-Type", "application/json")

        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 200) {
                    try {
                        var response = JSON.parse(xhr.responseText)
                        if (response.success && response.output) {
                            callback({
                                success: true,
                                content: response.output
                            })
                        } else {
                            callback({
                                success: false,
                                error: response.error || "Unknown error from backend"
                            })
                        }
                    } catch (e) {
                        callback({
                            success: false,
                            error: "Invalid JSON response: " + e.message
                        })
                    }
                } else {
                    var errorMsg = "Server error: " + xhr.status
                    try {
                        var errResponse = JSON.parse(xhr.responseText)
                        errorMsg += " - " + (errResponse.error || errResponse.message || "Unknown error")
                    } catch (e) {
                        if (xhr.responseText) errorMsg += " - " + xhr.responseText.substring(0, 100)
                    }

                    callback({
                        success: false,
                        error: errorMsg
                    })
                }
            }
        }

        xhr.onerror = function() {
            callback({
                success: false,
                error: "Network error - failed to connect to backend"
            })
        }

        xhr.timeout = 30000 // 30 seconds
        xhr.ontimeout = function() {
            callback({
                success: false,
                error: "Request timed out after 30 seconds"
            })
        }

        // Backend expects: { "input": "user message" }
        var requestData = {
            input: lastUserMessage
        }

        console.log("➤ Sending to /think/fusion:", JSON.stringify(requestData))
        xhr.send(JSON.stringify(requestData))
    }
}
