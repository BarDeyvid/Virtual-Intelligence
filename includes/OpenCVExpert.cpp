// OpenCVExpert.cpp

std::string alyssa_experts::OpenCVExpert::run(
    const std::string& input,
    alyssa_core::AlyssaCore* core_instance,
    llama_adapter_lora* lora_override,
    std::vector<llama_chat_message>& current_history,
    llama_adapter_lora** active_lora_in_context,
    std::function<void(const std::string&)> stream_callback
) {
    // 1️⃣ Parse command – we keep the same “camera / filtro / faces” logic.
    std::string cmd = input;
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    std::ostringstream out;

    // 2️⃣ Perform OpenCV work (image loading, filtering, face detection, …)
    const std::string img_path = "input.jpg";   // In production you would receive a path
    cv::Mat img = cv::imread(img_path);
    if (img.empty()) {
        out << "Imagem não encontrada: " << img_path;
    } else if (cmd.find("filtro") != std::string::npos) {
        cv::GaussianBlur(img, img, cv::Size(15, 15), 0);
        cv::imwrite("blurred.jpg", img);
        out << "Filtro gaussiano aplicado. Resultado salvo em blurred.jpg.";
    } else if (cmd.find("faces") != std::string::npos) {
        cv::CascadeClassifier face_cascade;
        face_cascade.load("haarcascade_frontalface_default.xml");
        std::vector<cv::Rect> faces;
        cv::Mat gray; cvtColor(img, gray, cv::COLOR_BGR2GRAY);
        face_cascade.detectMultiScale(gray, faces);
        out << "Foram detectadas " << faces.size() << " faces na imagem.";
    } else if (cmd.find("camera") != std::string::npos) {
        cv::VideoCapture cap(0);
        if (!cap.isOpened())
            out << "Não consegui abrir a câmera.";
        else
            out << "Câmera aberta com sucesso. (Pressione Esc para sair)";
    } else {
        out << "Comando desconhecido. Tente \"camera\", \"filtro\" ou \"faces\".";
    }

    // 3️⃣ Convert result to the LLM‑friendly text that will be fused
    std::string raw_response = out.str();

    // 4️⃣ Store in history (so other experts can see it)
    char* msg = strdup(raw_response.c_str());
    current_history.push_back({"assistant", msg});

    // 5️⃣ If a stream callback is supplied, forward the whole text at once
    if (stream_callback) stream_callback(raw_response);

    return raw_response;
}

alyssa_fusion::ExpertContribution alyssa_experts::OpenCVExpert::get_contribution(
    const std::string& input,
    alyssa_core::AlyssaCore* core_instance,
    std::shared_ptr<Embedder> embedder,
    llama_adapter_lora* lora_override,
    std::vector<llama_chat_message>& current_history,
    llama_adapter_lora** active_lora_in_context,
    std::function<void(const std::string&)> stream_callback
) {
    alyssa_fusion::ExpertContribution contrib;
    contrib.expert_id = expert_id;

    // Run the normal execution path – this will also write to history.
    std::string raw_response = run(input, core_instance, lora_override,
                                   current_history, active_lora_in_context,
                                   stream_callback);

    // Parse into a *signal* that the fusion engine can ignore
    contrib.response = parse_expert_signal(raw_response, expert_id);
    contrib.source   = expert_id;

    if (embedder) {
        try { contrib.embedding = embedder->generate_embedding(contrib.response); }
        catch (...) {}
    }

    return contrib;
}
