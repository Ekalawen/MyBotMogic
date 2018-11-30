
#include "Carte.h"
#include "MapTile.h"
#include "GameManager.h"
#include "Globals.h"
#include "Voisin.h"
#include "Porte.h"
#include "Noeud.h"
#include "MyBotLogic/Tools/Profiler.h"

#include <algorithm>
#include <chrono>
#include <sstream>

using std::stringstream;
using std::max;
using std::vector;
using std::endl;

Carte::Carte(const LevelInfo& _levelInfo) :
    rowCount{ _levelInfo.rowCount },
    colCount{ _levelInfo.colCount },
    nbTilesDecouvertes{ 0 }
{
    // Créer toutes les tiles !
    tiles.reserve(getNbTiles());
    for (int id = 0; id < getNbTiles(); ++id) {
        tiles.push_back(MapTile(id, *this));
    }

    // Mettre à jour les tiles connues
    for (auto tile : _levelInfo.tiles) {
        addTile(tile.second);
    }

    // Enregistrer les objets
    for (auto object : _levelInfo.objects) {
        addObject(object.second);
    }

    // Mettre à visiter les cases initiales des NPCs
    for (auto pair_npc : _levelInfo.npcs) {
        tiles[pair_npc.second.tileID].setStatut(MapTile::Statut::VISITE);
    }
}

bool Carte::isInMap(const int idTile) const noexcept {
    return idTile > -1 && idTile < rowCount * colCount;
}

vector<unsigned int> Carte::getObjectifs() const noexcept {
    return objectifs;
}


// Il s'agit de l'algorithme AStar auquel on peut rajouter un coefficiant à l'évaluation pour modifier l'heuristique.
// Par défaut sa valeur est 1. Si on l'augmente l'algorithme ira plus vite au détriment de trouver un chemin optimal.
// Si on le diminue l'algorithme se rapproche de plus en plus d'un parcours en largeur.
Chemin Carte::aStar(const int depart, const int arrivee, const float coefEvaluation) const noexcept {
   ProfilerDebug profiler{ GameManager::getLogger(), "aStar" };

   //Noeud::coefEvaluation = coefEvaluation;
   vector<Noeud> closedList;
   vector<Noeud> openList;
   openList.reserve(tiles.size());
   openList.emplace_back(depart, -1, 0, distanceHex(depart, arrivee));

   while (!openList.empty()) {

       const auto noeudIndex = openList.size() - 1;
       const auto noeudCourant = openList.back();

       if (noeudCourant.tileID == arrivee)
       {
           closedList.push_back(noeudCourant);
           break;
       }

       for (auto voisinID : tiles[noeudCourant.tileID].getVoisinsIDParEtat(Etats::ACCESSIBLE)) {
           if (voisinID == noeudCourant.tileID) continue;

           auto coutEstime = noeudCourant.cost_so_far + 1;

           const auto recherche = [&noeudCourant](const Noeud& n) {
               return n.tileID == noeudCourant.tileID;
           };

           auto closedIt = std::find_if(closedList.begin(), closedList.end(), recherche);
           auto openIt = std::find_if(openList.begin(), openList.end(), recherche);

           if (closedIt != closedList.end()) {
               if (closedIt->estimated_total_cost <= coutEstime + distanceHex(voisinID, arrivee)) continue;

               std::swap(*closedIt, closedList.back());
               closedList.pop_back();
           }
           else if (openIt != openList.end()) {
               if (openIt->cost_so_far <= coutEstime) continue;
           }

           openList.emplace_back(tiles[voisinID], coutEstime, coutEstime + distanceHex(voisinID, arrivee), noeudCourant.tileID);
       }

       closedList.push_back(noeudCourant);
       std::swap(openList[noeudIndex], openList.back());
       openList.pop_back();

       if (!openList.empty()) {
           auto plusPetitIt = std::min_element(openList.begin(), openList.end(), [](const Noeud& nL, const Noeud& nR) {
               return nL.estimated_total_cost < nR.estimated_total_cost;
           });
           std::swap(*plusPetitIt, openList.back());
       }
   }

   Chemin path;
   if (!closedList.empty() && closedList.back().tileID == arrivee) {
        auto noeud = closedList.back();

        while (noeud.tileID != -1) {
            // On enregistre dans le path ...
            path.addFirst(noeud.tileID);
            // On cherche l'antécédant ...
            for (auto n : closedList) {
                if (n.tile.getId() == noeud.idPrecedant) {
                    // On remet à jour le noeud ...
                    noeud = n;
                    break;
                }
            }
        }
   }
   else {
       path.setInaccessible();
   }

   /*
      MapGraph::Path path;

   // A path was found
   if (!nodes.empty() && nodes.back().tile == end) {
      PathNode& current_node = nodes.back();

      while (current_node.previous != Tile::INVALID_TILE_ID) {
         path.push_back(graph.tile_position(current_node.tile));

         auto it = std::find_if(std::begin(nodes), std::end(nodes), [&current_node](const PathNode& node) {
            return node.tile == current_node.previous;
         });

         if (it != std::end(nodes)) {
            current_node = *it;
         }
         else {
            throw MapGraph::NodeNotFoundInCloseList{};
         }
      }
   }

   return path;
   
   */

   return path;


    //    // On trie notre openList pour que le dernier soit le meilleur !
    //    // Donc celui qui minimise et le cout, et l'évaluation !
    //    sort(openList.begin(), openList.end(), [](const Noeud a, const Noeud b) {
    //        return a.heuristique > b.heuristique; // Par ordre décroissant
    //    });

    //    // On ferme notre noeud
    //    closedList.push_back(noeudCourant);
    //}

    //// On test si on a atteint l'objectif ou pas
    //if (noeudCourant.tile.getId() == arrivee) {
    //    // Si oui on reconstruit le path !
    //    while (noeudCourant.tile.getId() != depart) {
    //        // On enregistre dans le path ...
    //        path.addFirst(noeudCourant.tile.getId());
    //        // On cherche l'antécédant ...
    //        for (auto n : closedList) {
    //            if (n.tile.getId() == noeudCourant.idPrecedant) {
    //                // On remet à jour le noeud ...
    //                noeudCourant = n;
    //                break;
    //            }
    //        }
    //    }

    //}
    //else {
    //    // Si non le path est inaccessible !
    //    path.setInaccessible();
    //}

    //return path;
}

