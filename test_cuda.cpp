#include <opencv2/opencv.hpp>
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

    while (true) {
        // Capture a new frame
        cap >> frame; // Or use cap.read(frame);

        // Check if the frame is empty (e.g., if the video stream ended)
        if (frame.empty()) {
            std::cerr << "Error: Blank frame grabbed." << std::endl;
            break;
        }

        // Display the frame in a window named "Camera Feed"
        cv::imshow("Camera Feed", frame);

        // Wait for 1 millisecond for a key press
        // If the 'Esc' key (ASCII value 27) is pressed, break the loop
        if (cv::waitKey(1) == 27) {
            break;
        }
    }

    // Release the camera resource and destroy all OpenCV windows
    cap.release();
    cv::destroyAllWindows();

    return 0;
}
