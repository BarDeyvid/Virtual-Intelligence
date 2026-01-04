// Storage.qml
import QtQuick 2.15
import QtQuick.LocalStorage 2.15

QtObject {
    id: storage

    property string dbName: "AlyssaAISettings"
    property string dbVersion: "1.0"
    property string dbDescription: "Application Settings"
    property int dbEstimatedSize: 10000

    function init() {
        var db = LocalStorage.openDatabaseSync(dbName, dbVersion, dbDescription, dbEstimatedSize)
        db.transaction(function(tx) {
            tx.executeSql('CREATE TABLE IF NOT EXISTS settings(key TEXT UNIQUE, value TEXT)')
        })
    }

    Component.onCompleted: init()

    function setValue(key, value) {
        var db = LocalStorage.openDatabaseSync(dbName, dbVersion, dbDescription, dbEstimatedSize)
        db.transaction(function(tx) {
            var result = tx.executeSql('SELECT value FROM settings WHERE key=?', [key])
            if (result.rows.length > 0) {
                tx.executeSql('UPDATE settings SET value=? WHERE key=?', [value.toString(), key])
            } else {
                tx.executeSql('INSERT INTO settings VALUES(?, ?)', [key, value.toString()])
            }
        })
    }

    function value(key, defaultValue) {
        var db = LocalStorage.openDatabaseSync(dbName, dbVersion, dbDescription, dbEstimatedSize)
        var result = ""
        db.transaction(function(tx) {
            var rs = tx.executeSql('SELECT value FROM settings WHERE key=?', [key])
            if (rs.rows.length > 0) {
                result = rs.rows.item(0).value
            } else {
                result = defaultValue.toString()
            }
        })
        return result
    }

    function remove(key) {
        var db = LocalStorage.openDatabaseSync(dbName, dbVersion, dbDescription, dbEstimatedSize)
        db.transaction(function(tx) {
            tx.executeSql('DELETE FROM settings WHERE key=?', [key])
        })
    }

    function clear() {
        var db = LocalStorage.openDatabaseSync(dbName, dbVersion, dbDescription, dbEstimatedSize)
        db.transaction(function(tx) {
            tx.executeSql('DELETE FROM settings')
        })
    }
}
