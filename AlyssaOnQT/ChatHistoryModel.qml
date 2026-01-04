import QtQuick 2.15

ListModel {
    id: chatHistoryModel

    function append(item) {
        var exists = false
        for (var i = 0; i < count; i++) {
            if (get(i).id === item.id) {
                exists = true
                break
            }
        }

        if (!exists) {
            insert(0, item)
        }
    }

    function update(index, item) {
        if (index >= 0 && index < count) {
            for (var key in item) {
                setProperty(index, key, item[key])
            }
        }
    }

    function getIndexById(id) {
        for (var i = 0; i < count; i++) {
            if (get(i).id === id) {
                return i
            }
        }
        return -1
    }

    function removeById(id) {
        var index = getIndexById(id)
        if (index !== -1) {
            remove(index)
        }
    }
}
