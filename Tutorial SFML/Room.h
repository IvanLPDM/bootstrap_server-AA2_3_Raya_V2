#pragma once
#include <vector>
#include <string>
#include "Client.h"

class Room {

public:

    Room(const std::string& id, int maxPlayers = 4);

    bool IsFull() const;
    void AddPlayer(Client* client);

    const std::vector<Client*>& GetPlayers() const;
    std::string GetID() const;
    int GetMaxPlayers() const;


    bool waitingToStart = false;
    sf::Clock startTimer;

private:
    std::string _id;
    int _maxPlayers;
    std::vector<Client*> _players;
};