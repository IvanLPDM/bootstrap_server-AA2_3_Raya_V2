#include "Room.h"

Room::Room(const std::string& id, int maxPlayers) : _id(id), _maxPlayers(maxPlayers) {}

bool Room::IsFull() const { return _players.size() >= _maxPlayers; }

void Room::AddPlayer(Client* client) 
{
	if (IsFull()) return;

	_players.push_back(client); 
}

const std::vector<Client*>& Room::GetPlayers() const { return _players; }

std::string Room::GetID() const { return _id; }
int Room::GetMaxPlayers() const { return _maxPlayers; }