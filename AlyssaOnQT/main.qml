import QtQuick 2.6
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import Qt.labs.platform 1.1
import "components"
import "models"

ApplicationWindow {
    id: window
    visible: true
    width: 1000
    height: 700
    minimumWidth: 800
    minimumHeight: 600
    title: "Alyssa AI Chat"

    // Theme management - FIXED: Added missing comma after last property
    property var theme: {
        surfaceColor: "#18181b",
        primaryColor: "#6366f1",
        secondaryColor: "#4f46e5",
        accentGreen: "#10b981",
        textPrimary: "#fafafa",
        textSecondary: "#9ca3af",
        fontSizeSmall: 13,
        fontSizeNormal: 14,
        fontSizeLarge: 16,  // <-- THIS COMMA WAS MISSING
    }

    // API Manager instance
    ApiManager {
        id: apiManager
        baseUrl: "http://localhost:8181"
    }

    // Chat history model
    ChatHistoryModel {
        id: chatHistoryModel
    }

    // Main content layout
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Sidebar (future implementation for chat history)
        Item {
            Layout.preferredWidth: 280
            Layout.fillHeight: true
            visible: false // Hidden for now
        }

        // Main chat area
        ChatArea {
            Layout.fillWidth: true
            Layout.fillHeight: true
            apiManager: window.apiManager
            chatHistoryModel: window.chatHistoryModel
        }
    }

    // Export notification component
    ExportNotification {
        id: exportNotification
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
    }

    Component.onCompleted: {
        // Initialize with sample data if needed
        if (chatHistoryModel.count === 0) {
            chatHistoryModel.append({
                id: "welcome",
                title: "Welcome to Alyssa AI",
                messageCount: 1,
                timestamp: new Date().toLocaleDateString()
            });
        }

        // Initialize API connection
        apiManager.checkConnection()
    }
}
