#pragma once
#include <vector>
#include <string>
#include <cstdint>

#include "Client.h"

class Room {

public:
    static constexpr short DEFAULT_MAX_PLAYERS = 4;

    Room(const std::string& id, short maxPlayers = DEFAULT_MAX_PLAYERS);

    bool IsFull() const;
    void AddPlayer(Client* client);

    const std::vector<Client*>& GetPlayers() const;
    std::string GetID() const;
    short GetMaxPlayers() const;


    bool waitingToStart = false;
    sf::Clock startTimer;

private:
    std::string _id;
    short _maxPlayers;
    std::vector<Client*> _players;
};