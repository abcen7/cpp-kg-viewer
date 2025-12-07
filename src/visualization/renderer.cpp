#include "visualization/renderer.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <filesystem>

namespace graph {

GraphRenderer::GraphRenderer(sf::RenderWindow& window) 
    : window_(window) {
    view_ = sf::View(sf::FloatRect({0, 0}, {static_cast<float>(window_.getSize().x), static_cast<float>(window_.getSize().y)}));
    
    // Попытаться загрузить системный шрифт
    loadFont();
}

void GraphRenderer::loadFont() {
    // Попробовать загрузить шрифт из стандартных системных путей
    std::vector<std::string> fontPaths = {
        // macOS системные шрифты (приоритет .ttf файлам)
        "/System/Library/Fonts/Geneva.ttf",
        "/System/Library/Fonts/SFNSMono.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Courier New.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",  // Попробуем и .ttc
        // Linux системные шрифты
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        // Windows системные шрифты (если нужно)
        "C:/Windows/Fonts/arial.ttf",
    };
    
    for (const auto& path : fontPaths) {
        if (std::filesystem::exists(path)) {
            font_.emplace();
            if (font_->openFromFile(path)) {
                fontLoaded_ = true;
                std::cout << "Шрифт загружен из: " << path << std::endl;
                return;
            }
        }
    }
    
    // Если не удалось загрузить, попробуем использовать встроенный механизм SFML
    // В SFML 3 можно попробовать использовать системный шрифт через другую функцию
    std::cout << "Предупреждение: не удалось загрузить шрифт, текст на рёбрах не будет отображаться" << std::endl;
}

void GraphRenderer::render(const Graph& g, const AlgorithmState& state, sf::RenderTarget& target) {
    // Отладочный вывод
    static int renderCallCount = 0;
    renderCallCount++;
    if (renderCallCount <= 3) {
        std::cout << "[Renderer] render() вызван #" << renderCallCount 
                  << ", вершин в графе: " << g.getVertexCount() << std::endl;
    }
    
    // Убедиться, что view правильно настроен
    if (view_.getSize().x == 0 || view_.getSize().y == 0) {
        view_ = sf::View(sf::FloatRect({0, 0}, {static_cast<float>(window_.getSize().x), static_cast<float>(window_.getSize().y)}));
    }
    
    // Сохранить текущий view и установить наш
    sf::View originalView = target.getView();
    target.setView(view_);
    
    // Отрисовать рёбра
    auto edges = g.getEdges();
    for (const auto& edge : edges) {
        bool isInPath = std::find(state.path.begin(), state.path.end(), edge.from) != state.path.end() &&
                       std::find(state.path.begin(), state.path.end(), edge.to) != state.path.end();
        
        // Проверить, является ли ребро частью пути
        bool isPathEdge = false;
        if (isInPath) {
            auto fromIt = std::find(state.path.begin(), state.path.end(), edge.from);
            auto toIt = std::find(state.path.begin(), state.path.end(), edge.to);
            if (fromIt != state.path.end() && toIt != state.path.end()) {
                int fromIdx = std::distance(state.path.begin(), fromIt);
                int toIdx = std::distance(state.path.begin(), toIt);
                isPathEdge = std::abs(fromIdx - toIdx) == 1;
            }
        }
        
        sf::Color edgeColor = isPathEdge ? pathColor_ : edgeColor_;
        drawEdge(g, edge.from, edge.to, edgeColor, target);
    }
    
    // Отрисовать вершины
    auto vertices = g.getVertices();
    for (int id : vertices) {
        bool isVisited = std::find(state.visited.begin(), state.visited.end(), id) != state.visited.end();
        bool isCurrent = (state.currentVertex == id);
        bool isInPath = std::find(state.path.begin(), state.path.end(), id) != state.path.end();
        
        sf::Color color = vertexColor_;
        if (isCurrent) {
            color = currentColor_;
        } else if (isInPath) {
            color = pathColor_;
        } else if (isVisited) {
            color = visitedColor_;
        }
        
        drawVertex(g, id, color, target);
        drawLabel(g, id, target);
    }
    
    // Восстановить оригинальный view
    target.setView(originalView);
}

void GraphRenderer::drawEdge(const Graph& g, int from, int to, const sf::Color& color, sf::RenderTarget& target) {
    const Vertex* v1 = g.getVertex(from);
    const Vertex* v2 = g.getVertex(to);
    if (!v1 || !v2) return;
    
    // Получить ребро для извлечения метки
    Edge edge = g.getEdge(from, to);
    
    sf::VertexArray lineArray(sf::PrimitiveType::Lines, 2);
    lineArray[0].position = sf::Vector2f(static_cast<float>(v1->x), static_cast<float>(v1->y));
    lineArray[0].color = color;
    lineArray[1].position = sf::Vector2f(static_cast<float>(v2->x), static_cast<float>(v2->y));
    lineArray[1].color = color;
    
    target.draw(lineArray);
    
    // Отрисовать метку ребра, если она есть
    if (!edge.label.empty()) {
        drawEdgeLabel(g, from, to, edge.label, target);
    }
}

void GraphRenderer::drawEdgeLabel(const Graph& g, int from, int to, const std::string& label, sf::RenderTarget& target) {
    const Vertex* v1 = g.getVertex(from);
    const Vertex* v2 = g.getVertex(to);
    if (!v1 || !v2 || label.empty()) return;
    
    // Вычислить середину ребра
    float midX = (static_cast<float>(v1->x) + static_cast<float>(v2->x)) / 2.0f;
    float midY = (static_cast<float>(v1->y) + static_cast<float>(v2->y)) / 2.0f;
    
    // Создать текст, если шрифт загружен
    if (fontLoaded_ && font_.has_value()) {
        sf::Text text(*font_, label, 10);  // SFML 3 требует шрифт в конструкторе
        text.setFillColor(sf::Color::Black);
        
        // Центрировать текст
        sf::FloatRect textBounds = text.getLocalBounds();
        sf::Vector2f textSize = textBounds.size;  // SFML 3: size - публичное поле
        text.setPosition({midX - textSize.x / 2.0f, midY - textSize.y / 2.0f - 2.0f});
        
        // Создать фон для текста
        float padding = 4.0f;
        float labelWidth = textSize.x + padding * 2.0f;
        float labelHeight = textSize.y + padding * 2.0f;
        
        sf::RectangleShape background(sf::Vector2f(labelWidth, labelHeight));
        background.setPosition({midX - labelWidth / 2.0f, midY - labelHeight / 2.0f});
        background.setFillColor(sf::Color(255, 255, 255, 220));  // Полупрозрачный белый
        background.setOutlineColor(sf::Color(100, 100, 100));
        background.setOutlineThickness(1.0f);
        
        // Отрисовать фон и текст
        target.draw(background);
        target.draw(text);
    } else {
        // Если шрифт не загружен, отрисовываем только фон с примерной шириной
        float labelWidth = label.length() * 6.0f;
        float labelHeight = 14.0f;
        
        sf::RectangleShape background(sf::Vector2f(labelWidth, labelHeight));
        background.setPosition({midX - labelWidth / 2.0f, midY - labelHeight / 2.0f});
        background.setFillColor(sf::Color(255, 255, 255, 200));
        background.setOutlineColor(sf::Color::Black);
        background.setOutlineThickness(1.0f);
        
        target.draw(background);
    }
}

void GraphRenderer::drawVertex(const Graph& g, int id, const sf::Color& color, sf::RenderTarget& target) {
    const Vertex* v = g.getVertex(id);
    if (!v) {
        std::cerr << "[Renderer] Вершина " << id << " не найдена!" << std::endl;
        return;
    }
    
    // Отладочный вывод для первых нескольких вершин
    static int debugCount = 0;
    if (debugCount < 3) {
        std::cout << "[Renderer] Отрисовка вершины " << id 
                  << " в позиции (" << v->x << ", " << v->y << ")" << std::endl;
        debugCount++;
    }
    
    sf::CircleShape circle(vertexRadius_);
    circle.setPosition({static_cast<float>(v->x) - vertexRadius_, 
                       static_cast<float>(v->y) - vertexRadius_});
    circle.setFillColor(color);
    circle.setOutlineColor(sf::Color::Black);
    circle.setOutlineThickness(2.0f);
    
    target.draw(circle);
}

void GraphRenderer::drawLabel(const Graph& g, int id, sf::RenderTarget& target) {
    const Vertex* v = g.getVertex(id);
    if (!v) return;
    
    (void)target;  // Пока не используется
}

void GraphRenderer::handleMouseWheel(float delta) {
    float zoomFactor = 1.0f + delta * 0.1f;
    view_.zoom(1.0f / zoomFactor);
    zoom_ *= zoomFactor;
}

void GraphRenderer::handleMouseDrag(sf::Vector2f delta) {
    view_.move(-delta);
    panOffset_ += delta;
}

void GraphRenderer::resetView() {
    view_ = sf::View(sf::FloatRect({0, 0}, {static_cast<float>(window_.getSize().x), static_cast<float>(window_.getSize().y)}));
    zoom_ = 1.0f;
    panOffset_ = sf::Vector2f(0.0f, 0.0f);
}

int GraphRenderer::getVertexAt(sf::Vector2f position, const Graph& g) const {
    auto vertices = g.getVertices();
    for (int id : vertices) {
        const Vertex* v = g.getVertex(id);
        if (!v) continue;
        
        float dx = position.x - static_cast<float>(v->x);
        float dy = position.y - static_cast<float>(v->y);
        float dist = std::sqrt(dx * dx + dy * dy);
        
        if (dist <= vertexRadius_) {
            return id;
        }
    }
    return -1;
}

} // namespace graph