Tile::ETilePosition Carte::getDirection(const int ind1, const int ind2) const noexcept {
    int y = getY(ind1);
    bool pair = (y % 2 == 0);
    if (pair) {
        if (ind2 == ind1 - colCount) {
            return Tile::NE;
        }
        else if (ind2 == ind1 + 1) {
            return Tile::E;
        }
        else if (ind2 == ind1 + colCount) {
            return Tile::SE;
        }
        else if (ind2 == ind1 + colCount - 1) {
            return Tile::SW;
        }
        else if (ind2 == ind1 - 1) {
            return Tile::W;
        }
        else if (ind2 == ind1 - colCount - 1) {
            return Tile::NW;
        }
    }
    else {
        if (ind2 == ind1 - colCount + 1) {
            return Tile::NE;
        }
        else if (ind2 == ind1 + 1) {
            return Tile::E;
        }
        else if (ind2 == ind1 + colCount + 1) {
            return Tile::SE;
        }
        else if (ind2 == ind1 + colCount) {
            return Tile::SW;
        }
        else if (ind2 == ind1 - 1) {
            return Tile::W;
        }
        else if (ind2 == ind1 - colCount) {
            return Tile::NW;
        }
    }

    GAME_MANAGER_LOG_DEBUG("Erreur dans l'appel de getDirection() !");
    return Tile::CENTER;
}

int Carte::getAdjacentTileAt(const int tileSource, const Tile::ETilePosition direction) const noexcept {
    int y = getY(tileSource);
    bool pair = (y % 2 == 0);
    int res;
    switch (direction)
    {
    case Tile::NE:
        if (pair) {
            res = tileSource - colCount;
        }
        else {
            res = tileSource - colCount + 1;
        }
        break;
    case Tile::E:
        res = tileSource + 1;
        break;
    case Tile::SE:
        if (pair) {
            res = tileSource + colCount;
        }
        else {
            res = tileSource + colCount + 1;
        }
        break;
    case Tile::SW:
        if (pair) {
            res = tileSource + colCount - 1;
        }
        else {
            res = tileSource + colCount;
        }
        break;
    case Tile::W:
        res = tileSource - 1;
        break;
    case Tile::NW:
        if (pair) {
            res = tileSource - colCount - 1;
        }
        else {
            res = tileSource - colCount;
        }
        break;
    case Tile::CENTER:
        res = tileSource;
        break;
    default:
        break;
    }

    if (isInMap(res)) {
        return res;
    }
    else {
        return -1;
    }
}

