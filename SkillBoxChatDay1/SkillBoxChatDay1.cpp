#include <iostream>
#include <uwebsockets/App.h>
#include <string>
#include <algorithm>
#include <map>
#include <regex>
using namespace std;

// Протокол общения с сервером:
// 1. Пользователь сообщает свое имя
// SET_NAME::Mike 

// 2. Пользователь может кому-то написать
// DIRECT::user_id::message_text
// DIRECT::1::Привет, Майк!
// DIRECT::2::И тебе привет, Джон!

// 3. Подключение/отключение
// ONLINE::1::Mike
// OFFLINE::1::Mike

const string SET_NAME = "SET_NAME::";
const string DIRECT = "DIRECT::";

map<string, string> knowledge = { // База вопросов и ответов
    {"hello", "Oh, hello hooman!"}, // {"вопрос", "ответ"}
    {"how are you", "Not too bad for a machine"},
    {"what is your name", "My creator gave me name ChatBot3000"},
    {"what are you up to", "Answering stupid question"},
};

bool isSetNameCommand(string_view message) {
    return message.find(SET_NAME) == 0;
}

string parseName(string_view message) {
    return string(message.substr(SET_NAME.length()));
}

string parseRecieverId(string_view message) {
    string_view rest = message.substr(DIRECT.length());
    int pos = rest.find("::");
    string_view id = rest.substr(0, pos);
    return string(id);
}

string parseDirectMessage(string_view message) {
    string_view rest = message.substr(DIRECT.length());
    int pos = rest.find("::");
    string_view text = rest.substr(pos + 2); 
    return string(text);
}

bool isDirectCommand(string_view message) {
    return message.find(DIRECT) == 0;
}
string to_lower(string text) {
    transform(text.begin(), text.end(), text.begin(), ::tolower);
    return text;
}
string botAnswer(string question) {
    bool foundAnswer = false; // Найден ли ответ
    for (auto entry : knowledge) { // Для каждой записи в базе:
        // entry.first - вопрос
        // entry.second - ответ
        regex expression = regex(".*" + entry.first + ".*");
        if (regex_match(question, expression)) {
            // Дать ответ!
            return entry.second;
            foundAnswer = true;
        }
    }
    if (!foundAnswer) { // Если не найден ответ
        return "Do not comprende";
    }
}

int main() {
    struct PerSocketData {
        int user_id; 
        string name; 
    };

    vector<int> userIds = {1};
    int TotalUserCount = 0;

    uWS::App().ws<PerSocketData>("/*",
            {
                .compression = uWS::SHARED_COMPRESSOR,
                .maxPayloadLength = 16 * 1024,
                .idleTimeout = 600, 
                .maxBackpressure = 1 * 1024 * 1024,

                .upgrade = nullptr,
                .open = [&userIds](auto* connection) {
                    cout << "New connection created\n";
                    PerSocketData* userData = (PerSocketData*)connection->getUserData();
                    auto maxElem = *max_element(userIds.begin(), userIds.end());
                    userData->user_id = maxElem + 1;
                    userIds.push_back(maxElem + 1);
                    userData->name = "UNNAMED";

                    cout << "Total users connected: " << userIds.size() - 1 << endl;

                    connection->subscribe("broadcast"); 
                    connection->subscribe("user#" + to_string(userData->user_id));
                },
                .message = [&userIds](auto* connection, string_view message, uWS::OpCode opCode) {
                    cout << "New message recieved: \n" << message << "\n";
                    PerSocketData* userData = (PerSocketData*)connection->getUserData();
                    if (isSetNameCommand(message)) 
                    {
                        cout << "User set their name\n";
                        auto name = parseName(message);
                        if (name.find("::") != -1 || name.size() > 255)
                        {
                            connection->send("Invalid name", opCode, true);
                        }
                        else 
                        {
                            userData->name = name;
                        }
                    }
                    if (isDirectCommand(message))
                    {
                        cout << "User sent direct message\n";
                        string id = parseRecieverId(message);
                        string text = parseDirectMessage(message);
                        if (id == "1")
                            connection->send(botAnswer(to_lower(string(message))), opCode, true);
                        for (int i = 0; i <= userIds.size(); i++)
                        {
                            if (i == userIds.size())
                            {
                                cout << "Error, there is no user with ID = " << id;
                                connection->send("Error, there is no user with ID = " + id, opCode, true);
                                break;
                            }
                            if (stoi(id) == userIds[i])
                            {
                                connection->publish("user#" + id, text);
                                break;
                            }
                        }
                    }
                    
                },
                .close = [&userIds](auto* connection, int /*code*/, std::string_view /*message*/) {
                    cout << "Connection closed\n";
                    PerSocketData* userData = (PerSocketData*)connection->getUserData();
                    for(int i = 0; i < userIds.size(); i++)
                    {
                        if (userData->user_id == userIds[i])
                        {
                            userIds.erase(userIds.begin() + i);
                            break;
                        }
                    }
                }
            }).listen(9001, [](auto* listen_socket) {
                    if (listen_socket) {
                        std::cout << "Listening on port " << 9001 << std::endl;
                    }
                }).run();
}


