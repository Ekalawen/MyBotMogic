
#include "ScoreStrategie.h"
#include "MyBotLogic/BehaviorTree/BT_Noeud.h"
#include "MyBotLogic/GameManager.h"
#include "MyBotLogic/Tools/Minuteur.h"
#include "MyBotLogic/MapTile.h"

#include <sstream>
#include <string>
#include <vector>

using std::string;
using std::stringstream;
using std::endl;

ScoreStrategie::ScoreStrategie(GameManager& _manager, string _nom)
   : manager{ _manager },
   nom{ _nom }
{
}

BT_Noeud::ETAT_ELEMENT ScoreStrategie::execute() noexcept {
   auto pre = Minuteur::now();

   stringstream ss;
   ss << nom << endl;

   // On ne sait pas o� se trouvent les objectifs !
   // On va les chercher !

   // Pour �a chaque npc va visiter en premier les tuiles avec le plus haut score

   // L'ensemble des tiles que l'on va visiter
   vector<int> tilesAVisiter;

   for (auto& pair : manager.getNpcs()) {
      Npc& npc = pair.second;
      npc.resetChemins();

      // Calculer le score de chaque tile pour le npc
      // En m�me temps on calcul le chemin pour aller � cette tile
      // On stocke ces deux informations dans l'attribut cheminsPossibles du Npc
      auto preCalcul = Minuteur::now();
      calculerScoresTilesPourNpc(npc, tilesAVisiter);
      auto postCalcul = Minuteur::now();
      ss << "Dur�e calculerScoresEtCheminsTilesPourNpc = " << Minuteur::dureeMicroseconds(preCalcul, postCalcul) / 1000.f << "ms" << endl;

      // Choisir la meilleure tile pour ce npc et lui affecter son chemin
      auto preAffect = Minuteur::now();
      int tileChoisi = npc.affecterMeilleurChemin(manager.c);
      auto postAffect = Minuteur::now();
      ss << "Dur�e AffectationChemin = " << Minuteur::dureeMicroseconds(preAffect, postAffect) / 1000.f << "ms" << endl;

      // Mettre � jour les tilesAVisiter
      tilesAVisiter.push_back(tileChoisi);
   }

   // Temps d'execution
   auto post = Minuteur::now();
   ss << "Dur�e " << nom << " = " << Minuteur::dureeMicroseconds(pre, post) / 1000.f << "ms";

   GameManager::log(ss.str());
   return ETAT_ELEMENT::REUSSI;
}

void ScoreStrategie::calculerScore1Tile(int _tileID, Carte& _carte, Npc& _npc, const vector<int>& _tilesAVisiter) {
   MapTile tile = _carte.getTile(_tileID);
   // On ne consid�re la tile que si on ne la visite pas d�j� !
   if (tile.getStatut() == MapTile::Statut::CONNU && find(_tilesAVisiter.begin(), _tilesAVisiter.end(), tile.getId()) == _tilesAVisiter.end()) {
      saveScore(tile, _npc, _tilesAVisiter);
   }
}

// Calcul le score de chaque tiles et son chemin pour un npc
// On prend en compte les tilesAVisiter des autres npcs pour que les tiles soient loins les unes des autres
void ScoreStrategie::calculerScoresTilesPourNpc(Npc& _npc, const vector<int>& _tilesAVisiter) noexcept {
   stringstream ss;
   ss << "Taille ensemble : " << _npc.getEnsembleAccessible().size();
   GameManager::log(ss.str());
   for (auto score : _npc.getEnsembleAccessible()) { // parcours toutes les tiles d�couvertes par l'ensemble des npcs et qui sont accessibles
      calculerScore1Tile(score.tuileID, manager.c, _npc, _tilesAVisiter);
   }
}

