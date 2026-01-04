.pragma library

// API管理器类
function ApiManager(serverUrl) {
    this.serverUrl = serverUrl || "http://localhost:8181";
    this.connected = false;
    this.modelLoaded = false;
    this.initializing = false;

    // 检查连接
    this.checkConnection = function(callback) {
        var xhr = new XMLHttpRequest();
        xhr.open("GET", this.serverUrl + "/health");
        xhr.timeout = 5000;

        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 200) {
                    try {
                        var response = JSON.parse(xhr.responseText);
                        this.connected = response.initialized !== undefined;
                        this.modelLoaded = response.initialized || false;

                        if (callback) callback(true, response);
                    } catch (e) {
                        this.connected = false;
                        this.modelLoaded = false;
                        if (callback) callback(false, e);
                    }
                } else {
                    this.connected = false;
                    if (callback) callback(false, "HTTP " + xhr.status);
                }
            }
        }.bind(this);

        xhr.onerror = function() {
            this.connected = false;
            if (callback) callback(false, "Network error");
        }.bind(this);

        xhr.send();
    };

    // 初始化模型
    this.initializeModel = function(modelPath, callback) {
        this.initializing = true;

        var xhr = new XMLHttpRequest();
        xhr.open("POST", this.serverUrl + "/initialize");
        xhr.setRequestHeader("Content-Type", "application/json");

        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                this.initializing = false;

                if (xhr.status === 200) {
                    try {
                        var response = JSON.parse(xhr.responseText);
                        this.modelLoaded = response.success;

                        if (callback) callback(true, response);
                    } catch (e) {
                        this.modelLoaded = false;
                        if (callback) callback(false, e);
                    }
                } else {
                    this.modelLoaded = false;
                    if (callback) callback(false, "HTTP " + xhr.status);
                }
            }
        }.bind(this);

        xhr.onerror = function() {
            this.initializing = false;
            this.modelLoaded = false;
            if (callback) callback(false, "Network error");
        }.bind(this);

        var data = JSON.stringify({
            base_model_path: modelPath
        });

        xhr.send(data);
    };

    // Think请求（带专家ID）
    this.think = function(input, expertId, callback) {
        this._makeRequest("/think", {
            input: input,
            expert_id: expertId || ""
        }, callback);
    };

    // Think Fusion请求（无TTS）
    this.thinkFusion = function(input, callback) {
        this._makeRequest("/think/fusion", {
            input: input
        }, callback);
    };

    // 通用请求方法
    this._makeRequest = function(endpoint, data, callback) {
        var xhr = new XMLHttpRequest();
        xhr.open("POST", this.serverUrl + endpoint);
        xhr.setRequestHeader("Content-Type", "application/json");

        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 200) {
                    try {
                        var response = JSON.parse(xhr.responseText);
                        if (callback) callback(response);
                    } catch (e) {
                        if (callback) callback({
                            success: false,
                            error: "Parse error: " + e
                        });
                    }
                } else {
                    if (callback) callback({
                        success: false,
                        error: "HTTP " + xhr.status
                    });
                }
            }
        };

        xhr.onerror = function() {
            if (callback) callback({
                success: false,
                error: "Network error"
            });
        };

        xhr.send(JSON.stringify(data));
    };
}
