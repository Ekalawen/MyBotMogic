#include "ObjectifsForAllNpcs.h"
#include "../BehaviorTree/BT_Noeud.h"
#include "../GameManager.h"

BT_Noeud::ETAT_ELEMENT ObjectifsForAllNpcs::execute() noexcept {
    GameManager::log("ObjectifsForAllNpcs");
    if (gm.c.getObjectifs().size() >= gm.getNpcs().size()) {
        return ETAT_ELEMENT::REUSSI;
    } else {
        return ETAT_ELEMENT::ECHEC;
    }
}
