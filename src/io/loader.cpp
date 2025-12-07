#include "io/loader.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>

namespace graph {

std::vector<std::string> GraphLoader::split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::unique_ptr<Graph> GraphLoader::loadFromCSV(const std::string& filename, bool directed) {
    auto graph = std::make_unique<Graph>(directed);
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Ошибка: не удалось открыть файл " << filename << std::endl;
        return nullptr;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        auto tokens = split(line, ',');
        if (tokens.size() >= 2) {
            try {
                int from = std::stoi(tokens[0]);
                int to = std::stoi(tokens[1]);
                double weight = tokens.size() >= 3 ? std::stod(tokens[2]) : 1.0;
                graph->addEdge(from, to, weight);
            } catch (const std::exception& e) {
                std::cerr << "Ошибка при парсинге строки: " << line << std::endl;
            }
        }
    }
    
    file.close();
    return graph;
}

std::unique_ptr<Graph> GraphLoader::loadFromJSON(const std::string& filename, bool directed) {
    // Упрощенная реализация JSON загрузки
    auto graph = std::make_unique<Graph>(directed);
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Ошибка: не удалось открыть файл " << filename << std::endl;
        return nullptr;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    // Простой парсер JSON (для полной реализации нужна библиотека)
    // Ищем массив edges
    size_t edgesPos = content.find("\"edges\"");
    if (edgesPos == std::string::npos) {
        std::cerr << "Ошибка: не найден массив edges в JSON" << std::endl;
        return nullptr;
    }
    
    size_t arrayStart = content.find('[', edgesPos);
    if (arrayStart == std::string::npos) return nullptr;
    
    size_t pos = arrayStart + 1;
    while (pos < content.length()) {
        size_t objStart = content.find('{', pos);
        if (objStart == std::string::npos) break;
        
        size_t objEnd = content.find('}', objStart);
        if (objEnd == std::string::npos) break;
        
        std::string obj = content.substr(objStart, objEnd - objStart + 1);
        
        // Извлечь from, to, weight
        size_t fromPos = obj.find("\"from\"");
        size_t toPos = obj.find("\"to\"");
        size_t weightPos = obj.find("\"weight\"");
        
        if (fromPos != std::string::npos && toPos != std::string::npos) {
            try {
                size_t fromValStart = obj.find(':', fromPos) + 1;
                size_t fromValEnd = obj.find_first_of(",}", fromValStart);
                int from = std::stoi(obj.substr(fromValStart, fromValEnd - fromValStart));
                
                size_t toValStart = obj.find(':', toPos) + 1;
                size_t toValEnd = obj.find_first_of(",}", toValStart);
                int to = std::stoi(obj.substr(toValStart, toValEnd - toValStart));
                
                double weight = 1.0;
                if (weightPos != std::string::npos) {
                    size_t weightValStart = obj.find(':', weightPos) + 1;
                    size_t weightValEnd = obj.find_first_of(",}", weightValStart);
                    weight = std::stod(obj.substr(weightValStart, weightValEnd - weightValStart));
                }
                
                graph->addEdge(from, to, weight);
            } catch (const std::exception& e) {
                std::cerr << "Ошибка при парсинге JSON объекта: " << obj << std::endl;
            }
        }
        
        pos = objEnd + 1;
    }
    
    return graph;
}

bool GraphLoader::saveToCSV(const Graph& g, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Ошибка: не удалось создать файл " << filename << std::endl;
        return false;
    }
    
    auto edges = g.getEdges();
    for (const auto& edge : edges) {
        file << edge.from << "," << edge.to << "," << edge.weight << "\n";
    }
    
    file.close();
    return true;
}

bool GraphLoader::saveToJSON(const Graph& g, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Ошибка: не удалось создать файл " << filename << std::endl;
        return false;
    }
    
    file << "{\n";
    file << "  \"directed\": " << (g.isDirected() ? "true" : "false") << ",\n";
    file << "  \"edges\": [\n";
    
    auto edges = g.getEdges();
    for (size_t i = 0; i < edges.size(); ++i) {
        const auto& edge = edges[i];
        file << "    { \"from\": " << edge.from 
             << ", \"to\": " << edge.to 
             << ", \"weight\": " << edge.weight << " }";
        if (i < edges.size() - 1) {
            file << ",";
        }
        file << "\n";
    }
    
    file << "  ]\n";
    file << "}\n";
    
    file.close();
    return true;
}

