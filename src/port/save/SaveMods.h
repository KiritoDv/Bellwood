#pragma once

#include "port/save/SaveJson.h"

#include <map>
#include <set>
#include <string>
#include <vector>

// Mod content in town files, kept under shipSaveData.mods. "ids" maps the content ids used in the file to the numbers
// the file stores, per kind of content. "data" holds the values mods keep for the town. "collection" lists the custom
// content each player in the file has owned, for the catalog.
namespace SaveMods {

// Renumbers the custom content of a file read from disk to this install's numbers. Returns true if anything changed.
bool Remap(const SaveJson& ship, Save_t* land, mCD_keep_mail_c* mail, const std::vector<mCD_foreigner_c*>& passports);
// Writes the ids of the custom content used in the file.
void WriteIds(SaveJson& ship, const Save_t* land, const mCD_keep_mail_c* mail,
              const std::vector<const mCD_foreigner_c*>& passports);

// Adds the catalog collections of a file read from disk to the ones in memory.
void ReadCollections(const SaveJson& ship);
// Writes the collections of the players in the file.
void WriteCollections(SaveJson& ship, const Save_t* land, const std::vector<const mCD_foreigner_c*>& passports);
// Custom content ids each player has owned, by PlayerKey.
std::map<std::string, std::set<std::string>>& Collections();
std::string PlayerKey(const PersonalID_c& pid);

// Reads the mod data of the town being loaded.
void LoadTown(const SaveJson& ship);
// Warns about content of the loaded town that no loaded mod registers.
void CheckMissing();
// Writes the mod data of the town in play.
void StoreTown(SaveJson& ship);

// Mod data of the town in play.
SaveJson& TownData();

} // namespace SaveMods