float Carte::distanceL2(const int depart, const int arrivee) const noexcept {
    int xd = depart % colCount;
    int yd = depart / colCount;
    int xa = arrivee % colCount;
    int ya = arrivee / colCount;
    return (float)sqrt(pow(xd - xa, 2) + pow(yd - ya, 2));
}

int Carte::distanceHex(const int tile1ID, const int tile2ID) const noexcept {
    int ligne1 = tile1ID / colCount;
    int colonne1 = tile1ID % colCount;
    int ligne2 = tile2ID / colCount;
    int colonne2 = tile2ID % colCount;
    int x1 = colonne1 - (ligne1 - ligne1 % 2) / 2;
    int z1 = ligne1;
    int y1 = -x1 - z1;
    int x2 = colonne2 - (ligne2 - ligne2 % 2) / 2;
    int z2 = ligne2;
    int y2 = -x2 - z2;
    return max(max(abs(x1 - x2), abs(y1 - y2)), abs(z1 - z2));
}

int Carte::tailleCheminMax() const noexcept {
    return colCount * rowCount + 1;
}

// Il ne faut pas ajouter une tile qui est déjà dans la map !
void Carte::addTile(const TileInfo& tile) noexcept {
    // On met à jour le nombre de tiles
    ++nbTilesDecouvertes;

    // On la rajoute aux tiles
    tiles[tile.tileID].setTileDecouverte(tile);

    if (tiles[tile.tileID].getType() == Tile::TileAttribute_Goal) {
        objectifs.push_back(tile.tileID);
    }

    if (tiles[tile.tileID].getType() == Tile::TileAttribute_Forbidden) {
        for (auto voisin : tiles[tile.tileID].getVoisins()) {
            tiles[voisin.getTuileIndex()].removeEtat(Etats::ACCESSIBLE, tile.tileID);
        }
    }

    // Puis on met à jour les voisins de ses voisins ! :D
    for (auto voisinID : tiles[tile.tileID].getVoisinsIDParEtat(Etats::VISIBLE)) {
        // Si ce voisin l'a en voisin mystérieux, on le lui enlève
        tiles[voisinID].removeEtat(Etats::MYSTERIEUX, tile.tileID);
    }

    // On le note !
    stringstream ss;
    ss << "Decouverte de la tile " << tile.tileID;
    GAME_MANAGER_LOG_DEBUG(ss.str());
}

