import QtQuick 2.6
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ColumnLayout {
    id: chatArea
    Layout.fillWidth: true
    Layout.fillHeight: true

    property var apiManager
    property var chatHistoryModel
    property string currentChatId: Date.now().toString()

    // Message history model
    ListModel {
        id: conversationModel
    }

    // Chat header
    Rectangle {
        Layout.preferredHeight: 60
        Layout.fillWidth: true
        color: theme.surfaceColor

        Label {
            anchors.centerIn: parent
            text: "Alyssa AI Chat"
            font.pixelSize: theme.fontSizeLarge
            font.bold: true
            color: theme.textPrimary
        }
    }

    // Chat messages area
    ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true

        contentWidth: -1
        contentHeight: chatColumn.height

        Column {
            id: chatColumn
            width: parent.width

            Repeater {
                model: conversationModel

                delegate: Rectangle {
                    width: parent.width
                    padding: 12
                    color: model.role === "user" ? "#25252b" : "#1e1e24"
                    border.color: model.role === "user" ? theme.primaryColor : "#3f3f46"
                    border.width: 1
                    radius: 8
                    margin: 4

                    Column {
                        Label {
                            text: model.role === "user" ? "You" : "Alyssa"
                            font.bold: true
                            color: model.role === "user" ? theme.primaryColor : theme.accentGreen
                            font.pixelSize: theme.fontSizeSmall
                        }

                        Text {
                            text: model.content
                            color: theme.textPrimary
                            font.pixelSize: theme.fontSizeNormal
                            wrapMode: Text.WordWrap
                            width: parent.width - 24
                        }
                    }
                }
            }
        }
    }

    // Input area
    Rectangle {
        Layout.preferredHeight: 100
        Layout.fillWidth: true
        color: theme.surfaceColor
        border.color: "#3f3f46"
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            anchors.margins: 12

            // Loading indicator
            RowLayout {
                Layout.fillWidth: true
                visible: loadingIndicator.visible

                BusyIndicator {
                    id: loadingIndicator
                    running: false
                    visible: false
                    Layout.alignment: Qt.AlignHCenter
                }

                Label {
                    text: "Thinking..."
                    color: theme.textSecondary
                    font.pixelSize: theme.fontSizeSmall
                }
            }

            // Message input
            TextArea {
                id: messageInput
                Layout.fillWidth: true
                Layout.fillHeight: true
                placeholderText: apiManager.connected ? "Type your message..." : "Connecting to backend..."
                color: theme.textPrimary
                selectByMouse: true
                wrapMode: TextArea.Wrap
                font.pixelSize: theme.fontSizeNormal
                readOnly: !apiManager.connected

                background: Rectangle {
                    radius: 6
                    border.color: theme.primaryColor
                    border.width: 1
                    color: "#25252b"
                }

                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Return && !event.modifiers) {
                        sendMessage()
                        event.accepted = true
                    }
                }
            }

            // Button row
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Button {
                    id: sendButton
                    text: "Send"
                    enabled: messageInput.text.trim().length > 0 && apiManager.connected
                    onClicked: sendMessage()

                    background: Rectangle {
                        radius: 6
                        color: enabled ? theme.primaryColor : "#3f3f46"
                    }

                    contentItem: Label {
                        text: parent.text
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.bold: true
                    }
                }

                Button {
                    text: "Export"
                    onClicked: exportChat()

                    background: Rectangle {
                        radius: 6
                        color: theme.secondaryColor
                    }

                    contentItem: Label {
                        text: parent.text
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.bold: true
                    }
                }
            }
        }
    }

    function addMessage(message) {
        conversationModel.append({
            role: message.role,
            content: message.content,
            timestamp: new Date().toISOString()
        })

        // Auto-scroll to bottom
        chatColumn.forceLayout()
        scrollArea.contentY = scrollArea.contentHeight - scrollArea.height
    }

    function sendMessage() {
        if (!apiManager || !apiManager.connected || messageInput.text.trim() === "") {
            return
        }

        var userMessage = {
            role: "user",
            content: messageInput.text.trim()
        }

        addMessage(userMessage)
        messageInput.text = ""
        loadingIndicator.visible = true

        // Prepare conversation history (backend only needs last user message)
        var messages = []
        for (var i = 0; i < conversationModel.count; i++) {
            messages.push({
                role: conversationModel.get(i).role,
                content: conversationModel.get(i).content
            })
        }

        apiManager.sendMessage(messages, function(response) {
            loadingIndicator.visible = false

            if (response.success) {
                addMessage({
                    role: "assistant",
                    content: response.content
                })
                saveToHistory()
            } else {
                addMessage({
                    role: "system",
                    content: "Error: " + response.error
                })

                // Show error notification
                exportNotification.showError("API Error: " + response.error)
            }
        })
    }

    function saveToHistory() {
        if (!currentChatId) {
            currentChatId = Date.now().toString()
            chatHistoryModel.append({
                id: currentChatId,
                title: conversationModel.get(0).content.substring(0, 30) + "...",
                messageCount: conversationModel.count,
                timestamp: new Date().toLocaleDateString()
            })
        } else {
            // Update existing chat
            var index = chatHistoryModel.getIndexById(currentChatId)
            if (index !== -1) {
                chatHistoryModel.update(index, {
                    messageCount: conversationModel.count,
                    timestamp: new Date().toLocaleDateString()
                })
            }
        }
    }

    function exportChat() {
        var exportData = "CHAT EXPORT\n" + "=".repeat(50) + "\n"

        for (var i = 0; i < conversationModel.count; i++) {
            var msg = conversationModel.get(i)
            exportData += `[${msg.role.toUpperCase()}] ${new Date(msg.timestamp).toLocaleString()}:\n`
            exportData += msg.content + "\n\n"
        }

        console.log("EXPORTED CHAT:\n" + exportData)
        exportNotification.showSuccess("Chat exported to console")
    }

    // Initialize with welcome message
    Component.onCompleted: {
        addMessage({
            role: "assistant",
            content: "Hello! I'm Alyssa AI. How can I help you today?"
        })
    }
}