// Вспомогательная функция для извлечения строкового значения из JSON
static std::string extractStringValue(const std::string& json, const std::string& key, size_t startPos = 0) {
    size_t keyPos = json.find("\"" + key + "\"", startPos);
    if (keyPos == std::string::npos) return "";
    
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) return "";
    
    size_t valueStart = json.find('"', colonPos);
    if (valueStart == std::string::npos) return "";
    
    size_t valueEnd = json.find('"', valueStart + 1);
    if (valueEnd == std::string::npos) return "";
    
    return json.substr(valueStart + 1, valueEnd - valueStart - 1);
}

std::unique_ptr<Graph> GraphLoader::loadFromKnowledgeGraph(const std::string& filename, bool directed) {
    auto graph = std::make_unique<Graph>(directed);
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Ошибка: не удалось открыть файл " << filename << std::endl;
        return nullptr;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    // Маппинг между строковыми ID (e1, e2...) и числовыми ID
    std::unordered_map<std::string, int> entityIdMap;
    std::unordered_map<int, std::string> entityNames;  // числовой ID -> имя сущности
    
    // Искать entities внутри knowledgeGraph, если есть
    size_t searchStart = 0;
    size_t kgPos = content.find("\"knowledgeGraph\"");
    if (kgPos != std::string::npos) {
        searchStart = kgPos;  // Искать entities внутри knowledgeGraph
    }
    
    // Парсинг entities
    size_t entitiesPos = content.find("\"entities\"", searchStart);
    if (entitiesPos != std::string::npos) {
        size_t arrayStart = content.find('[', entitiesPos);
        if (arrayStart != std::string::npos) {
            size_t pos = arrayStart + 1;
            int numericId = 1;  // Начинаем с 1
            
            while (pos < content.length()) {
                size_t objStart = content.find('{', pos);
                if (objStart == std::string::npos) break;
                
                // Найти конец объекта (учитывая вложенные объекты)
                int braceCount = 0;
                size_t objEnd = objStart;
                for (size_t i = objStart; i < content.length(); ++i) {
                    if (content[i] == '{') braceCount++;
                    if (content[i] == '}') braceCount--;
                    if (braceCount == 0) {
                        objEnd = i;
                        break;
                    }
                }
                if (objEnd == objStart) break;
                
                std::string obj = content.substr(objStart, objEnd - objStart + 1);
                
                // Извлечь id сущности
                std::string entityId = extractStringValue(obj, "id");
                if (!entityId.empty()) {
                    entityIdMap[entityId] = numericId;
                    
                    // Извлечь name сущности
                    std::string entityName = extractStringValue(obj, "name");
                    if (!entityName.empty()) {
                        entityNames[numericId] = entityName;
                        graph->addVertex(numericId, entityName);
                    } else {
                        graph->addVertex(numericId, entityId);
                    }
                    
                    numericId++;
                }
                
                pos = objEnd + 1;
            }
        }
    }
    
    // Парсинг relationships
    size_t relationshipsPos = content.find("\"relationships\"", searchStart);
    if (relationshipsPos != std::string::npos) {
        size_t arrayStart = content.find('[', relationshipsPos);
        if (arrayStart != std::string::npos) {
            size_t pos = arrayStart + 1;
            
            while (pos < content.length()) {
                size_t objStart = content.find('{', pos);
                if (objStart == std::string::npos) break;
                
                // Найти конец объекта
                int braceCount = 0;
                size_t objEnd = objStart;
                for (size_t i = objStart; i < content.length(); ++i) {
                    if (content[i] == '{') braceCount++;
                    if (content[i] == '}') braceCount--;
                    if (braceCount == 0) {
                        objEnd = i;
                        break;
                    }
                }
                if (objEnd == objStart) break;
                
                std::string obj = content.substr(objStart, objEnd - objStart + 1);
                
                // Извлечь source, target и type
                std::string sourceId = extractStringValue(obj, "source");
                std::string targetId = extractStringValue(obj, "target");
                std::string relationshipType = extractStringValue(obj, "type");
                
                if (!sourceId.empty() && !targetId.empty() && entityIdMap.find(sourceId) != entityIdMap.end() 
                    && entityIdMap.find(targetId) != entityIdMap.end()) {
                    int from = entityIdMap[sourceId];
                    int to = entityIdMap[targetId];
                    
                    // Использовать тип связи как метку ребра
                    graph->addEdge(from, to, 1.0, relationshipType);
                }
                
                pos = objEnd + 1;
            }
        }
    }
    
    std::cout << "Загружено сущностей: " << entityIdMap.size() 
              << ", связей: " << graph->getEdgeCount() << std::endl;
    
    return graph;
}

} // namespace graph