// Il ne faut pas ajouter un objet qui est déjà dans la map !
void Carte::addObject(const ObjectInfo& object) noexcept {
    int voisin1 = object.tileID;
    int voisin2 = getAdjacentTileAt(object.tileID, object.position);

    // On ajoute notre objet à l'ensemble de nos objets
    if (object.objectTypes.find(Object::ObjectType_Wall) != object.objectTypes.end()) {
        // Fenetre
        if (object.objectTypes.find(Object::ObjectType_Window) != object.objectTypes.end()) {
            fenetres[object.objectID] = object;
            if (isInMap(voisin1))
                tiles[voisin1].removeEtat(Etats::ACCESSIBLE, voisin2);
            if (isInMap(voisin2))
                tiles[voisin2].removeEtat(Etats::ACCESSIBLE, voisin1);
            // Mur
        }
        else {
            murs[object.objectID] = object;
            if (isInMap(voisin1)) {
                tiles[voisin1].removeEtat(Etats::MYSTERIEUX, voisin2);
                tiles[voisin1].removeEtat(Etats::ACCESSIBLE, voisin2);
                tiles[voisin1].removeEtat(Etats::VISIBLE, voisin2);
            }
            if (isInMap(voisin2)) {
                tiles[voisin2].removeEtat(Etats::MYSTERIEUX, voisin1);
                tiles[voisin2].removeEtat(Etats::ACCESSIBLE, voisin1);
                tiles[voisin2].removeEtat(Etats::VISIBLE, voisin1);
            }
        }
    }
    // Porte
    // Dans l'implémentation actuelle, les portes ne bloques plus l'accessibilitée !
    if (object.objectTypes.find(Object::ObjectType_Door) != object.objectTypes.end()) {
        // On crée notre porte
        portes[object.objectID] = Porte{ object, *this };
        // On l'ajoute à ses voisins et on les sets comme existants, car les voisins d'une porte existent forcément !
        if (isInMap(voisin1)) {
            tiles[voisin1].addPorte(object.objectID);
            tiles[voisin1].setStatut(MapTile::Statut::CONNU);
        }
        if (isInMap(voisin2)) {
            tiles[voisin2].addPorte(object.objectID);
            tiles[voisin2].setStatut(MapTile::Statut::CONNU);
        }
        //Porte Ferme
        if (object.objectStates.find(Object::ObjectState_Closed) != object.objectStates.end()) {
            // Porte Fenetre
            if (object.objectTypes.find(Object::ObjectType_Window) != object.objectTypes.end()) {
                //if (isInMap(voisin1))
                //   tiles[voisin1].removeEtat(Etats::ACCESSIBLE, voisin2);
                //if (isInMap(voisin2))
                //   tiles[voisin2].removeEtat(Etats::ACCESSIBLE, voisin1);
                // Porte
            }
            else {
                if (isInMap(voisin1)) {
                    //tiles[voisin1].removeEtat(Etats::ACCESSIBLE, voisin2);
                    tiles[voisin1].removeEtat(Etats::VISIBLE, voisin2);
                }
                if (isInMap(voisin2)) {
                    //tiles[voisin2].removeEtat(Etats::ACCESSIBLE, voisin1);
                    tiles[voisin2].removeEtat(Etats::VISIBLE, voisin1);
                }
            }
            // Porte ouverte
        }
        else {
            // Si la porte est ouverte et est accessible ET visible ! =)
        }
    }
    if (object.objectTypes.find(Object::ObjectType_PressurePlate) != object.objectTypes.end()) {
        activateurs[object.objectID] = object;
        // prout !
    }

    // On le note !

    stringstream ss;
    ss << "Decouverte de l'objet " << object.objectID << " sur la tuile " << object.tileID << " orienté en " << object.position;
    GAME_MANAGER_LOG_DEBUG(ss.str());
}

int Carte::getX(const int id) const noexcept {
    return id % colCount;
}
int Carte::getY(const int id) const noexcept {
    return id / colCount;
}
int Carte::getRowCount() const noexcept {
    return rowCount;
}

int Carte::getColCount() const noexcept {
    return colCount;
}

int Carte::getNbTiles() const noexcept {
    return getRowCount() * getColCount();
}

int Carte::getNbTilesDecouvertes() const noexcept {
    return nbTilesDecouvertes;
}

MapTile& Carte::getTile(const int id) {
    if (id < 0 || id >= getNbTiles())
        throw tile_inexistante{};
    return tiles[id];
}

const MapTile& Carte::getTile(const int id) const {
    if (id < 0 || id >= getNbTiles())
        throw tile_inexistante{};
    return tiles[id];
}

vector<unsigned int> Carte::getObjectifs() {
    return objectifs;
}

map<unsigned int, ObjectInfo> Carte::getMurs() {
    return murs;
}

map<int, Porte> Carte::getPortes() {
    return portes;
}

Porte Carte::getPorte(const int id) const noexcept {
    return portes.at(id);
}

Porte& Carte::getPorte(const int tileIdVoisine1, const int tileIdVoisine2) {
    for (auto& pair : portes) {
        Porte& porte = pair.second;
        if (porte.isVoisine(tileIdVoisine1) && porte.isVoisine(tileIdVoisine2))
            return porte;
    }
    throw porte_inexistante{};
}

map<unsigned int, ObjectInfo> Carte::getFenetres() {
    return fenetres;
}

map<unsigned int, ObjectInfo> Carte::getActivateurs() {
    return activateurs;
}

bool Carte::objectExist(const int objet) const noexcept {
    return murs.find(objet) != murs.end()
        || portes.find(objet) != portes.end()
        || fenetres.find(objet) != fenetres.end()
        || activateurs.find(objet) != activateurs.end();
}
