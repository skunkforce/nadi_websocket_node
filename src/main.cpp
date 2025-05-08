#include <format>
#include <string>
#include <nlohmann/json.hpp>
#include <print>
#include <mutex>
#include <optional>
#include <unordered_set>
#include "nadi.h"
#include "crow.h"
#include "crow/middlewares/cors.h"

constexpr int default_port = 9090;

class websocket_t {
    std::optional<std::jthread> websocket_;
    crow::App<crow::CORSHandler> crow_app_;
    void thread(int port){
        std::mutex mtx;
        std::unordered_set<crow::websocket::connection*> users;
        std::unordered_map<crow::websocket::connection*, std::atomic<bool>> thread_control_flags;
        bool json_temp = false;
        bool ws_temp = true;
        std::string file_temp = " ";
    
        auto& cors = crow_app_.get_middleware<crow::CORSHandler>();
    
        cors
        .global()
        .origin("*") // Erlaube Anfragen von allen Domains (Browser-freundlich)
        .headers("Origin", "Content-Type", "Accept", "X-Custom-Header", "Authorization") // Alle relevanten Header
        .methods("POST"_method, "GET"_method, "OPTIONS"_method) // Erlaube POST, GET und OPTIONS (für Preflight)
        .max_age(600); // Cache Preflight-Antworten für 10 Minuten
        // clang-format on
    
        // OPTIONS-Endpunkt, um Preflight-Anfragen zu behandeln
        CROW_ROUTE(crow_app_, "/cors").methods("OPTIONS"_method)([]() {
            return crow::response(204); // Antwort ohne Inhalt
        });
    
        // Websocket
        CROW_WEBSOCKET_ROUTE(crow_app_, "/ws")
        .onopen([&](crow::websocket::connection& conn) {
            //websocketConnectionActive = true;
            CROW_LOG_INFO << "new websocket connection from " << conn.get_remote_ip();
            std::lock_guard<std::mutex> _(mtx);
            users.insert(&conn);
            conn.send_text("Hello, connection to websocket established. To start a measurement send the wished UUID, optional send a sampling rate between 10 and 100000");
        })
        .onclose([&](crow::websocket::connection& conn, const std::string& reason) {
            //websocketConnectionActive = false;
            CROW_LOG_INFO << "websocket connection closed. Your measurement was stopped. " << reason;
            std::lock_guard<std::mutex> _(mtx);
            users.erase(&conn);
        })
        .onmessage([&](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
            CROW_LOG_INFO << "Received message: " << data;
            /*std::lock_guard<std::mutex> _(mtx);
            auto json_msg = nlohmann::json::parse(data); //TODO handle multiple 
            if(json_msg.contains("command") && json_msg["command"].is_string()){
                const std::string cmd = json_msg["command"];
                if(cmd == "get_downsampled_in_range" && json_msg.contains("tmin") && json_msg.contains("tmax") && json_msg.contains("desired_number_of_samples")){
                    int64_t tmin = json_msg["tmin"];
                    int64_t tmax = json_msg["tmax"];
                    conn.send_text(handle_get_downsampled_in_range(tmin,tmax,json_msg["desired_number_of_samples"]));
                }
    
            }*/
        });
        crow_app_.signal_clear();
    
        crow_app_.port(port).multithreaded().run();
    }
    public:
    void start(int port){
        websocket_ = std::jthread([this,port](){
            thread(port);
        });
    }
};

class main_t{
    std::mutex lock_;
    std::optional<std::jthread> thread_;
    websocket_t websocket_;
    nadi_receive_callback receive_;
    nadi_status handle_management(nadi_message* message){
        auto json = nlohmann::json::parse(message->data);
        if(json.contains("command")){
            auto command  = json["command"];
            if(command == "open_websocket"){
                int port = default_port;
                if(json.contains("port")){
                    auto port_str = json["port"].dump();
                    int p;
                    auto [ptr, ec] = std::from_chars(port_str.c_str(),port_str.c_str() + port_str.size(),p);
                    if (ec == std::errc())
                        port = p;
                    else if (ec == std::errc::invalid_argument){}
                        //std::cout << "This is not a number.\n";
                    else if (ec == std::errc::result_out_of_range){}
                        //std::cout << "This number is larger than an int.\n";
                        
                }
                websocket_.start(port);
            }
        }
        return 0;
    }
    public:
    main_t(nadi_receive_callback cb):receive_{cb}{}
    nadi_status send(nadi_message* message){
        try{
            switch (message->channel){
                case 0x8000:
                return handle_management(message);
                default:
                return 0;
            }
        }
        catch(...){

        }
        return 0;
    }
    void free(nadi_message* message){
        delete[] message->meta;
        delete[] message->data;
        delete message;
    }
};



extern "C" {
    DLL_EXPORT nadi_status nadi_init(nadi_instance_handle* instance, nadi_receive_callback cb){
        *instance = new main_t(cb);
        return 0;
    }

    DLL_EXPORT nadi_status nadi_deinit(nadi_instance_handle instance){
        delete instance;
        return 0;
    }

    DLL_EXPORT nadi_status nadi_send(nadi_message* message, nadi_instance_handle instance){
        static_cast<main_t*>(instance)->send(message);
        return 0;
    }

    DLL_EXPORT void nadi_free(nadi_message* message){
        static_cast<main_t*>(message->instance)->free(message);
    }

    DLL_EXPORT const char* nadi_descriptor(){
        return R"({"name":"nadi-websocket-node","version":"1.0"})";
    }
}
