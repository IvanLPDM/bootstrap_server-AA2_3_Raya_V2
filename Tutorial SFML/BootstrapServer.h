#pragma once
#include <SFML/Network.hpp>
#include <vector>
#include <unordered_map>
#include <memory>

#include "Client.h"
#include "Room.h"
#include "DataBaseManager.h"
#include <string>
#include <cstdint>

class BootstrapServer {
public:
    static constexpr float   MATCH_START_COUNTDOWN = 10.f;
    static constexpr short MIN_PLAYERS_TO_START  = 2;
    static constexpr short MAX_ROOM_PLAYERS      = 4;
    static constexpr short ROOM_ID_LENGTH        = 6;

    BootstrapServer(unsigned short port = 55000);
    void Run();

private:

    void AcceptNewConnection();
    void ReceiveData(Client* client);
    
    void HandleCommand(Client* client, const std::string& command, const std::string& nick, const std::string& pass);
    void HandleCommand(Client* client, const std::string& command, sf::Packet& packet);
    
    void CreateRoom(Client* client, const std::string& roomID);
    void JoinRoom(Client* client, const std::string& roomID);

    void StartMatch(Room* room);
    void RemoveClient(Client* client);

    void HandleRankingCommand(Client* client, const std::string& command, sf::Packet& packet);

    std::string GenerateRandomRoomID();
    // clave con pos:nick de cada jugador, ordenada, para comparar resultados
    std::string BuildResultKey(const std::vector<std::pair<std::string, short>>& results) const;

    sf::TcpListener   _listener;
    sf::SocketSelector _selector;
    std::vector<std::unique_ptr<Client>> _clients;
    std::unordered_map<std::string, std::unique_ptr<Room>> _rooms;
    // cuántas veces se ha reportado cada resultado
    std::unordered_map<std::string, short> _pendingResults;
    DataBaseManager _db;
};