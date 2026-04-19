#pragma once
#include <SFML/Network.hpp>
#include <vector>
#include <unordered_map>
#include <memory>
#include "Client.h"
#include "Room.h"
#include "DataBaseManager.h"
#include <string>

class BootstrapServer {
public:
    BootstrapServer(unsigned short port = 50000);
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
    std::string BuildResultKey(const std::vector<std::pair<std::string, int>>& results) const;

    sf::TcpListener   _listener;
    sf::SocketSelector _selector;
    std::vector<std::unique_ptr<Client>> _clients;
    std::unordered_map<std::string, std::unique_ptr<Room>> _rooms;
    // cuántas veces se ha reportado cada resultado
    std::unordered_map<std::string, int> _pendingResults;
    DataBaseManager _db;
};