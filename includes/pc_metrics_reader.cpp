#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <regex>
#include <cstdio>
#include <iomanip>
#include <unistd.h> 

#include "json.hpp"
using json = nlohmann::json;

class PCMetricsReader {
private:
    struct CpuTime {
        long long total_time = 0;
        long long idle_time = 0;
    };
    CpuTime prev_cpu_time;
    
    std::string execute_command(const std::string& command) {
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) return "";

        char buffer[128];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
        return result;
    }

    float extract_temperature(const std::string& data, const std::string& label) {
        std::regex temp_regex("(" + label + "):\\s+\\+(\\d+\\.\\d+)°C");
        std::smatch match;

        if (std::regex_search(data, match, temp_regex) && match.size() > 2) {
            try {
                return std::stof(match[2].str());
            } catch (const std::exception& e) {
                return -1.0f;
            }
        }
        return -1.0f; 
    }

public:
    PCMetricsReader() {
        update_cpu_time(prev_cpu_time);
    }

    float get_cpu_temp() {
        std::string sensor_data = execute_command("sensors");
        if (sensor_data.empty()) return -1.0f;

        float temp = extract_temperature(sensor_data, "Tctl");

        if (temp == -1.0f) {
            std::regex fallback_regex("(Core 0|temp1):\\s+\\+(\\d+\\.\\d+)°C");
            std::smatch match;
            if (std::regex_search(sensor_data, match, fallback_regex) && match.size() > 2) {
                try {
                    temp = std::stof(match[2].str());
                } catch (...) {
                    return -1.0f;
                }
            }
        }
        
        return temp;
    }

    float get_ram_usage() {
        std::ifstream file("/proc/meminfo");
        if (!file.is_open()) return -1.0f;

        std::string line;
        long long total = 0, available = 0;
        
        while (std::getline(file, line)) {
            if (line.rfind("MemTotal:", 0) == 0) { 
                std::stringstream ss(line);
                std::string label;
                ss >> label >> total;
            } else if (line.rfind("MemAvailable:", 0) == 0) { 
                std::stringstream ss(line);
                std::string label;
                ss >> label >> available;
            }
            if (total > 0 && available > 0) break;
        }

        if (total > 0) {
            long long used = total - available;
            return (float)used / total * 100.0f;
        }
        return -1.0f;
    }

    void update_cpu_time(CpuTime& current_time) {
        std::ifstream file("/proc/stat");
        if (!file.is_open()) return;

        std::string line;
        std::getline(file, line); 
        file.close();

        std::stringstream ss(line);
        std::string label;
        long long user, nice, system, idle, iowait, irq, softirq, softirq_val;

        if (!(ss >> label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> softirq_val)) return;

        current_time.idle_time = idle + iowait; 
        current_time.total_time = user + nice + system + current_time.idle_time + irq + softirq + softirq_val; 
    }

    float get_cpu_usage() {
        CpuTime current_cpu_time;
        update_cpu_time(current_cpu_time);

        if (prev_cpu_time.total_time == 0 || current_cpu_time.total_time == 0) {
            prev_cpu_time = current_cpu_time;
            return 0.0f; 
        }

        long long total_delta = current_cpu_time.total_time - prev_cpu_time.total_time;
        long long idle_delta = current_cpu_time.idle_time - prev_cpu_time.idle_time;

        prev_cpu_time = current_cpu_time;
        
        if (total_delta == 0) return 0.0f;

        return 100.0f * (1.0f - (float)idle_delta / total_delta);
    }

    /**
     * @brief Novo método para compilar todas as métricas em um objeto.
     * @return Uma string com todas as métricas.
     */
    std::string get_simple_metrics_text() {
        float cpu_usage = get_cpu_usage();
        float cpu_usage_rounded = (cpu_usage != 0.0f) ? std::round(cpu_usage * 10) / 10.0f : -1.0f;

        float cpu_temp = get_cpu_temp();
        float cpu_temp_rounded = std::round(cpu_temp * 10) / 10.0f;

        float ram_usage = get_ram_usage();
        float ram_usage_rounded = std::round(ram_usage * 10) / 10.0f;

        std::string sensor_data = execute_command("sensors");
        float nvme_temp = extract_temperature(sensor_data, "Composite");
        float nvme_temp_rounded = std::round(nvme_temp * 10) / 10.0f;

        std::stringstream ss;
        ss << "\n[DADOS DO AMBIENTE]";
        
        float temp_media = (cpu_temp_rounded + nvme_temp_rounded) / 2.0f;
        float uso_medio = (cpu_usage_rounded + ram_usage_rounded) / 2.0f;

        ss << "\n- Temperatura média do hardware: " << temp_media << " °C";
        ss << "\n- Uso médio de recursos (CPU/RAM): " << uso_medio << " %";
        ss << "\n[FIM DADOS DO AMBIENTE]\n";

        return ss.str();
    };
};

/**
 * @brief Função principal para inicializar e coletar as métricas em JSON.
 */
/** 
 int ReaderShowOff() {
    PCMetricsReader reader;
    
    reader.get_cpu_usage(); 
    std::cout << "--- Inicialização: Coletando Primeiro Estado da CPU ---" << std::endl;
    
    usleep(1000000);
    
    std::cout << "--- Coleta Finalizada ---" << std::endl;
    
    std::string json_data = reader.get_all_metrics_json();
    
    std::cout << "\n--- JSON de Métricas ---" << std::endl;
    std::cout << json_data << std::endl;
    std::cout << "------------------------" << std::endl;
    
    return 0;
}
**/