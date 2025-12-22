#include <opencv2/opencv.hpp>
#include <opencv2/cudaimgproc.hpp> // Header para filtros na GPU
#include <iostream>

int main() {
    // Open the default camera (index 0).
    // If you have multiple cameras, you can try indices 1, 2, etc.
    cv::VideoCapture cap(0);

    // Check if the camera opened successfully
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open camera." << std::endl;
        return -1;
    }

    std::cout << "Camera opened successfully. Press 'Esc' to exit." << std::endl;

    cv::Mat frame; // Matrix to store each frame

    cv::cuda::GpuMat gpuFrame, gpuGray; 

    while (true) {
        cap >> frame;
        if (frame.empty()) break;

        // 1. Upload para a VRAM da RTX 5060 Ti
        gpuFrame.upload(frame);

        // 2. Processamento na GPU (ex: converter para cinza)
        cv::cuda::cvtColor(gpuFrame, gpuGray, cv::COLOR_BGR2GRAY);

        // 3. Download de volta para a RAM (apenas para exibir)
        cv::Mat result;
        gpuGray.download(result);

        cv::imshow("Feed Acelerado pela GPU", result);

        if (cv::waitKey(1) == 27) break;
    }

    // Release the camera resource and destroy all OpenCV windows
    cap.release();
    cv::destroyAllWindows();

    return 0;
}
