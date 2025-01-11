#include "thread/ThreadPool.h"
#include <webui.hpp>
#undef min
#undef max
#include <chrono>
#include <date/date.h>
#include <format>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

template<typename ... Args>
std::string concatEven(std::string firstString, std::string secondString, std::string str, Args... args);

template<typename ... Args>
std::string concatOdd(std::string firstString, std::string secondString, std::string str, Args... args) {
    return str + firstString + concatEven(firstString, secondString, args ...);
}

template<>
std::string concatOdd(std::string firstString, std::string secondString, std::string str) {
    return str + firstString;
}

template<typename ... Args>
std::string concatEven(std::string firstString, std::string secondString, std::string str, Args... args) {
    return str + secondString + concatOdd(firstString, secondString, args ...);
}

template<>
std::string concatEven(std::string firstString, std::string secondString, std::string str) {
    return str + secondString;
}

// 生成自定义事件执行脚本
template<typename ... Args>
std::string generateCustomEvent(std::string className, std::string ev, Args... args) {
    std::string str = "for(e of document.getElementsByClassName('" + className + "')){";
    str += "e.dispatchEvent(new CustomEvent('" + ev + "', { detail: { ";

    str += concatOdd(": '", "', ", args ...);

    str += " }, bubbles: false, cancelable: true }));";
    str += "}";
    return str;
}

int64_t count = 0;
thread::ThreadPool pool;
bool running = true;
webui::window myWindow;
std::function<void()> timeSend = []() {
        if (running) {
            // 获取当前时间
            auto now = std::chrono::system_clock::now() + std::chrono::hours(8);
            // 转换为系统时间
            auto currentTime = date::floor<std::chrono::seconds>(now);
            // 设置自定义格式
            std::ostringstream oss;
            oss << date::format("%Y-%m-%d %H:%M:%S", currentTime);
            myWindow.run(generateCustomEvent("receiveCppData", "receive-data", "message", "当前时间是：" + oss.str()));
            pool.addDelayTask(std::chrono::seconds(1), timeSend);
        }
    };
nlohmann::json recipeJson = nlohmann::json::array();
nlohmann::json targetItemJson = nlohmann::json::array();
std::function<void()> saveData = []() {
        std::ofstream outfile("data.json");
        if (outfile) {
            nlohmann::json json = { { "recipe", recipeJson }, { "targetItem", targetItemJson } };
            outfile << json.dump();
        }
    };
int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    webui::set_default_root_folder("dist");

    // myWindow.bind("", [](webui::window::event *e) {
    //     if (e->event_type == WEBUI_EVENT_CONNECTED) {
    //         pool.addDelayTask(std::chrono::seconds(1), timeSend);
    //         std::cout << "Connected." << std::endl;
    //     } else if (e->event_type == WEBUI_EVENT_DISCONNECTED) {
    //         running = false;
    //         std::cout << "Disconnected." << std::endl;
    //     } else if (e->event_type == WEBUI_EVENT_MOUSE_CLICK) {
    //         std::cout << "Click." << std::endl;
    //     } else if (e->event_type == WEBUI_EVENT_NAVIGATION) {
    //         std::cout << "Starting navigation to: " << e->get_string_view() << std::endl;
    //     }
    // });
    std::ifstream f("data.json");
    if (f.is_open()) {
        nlohmann::json parseJson = nlohmann::json::parse(f);
        if (parseJson.contains("recipe")) {
            auto& recipeList = parseJson["recipe"];
            if (recipeList.is_array()) {
                for (auto& recipe : recipeList) {
                    nlohmann::json ingredientJson = nlohmann::json::array();
                    if (recipe["ingredients"].is_array()) {
                        for (auto& ingredient : recipe["ingredients"]) {
                            ingredientJson.emplace_back(nlohmann::json{ { "id", ingredient["id"].get_ref<const std::string&>() }, { "count", ingredient["count"].get_ref<int64_t&>() } });
                        }
                    }
                    recipeJson.emplace_back(nlohmann::json{ { "id", recipe["id"].get_ref<const std::string&>() }, { "count", recipe["count"].get_ref<int64_t&>() }, { "ingredients", ingredientJson } });
                }
            }
            auto& targetItem = parseJson["targetItem"];
            if (targetItem.is_array()) {
                for (auto& e : targetItem) {
                    targetItemJson.emplace_back(e.get_ref<const std::string&>());
                }
            }
        }
    }
    myWindow.bind("getRecipeData", [](webui::window::event *e) {
        e->return_string(recipeJson.dump());
    });
    myWindow.bind("getTargetItemData", [](webui::window::event *e) {
        e->return_string(targetItemJson.dump());
    });
    myWindow.bind("addTargetItem", [](webui::window::event *e) {
        auto exist = false;
        auto name = e->get_string_view(0);
        for (auto& json : targetItemJson) {
            if (json.get_ref<const std::string&>() == name) {
                exist = true;
                break;
            }
        }
        if (!exist) {
            targetItemJson.emplace_back(e->get_string_view(0));
        }
        e->return_string(targetItemJson.dump());
        saveData();
    });
    myWindow.bind("addRecipe", [](webui::window::event *e) {
        recipeJson.emplace_back(nlohmann::json::parse(e->get_string_view(0)));
        e->return_string(recipeJson.dump());
        saveData();
    });
    myWindow.bind("updateRecipe", [](webui::window::event *e) {
        recipeJson[e->get_int(0)] = (nlohmann::json::parse(e->get_string_view(1)));
        e->return_string(recipeJson.dump());
        saveData();
    });
#ifdef _WIN32
    // 获取主屏幕的宽度和高度
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);  // 屏幕宽度
    int screenHeight = GetSystemMetrics(SM_CYSCREEN); // 屏幕高度
    myWindow.set_size(screenWidth * 5 / 6, screenHeight * 5 / 6);
#endif
    myWindow.show("index.html");
    webui::wait();
    return 0;
}

#ifdef _WIN32
// Release build
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;
    return main();
}

#endif
